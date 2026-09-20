#include <lispb/schema/editable_document.h>

#include <codegen/manifest_error.h>
#include <codegen/sexpr/reader.h>
#include <codegen/source_loader.h>
#include <codegen/validation.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <functional>
#include <iterator>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <utility>

#if defined(_WIN32)
#include <Windows.h>
#endif

namespace lispb::schema {
namespace {

using codegen::sexpr::Form;

auto read_file(std::filesystem::path const& path) -> std::string {
    std::ifstream input{path, std::ios::binary};
    if (!input) {
        throw codegen::ManifestError{"Cannot open manifest file: " + path.string()};
    }
    return std::string{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}

auto form_range(Form const& form, std::size_t const source_file_index) -> SourceRange {
    auto const end_offset{form.is_list() ? form.closing.span.offset + 1
                                         : form.token.span.offset + form.token.text.size()};
    return {.source_file_index = source_file_index,
            .begin_offset = form.token.span.offset,
            .end_offset = end_offset,
            .line = form.token.span.line,
            .column = form.token.span.column};
}

auto quote(std::string_view const value) -> std::string {
    std::string result{"\""};
    for (auto const character : value) {
        switch (character) {
            case '\\':
                result += "\\\\";
                break;
            case '"':
                result += "\\\"";
                break;
            case '\n':
                result += "\\n";
                break;
            case '\r':
                result += "\\r";
                break;
            case '\t':
                result += "\\t";
                break;
            default:
                result += character;
                break;
        }
    }
    result += '"';
    return result;
}

auto render_type_ref(codegen::TypeRef const& type) -> std::string {
    if (type.suffix.empty() && !type.nested.has_value()) {
        return type.name;
    }
    auto result{"(type-ref " + type.name};
    if (!type.suffix.empty()) {
        result += " :suffix " + quote(type.suffix);
    }
    if (type.nested.has_value()) {
        result += " :nested " + quote(*type.nested);
    }
    return result + ')';
}

auto reflection_name(codegen::EnumReflection const reflection) -> std::string_view {
    switch (reflection) {
        case codegen::EnumReflection::none:
            return "none";
        case codegen::EnumReflection::uenum:
            return "uenum";
        case codegen::EnumReflection::blueprint:
            return "blueprint";
    }
    return "none";
}

auto conversion_name(codegen::EnumConversion const conversion) -> std::string_view {
    switch (conversion) {
        case codegen::EnumConversion::lex_to_string:
            return "lex-to-string";
        case codegen::EnumConversion::string_view:
            return "string-view";
        case codegen::EnumConversion::string:
            return "string";
        case codegen::EnumConversion::lex_to_display_string:
            return "lex-to-display-string";
        case codegen::EnumConversion::display_string_view:
            return "display-string-view";
        case codegen::EnumConversion::display_string:
            return "display-string";
        case codegen::EnumConversion::lex_to_serialized_string:
            return "lex-to-serialized-string";
        case codegen::EnumConversion::try_parse_serialized:
            return "try-parse-serialized";
    }
    return "lex-to-string";
}

auto render_enum_value(codegen::EnumeratorSchema const& value) -> std::string {
    std::ostringstream output;
    output << "(value " << value.name;
    if (value.initializer.has_value()) {
        output << "\n      :value " << quote(*value.initializer);
    }
    if (value.display_name.has_value()) {
        output << "\n      :display-name " << quote(*value.display_name);
    }
    if (value.hidden) {
        output << "\n      :hidden true";
    }
    if (value.serialized_name.has_value()) {
        output << "\n      :serialized-name " << quote(*value.serialized_name);
    }
    if (value.sentinel) {
        output << "\n      :sentinel true";
    }
    output << ')';
    return output.str();
}

auto render_enum(codegen::EnumSchema const& schema) -> std::string {
    std::ostringstream output;
    output << "(enum " << schema.name << ' ' << render_type_ref(schema.underlying_type);
    if (schema.bit_width.has_value()) {
        output << "\n    :bit-width " << *schema.bit_width;
    }
    if (schema.signedness.has_value()) {
        output << "\n    :signed " << (*schema.signedness ? "true" : "false");
    }
    if (schema.reflection != codegen::EnumReflection::none) {
        output << "\n    :reflection " << reflection_name(schema.reflection);
    }
    if (schema.enum_array) {
        output << "\n    :enum-array true";
    }
    if (schema.count.has_value()) {
        output << "\n    :count " << *schema.count;
    }
    if (!schema.conversions.empty()) {
        output << "\n    :conversions (";
        for (std::size_t index{}; index < schema.conversions.size(); ++index) {
            output << (index == 0 ? "" : " ") << conversion_name(schema.conversions[index]);
        }
        output << ')';
    }
    if (schema.export_specifier.has_value()) {
        output << "\n    :export-specifier " << *schema.export_specifier;
    }
    if (schema.native_api) {
        output << "\n    :native-api true";
    }
    for (auto const& value : schema.values) {
        output << "\n    " << render_enum_value(value);
    }
    if (schema.unreal_projection.has_value()) {
        auto const& projection{*schema.unreal_projection};
        output << "\n    (unreal-projection " << projection.name << "\n      :header "
               << quote(projection.header.string()) << "\n      :header-include "
               << quote(projection.header_include) << "\n      :conversion-header "
               << quote(projection.conversion_header.string()) << "\n      :native-header-include "
               << quote(projection.native_header_include);
        if (projection.reflection != codegen::EnumReflection::uenum) {
            output << "\n      :reflection " << reflection_name(projection.reflection);
        }
        output << ')';
    }
    output << ')';
    return output.str();
}

auto source_form_end(Form const& form, std::string_view const source)
    -> std::optional<std::size_t> {
    if (form.is_list()) {
        return form.closing.span.offset + 1;
    }
    auto const begin{form.token.span.offset};
    if (begin >= source.size()) {
        return std::nullopt;
    }
    if (form.token.kind == codegen::sexpr::TokenKind::string) {
        auto escaped{false};
        for (auto index{begin + 1}; index < source.size(); ++index) {
            auto const character{source[index]};
            if (!escaped && character == '"') {
                return index + 1;
            }
            if (!escaped && character == '\\') {
                escaped = true;
            } else {
                escaped = false;
            }
        }
        return std::nullopt;
    }
    auto index{begin};
    while (index < source.size()) {
        auto const character{source[index]};
        if (character == '(' || character == ')' || character == ';' ||
            std::isspace(static_cast<unsigned char>(character)) != 0) {
            break;
        }
        ++index;
    }
    return index;
}

auto source_forms_equal(Form const& left, Form const& right) -> bool {
    if (left.is_list() != right.is_list()) {
        return false;
    }
    if (!left.is_list()) {
        return left.token.kind == right.token.kind && left.token.text == right.token.text;
    }
    if (left.children.size() != right.children.size()) {
        return false;
    }
    for (std::size_t index{}; index < left.children.size(); ++index) {
        if (!source_forms_equal(left.children[index], right.children[index])) {
            return false;
        }
    }
    return true;
}

auto source_form_matches_rendered(Form const& form, std::string_view const rendered) -> bool {
    if (!form.is_list()) {
        if (form.token.kind == codegen::sexpr::TokenKind::string) {
            return rendered == form.token.text || rendered == quote(form.token.text);
        }
        return rendered == form.token.text;
    }

    try {
        auto const rendered_forms{codegen::sexpr::read_forms("rendered enum property", rendered)};
        return rendered_forms.size() == 1 && source_forms_equal(form, rendered_forms.front());
    } catch (std::exception const&) {
        return false;
    }
}

struct SourceReplacement {
    std::size_t begin{};
    std::size_t end{};
    std::string text;
};

using SourceProperty = std::pair<std::string_view, std::optional<std::string>>;

auto replace_source_form(Form const& target,
                         std::string text,
                         std::string_view const original,
                         std::vector<SourceReplacement>& replacements) -> bool {
    auto const end{source_form_end(target, original)};
    if (!end.has_value()) {
        return false;
    }
    replacements.push_back(
        {.begin = target.token.span.offset, .end = *end, .text = std::move(text)});
    return true;
}

auto patch_source_form(Form const& target,
                       std::string const& rendered,
                       std::string_view const original,
                       std::vector<SourceReplacement>& replacements) -> bool {
    return source_form_matches_rendered(target, rendered) ||
           replace_source_form(target, rendered, original, replacements);
}

auto patch_source_properties(Form const& target,
                             std::size_t const positional_count,
                             std::span<SourceProperty const> properties,
                             std::string_view const indentation,
                             std::string_view const original,
                             std::vector<SourceReplacement>& replacements) -> bool {
    std::map<std::string_view, std::pair<Form const*, Form const*>> existing;
    for (auto index{positional_count + 1}; index < target.children.size();) {
        auto const& child{target.children[index]};
        if (child.token.kind == codegen::sexpr::TokenKind::keyword &&
            index + 1 < target.children.size()) {
            existing.emplace(child.token.text, std::pair{&child, &target.children[index + 1]});
            index += 2;
        } else {
            ++index;
        }
    }

    std::string additions;
    for (auto const& [name, value] : properties) {
        auto const found{existing.find(name)};
        if (found == existing.end()) {
            if (value.has_value()) {
                additions +=
                    "\n" + std::string{indentation} + ":" + std::string{name} + " " + *value;
            }
            continue;
        }
        auto const& [keyword, existing_value]{found->second};
        if (value.has_value()) {
            if (!patch_source_form(*existing_value, *value, original, replacements)) {
                return false;
            }
        } else {
            auto const keyword_end{source_form_end(*keyword, original)};
            auto const value_end{source_form_end(*existing_value, original)};
            if (!keyword_end.has_value() || !value_end.has_value()) {
                return false;
            }
            replacements.push_back(
                {.begin = keyword->token.span.offset, .end = *keyword_end, .text = {}});
            replacements.push_back(
                {.begin = existing_value->token.span.offset, .end = *value_end, .text = {}});
        }
    }
    if (!additions.empty()) {
        replacements.push_back({.begin = target.closing.span.offset,
                                .end = target.closing.span.offset,
                                .text = std::move(additions)});
    }
    return true;
}

auto apply_source_replacements(std::string_view const original,
                               std::vector<SourceReplacement> replacements)
    -> std::optional<std::string> {
    std::ranges::sort(replacements,
                      [](SourceReplacement const& left, SourceReplacement const& right) {
                          if (left.begin != right.begin) {
                              return left.begin > right.begin;
                          }
                          return left.end > right.end;
                      });
    auto result{std::string{original}};
    for (auto& replacement : replacements) {
        if (replacement.end > result.size() || replacement.begin > replacement.end) {
            return std::nullopt;
        }
        result.replace(
            replacement.begin, replacement.end - replacement.begin, std::move(replacement.text));
    }
    return result;
}

auto apply_source_replacements_to_range(std::string_view const original,
                                        std::size_t const begin,
                                        std::size_t const end,
                                        std::vector<SourceReplacement> replacements)
    -> std::optional<std::string> {
    if (begin > end || end > original.size()) {
        return std::nullopt;
    }
    for (auto& replacement : replacements) {
        if (replacement.begin < begin || replacement.end > end) {
            return std::nullopt;
        }
        replacement.begin -= begin;
        replacement.end -= begin;
    }
    return apply_source_replacements(original.substr(begin, end - begin), std::move(replacements));
}

auto source_form_line_end(Form const& form, std::string_view const source, std::size_t const limit)
    -> std::optional<std::size_t> {
    auto const form_end{source_form_end(form, source)};
    if (!form_end.has_value() || *form_end > limit || limit > source.size()) {
        return std::nullopt;
    }

    auto cursor{*form_end};
    while (cursor < limit && source[cursor] != '\n' &&
           std::isspace(static_cast<unsigned char>(source[cursor])) != 0) {
        ++cursor;
    }
    if (cursor < limit && source[cursor] == ';') {
        while (cursor < limit && source[cursor] != '\n') {
            ++cursor;
        }
    }
    if (cursor < limit && source[cursor] == '\n') {
        return cursor + 1;
    }
    return *form_end;
}

auto parse_owned_source_declaration(std::string_view const original,
                                    std::string_view const head,
                                    std::string_view const name) -> std::optional<Form> {
    try {
        auto forms{codegen::sexpr::read_forms("editable declaration", original)};
        if (forms.size() != 1 || forms.front().head() != head ||
            forms.front().children.size() < 2 || forms.front().children[1].token.text != name) {
            return std::nullopt;
        }
        return std::move(forms.front());
    } catch (std::exception const&) {
        return std::nullopt;
    }
}

auto try_render_source_preserved_enum(codegen::EnumSchema const& schema,
                                      std::string_view const original)
    -> std::optional<std::string> {
    auto parsed{parse_owned_source_declaration(original, "enum", schema.name)};
    if (!parsed.has_value()) {
        return std::nullopt;
    }
    auto const& form{*parsed};
    if (form.children.size() < 3) {
        return std::nullopt;
    }
    auto const& underlying{form.children[2]};
    if (underlying.is_list() || !schema.underlying_type.suffix.empty() ||
        schema.underlying_type.nested.has_value()) {
        return std::nullopt;
    }

    std::vector<Form const*> values;
    for (auto const& child : form.children) {
        if (child.head() == "unreal-projection") {
            return std::nullopt;
        }
        if (child.head() == "value") {
            values.push_back(&child);
        }
    }
    if (schema.unreal_projection.has_value()) {
        return std::nullopt;
    }
    for (auto const* value : values) {
        if (value->children.size() < 2) {
            return std::nullopt;
        }
    }

    std::vector<SourceReplacement> replacements;

    if (underlying.token.text != schema.underlying_type.name &&
        !replace_source_form(underlying, schema.underlying_type.name, original, replacements)) {
        return std::nullopt;
    }

    std::string conversions;
    if (!schema.conversions.empty()) {
        conversions = "(";
        for (std::size_t index{}; index < schema.conversions.size(); ++index) {
            conversions += index == 0 ? "" : " ";
            conversions += conversion_name(schema.conversions[index]);
        }
        conversions += ')';
    }
    auto const enum_properties{std::array<SourceProperty, 8>{
        std::pair{"bit-width",
                  schema.bit_width.has_value() ? std::optional{std::to_string(*schema.bit_width)}
                                               : std::nullopt},
        std::pair{"signed",
                  schema.signedness.has_value()
                      ? std::optional{*schema.signedness ? "true" : "false"}
                      : std::nullopt},
        std::pair{"reflection",
                  schema.reflection != codegen::EnumReflection::none
                      ? std::optional{std::string{reflection_name(schema.reflection)}}
                      : std::nullopt},
        std::pair{"enum-array",
                  schema.enum_array ? std::optional<std::string>{"true"} : std::nullopt},
        std::pair{"count", schema.count},
        std::pair{"conversions", conversions.empty() ? std::nullopt : std::optional{conversions}},
        std::pair{"export-specifier", schema.export_specifier},
        std::pair{"native-api",
                  schema.native_api ? std::optional<std::string>{"true"} : std::nullopt}}};
    if (!patch_source_properties(form, 2, enum_properties, "    ", original, replacements)) {
        return std::nullopt;
    }

    if (values.empty()) {
        return schema.values.empty() ? apply_source_replacements(original, std::move(replacements))
                                     : std::nullopt;
    }

    auto const first_value_offset{values.front()->token.span.offset};
    for (auto const& child : form.children) {
        if (child.token.span.offset > first_value_offset && child.head() != "value") {
            return std::nullopt;
        }
    }

    auto values_begin{std::size_t{}};
    for (auto const& child : form.children) {
        if (child.token.span.offset >= first_value_offset) {
            continue;
        }
        auto const child_end{source_form_line_end(child, original, first_value_offset)};
        if (!child_end.has_value()) {
            return std::nullopt;
        }
        values_begin = (std::max)(values_begin, *child_end);
    }
    if (values_begin > first_value_offset) {
        return std::nullopt;
    }

    struct SourceValue {
        Form const* form{};
        std::size_t begin{};
        std::size_t end{};
    };
    std::map<std::string_view, SourceValue> source_values;
    auto row_begin{values_begin};
    for (auto const* value : values) {
        auto const row_end{source_form_line_end(*value, original, form.closing.span.offset)};
        if (!row_end.has_value() || row_begin > value->token.span.offset ||
            *row_end < value->closing.span.offset + 1) {
            return std::nullopt;
        }
        if (!source_values
                 .emplace(value->children[1].token.text,
                          SourceValue{.form = value, .begin = row_begin, .end = *row_end})
                 .second) {
            return std::nullopt;
        }
        row_begin = *row_end;
    }

    auto const values_end{row_begin};
    auto rendered_values{std::string{}};
    auto append_row = [&](std::string row) {
        auto const preceding_newline{rendered_values.empty()
                                         ? values_begin > 0 && original[values_begin - 1] == '\n'
                                         : rendered_values.back() == '\n'};
        if (!preceding_newline && (row.empty() || row.front() != '\n')) {
            rendered_values += '\n';
        }
        rendered_values += std::move(row);
    };

    for (auto const& value : schema.values) {
        auto const found{source_values.find(value.name)};
        if (found == source_values.end()) {
            append_row("    " + render_enum_value(value));
            continue;
        }

        auto const value_properties{std::array<SourceProperty, 5>{
            std::pair{"value", value.initializer.transform(quote)},
            std::pair{"display-name", value.display_name.transform(quote)},
            std::pair{"hidden", value.hidden ? std::optional<std::string>{"true"} : std::nullopt},
            std::pair{"serialized-name", value.serialized_name.transform(quote)},
            std::pair{"sentinel",
                      value.sentinel ? std::optional<std::string>{"true"} : std::nullopt}}};
        std::vector<SourceReplacement> value_replacements;
        if (!patch_source_properties(
                *found->second.form, 1, value_properties, "      ", original, value_replacements)) {
            return std::nullopt;
        }
        auto rendered{apply_source_replacements_to_range(
            original, found->second.begin, found->second.end, std::move(value_replacements))};
        if (!rendered.has_value()) {
            return std::nullopt;
        }
        append_row(std::move(*rendered));
    }
    replacements.push_back(
        {.begin = values_begin, .end = values_end, .text = std::move(rendered_values)});

    return apply_source_replacements(original, std::move(replacements));
}

auto packed_field_kind_name(codegen::PackedFieldKind const kind) -> std::string_view {
    switch (kind) {
        case codegen::PackedFieldKind::unsigned_integer:
            return "unsigned";
        case codegen::PackedFieldKind::signed_integer:
            return "signed";
        case codegen::PackedFieldKind::enumeration:
            return "enum";
    }
    return "unsigned";
}

auto soa_member_kind_name(codegen::SoaMemberKind const kind) -> std::string_view {
    switch (kind) {
        case codegen::SoaMemberKind::array:
            return "array";
        case codegen::SoaMemberKind::nested:
            return "nested";
    }
    return "array";
}

auto storage_operation_name(codegen::StorageOperation const operation) -> std::string_view {
    switch (operation) {
        case codegen::StorageOperation::reset:
            return "reset";
        case codegen::StorageOperation::reserve:
            return "reserve";
        case codegen::StorageOperation::add_uninitialised:
            return "add-uninitialised";
        case codegen::StorageOperation::add_defaulted:
            return "add-defaulted";
        case codegen::StorageOperation::remove_at_swap:
            return "remove-at-swap";
        case codegen::StorageOperation::set_num:
            return "set-num";
        case codegen::StorageOperation::copy_element:
            return "copy-element";
        case codegen::StorageOperation::append_from:
            return "append-from";
    }
    return "reset";
}

void render_quoted_list(std::ostringstream& output, std::vector<std::string> const& values) {
    output << '(';
    for (std::size_t index{}; index < values.size(); ++index) {
        output << (index == 0 ? "" : " ") << quote(values[index]);
    }
    output << ')';
}

void render_function(std::ostringstream& output,
                     codegen::FunctionSchema const& function,
                     std::string_view const indent) {
    output << indent << "(function " << function.name << ' '
           << render_type_ref(function.return_type);
    if (!function.body_lines.empty()) {
        output << "\n" << indent << "  :body ";
        render_quoted_list(output, function.body_lines);
    }
    if (!function.dependencies.empty()) {
        output << "\n" << indent << "  :dependencies ";
        render_quoted_list(output, function.dependencies);
    }
    if (function.trailing_return_type.has_value()) {
        output << "\n"
               << indent << "  :trailing-return-type "
               << render_type_ref(*function.trailing_return_type);
    }
    if (function.is_const) {
        output << "\n" << indent << "  :const true";
    }
    if (function.is_noexcept) {
        output << "\n" << indent << "  :noexcept true";
    }
    if (function.is_static) {
        output << "\n" << indent << "  :static true";
    }
    if (function.is_inline) {
        output << "\n" << indent << "  :inline true";
    }
    if (function.definition_in_source) {
        output << "\n" << indent << "  :definition-in-source true";
    }
    if (function.template_parameters.has_value()) {
        output << "\n"
               << indent << "  :template-parameters " << quote(*function.template_parameters);
    }
    if (function.requires_clause.has_value()) {
        output << "\n" << indent << "  :requires " << quote(*function.requires_clause);
    }
    for (auto const& parameter : function.parameters) {
        output << "\n"
               << indent << "  (parameter " << parameter.name << ' '
               << render_type_ref(parameter.type);
        if (parameter.default_value.has_value()) {
            output << " :default " << quote(*parameter.default_value);
        }
        output << ')';
    }
    output << ')';
}

auto render_packed_segment(codegen::PackedSegmentSchema const& segment) -> std::string {
    std::ostringstream output;
    std::visit(
        [&](auto const& value) {
            using Segment = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<Segment, codegen::PackedFieldSchema>) {
                output << "(field " << value.name << ' ' << render_type_ref(value.type)
                       << " :bits ";
                if (value.bits.has_value()) {
                    output << *value.bits;
                } else {
                    output << "auto";
                }
                if (value.kind != codegen::PackedFieldKind::unsigned_integer) {
                    output << " :kind " << packed_field_kind_name(value.kind);
                }
                if (value.range_helper) {
                    output << " :range-helper true";
                }
                if (value.minimum_value.has_value()) {
                    output << " :minimum " << format_packed_integer(*value.minimum_value);
                }
                if (value.maximum_value.has_value()) {
                    output << " :maximum " << format_packed_integer(*value.maximum_value);
                }
                for (auto const& code : value.named_codes) {
                    output << "\n      (code " << code.name << " :value "
                           << format_packed_integer(code.value);
                    if (code.sentinel) {
                        output << " :sentinel true";
                    }
                    output << ')';
                }
                if (value.relationship.has_value()) {
                    output << "\n      (relation "
                           << packed_field_relation_kind_name(value.relationship->kind) << ' '
                           << render_type_ref(value.relationship->target) << ')';
                }
            } else {
                output << "(reserved " << value.name << " :bits " << value.bits;
            }
            output << ')';
        },
        segment);
    return output.str();
}

auto render_packed_value(codegen::PackedValueSchema const& schema) -> std::string {
    std::ostringstream output;
    output << "(packed-value " << schema.name << "\n    :storage "
           << render_type_ref(schema.storage_type);
    if (schema.byte_order.has_value()) {
        output << "\n    :byte-order " << packed_byte_order_name(*schema.byte_order);
    }
    if (schema.bit_order.has_value()) {
        output << "\n    :bit-order " << packed_bit_order_name(*schema.bit_order);
    }
    if (schema.invalid_value.has_value()) {
        output << "\n    :invalid-value " << *schema.invalid_value;
    }
    if (schema.export_specifier.has_value()) {
        output << "\n    :export-specifier " << *schema.export_specifier;
    }
    if (schema.mutable_value) {
        output << "\n    :mutable true";
    }
    for (auto const& segment : schema.segments) {
        output << "\n    " << render_packed_segment(segment);
    }
    output << ')';
    return output.str();
}

auto render_integer_scalar(codegen::IntegerScalarSchema const& schema) -> std::string {
    std::ostringstream output;
    output << "(integer-scalar " << schema.name << "\n    :signed "
           << (schema.signedness ? "true" : "false") << "\n    :minimum "
           << format_packed_integer(schema.minimum_value) << "\n    :maximum "
           << format_packed_integer(schema.maximum_value) << "\n    :bit-width ";
    if (schema.bit_width.has_value()) {
        output << *schema.bit_width;
    } else {
        output << "auto";
    }
    for (auto const& code : schema.named_codes) {
        output << "\n    (code " << code.name << " :value " << format_packed_integer(code.value);
        if (code.sentinel) {
            output << " :sentinel true";
        }
        output << ')';
    }
    output << ')';
    return output.str();
}

auto render_linear_quantized(codegen::LinearQuantizedSchema const& schema) -> std::string {
    std::ostringstream output;
    output << "(linear-quantized " << schema.name << "\n    :source "
           << render_type_ref(schema.source) << "\n    :bits " << schema.bit_width
           << "\n    :reserved-codes " << schema.reserved_codes << "\n    :clipping "
           << quantization_clipping_name(schema.clipping) << ')';
    return output.str();
}

auto render_integer_varint(codegen::IntegerVarintSchema const& schema) -> std::string {
    std::ostringstream output;
    output << "(integer-varint " << schema.name << "\n    :source "
           << render_type_ref(schema.source) << "\n    :encoding "
           << integer_varint_encoding_name(schema.encoding) << ')';
    return output.str();
}

auto render_fixed_point(codegen::FixedPointSchema const& schema) -> std::string {
    std::ostringstream output;
    output << "(fixed-point " << schema.name << "\n    :signed "
           << (schema.signedness ? "true" : "false") << "\n    :total-bits " << schema.total_bits
           << "\n    :fractional-bits " << schema.fractional_bits << "\n    :rounding "
           << fixed_point_rounding_name(schema.rounding) << ')';
    return output.str();
}

auto render_mini_float(codegen::MiniFloatSchema const& schema) -> std::string {
    std::ostringstream output;
    output << "(mini-float " << schema.name << "\n    :sign-bits " << schema.sign_bits
           << "\n    :exponent-bits " << schema.exponent_bits << "\n    :significand-bits "
           << schema.significand_bits << "\n    :bias " << schema.exponent_bias << ')';
    return output.str();
}

auto render_optional_sentinel(codegen::OptionalSentinelSchema const& schema) -> std::string {
    std::ostringstream output;
    output << "(optional-sentinel " << schema.name << "\n    :source "
           << render_type_ref(schema.source) << "\n    :sentinel " << schema.sentinel << ')';
    return output.str();
}

auto render_optional_presence_bit(codegen::OptionalPresenceBitSchema const& schema) -> std::string {
    std::ostringstream output;
    output << "(optional-presence-bit " << schema.name << "\n    :source "
           << render_type_ref(schema.source) << ')';
    return output.str();
}

auto patch_source_packed_segment(codegen::PackedSegmentSchema const& segment,
                                 Form const& source_segment,
                                 std::string_view const original,
                                 std::vector<SourceReplacement>& replacements) -> bool {
    if (auto const field{std::get_if<codegen::PackedFieldSchema>(&segment)}) {
        if (source_segment.children.size() < 3 ||
            !patch_source_form(
                source_segment.children[2], render_type_ref(field->type), original, replacements)) {
            return false;
        }
        auto const field_properties{std::array<SourceProperty, 5>{
            std::pair{"bits",
                      std::optional{field->bits.has_value() ? std::to_string(*field->bits)
                                                            : std::string{"auto"}}},
            std::pair{"kind",
                      field->kind != codegen::PackedFieldKind::unsigned_integer
                          ? std::optional{std::string{packed_field_kind_name(field->kind)}}
                          : std::nullopt},
            std::pair{"range-helper",
                      field->range_helper ? std::optional<std::string>{"true"} : std::nullopt},
            std::pair{"minimum",
                      field->minimum_value.has_value()
                          ? std::optional{format_packed_integer(*field->minimum_value)}
                          : std::nullopt},
            std::pair{"maximum",
                      field->maximum_value.has_value()
                          ? std::optional{format_packed_integer(*field->maximum_value)}
                          : std::nullopt}}};
        if (!patch_source_properties(
                source_segment, 2, field_properties, "      ", original, replacements)) {
            return false;
        }

        std::vector<Form const*> codes;
        Form const* relation{};
        for (auto const& child : source_segment.children) {
            if (child.head() == "code") {
                codes.push_back(&child);
            } else if (child.head() == "relation") {
                relation = &child;
            }
        }
        if (codes.size() != field->named_codes.size() ||
            (relation != nullptr) != field->relationship.has_value()) {
            return false;
        }
        for (std::size_t code_index{}; code_index < codes.size(); ++code_index) {
            auto const& code{field->named_codes[code_index]};
            if (codes[code_index]->children.size() < 2 ||
                codes[code_index]->children[1].token.text != code.name) {
                return false;
            }
            auto const code_properties{std::array<SourceProperty, 2>{
                std::pair{"value", std::optional{format_packed_integer(code.value)}},
                std::pair{"sentinel",
                          code.sentinel ? std::optional<std::string>{"true"} : std::nullopt}}};
            if (!patch_source_properties(
                    *codes[code_index], 1, code_properties, "        ", original, replacements)) {
                return false;
            }
        }
        if (relation != nullptr &&
            (relation->children.size() < 3 ||
             !patch_source_form(
                 relation->children[1],
                 std::string{codegen::packed_field_relation_kind_name(field->relationship->kind)},
                 original,
                 replacements) ||
             !patch_source_form(relation->children[2],
                                render_type_ref(field->relationship->target),
                                original,
                                replacements))) {
            return false;
        }
        return true;
    }

    auto const& reserved{std::get<codegen::PackedReservedBitsSchema>(segment)};
    auto const reserved_properties{std::array<SourceProperty, 1>{
        std::pair{"bits", std::optional{std::to_string(reserved.bits)}}}};
    return patch_source_properties(
        source_segment, 1, reserved_properties, "      ", original, replacements);
}

auto try_render_source_preserved_packed_value(codegen::PackedValueSchema const& schema,
                                              std::string_view const original)
    -> std::optional<std::string> {
    auto parsed{parse_owned_source_declaration(original, "packed-value", schema.name)};
    if (!parsed.has_value()) {
        return std::nullopt;
    }

    std::vector<Form const*> segments;
    for (auto const& child : parsed->children) {
        if (child.head() == "field" || child.head() == "reserved") {
            if (child.children.size() < 2) {
                return std::nullopt;
            }
            segments.push_back(&child);
        }
    }

    auto const properties{std::array<SourceProperty, 6>{
        std::pair{"storage", std::optional{render_type_ref(schema.storage_type)}},
        std::pair{"byte-order",
                  schema.byte_order.has_value()
                      ? std::optional{std::string{packed_byte_order_name(*schema.byte_order)}}
                      : std::nullopt},
        std::pair{"bit-order",
                  schema.bit_order.has_value()
                      ? std::optional{std::string{packed_bit_order_name(*schema.bit_order)}}
                      : std::nullopt},
        std::pair{"invalid-value",
                  schema.invalid_value.has_value()
                      ? std::optional{std::to_string(*schema.invalid_value)}
                      : std::nullopt},
        std::pair{"export-specifier", schema.export_specifier},
        std::pair{"mutable",
                  schema.mutable_value ? std::optional{std::string{"true"}} : std::nullopt}}};
    std::vector<SourceReplacement> replacements;
    if (!patch_source_properties(*parsed, 1, properties, "    ", original, replacements)) {
        return std::nullopt;
    }

    if (segments.empty()) {
        return schema.segments.empty()
                 ? apply_source_replacements(original, std::move(replacements))
                 : std::nullopt;
    }

    auto const first_segment_offset{segments.front()->token.span.offset};
    for (auto const& child : parsed->children) {
        if (child.token.span.offset > first_segment_offset && child.head() != "field" &&
            child.head() != "reserved") {
            return std::nullopt;
        }
    }

    auto segments_begin{std::size_t{}};
    for (auto const& child : parsed->children) {
        if (child.token.span.offset >= first_segment_offset) {
            continue;
        }
        auto const child_end{source_form_line_end(child, original, first_segment_offset)};
        if (!child_end.has_value()) {
            return std::nullopt;
        }
        segments_begin = (std::max)(segments_begin, *child_end);
    }
    if (segments_begin > first_segment_offset) {
        return std::nullopt;
    }

    struct SourceSegment {
        Form const* form{};
        std::size_t begin{};
        std::size_t end{};
    };
    using SegmentKey = std::pair<std::string_view, std::string_view>;
    std::map<SegmentKey, SourceSegment> source_segments;
    auto row_begin{segments_begin};
    for (auto const* segment : segments) {
        auto const row_end{source_form_line_end(*segment, original, parsed->closing.span.offset)};
        if (!row_end.has_value() || row_begin > segment->token.span.offset ||
            *row_end < segment->closing.span.offset + 1) {
            return std::nullopt;
        }
        auto const key{SegmentKey{segment->head(), segment->children[1].token.text}};
        if (!source_segments
                 .emplace(key, SourceSegment{.form = segment, .begin = row_begin, .end = *row_end})
                 .second) {
            return std::nullopt;
        }
        row_begin = *row_end;
    }

    auto const segments_end{row_begin};
    auto rendered_segments{std::string{}};
    auto append_row = [&](std::string row) {
        auto const preceding_newline{
            rendered_segments.empty() ? segments_begin > 0 && original[segments_begin - 1] == '\n'
                                      : rendered_segments.back() == '\n'};
        if (!preceding_newline && (row.empty() || row.front() != '\n')) {
            rendered_segments += '\n';
        }
        rendered_segments += std::move(row);
    };

    for (auto const& segment : schema.segments) {
        auto const head{std::holds_alternative<codegen::PackedFieldSchema>(segment)
                            ? std::string_view{"field"}
                            : std::string_view{"reserved"}};
        auto const key{SegmentKey{head, codegen::packed_segment_name(segment)}};
        auto const found{source_segments.find(key)};
        if (found == source_segments.end()) {
            append_row("    " + render_packed_segment(segment));
            continue;
        }

        std::vector<SourceReplacement> segment_replacements;
        if (!patch_source_packed_segment(
                segment, *found->second.form, original, segment_replacements)) {
            return std::nullopt;
        }
        auto rendered{apply_source_replacements_to_range(
            original, found->second.begin, found->second.end, std::move(segment_replacements))};
        if (!rendered.has_value()) {
            return std::nullopt;
        }
        append_row(std::move(*rendered));
    }
    replacements.push_back(
        {.begin = segments_begin, .end = segments_end, .text = std::move(rendered_segments)});
    return apply_source_replacements(original, std::move(replacements));
}

auto try_render_source_preserved_properties(std::string_view const head,
                                            std::string_view const name,
                                            std::span<SourceProperty const> properties,
                                            std::string_view const original)
    -> std::optional<std::string> {
    auto parsed{parse_owned_source_declaration(original, head, name)};
    if (!parsed.has_value()) {
        return std::nullopt;
    }
    std::vector<SourceReplacement> replacements;
    if (!patch_source_properties(*parsed, 1, properties, "    ", original, replacements)) {
        return std::nullopt;
    }
    return apply_source_replacements(original, std::move(replacements));
}

auto try_render_source_preserved_integer_scalar(codegen::IntegerScalarSchema const& schema,
                                                std::string_view const original)
    -> std::optional<std::string> {
    auto parsed{parse_owned_source_declaration(original, "integer-scalar", schema.name)};
    if (!parsed.has_value()) {
        return std::nullopt;
    }

    std::vector<Form const*> codes;
    for (auto const& child : parsed->children) {
        if (child.head() == "code") {
            codes.push_back(&child);
        }
    }
    if (codes.size() != schema.named_codes.size()) {
        return std::nullopt;
    }
    for (std::size_t index{}; index < codes.size(); ++index) {
        if (codes[index]->children.size() < 2 ||
            codes[index]->children[1].token.text != schema.named_codes[index].name) {
            return std::nullopt;
        }
    }

    auto const properties{std::array<SourceProperty, 4>{
        std::pair{"signed", std::optional{schema.signedness ? "true" : "false"}},
        std::pair{"minimum", std::optional{format_packed_integer(schema.minimum_value)}},
        std::pair{"maximum", std::optional{format_packed_integer(schema.maximum_value)}},
        std::pair{"bit-width",
                  std::optional{schema.bit_width.has_value() ? std::to_string(*schema.bit_width)
                                                             : std::string{"auto"}}}}};
    std::vector<SourceReplacement> replacements;
    if (!patch_source_properties(*parsed, 1, properties, "    ", original, replacements)) {
        return std::nullopt;
    }
    for (std::size_t index{}; index < codes.size(); ++index) {
        auto const& code{schema.named_codes[index]};
        auto const code_properties{std::array<SourceProperty, 2>{
            std::pair{"value", std::optional{format_packed_integer(code.value)}},
            std::pair{"sentinel",
                      code.sentinel ? std::optional<std::string>{"true"} : std::nullopt}}};
        if (!patch_source_properties(
                *codes[index], 1, code_properties, "      ", original, replacements)) {
            return std::nullopt;
        }
    }
    return apply_source_replacements(original, std::move(replacements));
}

auto try_render_source_preserved_linear_quantized(codegen::LinearQuantizedSchema const& schema,
                                                  std::string_view const original)
    -> std::optional<std::string> {
    auto const properties{std::array<SourceProperty, 4>{
        std::pair{"source", std::optional{render_type_ref(schema.source)}},
        std::pair{"bits", std::optional{std::to_string(schema.bit_width)}},
        std::pair{"reserved-codes", std::optional{std::to_string(schema.reserved_codes)}},
        std::pair{"clipping",
                  std::optional{std::string{quantization_clipping_name(schema.clipping)}}}}};
    return try_render_source_preserved_properties(
        "linear-quantized", schema.name, properties, original);
}

auto try_render_source_preserved_integer_varint(codegen::IntegerVarintSchema const& schema,
                                                std::string_view const original)
    -> std::optional<std::string> {
    auto const properties{std::array<SourceProperty, 2>{
        std::pair{"source", std::optional{render_type_ref(schema.source)}},
        std::pair{"encoding",
                  std::optional{std::string{integer_varint_encoding_name(schema.encoding)}}}}};
    return try_render_source_preserved_properties(
        "integer-varint", schema.name, properties, original);
}

auto try_render_source_preserved_fixed_point(codegen::FixedPointSchema const& schema,
                                             std::string_view const original)
    -> std::optional<std::string> {
    auto const properties{std::array<SourceProperty, 4>{
        std::pair{"signed", std::optional{schema.signedness ? "true" : "false"}},
        std::pair{"total-bits", std::optional{std::to_string(schema.total_bits)}},
        std::pair{"fractional-bits", std::optional{std::to_string(schema.fractional_bits)}},
        std::pair{"rounding",
                  std::optional{std::string{fixed_point_rounding_name(schema.rounding)}}}}};
    return try_render_source_preserved_properties("fixed-point", schema.name, properties, original);
}

auto try_render_source_preserved_mini_float(codegen::MiniFloatSchema const& schema,
                                            std::string_view const original)
    -> std::optional<std::string> {
    auto const properties{std::array<SourceProperty, 4>{
        std::pair{"sign-bits", std::optional{std::to_string(schema.sign_bits)}},
        std::pair{"exponent-bits", std::optional{std::to_string(schema.exponent_bits)}},
        std::pair{"significand-bits", std::optional{std::to_string(schema.significand_bits)}},
        std::pair{"bias", std::optional{std::to_string(schema.exponent_bias)}}}};
    return try_render_source_preserved_properties("mini-float", schema.name, properties, original);
}

auto try_render_source_preserved_optional_sentinel(codegen::OptionalSentinelSchema const& schema,
                                                   std::string_view const original)
    -> std::optional<std::string> {
    auto const properties{std::array<SourceProperty, 2>{
        std::pair{"source", std::optional{render_type_ref(schema.source)}},
        std::pair{"sentinel", std::optional{schema.sentinel}}}};
    return try_render_source_preserved_properties(
        "optional-sentinel", schema.name, properties, original);
}

auto try_render_source_preserved_optional_presence_bit(
    codegen::OptionalPresenceBitSchema const& schema, std::string_view const original)
    -> std::optional<std::string> {
    auto const properties{std::array<SourceProperty, 1>{
        std::pair{"source", std::optional{render_type_ref(schema.source)}}}};
    return try_render_source_preserved_properties(
        "optional-presence-bit", schema.name, properties, original);
}

template <typename Child>
auto render_aggregate_child(std::string_view const head, Child const& child) -> std::string {
    std::ostringstream output;
    output << '(' << head << ' ' << child.name << ' ' << render_type_ref(child.type);
    if (child.count.has_value()) {
        output << " :count " << *child.count;
    }
    output << ')';
    return output.str();
}

auto render_record(codegen::RecordSchema const& schema) -> std::string {
    std::ostringstream output;
    output << "(record " << schema.name;
    if (schema.export_specifier.has_value()) {
        output << "\n    :export-specifier " << *schema.export_specifier;
    }
    for (auto const& member : schema.members) {
        output << "\n    " << render_aggregate_child("member", member);
    }
    output << ')';
    return output.str();
}

auto render_union(codegen::UnionSchema const& schema) -> std::string {
    std::ostringstream output;
    output << "(union " << schema.name;
    if (schema.export_specifier.has_value()) {
        output << "\n    :export-specifier " << *schema.export_specifier;
    }
    for (auto const& alternative : schema.alternatives) {
        output << "\n    " << render_aggregate_child("alternative", alternative);
    }
    output << ')';
    return output.str();
}

auto render_tagged_union_alternative(codegen::TaggedUnionAlternativeSchema const& alternative)
    -> std::string {
    std::ostringstream output;
    output << "(alternative " << alternative.name << ' ' << render_type_ref(alternative.type)
           << " :tag " << alternative.tag;
    if (alternative.count.has_value()) {
        output << " :count " << *alternative.count;
    }
    output << ')';
    return output.str();
}

auto render_tagged_union(codegen::TaggedUnionSchema const& schema) -> std::string {
    std::ostringstream output;
    output << "(tagged-union " << schema.name << "\n    :discriminant "
           << render_type_ref(schema.discriminant);
    if (schema.export_specifier.has_value()) {
        output << "\n    :export-specifier " << *schema.export_specifier;
    }
    for (auto const& alternative : schema.alternatives) {
        output << "\n    " << render_tagged_union_alternative(alternative);
    }
    output << ')';
    return output.str();
}

template <typename Child>
auto try_render_source_preserved_aggregate(std::string_view const declaration_head,
                                           std::string_view const child_head,
                                           std::string const& name,
                                           std::vector<Child> const& children,
                                           std::optional<std::string> const& export_specifier,
                                           std::string_view const original)
    -> std::optional<std::string> {
    auto parsed{parse_owned_source_declaration(original, declaration_head, name)};
    if (!parsed.has_value()) {
        return std::nullopt;
    }

    std::vector<Form const*> source_children;
    for (auto const& child : parsed->children) {
        if (child.head() == child_head) {
            if (child.children.size() < 3) {
                return std::nullopt;
            }
            source_children.push_back(&child);
        }
    }

    auto const properties{
        std::array<SourceProperty, 1>{std::pair{"export-specifier", export_specifier}}};
    std::vector<SourceReplacement> replacements;
    if (!patch_source_properties(*parsed, 1, properties, "    ", original, replacements)) {
        return std::nullopt;
    }

    if (source_children.empty()) {
        return children.empty() ? apply_source_replacements(original, std::move(replacements))
                                : std::nullopt;
    }

    auto const first_child_offset{source_children.front()->token.span.offset};
    for (auto const& child : parsed->children) {
        if (child.token.span.offset > first_child_offset && child.head() != child_head) {
            return std::nullopt;
        }
    }

    auto children_begin{std::size_t{}};
    for (auto const& child : parsed->children) {
        if (child.token.span.offset >= first_child_offset) {
            continue;
        }
        auto const child_end{source_form_line_end(child, original, first_child_offset)};
        if (!child_end.has_value()) {
            return std::nullopt;
        }
        children_begin = (std::max)(children_begin, *child_end);
    }
    if (children_begin > first_child_offset) {
        return std::nullopt;
    }

    struct SourceChild {
        Form const* form{};
        std::size_t begin{};
        std::size_t end{};
    };
    std::map<std::string_view, SourceChild> source_by_name;
    auto row_begin{children_begin};
    for (auto const* child : source_children) {
        auto const row_end{source_form_line_end(*child, original, parsed->closing.span.offset)};
        if (!row_end.has_value() || row_begin > child->token.span.offset ||
            *row_end < child->closing.span.offset + 1) {
            return std::nullopt;
        }
        if (!source_by_name
                 .emplace(child->children[1].token.text,
                          SourceChild{.form = child, .begin = row_begin, .end = *row_end})
                 .second) {
            return std::nullopt;
        }
        row_begin = *row_end;
    }

    auto const children_end{row_begin};
    auto rendered_children{std::string{}};
    auto append_row = [&](std::string row) {
        auto const preceding_newline{
            rendered_children.empty() ? children_begin > 0 && original[children_begin - 1] == '\n'
                                      : rendered_children.back() == '\n'};
        if (!preceding_newline && (row.empty() || row.front() != '\n')) {
            rendered_children += '\n';
        }
        rendered_children += std::move(row);
    };

    for (auto const& child : children) {
        auto const found{source_by_name.find(child.name)};
        if (found == source_by_name.end()) {
            append_row("    " + render_aggregate_child(child_head, child));
            continue;
        }

        std::vector<SourceReplacement> child_replacements;
        if (!patch_source_form(found->second.form->children[2],
                               render_type_ref(child.type),
                               original,
                               child_replacements)) {
            return std::nullopt;
        }
        auto const child_properties{std::array<SourceProperty, 1>{std::pair{
            "count",
            child.count.has_value() ? std::optional{std::to_string(*child.count)} : std::nullopt}}};
        if (!patch_source_properties(
                *found->second.form, 2, child_properties, "      ", original, child_replacements)) {
            return std::nullopt;
        }
        auto rendered{apply_source_replacements_to_range(
            original, found->second.begin, found->second.end, std::move(child_replacements))};
        if (!rendered.has_value()) {
            return std::nullopt;
        }
        append_row(std::move(*rendered));
    }
    replacements.push_back(
        {.begin = children_begin, .end = children_end, .text = std::move(rendered_children)});
    return apply_source_replacements(original, std::move(replacements));
}

auto try_render_source_preserved_record(codegen::RecordSchema const& schema,
                                        std::string_view const original)
    -> std::optional<std::string> {
    return try_render_source_preserved_aggregate(
        "record", "member", schema.name, schema.members, schema.export_specifier, original);
}

auto try_render_source_preserved_union(codegen::UnionSchema const& schema,
                                       std::string_view const original)
    -> std::optional<std::string> {
    return try_render_source_preserved_aggregate("union",
                                                 "alternative",
                                                 schema.name,
                                                 schema.alternatives,
                                                 schema.export_specifier,
                                                 original);
}

auto try_render_source_preserved_tagged_union(codegen::TaggedUnionSchema const& schema,
                                              std::string_view const original)
    -> std::optional<std::string> {
    auto parsed{parse_owned_source_declaration(original, "tagged-union", schema.name)};
    if (!parsed.has_value()) {
        return std::nullopt;
    }

    std::vector<Form const*> alternatives;
    for (auto const& child : parsed->children) {
        if (child.head() == "alternative") {
            if (child.children.size() < 3) {
                return std::nullopt;
            }
            alternatives.push_back(&child);
        }
    }

    auto const properties{std::array<SourceProperty, 2>{
        std::pair{"discriminant", std::optional{render_type_ref(schema.discriminant)}},
        std::pair{"export-specifier", schema.export_specifier}}};
    std::vector<SourceReplacement> replacements;
    if (!patch_source_properties(*parsed, 1, properties, "    ", original, replacements)) {
        return std::nullopt;
    }

    if (alternatives.empty()) {
        return schema.alternatives.empty()
                 ? apply_source_replacements(original, std::move(replacements))
                 : std::nullopt;
    }

    auto const first_alternative_offset{alternatives.front()->token.span.offset};
    for (auto const& child : parsed->children) {
        if (child.token.span.offset > first_alternative_offset && child.head() != "alternative") {
            return std::nullopt;
        }
    }

    auto alternatives_begin{std::size_t{}};
    for (auto const& child : parsed->children) {
        if (child.token.span.offset >= first_alternative_offset) {
            continue;
        }
        auto const child_end{source_form_line_end(child, original, first_alternative_offset)};
        if (!child_end.has_value()) {
            return std::nullopt;
        }
        alternatives_begin = (std::max)(alternatives_begin, *child_end);
    }
    if (alternatives_begin > first_alternative_offset) {
        return std::nullopt;
    }

    struct SourceAlternative {
        Form const* form{};
        std::size_t begin{};
        std::size_t end{};
    };
    std::map<std::string_view, SourceAlternative> source_by_name;
    auto row_begin{alternatives_begin};
    for (auto const* alternative : alternatives) {
        auto const row_end{
            source_form_line_end(*alternative, original, parsed->closing.span.offset)};
        if (!row_end.has_value() || row_begin > alternative->token.span.offset ||
            *row_end < alternative->closing.span.offset + 1) {
            return std::nullopt;
        }
        if (!source_by_name
                 .emplace(
                     alternative->children[1].token.text,
                     SourceAlternative{.form = alternative, .begin = row_begin, .end = *row_end})
                 .second) {
            return std::nullopt;
        }
        row_begin = *row_end;
    }

    auto const alternatives_end{row_begin};
    auto rendered_alternatives{std::string{}};
    auto append_row = [&](std::string row) {
        auto const preceding_newline{rendered_alternatives.empty()
                                         ? alternatives_begin > 0 &&
                                               original[alternatives_begin - 1] == '\n'
                                         : rendered_alternatives.back() == '\n'};
        if (!preceding_newline && (row.empty() || row.front() != '\n')) {
            rendered_alternatives += '\n';
        }
        rendered_alternatives += std::move(row);
    };

    for (auto const& alternative : schema.alternatives) {
        auto const found{source_by_name.find(alternative.name)};
        if (found == source_by_name.end()) {
            append_row("    " + render_tagged_union_alternative(alternative));
            continue;
        }

        std::vector<SourceReplacement> alternative_replacements;
        if (!patch_source_form(found->second.form->children[2],
                               render_type_ref(alternative.type),
                               original,
                               alternative_replacements)) {
            return std::nullopt;
        }
        auto const alternative_properties{std::array<SourceProperty, 2>{
            std::pair{"tag", std::optional{alternative.tag}},
            std::pair{"count",
                      alternative.count.has_value()
                          ? std::optional{std::to_string(*alternative.count)}
                          : std::nullopt}}};
        if (!patch_source_properties(*found->second.form,
                                     2,
                                     alternative_properties,
                                     "      ",
                                     original,
                                     alternative_replacements)) {
            return std::nullopt;
        }
        auto rendered{apply_source_replacements_to_range(
            original, found->second.begin, found->second.end, std::move(alternative_replacements))};
        if (!rendered.has_value()) {
            return std::nullopt;
        }
        append_row(std::move(*rendered));
    }
    replacements.push_back({.begin = alternatives_begin,
                            .end = alternatives_end,
                            .text = std::move(rendered_alternatives)});
    return apply_source_replacements(original, std::move(replacements));
}

auto render_soa(codegen::SoaSchema const& schema) -> std::string {
    std::ostringstream output;
    output << "(struct " << schema.name;
    if (schema.view_name.has_value()) {
        output << "\n    :view-name " << *schema.view_name;
    }
    if (schema.const_view_name.has_value()) {
        output << "\n    :const-view-name " << *schema.const_view_name;
    }
    if (!schema.operations.empty()) {
        output << "\n    :operations (";
        for (std::size_t index{}; index < schema.operations.size(); ++index) {
            output << (index == 0 ? "" : " ") << storage_operation_name(schema.operations[index]);
        }
        output << ')';
    }
    if (schema.export_specifier.has_value()) {
        output << "\n    :export-specifier " << *schema.export_specifier;
    }
    if (!schema.using_declarations.empty()) {
        output << "\n    :using-declarations ";
        render_quoted_list(output, schema.using_declarations);
    }
    if (schema.equivalent_type.has_value()) {
        output << "\n    :equivalent-type " << render_type_ref(*schema.equivalent_type);
    }
    if (schema.copy_element_memberwise) {
        output << "\n    :copy-element-memberwise true";
    }
    if (schema.layout_only) {
        output << "\n    :layout-only true";
    }
    if (schema.field_mask_name.has_value()) {
        output << "\n    :field-mask-name " << *schema.field_mask_name;
    }
    if (schema.field_enum_name.has_value()) {
        output << "\n    :field-enum-name " << *schema.field_enum_name;
    }
    for (auto const& member : schema.members) {
        output << "\n    (member " << member.name << ' ' << soa_member_kind_name(member.kind) << ' '
               << render_type_ref(member.type);
        if (member.fixed_schema.has_value()) {
            output << "\n      :fixed-schema " << *member.fixed_schema;
        }
        if (member.nested_schema.has_value()) {
            output << "\n      :nested-schema " << *member.nested_schema;
        }
        if (member.mask_field) {
            output << "\n      :mask-field true";
        }
        if (!member.mask_dimensions.empty()) {
            output << "\n      :mask-dimensions (";
            for (std::size_t index{}; index < member.mask_dimensions.size(); ++index) {
                auto const& dimension{member.mask_dimensions[index]};
                output << (index == 0 ? "" : " ") << '(' << dimension.index_name << ' '
                       << quote(dimension.extent) << ')';
            }
            output << ')';
        }
        output << ')';
    }
    for (auto const& function : schema.functions) {
        output << '\n';
        render_function(output, function, "    ");
    }
    if (schema.fixed.has_value()) {
        output << "\n    (fixed " << schema.fixed->storage_name;
        if (!schema.fixed->containers.empty()) {
            output << " :containers (";
            for (std::size_t index{}; index < schema.fixed->containers.size(); ++index) {
                output << (index == 0 ? "" : " ") << schema.fixed->containers[index];
            }
            output << ')';
        }
        output << ')';
    }
    if (schema.single_allocation.has_value()) {
        output << "\n    (single-allocation " << *schema.single_allocation;
        for (auto const& variant : schema.single_allocation_variants) {
            output << "\n      (variant " << variant.name << ' '
                   << render_type_ref(variant.allocator) << ')';
        }
        output << ')';
    }
    output << ')';
    return output.str();
}

auto try_render_source_preserved_soa(codegen::SoaSchema const& schema,
                                     std::string_view const original)
    -> std::optional<std::string> {
    if (!schema.mutable_view_functions.empty() || schema.array_allocator.has_value() ||
        schema.single_allocation_allocator.has_value()) {
        return std::nullopt;
    }
    auto parsed{parse_owned_source_declaration(original, "struct", schema.name)};
    if (!parsed.has_value()) {
        return std::nullopt;
    }

    std::vector<Form const*> members;
    std::vector<Form const*> functions;
    Form const* fixed{};
    Form const* single_allocation{};
    for (auto const& child : parsed->children) {
        if (child.head() == "member") {
            members.push_back(&child);
        } else if (child.head() == "function") {
            functions.push_back(&child);
        } else if (child.head() == "fixed") {
            fixed = &child;
        } else if (child.head() == "single-allocation") {
            single_allocation = &child;
        }
    }
    if (members.size() != schema.members.size() || functions.size() != schema.functions.size() ||
        (fixed != nullptr) != schema.fixed.has_value() ||
        (single_allocation != nullptr) != schema.single_allocation.has_value()) {
        return std::nullopt;
    }
    for (std::size_t index{}; index < members.size(); ++index) {
        if (members[index]->children.size() < 4 ||
            members[index]->children[1].token.text != schema.members[index].name) {
            return std::nullopt;
        }
    }
    for (std::size_t index{}; index < functions.size(); ++index) {
        std::ostringstream rendered;
        render_function(rendered, schema.functions[index], {});
        if (!source_form_matches_rendered(*functions[index], rendered.str())) {
            return std::nullopt;
        }
    }
    if (fixed != nullptr) {
        std::ostringstream rendered;
        rendered << "(fixed " << schema.fixed->storage_name;
        if (!schema.fixed->containers.empty()) {
            rendered << " :containers (";
            for (std::size_t index{}; index < schema.fixed->containers.size(); ++index) {
                rendered << (index == 0 ? "" : " ") << schema.fixed->containers[index];
            }
            rendered << ')';
        }
        rendered << ')';
        if (!source_form_matches_rendered(*fixed, rendered.str())) {
            return std::nullopt;
        }
    }
    if (single_allocation != nullptr) {
        std::ostringstream rendered;
        rendered << "(single-allocation " << *schema.single_allocation;
        for (auto const& variant : schema.single_allocation_variants) {
            rendered << "\n  (variant " << variant.name << ' ' << render_type_ref(variant.allocator)
                     << ')';
        }
        rendered << ')';
        if (!source_form_matches_rendered(*single_allocation, rendered.str())) {
            return std::nullopt;
        }
    }

    std::string operations;
    if (!schema.operations.empty()) {
        operations = "(";
        for (std::size_t index{}; index < schema.operations.size(); ++index) {
            operations += index == 0 ? "" : " ";
            operations += storage_operation_name(schema.operations[index]);
        }
        operations += ')';
    }
    std::string using_declarations;
    if (!schema.using_declarations.empty()) {
        std::ostringstream rendered;
        render_quoted_list(rendered, schema.using_declarations);
        using_declarations = std::move(rendered).str();
    }
    auto const properties{std::array<SourceProperty, 10>{
        std::pair{"view-name", schema.view_name},
        std::pair{"const-view-name", schema.const_view_name},
        std::pair{"operations", operations.empty() ? std::nullopt : std::optional{operations}},
        std::pair{"export-specifier", schema.export_specifier},
        std::pair{"using-declarations",
                  using_declarations.empty() ? std::nullopt : std::optional{using_declarations}},
        std::pair{"equivalent-type",
                  schema.equivalent_type.has_value()
                      ? std::optional{render_type_ref(*schema.equivalent_type)}
                      : std::nullopt},
        std::pair{"copy-element-memberwise",
                  schema.copy_element_memberwise ? std::optional<std::string>{"true"}
                                                 : std::nullopt},
        std::pair{"layout-only",
                  schema.layout_only ? std::optional<std::string>{"true"} : std::nullopt},
        std::pair{"field-mask-name", schema.field_mask_name},
        std::pair{"field-enum-name", schema.field_enum_name}}};
    std::vector<SourceReplacement> replacements;
    if (!patch_source_properties(*parsed, 1, properties, "    ", original, replacements)) {
        return std::nullopt;
    }

    for (std::size_t index{}; index < members.size(); ++index) {
        auto const& member{schema.members[index]};
        if (!patch_source_form(members[index]->children[2],
                               std::string{soa_member_kind_name(member.kind)},
                               original,
                               replacements) ||
            !patch_source_form(members[index]->children[3],
                               render_type_ref(member.type),
                               original,
                               replacements)) {
            return std::nullopt;
        }
        std::string dimensions;
        if (!member.mask_dimensions.empty()) {
            dimensions = "(";
            for (std::size_t dimension_index{}; dimension_index < member.mask_dimensions.size();
                 ++dimension_index) {
                auto const& dimension{member.mask_dimensions[dimension_index]};
                dimensions += dimension_index == 0 ? "" : " ";
                dimensions += "(" + dimension.index_name + " " + quote(dimension.extent) + ")";
            }
            dimensions += ')';
        }
        auto const member_properties{std::array<SourceProperty, 4>{
            std::pair{"fixed-schema", member.fixed_schema},
            std::pair{"nested-schema", member.nested_schema},
            std::pair{"mask-field",
                      member.mask_field ? std::optional<std::string>{"true"} : std::nullopt},
            std::pair{"mask-dimensions",
                      dimensions.empty() ? std::nullopt : std::optional{dimensions}}}};
        if (!patch_source_properties(
                *members[index], 3, member_properties, "      ", original, replacements)) {
            return std::nullopt;
        }
    }
    return apply_source_replacements(original, std::move(replacements));
}

void replace_file(std::filesystem::path const& source, std::filesystem::path const& destination) {
#if defined(_WIN32)
    if (!MoveFileExW(source.c_str(),
                     destination.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        throw std::filesystem::filesystem_error{
            "Cannot replace LispB source file",
            source,
            destination,
            std::error_code{static_cast<int>(GetLastError()), std::system_category()}};
    }
#else
    std::filesystem::rename(source, destination);
#endif
}

auto declaration_head(codegen::ModuleSchema const& module) -> std::string_view {
    return std::visit(
        [](auto const& value) -> std::string_view {
            using Module = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<Module, codegen::EnumModuleSchema>) {
                return "enum";
            } else if constexpr (std::is_same_v<Module, codegen::PackedValueModuleSchema>) {
                return "packed-value";
            } else if constexpr (std::is_same_v<Module, codegen::ScalarModuleSchema>) {
                return "integer-scalar";
            } else if constexpr (std::is_same_v<Module, codegen::RepresentationModuleSchema>) {
                return "linear-quantized";
            } else if constexpr (std::is_same_v<Module, codegen::RecordModuleSchema>) {
                return "record";
            } else if constexpr (std::is_same_v<Module, codegen::UnionModuleSchema>) {
                return "union";
            } else if constexpr (std::is_same_v<Module, codegen::SoaModuleSchema>) {
                return "struct";
            }
            return {};
        },
        module);
}

auto declaration_count(codegen::ModuleSchema const& module) -> std::size_t {
    return std::visit(
        [](auto const& value) -> std::size_t {
            using Module = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<Module, codegen::EnumModuleSchema>) {
                return value.enums.size();
            } else if constexpr (std::is_same_v<Module, codegen::PackedValueModuleSchema>) {
                return value.values.size();
            } else if constexpr (std::is_same_v<Module, codegen::ScalarModuleSchema>) {
                return value.scalars.size();
            } else if constexpr (std::is_same_v<Module, codegen::RepresentationModuleSchema>) {
                return value.linear_quantized.size() + value.integer_varints.size() +
                       value.fixed_points.size() + value.optional_sentinels.size() +
                       value.optional_presence_bits.size() + value.mini_floats.size();
            } else if constexpr (std::is_same_v<Module, codegen::RecordModuleSchema>) {
                return value.records.size();
            } else if constexpr (std::is_same_v<Module, codegen::UnionModuleSchema>) {
                return value.unions.size() + value.tagged_unions.size();
            } else if constexpr (std::is_same_v<Module, codegen::SoaModuleSchema>) {
                return value.structs.size();
            } else if constexpr (std::is_same_v<Module, codegen::VectorModuleSchema>) {
                return 1;
            }
            return 0;
        },
        module);
}

template <typename Function>
void for_each_declaration(codegen::Manifest const& manifest, Function&& function) {
    for (std::size_t module_index{}; module_index < manifest.modules.size(); ++module_index) {
        auto const& module{manifest.modules[module_index]};
        std::visit(
            [&](auto const& value) {
                using Module = std::decay_t<decltype(value)>;
                auto const add{[&](std::size_t const declaration_index, std::string const& name) {
                    function(module_index, declaration_index, value.settings, name);
                }};
                if constexpr (std::is_same_v<Module, codegen::EnumModuleSchema>) {
                    for (std::size_t index{}; index < value.enums.size(); ++index) {
                        add(index, value.enums[index].name);
                    }
                } else if constexpr (std::is_same_v<Module, codegen::PackedValueModuleSchema>) {
                    for (std::size_t index{}; index < value.values.size(); ++index) {
                        add(index, value.values[index].name);
                    }
                } else if constexpr (std::is_same_v<Module, codegen::ScalarModuleSchema>) {
                    for (std::size_t index{}; index < value.scalars.size(); ++index) {
                        add(index, value.scalars[index].name);
                    }
                } else if constexpr (std::is_same_v<Module, codegen::RepresentationModuleSchema>) {
                    for (std::size_t index{}; index < value.linear_quantized.size(); ++index) {
                        add(index, value.linear_quantized[index].name);
                    }
                    for (std::size_t index{}; index < value.integer_varints.size(); ++index) {
                        add(value.linear_quantized.size() + index,
                            value.integer_varints[index].name);
                    }
                    for (std::size_t index{}; index < value.fixed_points.size(); ++index) {
                        add(value.linear_quantized.size() + value.integer_varints.size() + index,
                            value.fixed_points[index].name);
                    }
                    for (std::size_t index{}; index < value.optional_sentinels.size(); ++index) {
                        add(value.linear_quantized.size() + value.integer_varints.size() +
                                value.fixed_points.size() + index,
                            value.optional_sentinels[index].name);
                    }
                    for (std::size_t index{}; index < value.optional_presence_bits.size();
                         ++index) {
                        add(value.linear_quantized.size() + value.integer_varints.size() +
                                value.fixed_points.size() + value.optional_sentinels.size() + index,
                            value.optional_presence_bits[index].name);
                    }
                    for (std::size_t index{}; index < value.mini_floats.size(); ++index) {
                        add(value.linear_quantized.size() + value.integer_varints.size() +
                                value.fixed_points.size() + value.optional_sentinels.size() +
                                value.optional_presence_bits.size() + index,
                            value.mini_floats[index].name);
                    }
                } else if constexpr (std::is_same_v<Module, codegen::RecordModuleSchema>) {
                    for (std::size_t index{}; index < value.records.size(); ++index) {
                        add(index, value.records[index].name);
                    }
                } else if constexpr (std::is_same_v<Module, codegen::UnionModuleSchema>) {
                    for (std::size_t index{}; index < value.unions.size(); ++index) {
                        add(index, value.unions[index].name);
                    }
                    for (std::size_t index{}; index < value.tagged_unions.size(); ++index) {
                        add(value.unions.size() + index, value.tagged_unions[index].name);
                    }
                } else if constexpr (std::is_same_v<Module, codegen::SoaModuleSchema>) {
                    for (std::size_t index{}; index < value.structs.size(); ++index) {
                        add(index, value.structs[index].name);
                    }
                } else if constexpr (std::is_same_v<Module, codegen::VectorModuleSchema>) {
                    add(0, value.storage_name);
                }
            },
            module);
    }
}

} // namespace

EditableSchemaDocument::EditableSchemaDocument(codegen::Manifest manifest,
                                               std::vector<SchemaSourceFile> source_files,
                                               std::filesystem::path types_path,
                                               std::vector<std::filesystem::path> module_paths)
    : manifest_{std::move(manifest)}
    , types_{resolve_type_graph(manifest_)}
    , source_files_{std::move(source_files)}
    , types_path_{std::move(types_path)}
    , module_paths_{std::move(module_paths)} {}

auto EditableSchemaDocument::from_manifest(codegen::Manifest manifest) -> EditableSchemaDocument {
    EditableSchemaDocument result{std::move(manifest), {}};
    result.initialize_declarations({});
    return result;
}

auto EditableSchemaDocument::manifest() const -> codegen::Manifest const& {
    return manifest_;
}

auto EditableSchemaDocument::types() const -> TypeGraph const& {
    return types_;
}

auto EditableSchemaDocument::source_files() const -> std::span<SchemaSourceFile const> {
    return source_files_;
}

auto EditableSchemaDocument::declarations() const -> std::span<DeclarationInfo const> {
    return declarations_;
}

auto EditableSchemaDocument::declaration(DeclarationId const id) const -> DeclarationInfo const* {
    auto const found{std::ranges::find(declarations_, id, &DeclarationInfo::id)};
    return found == declarations_.end() ? nullptr : &*found;
}

auto EditableSchemaDocument::find_declaration(TypeIdentity const& identity) const
    -> std::optional<DeclarationId> {
    auto const found{std::ranges::find(declarations_, identity, &DeclarationInfo::identity)};
    return found == declarations_.end() ? std::nullopt : std::optional{found->id};
}

auto EditableSchemaDocument::enum_schema(DeclarationId const declaration_id) const
    -> codegen::EnumSchema const* {
    auto const* info{declaration(declaration_id)};
    if (info == nullptr) {
        return nullptr;
    }
    auto const* module{
        std::get_if<codegen::EnumModuleSchema>(&manifest_.modules[info->module_index])};
    return module == nullptr || info->declaration_index >= module->enums.size()
             ? nullptr
             : &module->enums[info->declaration_index];
}

auto EditableSchemaDocument::packed_value_schema(DeclarationId const declaration_id) const
    -> codegen::PackedValueSchema const* {
    auto const* info{declaration(declaration_id)};
    if (info == nullptr) {
        return nullptr;
    }
    auto const* module{
        std::get_if<codegen::PackedValueModuleSchema>(&manifest_.modules[info->module_index])};
    return module == nullptr || info->declaration_index >= module->values.size()
             ? nullptr
             : &module->values[info->declaration_index];
}

auto EditableSchemaDocument::integer_scalar_schema(DeclarationId const declaration_id) const
    -> codegen::IntegerScalarSchema const* {
    auto const* info{declaration(declaration_id)};
    if (info == nullptr) {
        return nullptr;
    }
    auto const* module{
        std::get_if<codegen::ScalarModuleSchema>(&manifest_.modules[info->module_index])};
    return module == nullptr || info->declaration_index >= module->scalars.size()
             ? nullptr
             : &module->scalars[info->declaration_index];
}

auto EditableSchemaDocument::linear_quantized_schema(DeclarationId const declaration_id) const
    -> codegen::LinearQuantizedSchema const* {
    auto const* info{declaration(declaration_id)};
    if (info == nullptr) {
        return nullptr;
    }
    auto const* module{
        std::get_if<codegen::RepresentationModuleSchema>(&manifest_.modules[info->module_index])};
    return module == nullptr || info->declaration_index >= module->linear_quantized.size()
             ? nullptr
             : &module->linear_quantized[info->declaration_index];
}

auto EditableSchemaDocument::integer_varint_schema(DeclarationId const declaration_id) const
    -> codegen::IntegerVarintSchema const* {
    auto const* info{declaration(declaration_id)};
    if (info == nullptr) {
        return nullptr;
    }
    auto const* module{
        std::get_if<codegen::RepresentationModuleSchema>(&manifest_.modules[info->module_index])};
    if (module == nullptr || info->declaration_index < module->linear_quantized.size()) {
        return nullptr;
    }
    auto const index{info->declaration_index - module->linear_quantized.size()};
    return index < module->integer_varints.size() ? &module->integer_varints[index] : nullptr;
}

auto EditableSchemaDocument::fixed_point_schema(DeclarationId const declaration_id) const
    -> codegen::FixedPointSchema const* {
    auto const* info{declaration(declaration_id)};
    if (info == nullptr) {
        return nullptr;
    }
    auto const* module{
        std::get_if<codegen::RepresentationModuleSchema>(&manifest_.modules[info->module_index])};
    if (module == nullptr) {
        return nullptr;
    }
    auto const first_index{module->linear_quantized.size() + module->integer_varints.size()};
    if (info->declaration_index < first_index) {
        return nullptr;
    }
    auto const index{info->declaration_index - first_index};
    return index < module->fixed_points.size() ? &module->fixed_points[index] : nullptr;
}

auto EditableSchemaDocument::optional_sentinel_schema(DeclarationId const declaration_id) const
    -> codegen::OptionalSentinelSchema const* {
    auto const* info{declaration(declaration_id)};
    if (info == nullptr) {
        return nullptr;
    }
    auto const* module{
        std::get_if<codegen::RepresentationModuleSchema>(&manifest_.modules[info->module_index])};
    if (module == nullptr) {
        return nullptr;
    }
    auto const first_index{module->linear_quantized.size() + module->integer_varints.size() +
                           module->fixed_points.size()};
    if (info->declaration_index < first_index) {
        return nullptr;
    }
    auto const index{info->declaration_index - first_index};
    return index < module->optional_sentinels.size() ? &module->optional_sentinels[index] : nullptr;
}

auto EditableSchemaDocument::optional_presence_bit_schema(DeclarationId const declaration_id) const
    -> codegen::OptionalPresenceBitSchema const* {
    auto const* info{declaration(declaration_id)};
    if (info == nullptr) {
        return nullptr;
    }
    auto const* module{
        std::get_if<codegen::RepresentationModuleSchema>(&manifest_.modules[info->module_index])};
    if (module == nullptr) {
        return nullptr;
    }
    auto const first_index{module->linear_quantized.size() + module->integer_varints.size() +
                           module->fixed_points.size() + module->optional_sentinels.size()};
    if (info->declaration_index < first_index) {
        return nullptr;
    }
    auto const index{info->declaration_index - first_index};
    return index < module->optional_presence_bits.size() ? &module->optional_presence_bits[index]
                                                         : nullptr;
}

auto EditableSchemaDocument::mini_float_schema(DeclarationId const declaration_id) const
    -> codegen::MiniFloatSchema const* {
    auto const* info{declaration(declaration_id)};
    if (info == nullptr) {
        return nullptr;
    }
    auto const* module{
        std::get_if<codegen::RepresentationModuleSchema>(&manifest_.modules[info->module_index])};
    if (module == nullptr) {
        return nullptr;
    }
    auto const first_index{module->linear_quantized.size() + module->integer_varints.size() +
                           module->fixed_points.size() + module->optional_sentinels.size() +
                           module->optional_presence_bits.size()};
    if (info->declaration_index < first_index) {
        return nullptr;
    }
    auto const index{info->declaration_index - first_index};
    return index < module->mini_floats.size() ? &module->mini_floats[index] : nullptr;
}

auto EditableSchemaDocument::record_schema(DeclarationId const declaration_id) const
    -> codegen::RecordSchema const* {
    auto const* info{declaration(declaration_id)};
    if (info == nullptr) {
        return nullptr;
    }
    auto const* module{
        std::get_if<codegen::RecordModuleSchema>(&manifest_.modules[info->module_index])};
    return module == nullptr || info->declaration_index >= module->records.size()
             ? nullptr
             : &module->records[info->declaration_index];
}

auto EditableSchemaDocument::union_schema(DeclarationId const declaration_id) const
    -> codegen::UnionSchema const* {
    auto const* info{declaration(declaration_id)};
    if (info == nullptr) {
        return nullptr;
    }
    auto const* module{
        std::get_if<codegen::UnionModuleSchema>(&manifest_.modules[info->module_index])};
    return module == nullptr || info->declaration_index >= module->unions.size()
             ? nullptr
             : &module->unions[info->declaration_index];
}

auto EditableSchemaDocument::tagged_union_schema(DeclarationId const declaration_id) const
    -> codegen::TaggedUnionSchema const* {
    auto const* info{declaration(declaration_id)};
    if (info == nullptr) {
        return nullptr;
    }
    auto const* module{
        std::get_if<codegen::UnionModuleSchema>(&manifest_.modules[info->module_index])};
    if (module == nullptr || info->declaration_index < module->unions.size()) {
        return nullptr;
    }
    auto const index{info->declaration_index - module->unions.size()};
    return index < module->tagged_unions.size() ? &module->tagged_unions[index] : nullptr;
}

auto EditableSchemaDocument::soa_schema(DeclarationId const declaration_id) const
    -> codegen::SoaSchema const* {
    auto const* info{declaration(declaration_id)};
    if (info == nullptr) {
        return nullptr;
    }
    auto const* module{
        std::get_if<codegen::SoaModuleSchema>(&manifest_.modules[info->module_index])};
    return module == nullptr || info->declaration_index >= module->structs.size()
             ? nullptr
             : &module->structs[info->declaration_index];
}

auto EditableSchemaDocument::allocate_declaration_id() -> DeclarationId {
    return DeclarationId{next_declaration_id_++};
}

auto EditableSchemaDocument::apply(SchemaEditCommand command)
    -> std::expected<bool, SchemaEditError> {
    auto inverse{execute(command)};
    if (!inverse.has_value()) {
        return std::unexpected{std::move(inverse.error())};
    }
    if (!inverse->has_value()) {
        return false;
    }

    if (saved_history_position_.has_value() && *saved_history_position_ > history_position_) {
        saved_history_position_.reset();
    }
    history_.erase(history_.begin() + static_cast<std::ptrdiff_t>(history_position_),
                   history_.end());
    history_.push_back(
        HistoryEntry{.forward = std::move(command), .inverse = std::move(**inverse)});
    ++history_position_;
    ++revision_;
    return true;
}

auto EditableSchemaDocument::undo() -> std::expected<bool, SchemaEditError> {
    if (!can_undo()) {
        return false;
    }
    auto const& entry{history_[history_position_ - 1]};
    auto result{execute(entry.inverse)};
    if (!result.has_value()) {
        return std::unexpected{std::move(result.error())};
    }
    if (!result->has_value()) {
        return std::unexpected{SchemaEditError{"Undo command did not change the schema draft"}};
    }
    --history_position_;
    ++revision_;
    return true;
}

auto EditableSchemaDocument::redo() -> std::expected<bool, SchemaEditError> {
    if (!can_redo()) {
        return false;
    }
    auto const& entry{history_[history_position_]};
    auto result{execute(entry.forward)};
    if (!result.has_value()) {
        return std::unexpected{std::move(result.error())};
    }
    if (!result->has_value()) {
        return std::unexpected{SchemaEditError{"Redo command did not change the schema draft"}};
    }
    ++history_position_;
    ++revision_;
    return true;
}

auto EditableSchemaDocument::can_undo() const -> bool {
    return history_position_ != 0;
}

auto EditableSchemaDocument::can_redo() const -> bool {
    return history_position_ != history_.size();
}

auto EditableSchemaDocument::dirty() const -> bool {
    return !saved_history_position_.has_value() || history_position_ != *saved_history_position_;
}

auto EditableSchemaDocument::revision() const -> std::uint64_t {
    return revision_;
}

void EditableSchemaDocument::mark_saved() {
    saved_history_position_ = history_position_;
}

auto EditableSchemaDocument::preview_source_updates() const
    -> std::expected<std::vector<SchemaSourceUpdate>, SchemaEditError> {
    if (!dirty()) {
        return std::vector<SchemaSourceUpdate>{};
    }
    if (source_files_.empty() || module_source_ranges_.size() != manifest_.modules.size()) {
        return std::unexpected{SchemaEditError{"Schema draft has no source ownership information"}};
    }

    std::set<DeclarationId> touched;
    for (std::size_t index{}; index < history_position_; ++index) {
        std::visit(
            [&](auto const& edit) {
                if constexpr (requires { edit.enum_declaration; }) {
                    touched.insert(edit.enum_declaration);
                } else {
                    touched.insert(edit.declaration);
                    if constexpr (std::is_same_v<std::decay_t<decltype(edit)>, RenameDeclaration>) {
                        auto const* renamed{declaration(edit.declaration)};
                        if (renamed != nullptr) {
                            auto const type{types_.find(renamed->identity)};
                            if (type.has_value()) {
                                for (auto const user : types_.users_of(*type)) {
                                    auto const user_declaration{
                                        find_declaration(types_.type(user).identity)};
                                    if (user_declaration.has_value()) {
                                        touched.insert(*user_declaration);
                                    }
                                }
                            }
                        }
                    }
                }
            },
            history_[index].forward);
    }

    struct Replacement {
        std::size_t begin{};
        std::size_t end{};
        std::string text;
    };
    std::vector<std::vector<Replacement>> replacements(source_files_.size());
    std::map<std::size_t, std::set<DeclarationId>> insertions;
    auto render_source_aware = [&](DeclarationId const id,
                                   auto const& schema,
                                   auto const preserve,
                                   auto const canonical) -> std::optional<std::string> {
        auto const* info{declaration(id)};
        if (info != nullptr && info->source.has_value()) {
            auto const& range{*info->source};
            auto const& source{source_files_[range.source_file_index].text};
            auto const original{std::string_view{source}.substr(
                range.begin_offset, range.end_offset - range.begin_offset)};
            if (auto preserved{preserve(schema, original)}) {
                return preserved;
            }
        }
        return canonical(schema);
    };
    auto render_declaration = [&](DeclarationId const id) -> std::optional<std::string> {
        if (auto const* schema{enum_schema(id)}) {
            return render_source_aware(id, *schema, try_render_source_preserved_enum, render_enum);
        }
        if (auto const* schema{packed_value_schema(id)}) {
            return render_source_aware(
                id, *schema, try_render_source_preserved_packed_value, render_packed_value);
        }
        if (auto const* schema{integer_scalar_schema(id)}) {
            return render_source_aware(
                id, *schema, try_render_source_preserved_integer_scalar, render_integer_scalar);
        }
        if (auto const* schema{linear_quantized_schema(id)}) {
            return render_source_aware(
                id, *schema, try_render_source_preserved_linear_quantized, render_linear_quantized);
        }
        if (auto const* schema{integer_varint_schema(id)}) {
            return render_source_aware(
                id, *schema, try_render_source_preserved_integer_varint, render_integer_varint);
        }
        if (auto const* schema{fixed_point_schema(id)}) {
            return render_source_aware(
                id, *schema, try_render_source_preserved_fixed_point, render_fixed_point);
        }
        if (auto const* schema{optional_sentinel_schema(id)}) {
            return render_source_aware(id,
                                       *schema,
                                       try_render_source_preserved_optional_sentinel,
                                       render_optional_sentinel);
        }
        if (auto const* schema{optional_presence_bit_schema(id)}) {
            return render_source_aware(id,
                                       *schema,
                                       try_render_source_preserved_optional_presence_bit,
                                       render_optional_presence_bit);
        }
        if (auto const* schema{mini_float_schema(id)}) {
            return render_source_aware(
                id, *schema, try_render_source_preserved_mini_float, render_mini_float);
        }
        if (auto const* schema{record_schema(id)}) {
            return render_source_aware(
                id, *schema, try_render_source_preserved_record, render_record);
        }
        if (auto const* schema{union_schema(id)}) {
            return render_source_aware(
                id, *schema, try_render_source_preserved_union, render_union);
        }
        if (auto const* schema{tagged_union_schema(id)}) {
            return render_source_aware(
                id, *schema, try_render_source_preserved_tagged_union, render_tagged_union);
        }
        if (auto const* schema{soa_schema(id)}) {
            return render_source_aware(id, *schema, try_render_source_preserved_soa, render_soa);
        }
        return std::nullopt;
    };
    for (auto const id : touched) {
        auto const* info{declaration(id)};
        if (info == nullptr) {
            auto const tombstone{source_tombstones_.find(id)};
            if (tombstone != source_tombstones_.end()) {
                auto const& source{tombstone->second};
                replacements[source.source_file_index].push_back(
                    {.begin = source.begin_offset, .end = source.end_offset, .text = {}});
            }
            continue;
        }
        auto const rendered{render_declaration(id)};
        if (!rendered.has_value()) {
            continue;
        }
        if (info->source.has_value()) {
            replacements[info->source->source_file_index].push_back(
                {.begin = info->source->begin_offset,
                 .end = info->source->end_offset,
                 .text = *rendered});
        } else {
            insertions[info->module_index].insert(id);
        }
    }
    for (auto const& [module_index, ids] : insertions) {
        if (module_index >= module_source_ranges_.size() ||
            !module_source_ranges_[module_index].has_value()) {
            return std::unexpected{SchemaEditError{"New declaration's module has no source range"}};
        }
        auto const& module_range{*module_source_ranges_[module_index]};
        std::string insertion;
        std::vector<DeclarationInfo const*> ordered;
        for (auto const& declaration_info : declarations_) {
            if (declaration_info.module_index == module_index &&
                ids.contains(declaration_info.id)) {
                ordered.push_back(&declaration_info);
            }
        }
        std::ranges::sort(ordered, {}, &DeclarationInfo::declaration_index);
        for (auto const* declaration_info : ordered) {
            auto const rendered{render_declaration(declaration_info->id)};
            if (!rendered.has_value()) {
                return std::unexpected{
                    SchemaEditError{"New declaration kind cannot be serialized"}};
            }
            insertion += "\n  " + *rendered;
        }
        replacements[module_range.source_file_index].push_back(
            {.begin = module_range.end_offset - 1,
             .end = module_range.end_offset - 1,
             .text = std::move(insertion)});
    }

    std::vector<SchemaSourceUpdate> updates;
    for (std::size_t source_index{}; source_index < replacements.size(); ++source_index) {
        auto& source_replacements{replacements[source_index]};
        if (source_replacements.empty()) {
            continue;
        }
        std::ranges::sort(source_replacements, std::greater{}, &Replacement::begin);
        auto updated{source_files_[source_index].text};
        for (auto const& replacement : source_replacements) {
            if (replacement.begin > replacement.end || replacement.end > updated.size()) {
                return std::unexpected{SchemaEditError{"Invalid source replacement range"}};
            }
            updated.replace(
                replacement.begin, replacement.end - replacement.begin, replacement.text);
        }
        updates.push_back({.path = source_files_[source_index].path,
                           .original = source_files_[source_index].text,
                           .updated = std::move(updated)});
    }
    return updates;
}

auto EditableSchemaDocument::save()
    -> std::expected<std::vector<std::filesystem::path>, SchemaEditError> {
    auto updates{preview_source_updates()};
    if (!updates.has_value()) {
        return std::unexpected{std::move(updates.error())};
    }
    if (updates->empty()) {
        return std::vector<std::filesystem::path>{};
    }

    std::map<std::filesystem::path, std::filesystem::path> temporary_paths;
    auto cleanup{[&] {
        std::error_code ignored;
        for (auto const& [path, temporary] : temporary_paths) {
            static_cast<void>(path);
            std::filesystem::remove(temporary, ignored);
        }
    }};
    try {
        for (auto const& update : *updates) {
            auto temporary{update.path};
            temporary += ".layout-planner.tmp";
            std::error_code ignored;
            std::filesystem::remove(temporary, ignored);
            std::ofstream output{temporary, std::ios::binary | std::ios::trunc};
            output.write(update.updated.data(),
                         static_cast<std::streamsize>(update.updated.size()));
            output.close();
            if (!output) {
                throw std::runtime_error{"Cannot write temporary LispB source: " +
                                         temporary.string()};
            }
            temporary_paths.emplace(update.path, std::move(temporary));
        }

        std::vector<std::filesystem::path> validation_modules;
        validation_modules.reserve(module_paths_.size());
        for (auto const& path : module_paths_) {
            auto const temporary{temporary_paths.find(path)};
            validation_modules.push_back(temporary == temporary_paths.end() ? path
                                                                            : temporary->second);
        }
        auto const validated{codegen::load_sources(types_path_, validation_modules)};
        static_cast<void>(resolve_type_graph(validated));

        std::vector<std::filesystem::path> saved;
        saved.reserve(updates->size());
        for (auto const& update : *updates) {
            replace_file(temporary_paths.at(update.path), update.path);
            saved.push_back(update.path);
        }
        auto reloaded{load_editable_schema_document(types_path_, module_paths_)};
        *this = std::move(reloaded);
        return saved;
    } catch (std::exception const& error) {
        cleanup();
        return std::unexpected{SchemaEditError{error.what()}};
    }
}

void EditableSchemaDocument::initialize_declarations(
    std::vector<std::optional<SourceRange>> source_ranges) {
    declarations_.clear();
    auto source_index{std::size_t{}};
    auto next_id{std::uint64_t{1}};
    for_each_declaration(manifest_,
                         [&](std::size_t const module_index,
                             std::size_t const declaration_index,
                             codegen::ModuleSettings const& settings,
                             std::string const& name) {
                             auto const type{types_.find_declared(settings.name, name)};
                             if (!type.has_value()) {
                                 throw std::logic_error{"Resolved graph omitted declaration '" +
                                                        settings.name + ":" + name + "'"};
                             }
                             auto source{source_index < source_ranges.size()
                                             ? source_ranges[source_index]
                                             : std::nullopt};
                             declarations_.push_back({.id = DeclarationId{next_id++},
                                                      .identity = types_.type(*type).identity,
                                                      .module_index = module_index,
                                                      .declaration_index = declaration_index,
                                                      .source = std::move(source)});
                             ++source_index;
                         });
    if (!source_ranges.empty() && source_index != source_ranges.size()) {
        throw std::logic_error{"Source declaration count does not match resolved schema"};
    }
    next_declaration_id_ = next_id;
}

auto EditableSchemaDocument::execute(SchemaEditCommand const& command)
    -> std::expected<std::optional<SchemaEditCommand>, SchemaEditError> {
    auto deletion_blocker = [&](DeclarationInfo const& info) -> std::optional<SchemaEditError> {
        auto const type{types_.find(info.identity)};
        if (!type.has_value()) {
            return SchemaEditError{"Declaration is missing from the type graph"};
        }

        for (auto const& [registered_name, cpp_type] : manifest_.types) {
            static_cast<void>(cpp_type);
            if (types_.find_registered(registered_name) == type) {
                return SchemaEditError{"Cannot delete declaration '" + info.identity.name +
                                       "'; it is registered as '@" + registered_name + "'"};
            }
        }

        auto const users{types_.users_of(*type)};
        if (users.empty()) {
            return std::nullopt;
        }

        auto message{"Cannot delete declaration '" + info.identity.name + "'; it is used by "};
        for (std::size_t index{}; index < users.size(); ++index) {
            if (index != 0) {
                message += ", ";
            }
            message += "'" + types_.type(users[index]).cpp_spelling + "'";
        }
        return SchemaEditError{std::move(message)};
    };
    auto creation_source = [&](DeclarationId const declaration_id) -> std::optional<SourceRange> {
        auto const found{source_tombstones_.find(declaration_id)};
        return found == source_tombstones_.end() ? std::nullopt : std::optional{found->second};
    };
    auto remember_source_tombstone = [&](DeclarationInfo const& info) {
        if (info.source.has_value()) {
            source_tombstones_.insert_or_assign(info.id, *info.source);
        }
    };

    return std::visit(
        [&](auto const& edit) -> std::expected<std::optional<SchemaEditCommand>, SchemaEditError> {
            using Edit = std::decay_t<decltype(edit)>;
            if constexpr (std::is_same_v<Edit, SetEnumeratorDisplayName>) {
                auto const* info{declaration(edit.enum_declaration)};
                if (info == nullptr) {
                    return std::unexpected{SchemaEditError{"Unknown declaration id"}};
                }
                auto* module{
                    std::get_if<codegen::EnumModuleSchema>(&manifest_.modules[info->module_index])};
                if (module == nullptr || info->declaration_index >= module->enums.size()) {
                    return std::unexpected{
                        SchemaEditError{"Display names can only be edited on enum declarations"}};
                }
                auto& schema{module->enums[info->declaration_index]};
                auto value{std::ranges::find(
                    schema.values, edit.enumerator_name, &codegen::EnumeratorSchema::name)};
                if (value == schema.values.end()) {
                    return std::unexpected{SchemaEditError{"Enum '" + schema.name +
                                                           "' has no enumerator named '" +
                                                           edit.enumerator_name + "'"}};
                }
                if (value->display_name == edit.display_name) {
                    return std::nullopt;
                }

                auto const previous{value->display_name};
                value->display_name = edit.display_name;
                try {
                    auto resolved{resolve_type_graph(manifest_)};
                    types_ = std::move(resolved);
                } catch (std::exception const& error) {
                    value->display_name = previous;
                    return std::unexpected{SchemaEditError{error.what()}};
                }
                return SchemaEditCommand{
                    SetEnumeratorDisplayName{.enum_declaration = edit.enum_declaration,
                                             .enumerator_name = edit.enumerator_name,
                                             .display_name = previous}};
            } else if constexpr (std::is_same_v<Edit, SetEnumeratorName>) {
                auto const* info{declaration(edit.enum_declaration)};
                if (info == nullptr) {
                    return std::unexpected{SchemaEditError{"Unknown declaration id"}};
                }
                auto* module{
                    std::get_if<codegen::EnumModuleSchema>(&manifest_.modules[info->module_index])};
                if (module == nullptr || info->declaration_index >= module->enums.size()) {
                    return std::unexpected{
                        SchemaEditError{"Enumerator names can only be edited on enums"}};
                }
                auto& schema{module->enums[info->declaration_index]};
                auto value{std::ranges::find(
                    schema.values, edit.current_name, &codegen::EnumeratorSchema::name)};
                if (value == schema.values.end()) {
                    return std::unexpected{SchemaEditError{"Enum '" + schema.name +
                                                           "' has no enumerator named '" +
                                                           edit.current_name + "'"}};
                }
                if (edit.current_name == edit.new_name) {
                    return std::nullopt;
                }

                auto const previous_count{schema.count};
                value->name = edit.new_name;
                if (schema.count == edit.current_name) {
                    schema.count = edit.new_name;
                }
                try {
                    auto resolved{resolve_type_graph(manifest_)};
                    types_ = std::move(resolved);
                } catch (std::exception const& error) {
                    value->name = edit.current_name;
                    schema.count = previous_count;
                    return std::unexpected{SchemaEditError{error.what()}};
                }
                return SchemaEditCommand{
                    SetEnumeratorName{.enum_declaration = edit.enum_declaration,
                                      .current_name = edit.new_name,
                                      .new_name = edit.current_name}};
            } else if constexpr (std::is_same_v<Edit, CreateEnum>) {
                if (!edit.declaration.valid() || declaration(edit.declaration) != nullptr) {
                    return std::unexpected{
                        SchemaEditError{"New enum requires a unique declaration id"}};
                }
                if (edit.module_index >= manifest_.modules.size()) {
                    return std::unexpected{SchemaEditError{"Unknown enum module index"}};
                }
                auto* module{
                    std::get_if<codegen::EnumModuleSchema>(&manifest_.modules[edit.module_index])};
                if (module == nullptr) {
                    return std::unexpected{
                        SchemaEditError{"New enums can only be added to enum modules"}};
                }
                auto const insertion_index{edit.insertion_index.value_or(module->enums.size())};
                if (insertion_index > module->enums.size()) {
                    return std::unexpected{SchemaEditError{"Invalid enum insertion index"}};
                }

                module->enums.insert(module->enums.begin() +
                                         static_cast<std::ptrdiff_t>(insertion_index),
                                     edit.schema);
                for (auto& existing : declarations_) {
                    if (existing.module_index == edit.module_index &&
                        existing.declaration_index >= insertion_index) {
                        ++existing.declaration_index;
                    }
                }
                auto const namespace_name{module->settings.namespace_name.value_or("")};
                declarations_.push_back(
                    {.id = edit.declaration,
                     .identity = TypeIdentity{.origin = TypeOrigin::declaration,
                                              .module_name = module->settings.name,
                                              .namespace_name = namespace_name,
                                              .name = edit.schema.name},
                     .module_index = edit.module_index,
                     .declaration_index = insertion_index,
                     .source = creation_source(edit.declaration)});
                try {
                    types_ = resolve_type_graph(manifest_);
                } catch (std::exception const& error) {
                    declarations_.pop_back();
                    for (auto& existing : declarations_) {
                        if (existing.module_index == edit.module_index &&
                            existing.declaration_index > insertion_index) {
                            --existing.declaration_index;
                        }
                    }
                    module->enums.erase(module->enums.begin() +
                                        static_cast<std::ptrdiff_t>(insertion_index));
                    return std::unexpected{SchemaEditError{error.what()}};
                }
                return SchemaEditCommand{DeleteEnum{.declaration = edit.declaration}};
            } else if constexpr (std::is_same_v<Edit, ReplaceEnum>) {
                auto const* info{declaration(edit.declaration)};
                auto const* current{enum_schema(edit.declaration)};
                if (info == nullptr || current == nullptr) {
                    return std::unexpected{SchemaEditError{"Unknown enum declaration"}};
                }
                if (edit.schema.name != current->name) {
                    return std::unexpected{SchemaEditError{
                        "ReplaceEnum cannot rename a declaration; use a rename command"}};
                }
                auto* module{
                    std::get_if<codegen::EnumModuleSchema>(&manifest_.modules[info->module_index])};
                auto previous{module->enums[info->declaration_index]};
                module->enums[info->declaration_index] = edit.schema;
                try {
                    types_ = resolve_type_graph(manifest_);
                } catch (std::exception const& error) {
                    module->enums[info->declaration_index] = std::move(previous);
                    return std::unexpected{SchemaEditError{error.what()}};
                }
                return SchemaEditCommand{
                    ReplaceEnum{.declaration = edit.declaration, .schema = std::move(previous)}};
            } else if constexpr (std::is_same_v<Edit, DeleteEnum>) {
                auto const* found{declaration(edit.declaration)};
                auto const* current{enum_schema(edit.declaration)};
                if (found == nullptr || current == nullptr) {
                    return std::unexpected{SchemaEditError{"Unknown enum declaration"}};
                }
                if (auto const blocker{deletion_blocker(*found)}; blocker.has_value()) {
                    return std::unexpected{*blocker};
                }
                auto const info{*found};
                auto schema{*current};
                auto* module{
                    std::get_if<codegen::EnumModuleSchema>(&manifest_.modules[info.module_index])};
                module->enums.erase(module->enums.begin() +
                                    static_cast<std::ptrdiff_t>(info.declaration_index));
                declarations_.erase(
                    std::ranges::find(declarations_, edit.declaration, &DeclarationInfo::id));
                for (auto& existing : declarations_) {
                    if (existing.module_index == info.module_index &&
                        existing.declaration_index > info.declaration_index) {
                        --existing.declaration_index;
                    }
                }
                try {
                    codegen::validate_manifest(manifest_);
                    types_ = resolve_type_graph(manifest_);
                } catch (std::exception const& error) {
                    module->enums.insert(module->enums.begin() +
                                             static_cast<std::ptrdiff_t>(info.declaration_index),
                                         schema);
                    for (auto& existing : declarations_) {
                        if (existing.module_index == info.module_index &&
                            existing.declaration_index >= info.declaration_index) {
                            ++existing.declaration_index;
                        }
                    }
                    declarations_.push_back(info);
                    return std::unexpected{SchemaEditError{error.what()}};
                }
                remember_source_tombstone(info);
                return SchemaEditCommand{CreateEnum{.declaration = edit.declaration,
                                                    .module_index = info.module_index,
                                                    .schema = std::move(schema),
                                                    .insertion_index = info.declaration_index}};
            } else if constexpr (std::is_same_v<Edit, CreatePackedValue>) {
                if (!edit.declaration.valid() || declaration(edit.declaration) != nullptr) {
                    return std::unexpected{
                        SchemaEditError{"New packed value requires a unique declaration id"}};
                }
                if (edit.module_index >= manifest_.modules.size()) {
                    return std::unexpected{SchemaEditError{"Unknown packed-value module index"}};
                }
                auto* module{std::get_if<codegen::PackedValueModuleSchema>(
                    &manifest_.modules[edit.module_index])};
                if (module == nullptr) {
                    return std::unexpected{SchemaEditError{
                        "New packed values can only be added to packed-value modules"}};
                }
                auto const insertion_index{edit.insertion_index.value_or(module->values.size())};
                if (insertion_index > module->values.size()) {
                    return std::unexpected{SchemaEditError{"Invalid packed-value insertion index"}};
                }

                module->values.insert(module->values.begin() +
                                          static_cast<std::ptrdiff_t>(insertion_index),
                                      edit.schema);
                for (auto& existing : declarations_) {
                    if (existing.module_index == edit.module_index &&
                        existing.declaration_index >= insertion_index) {
                        ++existing.declaration_index;
                    }
                }
                auto const namespace_name{module->settings.namespace_name.value_or("")};
                declarations_.push_back(
                    {.id = edit.declaration,
                     .identity = TypeIdentity{.origin = TypeOrigin::declaration,
                                              .module_name = module->settings.name,
                                              .namespace_name = namespace_name,
                                              .name = edit.schema.name},
                     .module_index = edit.module_index,
                     .declaration_index = insertion_index,
                     .source = creation_source(edit.declaration)});
                try {
                    types_ = resolve_type_graph(manifest_);
                } catch (std::exception const& error) {
                    declarations_.pop_back();
                    for (auto& existing : declarations_) {
                        if (existing.module_index == edit.module_index &&
                            existing.declaration_index > insertion_index) {
                            --existing.declaration_index;
                        }
                    }
                    module->values.erase(module->values.begin() +
                                         static_cast<std::ptrdiff_t>(insertion_index));
                    return std::unexpected{SchemaEditError{error.what()}};
                }
                return SchemaEditCommand{DeletePackedValue{.declaration = edit.declaration}};
            } else if constexpr (std::is_same_v<Edit, ReplacePackedValue>) {
                auto const* info{declaration(edit.declaration)};
                auto const* current{packed_value_schema(edit.declaration)};
                if (info == nullptr || current == nullptr) {
                    return std::unexpected{SchemaEditError{"Unknown packed-value declaration"}};
                }
                if (edit.schema.name != current->name) {
                    return std::unexpected{SchemaEditError{
                        "ReplacePackedValue cannot rename a declaration; use a rename command"}};
                }
                auto* module{std::get_if<codegen::PackedValueModuleSchema>(
                    &manifest_.modules[info->module_index])};
                auto previous{module->values[info->declaration_index]};
                module->values[info->declaration_index] = edit.schema;
                try {
                    types_ = resolve_type_graph(manifest_);
                } catch (std::exception const& error) {
                    module->values[info->declaration_index] = std::move(previous);
                    return std::unexpected{SchemaEditError{error.what()}};
                }
                return SchemaEditCommand{ReplacePackedValue{.declaration = edit.declaration,
                                                            .schema = std::move(previous)}};
            } else if constexpr (std::is_same_v<Edit, DeletePackedValue>) {
                auto const* found{declaration(edit.declaration)};
                auto const* current{packed_value_schema(edit.declaration)};
                if (found == nullptr || current == nullptr) {
                    return std::unexpected{SchemaEditError{"Unknown packed-value declaration"}};
                }
                if (auto const blocker{deletion_blocker(*found)}; blocker.has_value()) {
                    return std::unexpected{*blocker};
                }
                auto const info{*found};
                auto schema{*current};
                auto* module{std::get_if<codegen::PackedValueModuleSchema>(
                    &manifest_.modules[info.module_index])};
                module->values.erase(module->values.begin() +
                                     static_cast<std::ptrdiff_t>(info.declaration_index));
                declarations_.erase(
                    std::ranges::find(declarations_, edit.declaration, &DeclarationInfo::id));
                for (auto& existing : declarations_) {
                    if (existing.module_index == info.module_index &&
                        existing.declaration_index > info.declaration_index) {
                        --existing.declaration_index;
                    }
                }
                try {
                    codegen::validate_manifest(manifest_);
                    types_ = resolve_type_graph(manifest_);
                } catch (std::exception const& error) {
                    module->values.insert(module->values.begin() +
                                              static_cast<std::ptrdiff_t>(info.declaration_index),
                                          schema);
                    for (auto& existing : declarations_) {
                        if (existing.module_index == info.module_index &&
                            existing.declaration_index >= info.declaration_index) {
                            ++existing.declaration_index;
                        }
                    }
                    declarations_.push_back(info);
                    return std::unexpected{SchemaEditError{error.what()}};
                }
                remember_source_tombstone(info);
                return SchemaEditCommand{
                    CreatePackedValue{.declaration = edit.declaration,
                                      .module_index = info.module_index,
                                      .schema = std::move(schema),
                                      .insertion_index = info.declaration_index}};
            } else if constexpr (std::is_same_v<Edit, CreateIntegerScalar>) {
                if (!edit.declaration.valid() || declaration(edit.declaration) != nullptr) {
                    return std::unexpected{
                        SchemaEditError{"New integer scalar requires a unique declaration id"}};
                }
                if (edit.module_index >= manifest_.modules.size()) {
                    return std::unexpected{SchemaEditError{"Unknown scalar module index"}};
                }
                auto* module{std::get_if<codegen::ScalarModuleSchema>(
                    &manifest_.modules[edit.module_index])};
                if (module == nullptr) {
                    return std::unexpected{
                        SchemaEditError{"New integer scalars can only be added to scalar modules"}};
                }
                auto const insertion_index{edit.insertion_index.value_or(module->scalars.size())};
                if (insertion_index > module->scalars.size()) {
                    return std::unexpected{SchemaEditError{"Invalid scalar insertion index"}};
                }

                module->scalars.insert(module->scalars.begin() +
                                           static_cast<std::ptrdiff_t>(insertion_index),
                                       edit.schema);
                for (auto& existing : declarations_) {
                    if (existing.module_index == edit.module_index &&
                        existing.declaration_index >= insertion_index) {
                        ++existing.declaration_index;
                    }
                }
                auto const namespace_name{module->settings.namespace_name.value_or("")};
                declarations_.push_back(
                    {.id = edit.declaration,
                     .identity = TypeIdentity{.origin = TypeOrigin::declaration,
                                              .module_name = module->settings.name,
                                              .namespace_name = namespace_name,
                                              .name = edit.schema.name},
                     .module_index = edit.module_index,
                     .declaration_index = insertion_index,
                     .source = creation_source(edit.declaration)});
                try {
                    types_ = resolve_type_graph(manifest_);
                } catch (std::exception const& error) {
                    declarations_.pop_back();
                    for (auto& existing : declarations_) {
                        if (existing.module_index == edit.module_index &&
                            existing.declaration_index > insertion_index) {
                            --existing.declaration_index;
                        }
                    }
                    module->scalars.erase(module->scalars.begin() +
                                          static_cast<std::ptrdiff_t>(insertion_index));
                    return std::unexpected{SchemaEditError{error.what()}};
                }
                return SchemaEditCommand{DeleteIntegerScalar{.declaration = edit.declaration}};
            } else if constexpr (std::is_same_v<Edit, ReplaceIntegerScalar>) {
                auto const* info{declaration(edit.declaration)};
                auto const* current{integer_scalar_schema(edit.declaration)};
                if (info == nullptr || current == nullptr) {
                    return std::unexpected{SchemaEditError{"Unknown integer-scalar declaration"}};
                }
                if (edit.schema.name != current->name) {
                    return std::unexpected{SchemaEditError{
                        "ReplaceIntegerScalar cannot rename a declaration; use a rename command"}};
                }
                auto* module{std::get_if<codegen::ScalarModuleSchema>(
                    &manifest_.modules[info->module_index])};
                auto previous{module->scalars[info->declaration_index]};
                module->scalars[info->declaration_index] = edit.schema;
                try {
                    types_ = resolve_type_graph(manifest_);
                } catch (std::exception const& error) {
                    module->scalars[info->declaration_index] = std::move(previous);
                    return std::unexpected{SchemaEditError{error.what()}};
                }
                return SchemaEditCommand{ReplaceIntegerScalar{.declaration = edit.declaration,
                                                              .schema = std::move(previous)}};
            } else if constexpr (std::is_same_v<Edit, RenameDeclaration>) {
                auto const declaration_it{
                    std::ranges::find(declarations_, edit.declaration, &DeclarationInfo::id)};
                if (declaration_it == declarations_.end()) {
                    return std::unexpected{SchemaEditError{"Unknown declaration"}};
                }
                if (edit.new_name == declaration_it->identity.name) {
                    return std::nullopt;
                }

                auto const target{types_.find(declaration_it->identity)};
                if (!target.has_value()) {
                    return std::unexpected{
                        SchemaEditError{"Declaration is missing from the type graph"}};
                }
                auto const& target_definition{types_.type(*target).definition};
                if (std::holds_alternative<SoaType>(target_definition)) {
                    return std::unexpected{SchemaEditError{
                        "Renaming SoA declarations requires nested/generated-name repair and is "
                        "not enabled yet"}};
                }
                for (auto const& [registered_name, cpp_type] : manifest_.types) {
                    static_cast<void>(cpp_type);
                    if (types_.find_registered(registered_name) == target) {
                        return std::unexpected{SchemaEditError{
                            "Renaming a declaration registered as '@" + registered_name +
                            "' requires source-aware types-registry editing"}};
                    }
                }

                auto const old_name{declaration_it->identity.name};
                auto const previous_manifest{manifest_};
                auto const previous_declarations{declarations_};
                auto const& target_settings{std::visit(
                    [](auto const& module) -> codegen::ModuleSettings const& {
                        return module.settings;
                    },
                    manifest_.modules[declaration_it->module_index])};
                auto const new_spelling{target_settings.namespace_name.has_value()
                                            ? *target_settings.namespace_name + "::" + edit.new_name
                                            : edit.new_name};
                auto repair_ref = [&](codegen::TypeRef& reference, TypeId const resolved) {
                    if (resolved != *target) {
                        return;
                    }
                    if (reference.name.starts_with('@')) {
                        throw std::invalid_argument{
                            "Cannot repair registered semantic reference '" + reference.name +
                            "' while renaming '" + old_name + "'"};
                    }
                    reference.name = new_spelling;
                };

                try {
                    for (auto const& user_info : declarations_) {
                        auto const user_type{types_.find(user_info.identity)};
                        if (!user_type.has_value()) {
                            continue;
                        }
                        auto const& definition{types_.type(*user_type).definition};
                        auto& module{manifest_.modules[user_info.module_index]};
                        if (auto const* resolved{std::get_if<LinearQuantizedType>(&definition)}) {
                            auto& representations{
                                std::get<codegen::RepresentationModuleSchema>(module)};
                            repair_ref(representations.linear_quantized[user_info.declaration_index]
                                           .source,
                                       resolved->source.type);
                        } else if (auto const* resolved{
                                       std::get_if<IntegerVarintType>(&definition)}) {
                            auto& representations{
                                std::get<codegen::RepresentationModuleSchema>(module)};
                            auto const index{user_info.declaration_index -
                                             representations.linear_quantized.size()};
                            repair_ref(representations.integer_varints[index].source,
                                       resolved->source.type);
                        } else if (auto const* resolved{
                                       std::get_if<OptionalSentinelType>(&definition)}) {
                            auto& representations{
                                std::get<codegen::RepresentationModuleSchema>(module)};
                            auto const index{user_info.declaration_index -
                                             representations.linear_quantized.size() -
                                             representations.integer_varints.size() -
                                             representations.fixed_points.size()};
                            repair_ref(representations.optional_sentinels[index].source,
                                       resolved->source.type);
                        } else if (auto const* resolved{
                                       std::get_if<OptionalPresenceBitType>(&definition)}) {
                            auto& representations{
                                std::get<codegen::RepresentationModuleSchema>(module)};
                            auto const index{user_info.declaration_index -
                                             representations.linear_quantized.size() -
                                             representations.integer_varints.size() -
                                             representations.fixed_points.size() -
                                             representations.optional_sentinels.size()};
                            repair_ref(representations.optional_presence_bits[index].source,
                                       resolved->source.type);
                        } else if (auto const* resolved{std::get_if<PackedType>(&definition)}) {
                            auto& schema{std::get<codegen::PackedValueModuleSchema>(module)
                                             .values[user_info.declaration_index]};
                            for (std::size_t index{}; index < schema.segments.size(); ++index) {
                                auto* source_field{std::get_if<codegen::PackedFieldSchema>(
                                    &schema.segments[index])};
                                auto const* resolved_field{
                                    std::get_if<PackedField>(&resolved->segments[index])};
                                if (source_field == nullptr || resolved_field == nullptr) {
                                    continue;
                                }
                                repair_ref(source_field->type, resolved_field->semantic_type.type);
                                if (source_field->relationship.has_value() &&
                                    resolved_field->relationship.has_value()) {
                                    repair_ref(source_field->relationship->target,
                                               resolved_field->relationship->target.type);
                                }
                            }
                        } else if (auto const* resolved{std::get_if<RecordType>(&definition)}) {
                            auto& schema{std::get<codegen::RecordModuleSchema>(module)
                                             .records[user_info.declaration_index]};
                            for (std::size_t index{}; index < schema.members.size(); ++index) {
                                repair_ref(schema.members[index].type,
                                           resolved->members[index].semantic_type.type);
                            }
                        } else if (auto const* resolved{std::get_if<UnionType>(&definition)}) {
                            auto& schema{std::get<codegen::UnionModuleSchema>(module)
                                             .unions[user_info.declaration_index]};
                            for (std::size_t index{}; index < schema.alternatives.size(); ++index) {
                                repair_ref(schema.alternatives[index].type,
                                           resolved->alternatives[index].semantic_type.type);
                            }
                        } else if (auto const* resolved{
                                       std::get_if<TaggedUnionType>(&definition)}) {
                            auto& union_module{std::get<codegen::UnionModuleSchema>(module)};
                            auto const tagged_index{user_info.declaration_index -
                                                    union_module.unions.size()};
                            auto& schema{union_module.tagged_unions[tagged_index]};
                            repair_ref(schema.discriminant, resolved->discriminant.type);
                            for (std::size_t index{}; index < schema.alternatives.size(); ++index) {
                                repair_ref(schema.alternatives[index].type,
                                           resolved->alternatives[index].semantic_type.type);
                            }
                        } else if (auto const* resolved{std::get_if<SoaType>(&definition)}) {
                            auto* soa_module{std::get_if<codegen::SoaModuleSchema>(&module)};
                            if (soa_module == nullptr) {
                                continue;
                            }
                            auto& schema{soa_module->structs[user_info.declaration_index]};
                            for (std::size_t index{}; index < schema.members.size(); ++index) {
                                repair_ref(schema.members[index].type,
                                           resolved->columns[index].semantic_type.type);
                            }
                        }
                    }

                    auto& target_module{manifest_.modules[declaration_it->module_index]};
                    if (std::holds_alternative<EnumType>(target_definition)) {
                        std::get<codegen::EnumModuleSchema>(target_module)
                            .enums[declaration_it->declaration_index]
                            .name = edit.new_name;
                    } else if (std::holds_alternative<IntegerScalarType>(target_definition)) {
                        std::get<codegen::ScalarModuleSchema>(target_module)
                            .scalars[declaration_it->declaration_index]
                            .name = edit.new_name;
                    } else if (std::holds_alternative<LinearQuantizedType>(target_definition)) {
                        std::get<codegen::RepresentationModuleSchema>(target_module)
                            .linear_quantized[declaration_it->declaration_index]
                            .name = edit.new_name;
                    } else if (std::holds_alternative<IntegerVarintType>(target_definition)) {
                        auto& representations{
                            std::get<codegen::RepresentationModuleSchema>(target_module)};
                        auto const index{declaration_it->declaration_index -
                                         representations.linear_quantized.size()};
                        representations.integer_varints[index].name = edit.new_name;
                    } else if (std::holds_alternative<FixedPointType>(target_definition)) {
                        auto& representations{
                            std::get<codegen::RepresentationModuleSchema>(target_module)};
                        auto const index{declaration_it->declaration_index -
                                         representations.linear_quantized.size() -
                                         representations.integer_varints.size()};
                        representations.fixed_points[index].name = edit.new_name;
                    } else if (std::holds_alternative<OptionalSentinelType>(target_definition)) {
                        auto& representations{
                            std::get<codegen::RepresentationModuleSchema>(target_module)};
                        auto const index{declaration_it->declaration_index -
                                         representations.linear_quantized.size() -
                                         representations.integer_varints.size() -
                                         representations.fixed_points.size()};
                        representations.optional_sentinels[index].name = edit.new_name;
                    } else if (std::holds_alternative<OptionalPresenceBitType>(target_definition)) {
                        auto& representations{
                            std::get<codegen::RepresentationModuleSchema>(target_module)};
                        auto const index{declaration_it->declaration_index -
                                         representations.linear_quantized.size() -
                                         representations.integer_varints.size() -
                                         representations.fixed_points.size() -
                                         representations.optional_sentinels.size()};
                        representations.optional_presence_bits[index].name = edit.new_name;
                    } else if (std::holds_alternative<MiniFloatType>(target_definition)) {
                        auto& representations{
                            std::get<codegen::RepresentationModuleSchema>(target_module)};
                        auto const index{declaration_it->declaration_index -
                                         representations.linear_quantized.size() -
                                         representations.integer_varints.size() -
                                         representations.fixed_points.size() -
                                         representations.optional_sentinels.size() -
                                         representations.optional_presence_bits.size()};
                        representations.mini_floats[index].name = edit.new_name;
                    } else if (std::holds_alternative<PackedType>(target_definition)) {
                        std::get<codegen::PackedValueModuleSchema>(target_module)
                            .values[declaration_it->declaration_index]
                            .name = edit.new_name;
                    } else if (std::holds_alternative<RecordType>(target_definition)) {
                        std::get<codegen::RecordModuleSchema>(target_module)
                            .records[declaration_it->declaration_index]
                            .name = edit.new_name;
                    } else if (std::holds_alternative<UnionType>(target_definition)) {
                        std::get<codegen::UnionModuleSchema>(target_module)
                            .unions[declaration_it->declaration_index]
                            .name = edit.new_name;
                    } else if (std::holds_alternative<TaggedUnionType>(target_definition)) {
                        auto& unions{std::get<codegen::UnionModuleSchema>(target_module)};
                        auto const index{declaration_it->declaration_index - unions.unions.size()};
                        unions.tagged_unions[index].name = edit.new_name;
                    } else {
                        throw std::invalid_argument{"Declaration kind cannot be renamed"};
                    }
                    declaration_it->identity.name = edit.new_name;
                    codegen::validate_manifest(manifest_);
                    types_ = resolve_type_graph(manifest_);
                } catch (std::exception const& error) {
                    manifest_ = previous_manifest;
                    declarations_ = previous_declarations;
                    return std::unexpected{SchemaEditError{error.what()}};
                }
                return SchemaEditCommand{
                    RenameDeclaration{.declaration = edit.declaration, .new_name = old_name}};
            } else if constexpr (std::is_same_v<Edit, DeleteIntegerScalar>) {
                auto const* found{declaration(edit.declaration)};
                auto const* current{integer_scalar_schema(edit.declaration)};
                if (found == nullptr || current == nullptr) {
                    return std::unexpected{SchemaEditError{"Unknown integer-scalar declaration"}};
                }
                if (auto const blocker{deletion_blocker(*found)}; blocker.has_value()) {
                    return std::unexpected{*blocker};
                }
                auto const info{*found};
                auto schema{*current};
                auto* module{std::get_if<codegen::ScalarModuleSchema>(
                    &manifest_.modules[info.module_index])};
                module->scalars.erase(module->scalars.begin() +
                                      static_cast<std::ptrdiff_t>(info.declaration_index));
                declarations_.erase(
                    std::ranges::find(declarations_, edit.declaration, &DeclarationInfo::id));
                for (auto& existing : declarations_) {
                    if (existing.module_index == info.module_index &&
                        existing.declaration_index > info.declaration_index) {
                        --existing.declaration_index;
                    }
                }
                try {
                    codegen::validate_manifest(manifest_);
                    types_ = resolve_type_graph(manifest_);
                } catch (std::exception const& error) {
                    module->scalars.insert(module->scalars.begin() +
                                               static_cast<std::ptrdiff_t>(info.declaration_index),
                                           schema);
                    for (auto& existing : declarations_) {
                        if (existing.module_index == info.module_index &&
                            existing.declaration_index >= info.declaration_index) {
                            ++existing.declaration_index;
                        }
                    }
                    declarations_.push_back(info);
                    return std::unexpected{SchemaEditError{error.what()}};
                }
                remember_source_tombstone(info);
                return SchemaEditCommand{
                    CreateIntegerScalar{.declaration = edit.declaration,
                                        .module_index = info.module_index,
                                        .schema = std::move(schema),
                                        .insertion_index = info.declaration_index}};
            } else if constexpr (std::is_same_v<Edit, CreateLinearQuantized>) {
                if (!edit.declaration.valid() || declaration(edit.declaration) != nullptr) {
                    return std::unexpected{SchemaEditError{
                        "New linear quantization requires a unique declaration id"}};
                }
                if (edit.module_index >= manifest_.modules.size()) {
                    return std::unexpected{SchemaEditError{"Unknown representation module index"}};
                }
                auto* module{std::get_if<codegen::RepresentationModuleSchema>(
                    &manifest_.modules[edit.module_index])};
                if (module == nullptr) {
                    return std::unexpected{SchemaEditError{
                        "New linear quantizations can only be added to representation modules"}};
                }
                auto const insertion_index{
                    edit.insertion_index.value_or(module->linear_quantized.size())};
                if (insertion_index > module->linear_quantized.size()) {
                    return std::unexpected{
                        SchemaEditError{"Invalid linear quantization insertion index"}};
                }

                module->linear_quantized.insert(module->linear_quantized.begin() +
                                                    static_cast<std::ptrdiff_t>(insertion_index),
                                                edit.schema);
                for (auto& existing : declarations_) {
                    if (existing.module_index == edit.module_index &&
                        existing.declaration_index >= insertion_index) {
                        ++existing.declaration_index;
                    }
                }
                auto const namespace_name{module->settings.namespace_name.value_or("")};
                declarations_.push_back(
                    {.id = edit.declaration,
                     .identity = TypeIdentity{.origin = TypeOrigin::declaration,
                                              .module_name = module->settings.name,
                                              .namespace_name = namespace_name,
                                              .name = edit.schema.name},
                     .module_index = edit.module_index,
                     .declaration_index = insertion_index,
                     .source = creation_source(edit.declaration)});
                try {
                    types_ = resolve_type_graph(manifest_);
                } catch (std::exception const& error) {
                    declarations_.pop_back();
                    for (auto& existing : declarations_) {
                        if (existing.module_index == edit.module_index &&
                            existing.declaration_index > insertion_index) {
                            --existing.declaration_index;
                        }
                    }
                    module->linear_quantized.erase(module->linear_quantized.begin() +
                                                   static_cast<std::ptrdiff_t>(insertion_index));
                    return std::unexpected{SchemaEditError{error.what()}};
                }
                return SchemaEditCommand{DeleteLinearQuantized{.declaration = edit.declaration}};
            } else if constexpr (std::is_same_v<Edit, ReplaceLinearQuantized>) {
                auto const* info{declaration(edit.declaration)};
                auto const* current{linear_quantized_schema(edit.declaration)};
                if (info == nullptr || current == nullptr) {
                    return std::unexpected{SchemaEditError{"Unknown linear-quantized declaration"}};
                }
                if (edit.schema.name != current->name) {
                    return std::unexpected{SchemaEditError{"ReplaceLinearQuantized cannot rename a "
                                                           "declaration; use a rename command"}};
                }
                auto* module{std::get_if<codegen::RepresentationModuleSchema>(
                    &manifest_.modules[info->module_index])};
                auto previous{module->linear_quantized[info->declaration_index]};
                module->linear_quantized[info->declaration_index] = edit.schema;
                try {
                    types_ = resolve_type_graph(manifest_);
                } catch (std::exception const& error) {
                    module->linear_quantized[info->declaration_index] = std::move(previous);
                    return std::unexpected{SchemaEditError{error.what()}};
                }
                return SchemaEditCommand{ReplaceLinearQuantized{.declaration = edit.declaration,
                                                                .schema = std::move(previous)}};
            } else if constexpr (std::is_same_v<Edit, DeleteLinearQuantized>) {
                auto const* found{declaration(edit.declaration)};
                auto const* current{linear_quantized_schema(edit.declaration)};
                if (found == nullptr || current == nullptr) {
                    return std::unexpected{SchemaEditError{"Unknown linear-quantized declaration"}};
                }
                if (auto const blocker{deletion_blocker(*found)}; blocker.has_value()) {
                    return std::unexpected{*blocker};
                }
                auto const info{*found};
                auto schema{*current};
                auto* module{std::get_if<codegen::RepresentationModuleSchema>(
                    &manifest_.modules[info.module_index])};
                module->linear_quantized.erase(module->linear_quantized.begin() +
                                               static_cast<std::ptrdiff_t>(info.declaration_index));
                declarations_.erase(
                    std::ranges::find(declarations_, edit.declaration, &DeclarationInfo::id));
                for (auto& existing : declarations_) {
                    if (existing.module_index == info.module_index &&
                        existing.declaration_index > info.declaration_index) {
                        --existing.declaration_index;
                    }
                }
                try {
                    codegen::validate_manifest(manifest_);
                    types_ = resolve_type_graph(manifest_);
                } catch (std::exception const& error) {
                    module->linear_quantized.insert(
                        module->linear_quantized.begin() +
                            static_cast<std::ptrdiff_t>(info.declaration_index),
                        schema);
                    for (auto& existing : declarations_) {
                        if (existing.module_index == info.module_index &&
                            existing.declaration_index >= info.declaration_index) {
                            ++existing.declaration_index;
                        }
                    }
                    declarations_.push_back(info);
                    return std::unexpected{SchemaEditError{error.what()}};
                }
                remember_source_tombstone(info);
                return SchemaEditCommand{
                    CreateLinearQuantized{.declaration = edit.declaration,
                                          .module_index = info.module_index,
                                          .schema = std::move(schema),
                                          .insertion_index = info.declaration_index}};
            } else if constexpr (std::is_same_v<Edit, CreateIntegerVarint>) {
                if (!edit.declaration.valid() || declaration(edit.declaration) != nullptr) {
                    return std::unexpected{
                        SchemaEditError{"New integer varint requires a unique declaration id"}};
                }
                if (edit.module_index >= manifest_.modules.size()) {
                    return std::unexpected{SchemaEditError{"Unknown representation module index"}};
                }
                auto* module{std::get_if<codegen::RepresentationModuleSchema>(
                    &manifest_.modules[edit.module_index])};
                if (module == nullptr) {
                    return std::unexpected{SchemaEditError{
                        "New integer varints can only be added to representation modules"}};
                }
                auto const insertion_index{
                    edit.insertion_index.value_or(module->integer_varints.size())};
                if (insertion_index > module->integer_varints.size()) {
                    return std::unexpected{
                        SchemaEditError{"Invalid integer varint insertion index"}};
                }
                auto const declaration_index{module->linear_quantized.size() + insertion_index};

                module->integer_varints.insert(module->integer_varints.begin() +
                                                   static_cast<std::ptrdiff_t>(insertion_index),
                                               edit.schema);
                for (auto& existing : declarations_) {
                    if (existing.module_index == edit.module_index &&
                        existing.declaration_index >= declaration_index) {
                        ++existing.declaration_index;
                    }
                }
                auto const namespace_name{module->settings.namespace_name.value_or("")};
                declarations_.push_back(
                    {.id = edit.declaration,
                     .identity = TypeIdentity{.origin = TypeOrigin::declaration,
                                              .module_name = module->settings.name,
                                              .namespace_name = namespace_name,
                                              .name = edit.schema.name},
                     .module_index = edit.module_index,
                     .declaration_index = declaration_index,
                     .source = creation_source(edit.declaration)});
                try {
                    types_ = resolve_type_graph(manifest_);
                } catch (std::exception const& error) {
                    declarations_.pop_back();
                    for (auto& existing : declarations_) {
                        if (existing.module_index == edit.module_index &&
                            existing.declaration_index > declaration_index) {
                            --existing.declaration_index;
                        }
                    }
                    module->integer_varints.erase(module->integer_varints.begin() +
                                                  static_cast<std::ptrdiff_t>(insertion_index));
                    return std::unexpected{SchemaEditError{error.what()}};
                }
                return SchemaEditCommand{DeleteIntegerVarint{.declaration = edit.declaration}};
            } else if constexpr (std::is_same_v<Edit, ReplaceIntegerVarint>) {
                auto const* info{declaration(edit.declaration)};
                auto const* current{integer_varint_schema(edit.declaration)};
                if (info == nullptr || current == nullptr) {
                    return std::unexpected{SchemaEditError{"Unknown integer-varint declaration"}};
                }
                if (edit.schema.name != current->name) {
                    return std::unexpected{SchemaEditError{"ReplaceIntegerVarint cannot rename a "
                                                           "declaration; use a rename command"}};
                }
                auto* module{std::get_if<codegen::RepresentationModuleSchema>(
                    &manifest_.modules[info->module_index])};
                auto const index{info->declaration_index - module->linear_quantized.size()};
                auto previous{module->integer_varints[index]};
                module->integer_varints[index] = edit.schema;
                try {
                    types_ = resolve_type_graph(manifest_);
                } catch (std::exception const& error) {
                    module->integer_varints[index] = std::move(previous);
                    return std::unexpected{SchemaEditError{error.what()}};
                }
                return SchemaEditCommand{ReplaceIntegerVarint{.declaration = edit.declaration,
                                                              .schema = std::move(previous)}};
            } else if constexpr (std::is_same_v<Edit, DeleteIntegerVarint>) {
                auto const* found{declaration(edit.declaration)};
                auto const* current{integer_varint_schema(edit.declaration)};
                if (found == nullptr || current == nullptr) {
                    return std::unexpected{SchemaEditError{"Unknown integer-varint declaration"}};
                }
                if (auto const blocker{deletion_blocker(*found)}; blocker.has_value()) {
                    return std::unexpected{*blocker};
                }
                auto const info{*found};
                auto schema{*current};
                auto* module{std::get_if<codegen::RepresentationModuleSchema>(
                    &manifest_.modules[info.module_index])};
                auto const index{info.declaration_index - module->linear_quantized.size()};
                module->integer_varints.erase(module->integer_varints.begin() +
                                              static_cast<std::ptrdiff_t>(index));
                declarations_.erase(
                    std::ranges::find(declarations_, edit.declaration, &DeclarationInfo::id));
                for (auto& existing : declarations_) {
                    if (existing.module_index == info.module_index &&
                        existing.declaration_index > info.declaration_index) {
                        --existing.declaration_index;
                    }
                }
                try {
                    codegen::validate_manifest(manifest_);
                    types_ = resolve_type_graph(manifest_);
                } catch (std::exception const& error) {
                    module->integer_varints.insert(module->integer_varints.begin() +
                                                       static_cast<std::ptrdiff_t>(index),
                                                   schema);
                    for (auto& existing : declarations_) {
                        if (existing.module_index == info.module_index &&
                            existing.declaration_index >= info.declaration_index) {
                            ++existing.declaration_index;
                        }
                    }
                    declarations_.push_back(info);
                    return std::unexpected{SchemaEditError{error.what()}};
                }
                remember_source_tombstone(info);
                return SchemaEditCommand{CreateIntegerVarint{.declaration = edit.declaration,
                                                             .module_index = info.module_index,
                                                             .schema = std::move(schema),
                                                             .insertion_index = index}};
            } else if constexpr (std::is_same_v<Edit, CreateFixedPoint>) {
                if (!edit.declaration.valid() || declaration(edit.declaration) != nullptr) {
                    return std::unexpected{
                        SchemaEditError{"New fixed point requires a unique declaration id"}};
                }
                if (edit.module_index >= manifest_.modules.size()) {
                    return std::unexpected{SchemaEditError{"Unknown representation module index"}};
                }
                auto* module{std::get_if<codegen::RepresentationModuleSchema>(
                    &manifest_.modules[edit.module_index])};
                if (module == nullptr) {
                    return std::unexpected{SchemaEditError{
                        "New fixed points can only be added to representation modules"}};
                }
                auto const insertion_index{
                    edit.insertion_index.value_or(module->fixed_points.size())};
                if (insertion_index > module->fixed_points.size()) {
                    return std::unexpected{SchemaEditError{"Invalid fixed-point insertion index"}};
                }
                auto const declaration_index{module->linear_quantized.size() +
                                             module->integer_varints.size() + insertion_index};
                module->fixed_points.insert(module->fixed_points.begin() +
                                                static_cast<std::ptrdiff_t>(insertion_index),
                                            edit.schema);
                for (auto& existing : declarations_) {
                    if (existing.module_index == edit.module_index &&
                        existing.declaration_index >= declaration_index) {
                        ++existing.declaration_index;
                    }
                }
                auto const namespace_name{module->settings.namespace_name.value_or("")};
                declarations_.push_back(
                    {.id = edit.declaration,
                     .identity = TypeIdentity{.origin = TypeOrigin::declaration,
                                              .module_name = module->settings.name,
                                              .namespace_name = namespace_name,
                                              .name = edit.schema.name},
                     .module_index = edit.module_index,
                     .declaration_index = declaration_index,
                     .source = creation_source(edit.declaration)});
                try {
                    codegen::validate_manifest(manifest_);
                    types_ = resolve_type_graph(manifest_);
                } catch (std::exception const& error) {
                    declarations_.pop_back();
                    for (auto& existing : declarations_) {
                        if (existing.module_index == edit.module_index &&
                            existing.declaration_index > declaration_index) {
                            --existing.declaration_index;
                        }
                    }
                    module->fixed_points.erase(module->fixed_points.begin() +
                                               static_cast<std::ptrdiff_t>(insertion_index));
                    return std::unexpected{SchemaEditError{error.what()}};
                }
                return SchemaEditCommand{DeleteFixedPoint{.declaration = edit.declaration}};
            } else if constexpr (std::is_same_v<Edit, ReplaceFixedPoint>) {
                auto const* info{declaration(edit.declaration)};
                auto const* current{fixed_point_schema(edit.declaration)};
                if (info == nullptr || current == nullptr) {
                    return std::unexpected{SchemaEditError{"Unknown fixed-point declaration"}};
                }
                if (edit.schema.name != current->name) {
                    return std::unexpected{SchemaEditError{
                        "ReplaceFixedPoint cannot rename a declaration; use a rename command"}};
                }
                auto* module{std::get_if<codegen::RepresentationModuleSchema>(
                    &manifest_.modules[info->module_index])};
                auto const index{info->declaration_index - module->linear_quantized.size() -
                                 module->integer_varints.size()};
                auto previous{module->fixed_points[index]};
                module->fixed_points[index] = edit.schema;
                try {
                    codegen::validate_manifest(manifest_);
                    types_ = resolve_type_graph(manifest_);
                } catch (std::exception const& error) {
                    module->fixed_points[index] = std::move(previous);
                    return std::unexpected{SchemaEditError{error.what()}};
                }
                return SchemaEditCommand{ReplaceFixedPoint{.declaration = edit.declaration,
                                                           .schema = std::move(previous)}};
            } else if constexpr (std::is_same_v<Edit, DeleteFixedPoint>) {
                auto const* found{declaration(edit.declaration)};
                auto const* current{fixed_point_schema(edit.declaration)};
                if (found == nullptr || current == nullptr) {
                    return std::unexpected{SchemaEditError{"Unknown fixed-point declaration"}};
                }
                if (auto const blocker{deletion_blocker(*found)}; blocker.has_value()) {
                    return std::unexpected{*blocker};
                }
                auto const info{*found};
                auto schema{*current};
                auto* module{std::get_if<codegen::RepresentationModuleSchema>(
                    &manifest_.modules[info.module_index])};
                auto const index{info.declaration_index - module->linear_quantized.size() -
                                 module->integer_varints.size()};
                module->fixed_points.erase(module->fixed_points.begin() +
                                           static_cast<std::ptrdiff_t>(index));
                declarations_.erase(
                    std::ranges::find(declarations_, edit.declaration, &DeclarationInfo::id));
                for (auto& existing : declarations_) {
                    if (existing.module_index == info.module_index &&
                        existing.declaration_index > info.declaration_index) {
                        --existing.declaration_index;
                    }
                }
                try {
                    codegen::validate_manifest(manifest_);
                    types_ = resolve_type_graph(manifest_);
                } catch (std::exception const& error) {
                    module->fixed_points.insert(
                        module->fixed_points.begin() + static_cast<std::ptrdiff_t>(index), schema);
                    for (auto& existing : declarations_) {
                        if (existing.module_index == info.module_index &&
                            existing.declaration_index >= info.declaration_index) {
                            ++existing.declaration_index;
                        }
                    }
                    declarations_.push_back(info);
                    return std::unexpected{SchemaEditError{error.what()}};
                }
                remember_source_tombstone(info);
                return SchemaEditCommand{CreateFixedPoint{.declaration = edit.declaration,
                                                          .module_index = info.module_index,
                                                          .schema = std::move(schema),
                                                          .insertion_index = index}};
            } else if constexpr (std::is_same_v<Edit, CreateOptionalSentinel>) {
                if (!edit.declaration.valid() || declaration(edit.declaration) != nullptr) {
                    return std::unexpected{
                        SchemaEditError{"New optional sentinel requires a unique declaration id"}};
                }
                if (edit.module_index >= manifest_.modules.size()) {
                    return std::unexpected{SchemaEditError{"Unknown representation module index"}};
                }
                auto* module{std::get_if<codegen::RepresentationModuleSchema>(
                    &manifest_.modules[edit.module_index])};
                if (module == nullptr) {
                    return std::unexpected{SchemaEditError{
                        "New optional sentinels can only be added to representation modules"}};
                }
                auto const insertion_index{
                    edit.insertion_index.value_or(module->optional_sentinels.size())};
                if (insertion_index > module->optional_sentinels.size()) {
                    return std::unexpected{
                        SchemaEditError{"Invalid optional-sentinel insertion index"}};
                }
                auto const declaration_index{module->linear_quantized.size() +
                                             module->integer_varints.size() +
                                             module->fixed_points.size() + insertion_index};
                module->optional_sentinels.insert(module->optional_sentinels.begin() +
                                                      static_cast<std::ptrdiff_t>(insertion_index),
                                                  edit.schema);
                for (auto& existing : declarations_) {
                    if (existing.module_index == edit.module_index &&
                        existing.declaration_index >= declaration_index) {
                        ++existing.declaration_index;
                    }
                }
                auto const namespace_name{module->settings.namespace_name.value_or("")};
                declarations_.push_back(
                    {.id = edit.declaration,
                     .identity = TypeIdentity{.origin = TypeOrigin::declaration,
                                              .module_name = module->settings.name,
                                              .namespace_name = namespace_name,
                                              .name = edit.schema.name},
                     .module_index = edit.module_index,
                     .declaration_index = declaration_index,
                     .source = creation_source(edit.declaration)});
                try {
                    codegen::validate_manifest(manifest_);
                    types_ = resolve_type_graph(manifest_);
                } catch (std::exception const& error) {
                    declarations_.pop_back();
                    for (auto& existing : declarations_) {
                        if (existing.module_index == edit.module_index &&
                            existing.declaration_index > declaration_index) {
                            --existing.declaration_index;
                        }
                    }
                    module->optional_sentinels.erase(module->optional_sentinels.begin() +
                                                     static_cast<std::ptrdiff_t>(insertion_index));
                    return std::unexpected{SchemaEditError{error.what()}};
                }
                return SchemaEditCommand{DeleteOptionalSentinel{.declaration = edit.declaration}};
            } else if constexpr (std::is_same_v<Edit, ReplaceOptionalSentinel>) {
                auto const* info{declaration(edit.declaration)};
                auto const* current{optional_sentinel_schema(edit.declaration)};
                if (info == nullptr || current == nullptr) {
                    return std::unexpected{
                        SchemaEditError{"Unknown optional-sentinel declaration"}};
                }
                if (edit.schema.name != current->name) {
                    return std::unexpected{SchemaEditError{"ReplaceOptionalSentinel cannot rename "
                                                           "a declaration; use a rename command"}};
                }
                auto* module{std::get_if<codegen::RepresentationModuleSchema>(
                    &manifest_.modules[info->module_index])};
                auto const index{info->declaration_index - module->linear_quantized.size() -
                                 module->integer_varints.size() - module->fixed_points.size()};
                auto previous{module->optional_sentinels[index]};
                module->optional_sentinels[index] = edit.schema;
                try {
                    codegen::validate_manifest(manifest_);
                    types_ = resolve_type_graph(manifest_);
                } catch (std::exception const& error) {
                    module->optional_sentinels[index] = std::move(previous);
                    return std::unexpected{SchemaEditError{error.what()}};
                }
                return SchemaEditCommand{ReplaceOptionalSentinel{.declaration = edit.declaration,
                                                                 .schema = std::move(previous)}};
            } else if constexpr (std::is_same_v<Edit, DeleteOptionalSentinel>) {
                auto const* found{declaration(edit.declaration)};
                auto const* current{optional_sentinel_schema(edit.declaration)};
                if (found == nullptr || current == nullptr) {
                    return std::unexpected{
                        SchemaEditError{"Unknown optional-sentinel declaration"}};
                }
                if (auto const blocker{deletion_blocker(*found)}; blocker.has_value()) {
                    return std::unexpected{*blocker};
                }
                auto const info{*found};
                auto schema{*current};
                auto* module{std::get_if<codegen::RepresentationModuleSchema>(
                    &manifest_.modules[info.module_index])};
                auto const index{info.declaration_index - module->linear_quantized.size() -
                                 module->integer_varints.size() - module->fixed_points.size()};
                module->optional_sentinels.erase(module->optional_sentinels.begin() +
                                                 static_cast<std::ptrdiff_t>(index));
                declarations_.erase(
                    std::ranges::find(declarations_, edit.declaration, &DeclarationInfo::id));
                for (auto& existing : declarations_) {
                    if (existing.module_index == info.module_index &&
                        existing.declaration_index > info.declaration_index) {
                        --existing.declaration_index;
                    }
                }
                try {
                    codegen::validate_manifest(manifest_);
                    types_ = resolve_type_graph(manifest_);
                } catch (std::exception const& error) {
                    module->optional_sentinels.insert(module->optional_sentinels.begin() +
                                                          static_cast<std::ptrdiff_t>(index),
                                                      schema);
                    for (auto& existing : declarations_) {
                        if (existing.module_index == info.module_index &&
                            existing.declaration_index >= info.declaration_index) {
                            ++existing.declaration_index;
                        }
                    }
                    declarations_.push_back(info);
                    return std::unexpected{SchemaEditError{error.what()}};
                }
                remember_source_tombstone(info);
                return SchemaEditCommand{CreateOptionalSentinel{.declaration = edit.declaration,
                                                                .module_index = info.module_index,
                                                                .schema = std::move(schema),
                                                                .insertion_index = index}};
            } else if constexpr (std::is_same_v<Edit, CreateOptionalPresenceBit>) {
                if (!edit.declaration.valid() || declaration(edit.declaration) != nullptr) {
                    return std::unexpected{SchemaEditError{
                        "New optional presence bit requires a unique declaration id"}};
                }
                if (edit.module_index >= manifest_.modules.size()) {
                    return std::unexpected{SchemaEditError{"Unknown representation module index"}};
                }
                auto* module{std::get_if<codegen::RepresentationModuleSchema>(
                    &manifest_.modules[edit.module_index])};
                if (module == nullptr) {
                    return std::unexpected{SchemaEditError{
                        "New optional presence bits can only be added to representation modules"}};
                }
                auto const insertion_index{
                    edit.insertion_index.value_or(module->optional_presence_bits.size())};
                if (insertion_index > module->optional_presence_bits.size()) {
                    return std::unexpected{
                        SchemaEditError{"Invalid optional-presence-bit insertion index"}};
                }
                auto const declaration_index{module->linear_quantized.size() +
                                             module->integer_varints.size() +
                                             module->fixed_points.size() +
                                             module->optional_sentinels.size() + insertion_index};
                module->optional_presence_bits.insert(
                    module->optional_presence_bits.begin() +
                        static_cast<std::ptrdiff_t>(insertion_index),
                    edit.schema);
                for (auto& existing : declarations_) {
                    if (existing.module_index == edit.module_index &&
                        existing.declaration_index >= declaration_index) {
                        ++existing.declaration_index;
                    }
                }
                auto const namespace_name{module->settings.namespace_name.value_or("")};
                declarations_.push_back(
                    {.id = edit.declaration,
                     .identity = TypeIdentity{.origin = TypeOrigin::declaration,
                                              .module_name = module->settings.name,
                                              .namespace_name = namespace_name,
                                              .name = edit.schema.name},
                     .module_index = edit.module_index,
                     .declaration_index = declaration_index,
                     .source = creation_source(edit.declaration)});
                try {
                    codegen::validate_manifest(manifest_);
                    types_ = resolve_type_graph(manifest_);
                } catch (std::exception const& error) {
                    declarations_.pop_back();
                    for (auto& existing : declarations_) {
                        if (existing.module_index == edit.module_index &&
                            existing.declaration_index > declaration_index) {
                            --existing.declaration_index;
                        }
                    }
                    module->optional_presence_bits.erase(
                        module->optional_presence_bits.begin() +
                        static_cast<std::ptrdiff_t>(insertion_index));
                    return std::unexpected{SchemaEditError{error.what()}};
                }
                return SchemaEditCommand{
                    DeleteOptionalPresenceBit{.declaration = edit.declaration}};
            } else if constexpr (std::is_same_v<Edit, ReplaceOptionalPresenceBit>) {
                auto const* info{declaration(edit.declaration)};
                auto const* current{optional_presence_bit_schema(edit.declaration)};
                if (info == nullptr || current == nullptr) {
                    return std::unexpected{
                        SchemaEditError{"Unknown optional-presence-bit declaration"}};
                }
                if (edit.schema.name != current->name) {
                    return std::unexpected{SchemaEditError{
                        "ReplaceOptionalPresenceBit cannot rename a declaration; use a rename "
                        "command"}};
                }
                auto* module{std::get_if<codegen::RepresentationModuleSchema>(
                    &manifest_.modules[info->module_index])};
                auto const index{info->declaration_index - module->linear_quantized.size() -
                                 module->integer_varints.size() - module->fixed_points.size() -
                                 module->optional_sentinels.size()};
                auto previous{module->optional_presence_bits[index]};
                module->optional_presence_bits[index] = edit.schema;
                try {
                    codegen::validate_manifest(manifest_);
                    types_ = resolve_type_graph(manifest_);
                } catch (std::exception const& error) {
                    module->optional_presence_bits[index] = std::move(previous);
                    return std::unexpected{SchemaEditError{error.what()}};
                }
                return SchemaEditCommand{ReplaceOptionalPresenceBit{.declaration = edit.declaration,
                                                                    .schema = std::move(previous)}};
            } else if constexpr (std::is_same_v<Edit, DeleteOptionalPresenceBit>) {
                auto const* found{declaration(edit.declaration)};
                auto const* current{optional_presence_bit_schema(edit.declaration)};
                if (found == nullptr || current == nullptr) {
                    return std::unexpected{
                        SchemaEditError{"Unknown optional-presence-bit declaration"}};
                }
                if (auto const blocker{deletion_blocker(*found)}; blocker.has_value()) {
                    return std::unexpected{*blocker};
                }
                auto const info{*found};
                auto schema{*current};
                auto* module{std::get_if<codegen::RepresentationModuleSchema>(
                    &manifest_.modules[info.module_index])};
                auto const index{info.declaration_index - module->linear_quantized.size() -
                                 module->integer_varints.size() - module->fixed_points.size() -
                                 module->optional_sentinels.size()};
                module->optional_presence_bits.erase(module->optional_presence_bits.begin() +
                                                     static_cast<std::ptrdiff_t>(index));
                declarations_.erase(
                    std::ranges::find(declarations_, edit.declaration, &DeclarationInfo::id));
                for (auto& existing : declarations_) {
                    if (existing.module_index == info.module_index &&
                        existing.declaration_index > info.declaration_index) {
                        --existing.declaration_index;
                    }
                }
                try {
                    codegen::validate_manifest(manifest_);
                    types_ = resolve_type_graph(manifest_);
                } catch (std::exception const& error) {
                    module->optional_presence_bits.insert(module->optional_presence_bits.begin() +
                                                              static_cast<std::ptrdiff_t>(index),
                                                          schema);
                    for (auto& existing : declarations_) {
                        if (existing.module_index == info.module_index &&
                            existing.declaration_index >= info.declaration_index) {
                            ++existing.declaration_index;
                        }
                    }
                    declarations_.push_back(info);
                    return std::unexpected{SchemaEditError{error.what()}};
                }
                remember_source_tombstone(info);
                return SchemaEditCommand{
                    CreateOptionalPresenceBit{.declaration = edit.declaration,
                                              .module_index = info.module_index,
                                              .schema = std::move(schema),
                                              .insertion_index = index}};
            } else if constexpr (std::is_same_v<Edit, CreateMiniFloat>) {
                if (!edit.declaration.valid() || declaration(edit.declaration) != nullptr) {
                    return std::unexpected{
                        SchemaEditError{"New mini-float requires a unique declaration id"}};
                }
                if (edit.module_index >= manifest_.modules.size()) {
                    return std::unexpected{SchemaEditError{"Unknown representation module index"}};
                }
                auto* module{std::get_if<codegen::RepresentationModuleSchema>(
                    &manifest_.modules[edit.module_index])};
                if (module == nullptr) {
                    return std::unexpected{SchemaEditError{
                        "New mini-floats can only be added to representation modules"}};
                }
                auto const insertion_index{
                    edit.insertion_index.value_or(module->mini_floats.size())};
                if (insertion_index > module->mini_floats.size()) {
                    return std::unexpected{SchemaEditError{"Invalid mini-float insertion index"}};
                }
                auto const declaration_index{
                    module->linear_quantized.size() + module->integer_varints.size() +
                    module->fixed_points.size() + module->optional_sentinels.size() +
                    module->optional_presence_bits.size() + insertion_index};
                module->mini_floats.insert(module->mini_floats.begin() +
                                               static_cast<std::ptrdiff_t>(insertion_index),
                                           edit.schema);
                for (auto& existing : declarations_) {
                    if (existing.module_index == edit.module_index &&
                        existing.declaration_index >= declaration_index) {
                        ++existing.declaration_index;
                    }
                }
                auto const namespace_name{module->settings.namespace_name.value_or("")};
                declarations_.push_back(
                    {.id = edit.declaration,
                     .identity = TypeIdentity{.origin = TypeOrigin::declaration,
                                              .module_name = module->settings.name,
                                              .namespace_name = namespace_name,
                                              .name = edit.schema.name},
                     .module_index = edit.module_index,
                     .declaration_index = declaration_index,
                     .source = creation_source(edit.declaration)});
                try {
                    codegen::validate_manifest(manifest_);
                    types_ = resolve_type_graph(manifest_);
                } catch (std::exception const& error) {
                    declarations_.pop_back();
                    for (auto& existing : declarations_) {
                        if (existing.module_index == edit.module_index &&
                            existing.declaration_index > declaration_index) {
                            --existing.declaration_index;
                        }
                    }
                    module->mini_floats.erase(module->mini_floats.begin() +
                                              static_cast<std::ptrdiff_t>(insertion_index));
                    return std::unexpected{SchemaEditError{error.what()}};
                }
                return SchemaEditCommand{DeleteMiniFloat{.declaration = edit.declaration}};
            } else if constexpr (std::is_same_v<Edit, ReplaceMiniFloat>) {
                auto const* info{declaration(edit.declaration)};
                auto const* current{mini_float_schema(edit.declaration)};
                if (info == nullptr || current == nullptr) {
                    return std::unexpected{SchemaEditError{"Unknown mini-float declaration"}};
                }
                if (edit.schema.name != current->name) {
                    return std::unexpected{SchemaEditError{
                        "ReplaceMiniFloat cannot rename a declaration; use a rename command"}};
                }
                auto* module{std::get_if<codegen::RepresentationModuleSchema>(
                    &manifest_.modules[info->module_index])};
                auto const index{info->declaration_index - module->linear_quantized.size() -
                                 module->integer_varints.size() - module->fixed_points.size() -
                                 module->optional_sentinels.size() -
                                 module->optional_presence_bits.size()};
                auto previous{module->mini_floats[index]};
                module->mini_floats[index] = edit.schema;
                try {
                    codegen::validate_manifest(manifest_);
                    types_ = resolve_type_graph(manifest_);
                } catch (std::exception const& error) {
                    module->mini_floats[index] = std::move(previous);
                    return std::unexpected{SchemaEditError{error.what()}};
                }
                return SchemaEditCommand{ReplaceMiniFloat{.declaration = edit.declaration,
                                                          .schema = std::move(previous)}};
            } else if constexpr (std::is_same_v<Edit, DeleteMiniFloat>) {
                auto const* found{declaration(edit.declaration)};
                auto const* current{mini_float_schema(edit.declaration)};
                if (found == nullptr || current == nullptr) {
                    return std::unexpected{SchemaEditError{"Unknown mini-float declaration"}};
                }
                if (auto const blocker{deletion_blocker(*found)}; blocker.has_value()) {
                    return std::unexpected{*blocker};
                }
                auto const info{*found};
                auto schema{*current};
                auto* module{std::get_if<codegen::RepresentationModuleSchema>(
                    &manifest_.modules[info.module_index])};
                auto const index{info.declaration_index - module->linear_quantized.size() -
                                 module->integer_varints.size() - module->fixed_points.size() -
                                 module->optional_sentinels.size() -
                                 module->optional_presence_bits.size()};
                module->mini_floats.erase(module->mini_floats.begin() +
                                          static_cast<std::ptrdiff_t>(index));
                declarations_.erase(
                    std::ranges::find(declarations_, edit.declaration, &DeclarationInfo::id));
                for (auto& existing : declarations_) {
                    if (existing.module_index == info.module_index &&
                        existing.declaration_index > info.declaration_index) {
                        --existing.declaration_index;
                    }
                }
                try {
                    codegen::validate_manifest(manifest_);
                    types_ = resolve_type_graph(manifest_);
                } catch (std::exception const& error) {
                    module->mini_floats.insert(
                        module->mini_floats.begin() + static_cast<std::ptrdiff_t>(index), schema);
                    for (auto& existing : declarations_) {
                        if (existing.module_index == info.module_index &&
                            existing.declaration_index >= info.declaration_index) {
                            ++existing.declaration_index;
                        }
                    }
                    declarations_.push_back(info);
                    return std::unexpected{SchemaEditError{error.what()}};
                }
                remember_source_tombstone(info);
                return SchemaEditCommand{CreateMiniFloat{.declaration = edit.declaration,
                                                         .module_index = info.module_index,
                                                         .schema = std::move(schema),
                                                         .insertion_index = index}};
            } else if constexpr (std::is_same_v<Edit, CreateRecord>) {
                if (!edit.declaration.valid() || declaration(edit.declaration) != nullptr) {
                    return std::unexpected{
                        SchemaEditError{"New record requires a unique declaration id"}};
                }
                if (edit.module_index >= manifest_.modules.size()) {
                    return std::unexpected{SchemaEditError{"Unknown record module index"}};
                }
                auto* module{std::get_if<codegen::RecordModuleSchema>(
                    &manifest_.modules[edit.module_index])};
                if (module == nullptr) {
                    return std::unexpected{
                        SchemaEditError{"New records can only be added to record modules"}};
                }
                auto const insertion_index{edit.insertion_index.value_or(module->records.size())};
                if (insertion_index > module->records.size()) {
                    return std::unexpected{SchemaEditError{"Invalid record insertion index"}};
                }

                module->records.insert(module->records.begin() +
                                           static_cast<std::ptrdiff_t>(insertion_index),
                                       edit.schema);
                for (auto& existing : declarations_) {
                    if (existing.module_index == edit.module_index &&
                        existing.declaration_index >= insertion_index) {
                        ++existing.declaration_index;
                    }
                }
                auto const namespace_name{module->settings.namespace_name.value_or("")};
                declarations_.push_back(
                    {.id = edit.declaration,
                     .identity = TypeIdentity{.origin = TypeOrigin::declaration,
                                              .module_name = module->settings.name,
                                              .namespace_name = namespace_name,
                                              .name = edit.schema.name},
                     .module_index = edit.module_index,
                     .declaration_index = insertion_index,
                     .source = creation_source(edit.declaration)});
                try {
                    types_ = resolve_type_graph(manifest_);
                } catch (std::exception const& error) {
                    declarations_.pop_back();
                    for (auto& existing : declarations_) {
                        if (existing.module_index == edit.module_index &&
                            existing.declaration_index > insertion_index) {
                            --existing.declaration_index;
                        }
                    }
                    module->records.erase(module->records.begin() +
                                          static_cast<std::ptrdiff_t>(insertion_index));
                    return std::unexpected{SchemaEditError{error.what()}};
                }
                return SchemaEditCommand{DeleteRecord{.declaration = edit.declaration}};
            } else if constexpr (std::is_same_v<Edit, ReplaceRecord>) {
                auto const* info{declaration(edit.declaration)};
                auto const* current{record_schema(edit.declaration)};
                if (info == nullptr || current == nullptr) {
                    return std::unexpected{SchemaEditError{"Unknown record declaration"}};
                }
                if (edit.schema.name != current->name) {
                    return std::unexpected{SchemaEditError{
                        "ReplaceRecord cannot rename a declaration; use a rename command"}};
                }
                auto* module{std::get_if<codegen::RecordModuleSchema>(
                    &manifest_.modules[info->module_index])};
                auto previous{module->records[info->declaration_index]};
                module->records[info->declaration_index] = edit.schema;
                try {
                    types_ = resolve_type_graph(manifest_);
                } catch (std::exception const& error) {
                    module->records[info->declaration_index] = std::move(previous);
                    return std::unexpected{SchemaEditError{error.what()}};
                }
                return SchemaEditCommand{
                    ReplaceRecord{.declaration = edit.declaration, .schema = std::move(previous)}};
            } else if constexpr (std::is_same_v<Edit, DeleteRecord>) {
                auto const* found{declaration(edit.declaration)};
                auto const* current{record_schema(edit.declaration)};
                if (found == nullptr || current == nullptr) {
                    return std::unexpected{SchemaEditError{"Unknown record declaration"}};
                }
                if (auto const blocker{deletion_blocker(*found)}; blocker.has_value()) {
                    return std::unexpected{*blocker};
                }
                auto const info{*found};
                auto schema{*current};
                auto* module{std::get_if<codegen::RecordModuleSchema>(
                    &manifest_.modules[info.module_index])};
                module->records.erase(module->records.begin() +
                                      static_cast<std::ptrdiff_t>(info.declaration_index));
                declarations_.erase(
                    std::ranges::find(declarations_, edit.declaration, &DeclarationInfo::id));
                for (auto& existing : declarations_) {
                    if (existing.module_index == info.module_index &&
                        existing.declaration_index > info.declaration_index) {
                        --existing.declaration_index;
                    }
                }
                try {
                    codegen::validate_manifest(manifest_);
                    types_ = resolve_type_graph(manifest_);
                } catch (std::exception const& error) {
                    module->records.insert(module->records.begin() +
                                               static_cast<std::ptrdiff_t>(info.declaration_index),
                                           schema);
                    for (auto& existing : declarations_) {
                        if (existing.module_index == info.module_index &&
                            existing.declaration_index >= info.declaration_index) {
                            ++existing.declaration_index;
                        }
                    }
                    declarations_.push_back(info);
                    return std::unexpected{SchemaEditError{error.what()}};
                }
                remember_source_tombstone(info);
                return SchemaEditCommand{CreateRecord{.declaration = edit.declaration,
                                                      .module_index = info.module_index,
                                                      .schema = std::move(schema),
                                                      .insertion_index = info.declaration_index}};
            } else if constexpr (std::is_same_v<Edit, CreateUnion>) {
                if (!edit.declaration.valid() || declaration(edit.declaration) != nullptr) {
                    return std::unexpected{
                        SchemaEditError{"New union requires a unique declaration id"}};
                }
                if (edit.module_index >= manifest_.modules.size()) {
                    return std::unexpected{SchemaEditError{"Unknown union module index"}};
                }
                auto* module{
                    std::get_if<codegen::UnionModuleSchema>(&manifest_.modules[edit.module_index])};
                if (module == nullptr) {
                    return std::unexpected{
                        SchemaEditError{"New unions can only be added to union modules"}};
                }
                auto const insertion_index{edit.insertion_index.value_or(module->unions.size())};
                if (insertion_index > module->unions.size()) {
                    return std::unexpected{SchemaEditError{"Invalid union insertion index"}};
                }

                module->unions.insert(module->unions.begin() +
                                          static_cast<std::ptrdiff_t>(insertion_index),
                                      edit.schema);
                for (auto& existing : declarations_) {
                    if (existing.module_index == edit.module_index &&
                        existing.declaration_index >= insertion_index) {
                        ++existing.declaration_index;
                    }
                }
                auto const namespace_name{module->settings.namespace_name.value_or("")};
                declarations_.push_back(
                    {.id = edit.declaration,
                     .identity = TypeIdentity{.origin = TypeOrigin::declaration,
                                              .module_name = module->settings.name,
                                              .namespace_name = namespace_name,
                                              .name = edit.schema.name},
                     .module_index = edit.module_index,
                     .declaration_index = insertion_index,
                     .source = creation_source(edit.declaration)});
                try {
                    codegen::validate_manifest(manifest_);
                    types_ = resolve_type_graph(manifest_);
                } catch (std::exception const& error) {
                    declarations_.pop_back();
                    for (auto& existing : declarations_) {
                        if (existing.module_index == edit.module_index &&
                            existing.declaration_index > insertion_index) {
                            --existing.declaration_index;
                        }
                    }
                    module->unions.erase(module->unions.begin() +
                                         static_cast<std::ptrdiff_t>(insertion_index));
                    return std::unexpected{SchemaEditError{error.what()}};
                }
                return SchemaEditCommand{DeleteUnion{.declaration = edit.declaration}};
            } else if constexpr (std::is_same_v<Edit, ReplaceUnion>) {
                auto const* info{declaration(edit.declaration)};
                auto const* current{union_schema(edit.declaration)};
                if (info == nullptr || current == nullptr) {
                    return std::unexpected{SchemaEditError{"Unknown union declaration"}};
                }
                if (edit.schema.name != current->name) {
                    return std::unexpected{SchemaEditError{
                        "ReplaceUnion cannot rename a declaration; use a rename command"}};
                }
                auto* module{std::get_if<codegen::UnionModuleSchema>(
                    &manifest_.modules[info->module_index])};
                auto previous{module->unions[info->declaration_index]};
                module->unions[info->declaration_index] = edit.schema;
                try {
                    codegen::validate_manifest(manifest_);
                    types_ = resolve_type_graph(manifest_);
                } catch (std::exception const& error) {
                    module->unions[info->declaration_index] = std::move(previous);
                    return std::unexpected{SchemaEditError{error.what()}};
                }
                return SchemaEditCommand{
                    ReplaceUnion{.declaration = edit.declaration, .schema = std::move(previous)}};
            } else if constexpr (std::is_same_v<Edit, DeleteUnion>) {
                auto const* found{declaration(edit.declaration)};
                auto const* current{union_schema(edit.declaration)};
                if (found == nullptr || current == nullptr) {
                    return std::unexpected{SchemaEditError{"Unknown union declaration"}};
                }
                if (auto const blocker{deletion_blocker(*found)}; blocker.has_value()) {
                    return std::unexpected{*blocker};
                }
                auto const info{*found};
                auto schema{*current};
                auto* module{
                    std::get_if<codegen::UnionModuleSchema>(&manifest_.modules[info.module_index])};
                module->unions.erase(module->unions.begin() +
                                     static_cast<std::ptrdiff_t>(info.declaration_index));
                declarations_.erase(
                    std::ranges::find(declarations_, edit.declaration, &DeclarationInfo::id));
                for (auto& existing : declarations_) {
                    if (existing.module_index == info.module_index &&
                        existing.declaration_index > info.declaration_index) {
                        --existing.declaration_index;
                    }
                }
                try {
                    codegen::validate_manifest(manifest_);
                    types_ = resolve_type_graph(manifest_);
                } catch (std::exception const& error) {
                    module->unions.insert(module->unions.begin() +
                                              static_cast<std::ptrdiff_t>(info.declaration_index),
                                          schema);
                    for (auto& existing : declarations_) {
                        if (existing.module_index == info.module_index &&
                            existing.declaration_index >= info.declaration_index) {
                            ++existing.declaration_index;
                        }
                    }
                    declarations_.push_back(info);
                    return std::unexpected{SchemaEditError{error.what()}};
                }
                remember_source_tombstone(info);
                return SchemaEditCommand{CreateUnion{.declaration = edit.declaration,
                                                     .module_index = info.module_index,
                                                     .schema = std::move(schema),
                                                     .insertion_index = info.declaration_index}};
            } else if constexpr (std::is_same_v<Edit, CreateTaggedUnion>) {
                if (!edit.declaration.valid() || declaration(edit.declaration) != nullptr) {
                    return std::unexpected{
                        SchemaEditError{"New tagged union requires a unique declaration id"}};
                }
                if (edit.module_index >= manifest_.modules.size()) {
                    return std::unexpected{SchemaEditError{"Unknown union module index"}};
                }
                auto* module{
                    std::get_if<codegen::UnionModuleSchema>(&manifest_.modules[edit.module_index])};
                if (module == nullptr) {
                    return std::unexpected{
                        SchemaEditError{"New tagged unions can only be added to union modules"}};
                }
                auto const insertion_index{
                    edit.insertion_index.value_or(module->tagged_unions.size())};
                if (insertion_index > module->tagged_unions.size()) {
                    return std::unexpected{SchemaEditError{"Invalid tagged-union insertion index"}};
                }
                auto const declaration_index{module->unions.size() + insertion_index};

                module->tagged_unions.insert(module->tagged_unions.begin() +
                                                 static_cast<std::ptrdiff_t>(insertion_index),
                                             edit.schema);
                for (auto& existing : declarations_) {
                    if (existing.module_index == edit.module_index &&
                        existing.declaration_index >= declaration_index) {
                        ++existing.declaration_index;
                    }
                }
                auto const namespace_name{module->settings.namespace_name.value_or("")};
                declarations_.push_back(
                    {.id = edit.declaration,
                     .identity = TypeIdentity{.origin = TypeOrigin::declaration,
                                              .module_name = module->settings.name,
                                              .namespace_name = namespace_name,
                                              .name = edit.schema.name},
                     .module_index = edit.module_index,
                     .declaration_index = declaration_index,
                     .source = creation_source(edit.declaration)});
                try {
                    codegen::validate_manifest(manifest_);
                    types_ = resolve_type_graph(manifest_);
                } catch (std::exception const& error) {
                    declarations_.pop_back();
                    for (auto& existing : declarations_) {
                        if (existing.module_index == edit.module_index &&
                            existing.declaration_index > declaration_index) {
                            --existing.declaration_index;
                        }
                    }
                    module->tagged_unions.erase(module->tagged_unions.begin() +
                                                static_cast<std::ptrdiff_t>(insertion_index));
                    return std::unexpected{SchemaEditError{error.what()}};
                }
                return SchemaEditCommand{DeleteTaggedUnion{.declaration = edit.declaration}};
            } else if constexpr (std::is_same_v<Edit, ReplaceTaggedUnion>) {
                auto const* info{declaration(edit.declaration)};
                auto const* current{tagged_union_schema(edit.declaration)};
                if (info == nullptr || current == nullptr) {
                    return std::unexpected{SchemaEditError{"Unknown tagged-union declaration"}};
                }
                if (edit.schema.name != current->name) {
                    return std::unexpected{SchemaEditError{
                        "ReplaceTaggedUnion cannot rename a declaration; use a rename command"}};
                }
                auto* module{std::get_if<codegen::UnionModuleSchema>(
                    &manifest_.modules[info->module_index])};
                auto const index{info->declaration_index - module->unions.size()};
                auto previous{module->tagged_unions[index]};
                module->tagged_unions[index] = edit.schema;
                try {
                    codegen::validate_manifest(manifest_);
                    types_ = resolve_type_graph(manifest_);
                } catch (std::exception const& error) {
                    module->tagged_unions[index] = std::move(previous);
                    return std::unexpected{SchemaEditError{error.what()}};
                }
                return SchemaEditCommand{ReplaceTaggedUnion{.declaration = edit.declaration,
                                                            .schema = std::move(previous)}};
            } else if constexpr (std::is_same_v<Edit, DeleteTaggedUnion>) {
                auto const* found{declaration(edit.declaration)};
                auto const* current{tagged_union_schema(edit.declaration)};
                if (found == nullptr || current == nullptr) {
                    return std::unexpected{SchemaEditError{"Unknown tagged-union declaration"}};
                }
                if (auto const blocker{deletion_blocker(*found)}; blocker.has_value()) {
                    return std::unexpected{*blocker};
                }
                auto const info{*found};
                auto schema{*current};
                auto* module{
                    std::get_if<codegen::UnionModuleSchema>(&manifest_.modules[info.module_index])};
                auto const index{info.declaration_index - module->unions.size()};
                module->tagged_unions.erase(module->tagged_unions.begin() +
                                            static_cast<std::ptrdiff_t>(index));
                declarations_.erase(
                    std::ranges::find(declarations_, edit.declaration, &DeclarationInfo::id));
                for (auto& existing : declarations_) {
                    if (existing.module_index == info.module_index &&
                        existing.declaration_index > info.declaration_index) {
                        --existing.declaration_index;
                    }
                }
                try {
                    codegen::validate_manifest(manifest_);
                    types_ = resolve_type_graph(manifest_);
                } catch (std::exception const& error) {
                    module->tagged_unions.insert(
                        module->tagged_unions.begin() + static_cast<std::ptrdiff_t>(index), schema);
                    for (auto& existing : declarations_) {
                        if (existing.module_index == info.module_index &&
                            existing.declaration_index >= info.declaration_index) {
                            ++existing.declaration_index;
                        }
                    }
                    declarations_.push_back(info);
                    return std::unexpected{SchemaEditError{error.what()}};
                }
                remember_source_tombstone(info);
                return SchemaEditCommand{CreateTaggedUnion{.declaration = edit.declaration,
                                                           .module_index = info.module_index,
                                                           .schema = std::move(schema),
                                                           .insertion_index = index}};
            } else if constexpr (std::is_same_v<Edit, CreateSoa>) {
                if (!edit.declaration.valid() || declaration(edit.declaration) != nullptr) {
                    return std::unexpected{
                        SchemaEditError{"New SoA requires a unique declaration id"}};
                }
                if (edit.module_index >= manifest_.modules.size()) {
                    return std::unexpected{SchemaEditError{"Unknown SoA module index"}};
                }
                auto* module{
                    std::get_if<codegen::SoaModuleSchema>(&manifest_.modules[edit.module_index])};
                if (module == nullptr) {
                    return std::unexpected{
                        SchemaEditError{"New SoAs can only be added to SoA modules"}};
                }
                auto const insertion_index{edit.insertion_index.value_or(module->structs.size())};
                if (insertion_index > module->structs.size()) {
                    return std::unexpected{SchemaEditError{"Invalid SoA insertion index"}};
                }

                module->structs.insert(module->structs.begin() +
                                           static_cast<std::ptrdiff_t>(insertion_index),
                                       edit.schema);
                for (auto& existing : declarations_) {
                    if (existing.module_index == edit.module_index &&
                        existing.declaration_index >= insertion_index) {
                        ++existing.declaration_index;
                    }
                }
                auto const namespace_name{module->settings.namespace_name.value_or("")};
                declarations_.push_back(
                    {.id = edit.declaration,
                     .identity = TypeIdentity{.origin = TypeOrigin::declaration,
                                              .module_name = module->settings.name,
                                              .namespace_name = namespace_name,
                                              .name = edit.schema.name},
                     .module_index = edit.module_index,
                     .declaration_index = insertion_index,
                     .source = creation_source(edit.declaration)});
                try {
                    types_ = resolve_type_graph(manifest_);
                } catch (std::exception const& error) {
                    declarations_.pop_back();
                    for (auto& existing : declarations_) {
                        if (existing.module_index == edit.module_index &&
                            existing.declaration_index > insertion_index) {
                            --existing.declaration_index;
                        }
                    }
                    module->structs.erase(module->structs.begin() +
                                          static_cast<std::ptrdiff_t>(insertion_index));
                    return std::unexpected{SchemaEditError{error.what()}};
                }
                return SchemaEditCommand{DeleteSoa{.declaration = edit.declaration}};
            } else if constexpr (std::is_same_v<Edit, ReplaceSoa>) {
                auto const* info{declaration(edit.declaration)};
                auto const* current{soa_schema(edit.declaration)};
                if (info == nullptr || current == nullptr) {
                    return std::unexpected{SchemaEditError{"Unknown SoA declaration"}};
                }
                if (edit.schema.name != current->name) {
                    return std::unexpected{SchemaEditError{
                        "ReplaceSoa cannot rename a declaration; use a rename command"}};
                }
                auto* module{
                    std::get_if<codegen::SoaModuleSchema>(&manifest_.modules[info->module_index])};
                auto previous{module->structs[info->declaration_index]};
                module->structs[info->declaration_index] = edit.schema;
                try {
                    types_ = resolve_type_graph(manifest_);
                } catch (std::exception const& error) {
                    module->structs[info->declaration_index] = std::move(previous);
                    return std::unexpected{SchemaEditError{error.what()}};
                }
                return SchemaEditCommand{
                    ReplaceSoa{.declaration = edit.declaration, .schema = std::move(previous)}};
            } else if constexpr (std::is_same_v<Edit, DeleteSoa>) {
                auto const* found{declaration(edit.declaration)};
                auto const* current{soa_schema(edit.declaration)};
                if (found == nullptr || current == nullptr) {
                    return std::unexpected{SchemaEditError{"Unknown SoA declaration"}};
                }
                if (auto const blocker{deletion_blocker(*found)}; blocker.has_value()) {
                    return std::unexpected{*blocker};
                }
                auto const info{*found};
                auto schema{*current};
                auto* module{
                    std::get_if<codegen::SoaModuleSchema>(&manifest_.modules[info.module_index])};
                module->structs.erase(module->structs.begin() +
                                      static_cast<std::ptrdiff_t>(info.declaration_index));
                declarations_.erase(
                    std::ranges::find(declarations_, edit.declaration, &DeclarationInfo::id));
                for (auto& existing : declarations_) {
                    if (existing.module_index == info.module_index &&
                        existing.declaration_index > info.declaration_index) {
                        --existing.declaration_index;
                    }
                }
                try {
                    codegen::validate_manifest(manifest_);
                    types_ = resolve_type_graph(manifest_);
                } catch (std::exception const& error) {
                    module->structs.insert(module->structs.begin() +
                                               static_cast<std::ptrdiff_t>(info.declaration_index),
                                           schema);
                    for (auto& existing : declarations_) {
                        if (existing.module_index == info.module_index &&
                            existing.declaration_index >= info.declaration_index) {
                            ++existing.declaration_index;
                        }
                    }
                    declarations_.push_back(info);
                    return std::unexpected{SchemaEditError{error.what()}};
                }
                remember_source_tombstone(info);
                return SchemaEditCommand{CreateSoa{.declaration = edit.declaration,
                                                   .module_index = info.module_index,
                                                   .schema = std::move(schema),
                                                   .insertion_index = info.declaration_index}};
            }
        },
        command);
}

auto load_editable_schema_document(std::filesystem::path const& types_path,
                                   std::span<std::filesystem::path const> const module_paths)
    -> EditableSchemaDocument {
    auto manifest{codegen::load_sources(types_path, module_paths)};
    std::vector<SchemaSourceFile> sources;
    sources.push_back({.path = types_path, .text = read_file(types_path)});

    std::vector<std::optional<SourceRange>> declaration_ranges;
    std::vector<std::optional<SourceRange>> module_ranges;
    auto module_index{std::size_t{}};
    for (auto const& path : module_paths) {
        auto source{read_file(path)};
        auto const source_file_index{sources.size()};
        auto const forms{codegen::sexpr::read_forms(path.string(), source)};
        sources.push_back({.path = path, .text = std::move(source)});
        for (auto const& form : forms) {
            if (module_index >= manifest.modules.size()) {
                throw std::logic_error{"Source contains more modules than the loaded manifest"};
            }
            auto const& module{manifest.modules[module_index++]};
            module_ranges.push_back(form_range(form, source_file_index));
            auto const expected_count{declaration_count(module)};
            if (expected_count == 0) {
                continue;
            }
            if (std::holds_alternative<codegen::VectorModuleSchema>(module)) {
                declaration_ranges.push_back(form_range(form, source_file_index));
                continue;
            }
            if (auto const* representations{
                    std::get_if<codegen::RepresentationModuleSchema>(&module)}) {
                auto append_range = [&](std::string_view const head, std::string const& name) {
                    auto const found{std::ranges::find_if(form.children, [&](Form const& child) {
                        return child.head() == head && child.children.size() >= 2 &&
                               child.children[1].token.text == name;
                    })};
                    if (found == form.children.end()) {
                        throw std::logic_error{"Source declaration does not match loaded module: " +
                                               name};
                    }
                    declaration_ranges.push_back(form_range(*found, source_file_index));
                };
                for (auto const& representation : representations->linear_quantized) {
                    append_range("linear-quantized", representation.name);
                }
                for (auto const& representation : representations->integer_varints) {
                    append_range("integer-varint", representation.name);
                }
                for (auto const& representation : representations->fixed_points) {
                    append_range("fixed-point", representation.name);
                }
                for (auto const& representation : representations->optional_sentinels) {
                    append_range("optional-sentinel", representation.name);
                }
                for (auto const& representation : representations->optional_presence_bits) {
                    append_range("optional-presence-bit", representation.name);
                }
                for (auto const& representation : representations->mini_floats) {
                    append_range("mini-float", representation.name);
                }
                continue;
            }
            if (auto const* unions{std::get_if<codegen::UnionModuleSchema>(&module)}) {
                auto append_range = [&](std::string_view const head, std::string const& name) {
                    auto const found{std::ranges::find_if(form.children, [&](Form const& child) {
                        return child.head() == head && child.children.size() >= 2 &&
                               child.children[1].token.text == name;
                    })};
                    if (found == form.children.end()) {
                        throw std::logic_error{"Source declaration does not match loaded module: " +
                                               name};
                    }
                    declaration_ranges.push_back(form_range(*found, source_file_index));
                };
                for (auto const& schema : unions->unions) {
                    append_range("union", schema.name);
                }
                for (auto const& schema : unions->tagged_unions) {
                    append_range("tagged-union", schema.name);
                }
                continue;
            }
            auto const head{declaration_head(module)};
            auto found_count{std::size_t{}};
            for (auto const& child : form.children) {
                if (child.head() == head) {
                    declaration_ranges.push_back(form_range(child, source_file_index));
                    ++found_count;
                }
            }
            if (found_count != expected_count) {
                throw std::logic_error{"Source declaration count does not match loaded module"};
            }
        }
    }
    if (module_index != manifest.modules.size()) {
        throw std::logic_error{"Source contains fewer modules than the loaded manifest"};
    }

    EditableSchemaDocument result{std::move(manifest),
                                  std::move(sources),
                                  types_path,
                                  {module_paths.begin(), module_paths.end()}};
    result.module_source_ranges_ = std::move(module_ranges);
    result.initialize_declarations(std::move(declaration_ranges));
    return result;
}

} // namespace lispb::schema
