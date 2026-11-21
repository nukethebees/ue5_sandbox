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

// The frontend appends named parameters and bindings in declaration order.
static auto material_row(std::span<material_synth::SourceSpan const> rows,
                         sexpr::Sources const& sources,
                         sexpr::SourceSpan span) -> std::optional<std::size_t> {
    auto const location{sources.location(span)};
    auto const found{std::ranges::partition_point(rows, [&](auto const& origin) {
        return origin.line < location.line ||
               (origin.line == location.line && origin.column < location.column);
    })};
    if (found == rows.end() || found->path != sources.path_id(span.file) ||
        found->line != location.line || found->column != location.column) {
        return {};
    }
    return static_cast<std::size_t>(found - rows.begin());
}
static void
    describe_parameter(DeclarationDetails& card, Compiler const& compiler, Symbol const& symbol) {
    auto const& storage{compiler.storage()};
    auto const& syntax{storage.syntax};
    auto const& material{compiler.material()};
    auto const& params{material.parameters};
    auto const row{material_row(params.span, storage.sources, syntax.token_span(symbol.form))};
    if (!row || params.name[*row] != syntax.identifier(symbol.name)) {
        return;
    }
    auto const i{*row};
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

static void describe_expression(DeclarationDetails& card,
                                material_synth::MaterialIR const& material,
                                material_synth::NodeHandle node) {
    using enum material_synth::NodeKind;
    auto const& nodes{material.nodes};
    card.field("Result type", to_serialized_string(nodes.type(node)));
    if (nodes.kind(node) == component_mask) {
        card.field("Channels", material.text(nodes.component_mask(node)));
        node = nodes.inputs(node).front();
    }
    if (nodes.kind(node) == sample) {
        auto const input{nodes.inputs(node).front()};
        if (nodes.kind(input) == parameter) {
            auto const id{nodes.parameter(input)};
            card.field("Texture", material.text(material.parameters.name[id.value]));
        } else if (nodes.kind(input) == texture_object) {
            card.field("Texture", material.text(nodes.texture_path(input)));
        }
        card.field("Sampler", to_serialized_string(nodes.texture_sampler_type(node)));
    }
}

static auto hover_result(std::string text,
                         sexpr::Sources const& sources,
                         sexpr::SourceSpan span,
                         MarkupKind format) -> Json {
    return {{"contents", {{"kind", to_serialized_string(format)}, {"value", std::move(text)}}},
            {"range", range(sources, span)}};
}
auto Analysis::expression_hover(sexpr::FormId form, sexpr::FormId use, MarkupKind format) const
    -> Json {
    auto const& storage{compiler_.storage()};
    auto const span{storage.syntax.token_span(form)};
    auto const location{storage.sources.location(span)};
    auto const path{storage.sources.path_id(span.file)};
    auto const& material{compiler_.material()};
    for (auto node : material.nodes.handles()) {
        auto const origin{material.nodes.span(node)};
        if (origin.path == path && origin.line == location.line &&
            origin.column == location.column) {
            DeclarationDetails card{format, {}};
            describe_expression(card, material, node);
            return hover_result(
                std::move(card.text), storage.sources, storage.syntax.token_span(use), format);
        }
    }
    return nullptr;
}

static auto leading_comment(Document const& doc,
                            sexpr::Sources const& sources,
                            sexpr::SourceSpan span,
                            MarkupKind format) -> std::string {
    auto const line{sources.location(span).line - 1};
    auto const offsets{sources.line_offsets(doc.file)};
    auto const source{sources.text(doc.file)};
    auto const indent{source.substr(offsets[line], span.offset - offsets[line])};
    if (indent.find_first_not_of(" \t") != std::string_view::npos) {
        return {};
    }
    auto it{std::ranges::lower_bound(doc.comment_lines, line)};
    auto first{line};
    while (it != doc.comment_lines.begin() && *std::prev(it) + 1 == first) {
        --it;
        --first;
    }
    std::string result;
    for (auto n{first}; n < line; ++n) {
        auto const raw{source.substr(offsets[n], offsets[n + 1] - offsets[n])};
        auto const start{raw.find_first_not_of(" \t;")};
        auto const end{raw.find_last_not_of(" \t\r\n")};
        if (n != first) {
            result += '\n';
        }
        if (start <= end) {
            for (char c : raw.substr(start, end - start + 1)) {
                if (format == MarkupKind::markdown &&
                    std::string_view{R"(\`*_[]<>#!|~&)"}.contains(c)) {
                    result += '\\';
                }
                result += c;
            }
        }
    }
    return result;
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
    } else if (head == FormHead::let && symbol.kind == SemanticKind::variable) {
        auto const& material{compiler_.material()};
        if (auto row{material_row(
                material.bindings.span, storage.sources, syntax.token_span(symbol.form))}) {
            describe_expression(card, material, material.bindings.node[*row]);
        }
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
        auto const comment{
            leading_comment(doc, storage.sources, syntax.token_span(symbol->form), format)};
        if (!comment.empty()) {
            text += "\n\n";
            text += comment;
        }
        auto const details{declaration_details(doc, *symbol, format)};
        if (!details.empty()) {
            text += format == MarkupKind::markdown ? "\n\n" : "\n";
            text += details;
        }
    }
    return hover_result(std::move(text), storage.sources, syntax.span(use), format);
}
}
