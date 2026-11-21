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

static auto code_block(MarkupKind format, std::string_view text, std::string_view language)
    -> std::string {
    if (format == MarkupKind::markdown) {
        auto const fence{code_delimiter(text, 3)};
        return std::format("{}{}\n{}\n{}", fence, language, text, fence);
    }
    return std::string{text};
}

struct DeclarationDetails {
    void field(std::string_view label, std::string_view value) {
        if (!text.empty()) {
            text += format == MarkupKind::markdown ? "\n\n" : "\n";
        }
        if (format == MarkupKind::markdown) {
            auto const fence{code_delimiter(value, 1)};
            text += std::format("**{}:** {} {} {}", label, fence, value, fence);
        } else {
            text += std::format("{}: {}", label, value);
        }
    }

    MarkupKind format;
    std::string text;
};

static void
    describe_parameter(DeclarationDetails& card, Compiler const& compiler, Symbol const& symbol) {
    auto const& storage{compiler.storage()};
    auto const& syntax{storage.syntax};
    auto const name{syntax.identifier(symbol.name)};
    auto const span{syntax.token_span(symbol.form)};
    auto const location{storage.sources.location(span)};
    auto const& material{compiler.material()};
    auto const& params{material.parameters};
    // The material frontend appends parameters in source order.
    auto const found{std::ranges::partition_point(params.span, [&](auto const& origin) {
        return origin.line < location.line ||
               (origin.line == location.line && origin.column < location.column);
    })};
    if (found == params.span.end() || found->path != storage.sources.path_id(span.file) ||
        found->line != location.line || found->column != location.column) {
        return;
    }
    auto const i{static_cast<std::size_t>(found - params.span.begin())};
    if (params.name[i] != name) {
        return;
    }
    card.field("Parameter type", syntax.text(syntax.child(symbol.form, 1)));
    auto const type{params.type[i]};
    if (type == material_synth::ValueType::texture) {
        card.field("Default texture", material.text(params.texture_path[i]));
        card.field("Sampler", to_serialized_string(params.texture_sampler_type[i]));
        return;
    }
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

static void
    describe_registered_type(DeclarationDetails& card, Compiler const& compiler, StringId name) {
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

static void describe_layout(DeclarationDetails& card,
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

auto Analysis::signature(Symbol const& symbol) const -> std::string_view {
    auto const& storage{compiler_.storage()};
    auto const span{storage.syntax.span(symbol.form)};
    auto const source{
        storage.sources.text(span.file).substr(span.offset, span.end_offset - span.offset)};
    return source.substr(0, source.find_first_of("\r\n"));
}
auto Analysis::declaration_details(Document const& doc,
                                   Symbol const& symbol,
                                   MarkupKind format) const -> std::string {
    auto const& storage{compiler_.storage()};
    auto const& syntax{storage.syntax};
    DeclarationDetails card{format, {}};
    auto const head{try_parse_serialized_form_head(syntax.head(symbol.form))};
    auto const name{syntax.identifier(symbol.name)};
    if (head == FormHead::parameter) {
        describe_parameter(card, compiler_, symbol);
    } else if (head == FormHead::type && !symbol.parent.value) {
        describe_registered_type(card, compiler_, name);
    } else if (head == FormHead::member && symbol.parent.value) {
        auto const owner{doc.symbols[symbol.parent.value - 1].type};
        if (schema::is_valid(owner)) {
            describe_layout(card, compiler_.types(), owner, name);
        }
    } else if (schema::is_valid(symbol.type)) {
        auto const& graph{compiler_.types()};
        auto const spelling{storage.strings.view(graph.cpp_spelling(symbol.type))};
        if (!spelling.empty()) {
            card.field("C++ type", spelling);
            auto const settings{storage.declarations.module_settings(graph.module(symbol.type))};
            auto const& modules{storage.declarations.module_settings_store};
            card.field("Header",
                       storage.strings.view(
                           modules.header_include[settings].value_or(modules.header[settings])));
        }
    }
    return std::move(card.text);
}
auto Analysis::hover(Document const& doc,
                     sexpr::SourceSpan declaration,
                     sexpr::FormId use,
                     MarkupKind format) const -> Json {
    auto const& storage{compiler_.storage()};
    auto const& syntax{storage.syntax};
    std::string text;
    if (!declaration.end_offset && syntax.kind(use) == sexpr::TokenKind::string) {
        text = code_block(format, codegen::generic_path_utf8(doc.path), "");
    } else {
        auto const symbol{std::ranges::find_if(doc.symbols, [&](auto const& item) {
            return syntax.token_span(item.name).offset == declaration.offset;
        })};
        if (symbol == doc.symbols.end()) {
            return nullptr;
        }
        text = code_block(format, signature(*symbol), "lispb");
        auto const details{declaration_details(doc, *symbol, format)};
        if (!details.empty()) {
            text += format == MarkupKind::markdown ? "\n\n" : "\n";
            text += details;
        }
    }
    return {{"contents", {{"kind", to_serialized_string(format)}, {"value", std::move(text)}}},
            {"range", range(storage.sources, syntax.span(use))}};
}
}
