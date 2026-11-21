#include "analysis.h"

#include <codegen/path_utils.h>
#include <codegen/sexpr/source_location.h>

#include <algorithm>
#include <format>

namespace lispb::lsp {

// Use a delimiter longer than any run of backticks in authored text.
static auto code_delimiter(std::string_view text, std::size_t minimum) -> std::string {
    std::size_t run{};
    for (char c : text) {
        run = c == '`' ? run + 1 : 0;
        minimum = std::max(minimum, run + 1);
    }
    return std::string(minimum, '`');
}

struct HoverCard {
    HoverCard(MarkupKind format, std::string_view signature, std::string_view language = "lispb")
        : format{format}
        , text{signature} {
        if (format == MarkupKind::markdown) {
            auto const fence{code_delimiter(signature, 3)};
            text = std::format("{}{}\n{}\n{}", fence, language, signature, fence);
        }
    }

    void field(std::string_view label, std::string_view value) {
        if (format == MarkupKind::markdown) {
            auto const fence{code_delimiter(value, 1)};
            text += std::format("\n\n**{}:** {} {} {}", label, fence, value, fence);
        } else {
            text += std::format("\n{}: {}", label, value);
        }
    }

    auto result(Json selection) && -> Json {
        return {{"contents", {{"kind", to_serialized_string(format)}, {"value", std::move(text)}}},
                {"range", std::move(selection)}};
    }

    MarkupKind format;
    std::string text;
};

static void describe_parameter(HoverCard& card, Compiler const& compiler, Symbol const& symbol) {
    auto const& storage{compiler.storage()};
    auto const& syntax{storage.syntax};
    auto const name{syntax.identifier(symbol.name)};
    auto const span{syntax.token_span(symbol.form)};
    auto const location{storage.sources.location(span)};
    auto const& material{compiler.material()};
    auto const& params{material.parameters};
    for (auto id : params.handles()) {
        auto const i{id.value};
        auto const origin{params.span[i]};
        if (params.name[i] != name || origin.path != storage.sources.path_id(span.file) ||
            origin.line != location.line || origin.column != location.column) {
            continue;
        }
        card.field("Parameter type", syntax.text(syntax.child(symbol.form, 1)));
        auto const type{params.type[i]};
        if (type == material_synth::ValueType::texture) {
            card.field("Default texture", material.text(params.texture_path[i]));
            card.field("Sampler", to_serialized_string(params.texture_sampler_type[i]));
        } else {
            std::string values;
            auto const count{material_synth::component_count(type)};
            for (std::size_t j{}; j < count; ++j) {
                if (j) {
                    values += ", ";
                }
                values += std::format("{}", params.default_values[j][i]);
            }
            card.field("Default", values);
        }
        return;
    }
}

static void describe_registered_type(HoverCard& card, Compiler const& compiler, StringId name) {
    auto const& registry{compiler.manifest().types};
    if (auto id{registry.find(name)}) {
        auto const type{registry.registered.cpp_type[registry.registered.index(*id)]};
        auto const& cpp{registry.cpp_types};
        card.field("C++ type", cpp.spelling(type));
        for (auto dependency : cpp.dependencies(type)) {
            if (auto header{cpp.dependency_header(dependency)}) {
                card.field("Header", *header);
            }
        }
    }
}

static void describe_layout(HoverCard& card,
                            schema::TypeGraph const& graph,
                            schema::TypeId owner,
                            StringId member) {
    auto const definition{graph.definition(owner)};
    auto const& defs{graph.definitions()};
    if (!(definition.kind() == schema::TypeDefinitionKind::record &&
          defs.records.layout[definition] == codegen::LayoutKind::packed) &&
        !(definition.kind() == schema::TypeDefinitionKind::union_type &&
          defs.unions.layout[definition] == codegen::LayoutKind::packed)) {
        return;
    }
    auto const& layout{graph.bit_layout(owner)};
    auto const& placements{layout.placements};
    auto const found{std::ranges::find_if(placements, [&](auto const& field) {
        return !field.parent && defs.children.name[field.child] == member;
    })};
    if (found == placements.end()) {
        return;
    }
    card.field("Packed owner", graph.strings().view(graph.name(owner)));
    card.field("Bit offset", std::to_string(found->offset));
    card.field("Bit width", std::to_string(found->bits));
    if (found->extent != found->bits) {
        card.field("Field extent", std::format("{} bits", found->extent));
    }
    if (layout.storage_bits) {
        card.field("Owner storage", std::format("{} bits", layout.storage_bits));
    }
    if (auto tag{defs.children.tag[found->child]}) {
        card.field("Discriminator", graph.strings().view(*tag));
        auto const index{static_cast<std::uint32_t>(found - placements.begin())};
        for (auto const& alternative : placements) {
            if (alternative.parent == index) {
                for (auto const& guard : alternative.guards) {
                    card.field(
                        "Active case",
                        std::format(
                            "{} when {} = {}", alternative.source_path, guard.path, guard.value));
                }
            }
        }
    }
}

auto Analysis::hover(Document const& doc,
                     sexpr::SourceSpan declaration,
                     sexpr::FormId use,
                     MarkupKind format) const -> Json {
    auto const& storage{compiler_.storage()};
    auto const& syntax{storage.syntax};
    auto selection = range(storage.sources, syntax.span(use));
    if (!declaration.end_offset && syntax.kind(use) == sexpr::TokenKind::string) {
        return HoverCard{format, codegen::generic_path_utf8(doc.path), ""}.result(
            std::move(selection));
    }

    auto const symbol{std::ranges::find_if(doc.symbols, [&](auto const& item) {
        return syntax.token_span(item.name).offset == declaration.offset;
    })};
    if (symbol == doc.symbols.end()) {
        return nullptr;
    }
    auto const span{syntax.span(symbol->form)};
    auto const source{
        storage.sources.text(span.file).substr(span.offset, span.end_offset - span.offset)};
    HoverCard card{format, source.substr(0, source.find_first_of("\r\n"))};
    auto const head{try_parse_serialized_form_head(syntax.head(symbol->form))};
    auto const name{syntax.identifier(symbol->name)};
    if (head == FormHead::parameter) {
        describe_parameter(card, compiler_, *symbol);
    } else if (head == FormHead::type && !symbol->parent.value) {
        describe_registered_type(card, compiler_, name);
    } else if (head == FormHead::member && symbol->parent.value) {
        auto const owner{doc.symbols[symbol->parent.value - 1].type};
        if (schema::is_valid(owner)) {
            describe_layout(card, compiler_.types(), owner, name);
        }
    } else if (schema::is_valid(symbol->type)) {
        auto const& graph{compiler_.types()};
        auto const spelling{storage.strings.view(graph.cpp_spelling(symbol->type))};
        if (!spelling.empty()) {
            card.field("C++ type", spelling);
            auto const settings{storage.declarations.module_settings(graph.module(symbol->type))};
            auto const& modules{storage.declarations.module_settings_store};
            card.field("Header",
                       storage.strings.view(
                           modules.header_include[settings].value_or(modules.header[settings])));
        }
    }
    return std::move(card).result(std::move(selection));
}
}
