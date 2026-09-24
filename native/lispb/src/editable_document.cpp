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
#include <initializer_list>
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

auto module_generated_type_names(codegen::NormalModuleSchema const& module)
    -> std::set<std::string> {
    std::set<std::string> names;
    for (auto const& declaration : module.declarations) {
        auto const generated{codegen::generated_cpp_names(declaration, module)};
        names.insert(generated.begin(), generated.end());
    }
    return names;
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

auto render_enum_unreal_projection(codegen::EnumUnrealProjection const& projection) -> std::string {
    std::ostringstream output;
    output << "(unreal-projection " << projection.name << "\n      :header "
           << quote(projection.header.string()) << "\n      :header-include "
           << quote(projection.header_include) << "\n      :conversion-header "
           << quote(projection.conversion_header.string()) << "\n      :native-header-include "
           << quote(projection.native_header_include);
    if (projection.reflection != codegen::EnumReflection::uenum) {
        output << "\n      :reflection " << codegen::enum_reflection_name(projection.reflection);
    }
    output << ')';
    return output.str();
}

auto render_enum(codegen::EnumSchema const& schema) -> std::string {
    std::ostringstream output;
    output << "(enum " << schema.name;
    if (schema.underlying_type.has_value()) {
        output << ' ' << render_type_ref(*schema.underlying_type);
    }
    if (schema.bit_width.has_value()) {
        output << "\n    :bit-width " << *schema.bit_width;
    }
    if (schema.signedness.has_value()) {
        output << "\n    :signed " << (*schema.signedness ? "true" : "false");
    }
    if (schema.reflection != codegen::EnumReflection::none) {
        output << "\n    :reflection " << codegen::enum_reflection_name(schema.reflection);
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
            output << (index == 0 ? "" : " ")
                   << codegen::enum_conversion_name(schema.conversions[index]);
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
        output << "\n    " << render_enum_unreal_projection(*schema.unreal_projection);
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
    if (form.token.kind == codegen::sexpr::TokenKind::raw_literal) {
        auto const opening{"#" + form.token.tag + "{"};
        auto const closing{"}" + form.token.tag + "#"};
        if (source.substr(begin, opening.size()) != opening) {
            return std::nullopt;
        }
        auto const closing_begin{source.find(closing, begin + opening.size())};
        if (closing_begin == std::string_view::npos) {
            return std::nullopt;
        }
        return closing_begin + closing.size();
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

auto source_property_matches(Form const& target,
                             std::size_t const positional_count,
                             std::string_view const name,
                             std::optional<std::string> const& expected) -> bool {
    for (auto index{positional_count + 1}; index + 1 < target.children.size(); ++index) {
        if (target.children[index].token.kind != codegen::sexpr::TokenKind::keyword ||
            target.children[index].token.text != name) {
            continue;
        }
        return expected.has_value() &&
               source_form_matches_rendered(target.children[index + 1], *expected);
    }
    return !expected.has_value();
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
        result.replace(replacement.begin, replacement.end - replacement.begin, replacement.text);
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

auto parse_owned_source_declaration(std::string_view const original, std::string_view const head)
    -> std::optional<Form> {
    try {
        auto forms{codegen::sexpr::read_forms("editable declaration", original)};
        if (forms.size() != 1 || forms.front().head() != head ||
            forms.front().children.size() < 2) {
            return std::nullopt;
        }
        return std::move(forms.front());
    } catch (std::exception const&) {
        return std::nullopt;
    }
}

auto try_patch_owned_declaration_name(std::string_view const original,
                                      std::string const& current_name)
    -> std::optional<std::string> {
    try {
        auto forms{codegen::sexpr::read_forms("editable declaration", original)};
        if (forms.size() != 1 || forms.front().children.size() < 2 ||
            forms.front().children[1].is_list() ||
            forms.front().children[1].token.kind != codegen::sexpr::TokenKind::atom ||
            forms.front().children[1].token.text == current_name) {
            return std::nullopt;
        }
        std::vector<SourceReplacement> replacements;
        if (!replace_source_form(forms.front().children[1], current_name, original, replacements)) {
            return std::nullopt;
        }
        return apply_source_replacements(original, std::move(replacements));
    } catch (std::exception const&) {
        return std::nullopt;
    }
}

auto patch_source_atom_list_property(Form const& source,
                                     std::size_t positional_count,
                                     std::string_view property_name,
                                     std::span<std::string const> values,
                                     std::string_view property_indentation,
                                     std::string_view row_indentation,
                                     std::string_view original,
                                     std::vector<SourceReplacement>& replacements) -> bool;

template <typename Child, typename FieldsMatch>
auto infer_single_positional_rename(std::span<Form const* const> const source_rows,
                                    std::span<Child const> const current_rows,
                                    FieldsMatch&& fields_match)
    -> std::optional<std::pair<std::string_view, std::string_view>> {
    if (source_rows.size() != current_rows.size()) {
        return std::nullopt;
    }

    std::optional<std::pair<std::string_view, std::string_view>> renamed;
    for (std::size_t index{}; index < source_rows.size(); ++index) {
        if (source_rows[index]->children.size() < 2) {
            return std::nullopt;
        }
        auto const& source_name{source_rows[index]->children[1].token.text};
        auto const& current_name{current_rows[index].name};
        if (source_name == current_name) {
            continue;
        }
        if (renamed.has_value()) {
            return std::nullopt;
        }
        if (!fields_match(*source_rows[index], current_rows[index])) {
            return std::nullopt;
        }
        renamed = std::pair{std::string_view{source_name}, std::string_view{current_name}};
    }

    if (!renamed.has_value()) {
        return std::nullopt;
    }
    auto const source_has_new_name{std::ranges::any_of(source_rows, [&](Form const* const row) {
        return row->children[1].token.text == renamed->second;
    })};
    auto const current_has_old_name{std::ranges::find(current_rows, renamed->first, &Child::name) !=
                                    current_rows.end()};
    if (source_has_new_name || current_has_old_name) {
        return std::nullopt;
    }
    return renamed;
}

auto try_render_source_preserved_enum(codegen::EnumSchema const& schema,
                                      std::string_view const original)
    -> std::optional<std::string> {
    auto parsed{parse_owned_source_declaration(original, "enum", schema.name)};
    if (!parsed.has_value()) {
        return std::nullopt;
    }
    auto const& form{*parsed};
    if (form.children.size() < 2) {
        return std::nullopt;
    }
    auto const source_has_underlying{
        form.children.size() > 2 &&
        form.children[2].token.kind != codegen::sexpr::TokenKind::keyword &&
        (!form.children[2].is_list() || form.children[2].head() == "type-ref")};
    if (schema.underlying_type.has_value() &&
        (!schema.underlying_type->suffix.empty() || schema.underlying_type->nested.has_value())) {
        return std::nullopt;
    }

    std::vector<Form const*> values;
    Form const* source_projection{};
    for (auto const& child : form.children) {
        if (child.head() == "unreal-projection") {
            if (source_projection != nullptr) {
                return std::nullopt;
            }
            source_projection = &child;
        }
        if (child.head() == "value") {
            values.push_back(&child);
        }
    }
    for (auto const* value : values) {
        if (value->children.size() < 2) {
            return std::nullopt;
        }
    }

    std::vector<SourceReplacement> replacements;

    if (source_projection != nullptr && schema.unreal_projection.has_value()) {
        auto const& projection{*schema.unreal_projection};
        if (source_projection->children.size() < 2 ||
            !patch_source_form(
                source_projection->children[1], projection.name, original, replacements)) {
            return std::nullopt;
        }
        auto const projection_properties{std::array<SourceProperty, 5>{
            std::pair{"header", std::optional{quote(projection.header.string())}},
            std::pair{"header-include", std::optional{quote(projection.header_include)}},
            std::pair{"conversion-header",
                      std::optional{quote(projection.conversion_header.string())}},
            std::pair{"native-header-include",
                      std::optional{quote(projection.native_header_include)}},
            std::pair{"reflection",
                      projection.reflection != codegen::EnumReflection::uenum
                          ? std::optional{std::string{
                                codegen::enum_reflection_name(projection.reflection)}}
                          : std::nullopt}}};
        if (!patch_source_properties(
                *source_projection, 1, projection_properties, "      ", original, replacements)) {
            return std::nullopt;
        }
    } else if (source_projection != nullptr) {
        auto begin{source_projection->token.span.offset};
        while (begin > 0 && original[begin - 1] != '\n' &&
               std::isspace(static_cast<unsigned char>(original[begin - 1])) != 0) {
            --begin;
        }
        auto const end{
            source_form_line_end(*source_projection, original, form.closing.span.offset)};
        if (!end.has_value()) {
            return std::nullopt;
        }
        replacements.push_back({.begin = begin, .end = *end, .text = {}});
    } else if (schema.unreal_projection.has_value()) {
        replacements.push_back(
            {.begin = form.closing.span.offset,
             .end = form.closing.span.offset,
             .text = "\n    " + render_enum_unreal_projection(*schema.unreal_projection)});
    }

    if (source_has_underlying && schema.underlying_type.has_value()) {
        auto const& underlying{form.children[2]};
        if (underlying.token.text != schema.underlying_type->name &&
            !replace_source_form(
                underlying, schema.underlying_type->name, original, replacements)) {
            return std::nullopt;
        }
    } else if (source_has_underlying) {
        auto const& underlying{form.children[2]};
        auto const underlying_end{source_form_end(underlying, original)};
        if (!underlying_end.has_value()) {
            return std::nullopt;
        }
        replacements.push_back(
            {.begin = underlying.token.span.offset, .end = *underlying_end, .text = {}});
    } else if (schema.underlying_type.has_value()) {
        auto const name_end{source_form_end(form.children[1], original)};
        if (!name_end.has_value()) {
            return std::nullopt;
        }
        replacements.push_back(
            {.begin = *name_end, .end = *name_end, .text = " " + schema.underlying_type->name});
    }

    std::vector<std::string> conversion_names;
    conversion_names.reserve(schema.conversions.size());
    for (auto const conversion : schema.conversions) {
        conversion_names.emplace_back(codegen::enum_conversion_name(conversion));
    }
    auto const enum_properties{std::array<SourceProperty, 7>{
        std::pair{"bit-width",
                  schema.bit_width.has_value() ? std::optional{std::to_string(*schema.bit_width)}
                                               : std::nullopt},
        std::pair{"signed",
                  schema.signedness.has_value()
                      ? std::optional{*schema.signedness ? "true" : "false"}
                      : std::nullopt},
        std::pair{"reflection",
                  schema.reflection != codegen::EnumReflection::none
                      ? std::optional{std::string{codegen::enum_reflection_name(schema.reflection)}}
                      : std::nullopt},
        std::pair{"enum-array",
                  schema.enum_array ? std::optional<std::string>{"true"} : std::nullopt},
        std::pair{"count", schema.count},
        std::pair{"export-specifier", schema.export_specifier},
        std::pair{"native-api",
                  schema.native_api ? std::optional<std::string>{"true"} : std::nullopt}}};
    auto const positional_count{source_has_underlying ? 2U : 1U};
    if (!patch_source_properties(
            form, positional_count, enum_properties, "    ", original, replacements) ||
        !patch_source_atom_list_property(form,
                                         positional_count,
                                         "conversions",
                                         conversion_names,
                                         "    ",
                                         "      ",
                                         original,
                                         replacements)) {
        return std::nullopt;
    }

    if (values.empty()) {
        return schema.values.empty() ? apply_source_replacements(original, std::move(replacements))
                                     : std::nullopt;
    }

    auto const first_value_offset{values.front()->token.span.offset};
    for (auto const& child : form.children) {
        if (child.token.span.offset > first_value_offset && child.head() != "value" &&
            child.head() != "unreal-projection") {
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
    auto const renamed_value{infer_single_positional_rename<codegen::EnumeratorSchema>(
        values, schema.values, [](Form const& source, codegen::EnumeratorSchema const& value) {
            return source_property_matches(
                       source, 1, "value", value.initializer.transform(quote)) &&
                   source_property_matches(
                       source, 1, "display-name", value.display_name.transform(quote)) &&
                   source_property_matches(source,
                                           1,
                                           "hidden",
                                           value.hidden ? std::optional<std::string>{"true"}
                                                        : std::nullopt) &&
                   source_property_matches(
                       source, 1, "serialized-name", value.serialized_name.transform(quote)) &&
                   source_property_matches(source,
                                           1,
                                           "sentinel",
                                           value.sentinel ? std::optional<std::string>{"true"}
                                                          : std::nullopt);
        })};
    auto rendered_values{std::string{}};
    auto append_row = [&](std::string row) {
        auto const preceding_newline{rendered_values.empty()
                                         ? values_begin > 0 && original[values_begin - 1] == '\n'
                                         : rendered_values.back() == '\n'};
        if (!preceding_newline && (row.empty() || row.front() != '\n')) {
            rendered_values += '\n';
        }
        rendered_values += row;
    };

    for (auto const& value : schema.values) {
        auto const renamed{renamed_value.has_value() && renamed_value->second == value.name};
        auto const found{
            source_values.find(renamed ? renamed_value->first : std::string_view{value.name})};
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
        if (renamed &&
            !patch_source_form(
                found->second.form->children[1], value.name, original, value_replacements)) {
            return std::nullopt;
        }
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
        case codegen::PackedFieldKind::linear_quantized:
            return "linear-quantized";
        case codegen::PackedFieldKind::fixed_point:
            return "fixed-point";
        case codegen::PackedFieldKind::mini_float:
            return "mini-float";
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

auto render_named_code(codegen::PackedNamedCodeSchema const& code) -> std::string;

auto render_semantic_relation(codegen::SemanticRelationSchema const& relationship) -> std::string {
    std::ostringstream output;
    output << "(relation " << semantic_relation_kind_name(relationship.kind) << ' '
           << render_type_ref(relationship.target);
    if (relationship.unit.has_value()) {
        output << " :unit " << semantic_relation_unit_name(*relationship.unit);
    }
    output << ')';
    return output.str();
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
                    output << "\n      " << render_named_code(code);
                }
                if (value.relationship.has_value()) {
                    output << "\n      " << render_semantic_relation(*value.relationship);
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

auto render_named_code(codegen::PackedNamedCodeSchema const& code) -> std::string {
    std::ostringstream output;
    output << "(code " << code.name << " :value " << format_packed_integer(code.value);
    if (code.sentinel) {
        output << " :sentinel true";
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
    if (schema.cpp_emission != codegen::IntegerScalarCppEmission::none) {
        output << "\n    :cpp-emission " << integer_scalar_cpp_emission_name(schema.cpp_emission);
    }
    if (schema.cpp_type.has_value()) {
        output << "\n    :cpp-type " << render_type_ref(*schema.cpp_type);
    }
    for (auto const& code : schema.named_codes) {
        output << "\n    " << render_named_code(code);
    }
    if (schema.relationship.has_value()) {
        output << "\n    " << render_semantic_relation(*schema.relationship);
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
           << fixed_point_rounding_name(schema.rounding);
    if (schema.minimum_value.has_value()) {
        output << "\n    :minimum " << *schema.minimum_value;
    }
    if (schema.maximum_value.has_value()) {
        output << "\n    :maximum " << *schema.maximum_value;
    }
    output << ')';
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

auto patch_source_named_code_rows(Form const& parent,
                                  std::span<Form const* const> const source_code_forms,
                                  std::span<codegen::PackedNamedCodeSchema const> const codes,
                                  Form const* const trailing_form,
                                  std::string_view const indentation,
                                  std::string_view const property_indentation,
                                  std::string_view const original,
                                  std::vector<SourceReplacement>& replacements) -> bool {
    if (source_code_forms.empty()) {
        if (codes.empty()) {
            return true;
        }

        auto insertion_begin{parent.closing.span.offset};
        if (trailing_form != nullptr) {
            insertion_begin = 0;
            for (auto const& child : parent.children) {
                if (child.token.span.offset >= trailing_form->token.span.offset) {
                    continue;
                }
                auto const child_end{
                    source_form_line_end(child, original, trailing_form->token.span.offset)};
                if (!child_end.has_value()) {
                    return false;
                }
                insertion_begin = (std::max)(insertion_begin, *child_end);
            }
            if (insertion_begin > trailing_form->token.span.offset) {
                return false;
            }
        }

        auto rendered_codes{std::string{}};
        if (insertion_begin > 0 && original[insertion_begin - 1] != '\n') {
            rendered_codes += '\n';
        }
        for (std::size_t index{}; index < codes.size(); ++index) {
            if (index != 0) {
                rendered_codes += '\n';
            }
            rendered_codes += std::string{indentation} + render_named_code(codes[index]);
        }
        if (trailing_form != nullptr && rendered_codes.back() != '\n') {
            rendered_codes += '\n';
        }
        replacements.push_back(
            {.begin = insertion_begin, .end = insertion_begin, .text = std::move(rendered_codes)});
        return true;
    }

    auto const first_code_offset{source_code_forms.front()->token.span.offset};
    if (trailing_form != nullptr && trailing_form->token.span.offset < first_code_offset) {
        return false;
    }
    for (auto const& child : parent.children) {
        if (child.token.span.offset > first_code_offset && child.head() != "code" &&
            &child != trailing_form) {
            return false;
        }
    }

    auto codes_begin{std::size_t{}};
    for (auto const& child : parent.children) {
        if (child.token.span.offset >= first_code_offset) {
            continue;
        }
        auto const child_end{source_form_line_end(child, original, first_code_offset)};
        if (!child_end.has_value()) {
            return false;
        }
        codes_begin = (std::max)(codes_begin, *child_end);
    }
    if (codes_begin > first_code_offset) {
        return false;
    }

    struct SourceCode {
        Form const* form{};
        std::size_t begin{};
        std::size_t end{};
    };
    std::map<std::string_view, SourceCode> source_codes;
    auto row_begin{codes_begin};
    auto const region_end{trailing_form != nullptr ? trailing_form->token.span.offset
                                                   : parent.closing.span.offset};
    for (auto const* code : source_code_forms) {
        if (code->children.size() < 2) {
            return false;
        }
        auto const row_end{source_form_line_end(*code, original, region_end)};
        if (!row_end.has_value() || row_begin > code->token.span.offset ||
            *row_end < code->closing.span.offset + 1) {
            return false;
        }
        if (!source_codes
                 .emplace(code->children[1].token.text,
                          SourceCode{.form = code, .begin = row_begin, .end = *row_end})
                 .second) {
            return false;
        }
        row_begin = *row_end;
    }

    auto const codes_end{row_begin};
    auto const renamed_code{infer_single_positional_rename<codegen::PackedNamedCodeSchema>(
        source_code_forms,
        codes,
        [](Form const& source, codegen::PackedNamedCodeSchema const& code) {
            return source_property_matches(
                       source, 1, "value", std::optional{format_packed_integer(code.value)}) &&
                   source_property_matches(source,
                                           1,
                                           "sentinel",
                                           code.sentinel ? std::optional<std::string>{"true"}
                                                         : std::nullopt);
        })};
    auto rendered_codes{std::string{}};
    auto append_row = [&](std::string row) {
        auto const preceding_newline{rendered_codes.empty()
                                         ? codes_begin > 0 && original[codes_begin - 1] == '\n'
                                         : rendered_codes.back() == '\n'};
        if (!preceding_newline && (row.empty() || row.front() != '\n')) {
            rendered_codes += '\n';
        }
        rendered_codes += row;
    };

    for (auto const& code : codes) {
        auto const renamed{renamed_code.has_value() && renamed_code->second == code.name};
        auto const found{
            source_codes.find(renamed ? renamed_code->first : std::string_view{code.name})};
        if (found == source_codes.end()) {
            append_row(std::string{indentation} + render_named_code(code));
            continue;
        }

        auto const code_properties{std::array<SourceProperty, 2>{
            std::pair{"value", std::optional{format_packed_integer(code.value)}},
            std::pair{"sentinel",
                      code.sentinel ? std::optional<std::string>{"true"} : std::nullopt}}};
        std::vector<SourceReplacement> code_replacements;
        if (renamed &&
            !patch_source_form(
                found->second.form->children[1], code.name, original, code_replacements)) {
            return false;
        }
        if (!patch_source_properties(*found->second.form,
                                     1,
                                     code_properties,
                                     property_indentation,
                                     original,
                                     code_replacements)) {
            return false;
        }
        auto rendered{apply_source_replacements_to_range(
            original, found->second.begin, found->second.end, std::move(code_replacements))};
        if (!rendered.has_value()) {
            return false;
        }
        append_row(std::move(*rendered));
    }
    if (!rendered_codes.empty() && rendered_codes.back() != '\n' && codes_end < original.size() &&
        original[codes_end] != ')') {
        rendered_codes += '\n';
    }
    replacements.push_back(
        {.begin = codes_begin, .end = codes_end, .text = std::move(rendered_codes)});
    return true;
}

auto patch_source_optional_relation(
    Form const& parent,
    Form const* const source_relation,
    std::optional<codegen::SemanticRelationSchema> const& relationship,
    std::string_view const indentation,
    std::string_view const property_indentation,
    std::string_view const original,
    std::vector<SourceReplacement>& replacements) -> bool {
    if (source_relation != nullptr && relationship.has_value()) {
        if (source_relation->children.size() < 3 ||
            !patch_source_form(source_relation->children[1],
                               std::string{semantic_relation_kind_name(relationship->kind)},
                               original,
                               replacements) ||
            !patch_source_form(source_relation->children[2],
                               render_type_ref(relationship->target),
                               original,
                               replacements)) {
            return false;
        }
        auto const relation_properties{std::array<SourceProperty, 1>{std::pair{
            "unit", relationship->unit.transform([](codegen::SemanticRelationUnit const unit) {
                return std::string{codegen::semantic_relation_unit_name(unit)};
            })}}};
        return patch_source_properties(
            *source_relation, 2, relation_properties, property_indentation, original, replacements);
    }

    if (source_relation == nullptr && relationship.has_value()) {
        replacements.push_back(
            {.begin = parent.closing.span.offset,
             .end = parent.closing.span.offset,
             .text = "\n" + std::string{indentation} + render_semantic_relation(*relationship)});
        return true;
    }

    if (source_relation == nullptr) {
        return true;
    }

    auto relation_begin{std::size_t{}};
    for (auto const& child : parent.children) {
        if (child.token.span.offset >= source_relation->token.span.offset) {
            continue;
        }
        auto const child_end{
            source_form_line_end(child, original, source_relation->token.span.offset)};
        if (!child_end.has_value()) {
            return false;
        }
        relation_begin = (std::max)(relation_begin, *child_end);
    }
    auto const relation_end{
        source_form_line_end(*source_relation, original, parent.closing.span.offset)};
    if (!relation_end.has_value() || relation_begin > source_relation->token.span.offset ||
        *relation_end < source_relation->closing.span.offset + 1) {
        return false;
    }
    replacements.push_back({.begin = relation_begin, .end = *relation_end, .text = {}});
    return true;
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
                if (relation != nullptr) {
                    return false;
                }
                relation = &child;
            }
        }
        if (codes.empty() && !field->named_codes.empty() && relation == nullptr &&
            field->relationship.has_value()) {
            return false;
        }
        if (!patch_source_named_code_rows(source_segment,
                                          codes,
                                          field->named_codes,
                                          relation,
                                          "      ",
                                          "        ",
                                          original,
                                          replacements) ||
            !patch_source_optional_relation(source_segment,
                                            relation,
                                            field->relationship,
                                            "      ",
                                            "        ",
                                            original,
                                            replacements)) {
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

auto packed_segment_matches_except_name(Form const& source_segment,
                                        codegen::PackedSegmentSchema const& segment) -> bool {
    if (auto const field{std::get_if<codegen::PackedFieldSchema>(&segment)}) {
        auto const structural_match{source_segment.head() == "field" &&
                                    source_segment.children.size() >= 3};
        auto const type_match{
            structural_match &&
            source_form_matches_rendered(source_segment.children[2], render_type_ref(field->type))};
        auto const bits_match{source_property_matches(
            source_segment,
            2,
            "bits",
            std::optional{field->bits.has_value() ? std::to_string(*field->bits)
                                                  : std::string{"auto"}})};
        auto const kind_match{source_property_matches(
            source_segment,
            2,
            "kind",
            field->kind != codegen::PackedFieldKind::unsigned_integer
                ? std::optional{std::string{packed_field_kind_name(field->kind)}}
                : std::nullopt)};
        auto const range_match{source_property_matches(
            source_segment,
            2,
            "range-helper",
            field->range_helper ? std::optional<std::string>{"true"} : std::nullopt)};
        auto const minimum_match{source_property_matches(
            source_segment,
            2,
            "minimum",
            field->minimum_value.transform(codegen::format_packed_integer))};
        auto const maximum_match{source_property_matches(
            source_segment,
            2,
            "maximum",
            field->maximum_value.transform(codegen::format_packed_integer))};
        if (!structural_match || !type_match || !bits_match || !kind_match || !range_match ||
            !minimum_match || !maximum_match) {
            return false;
        }

        std::vector<Form const*> codes;
        Form const* relation{};
        for (auto const& child : source_segment.children) {
            if (child.head() == "code") {
                codes.push_back(&child);
            } else if (child.head() == "relation") {
                if (relation != nullptr) {
                    return false;
                }
                relation = &child;
            }
        }
        if (codes.size() != field->named_codes.size()) {
            return false;
        }
        for (std::size_t index{}; index < codes.size(); ++index) {
            auto const& code{field->named_codes[index]};
            if (codes[index]->children.size() < 2 ||
                codes[index]->children[1].token.text != code.name ||
                !source_property_matches(
                    *codes[index], 1, "value", std::optional{format_packed_integer(code.value)}) ||
                !source_property_matches(*codes[index],
                                         1,
                                         "sentinel",
                                         code.sentinel ? std::optional<std::string>{"true"}
                                                       : std::nullopt)) {
                return false;
            }
        }

        if (relation == nullptr || !field->relationship.has_value()) {
            return relation == nullptr && !field->relationship.has_value();
        }
        auto const relation_match{
            relation->children.size() >= 3 &&
            source_form_matches_rendered(
                relation->children[1],
                std::string{semantic_relation_kind_name(field->relationship->kind)}) &&
            source_form_matches_rendered(relation->children[2],
                                         render_type_ref(field->relationship->target)) &&
            source_property_matches(
                *relation, 2, "unit", field->relationship->unit.transform([](auto const unit) {
                    return std::string{semantic_relation_unit_name(unit)};
                }))};
        return relation_match;
    }

    auto const& reserved{std::get<codegen::PackedReservedBitsSchema>(segment)};
    return source_segment.head() == "reserved" && source_segment.children.size() >= 2 &&
           source_property_matches(
               source_segment, 1, "bits", std::optional{std::to_string(reserved.bits)});
}

auto infer_single_packed_segment_rename(
    std::span<Form const* const> const source_segments,
    std::span<codegen::PackedSegmentSchema const> const current_segments)
    -> std::optional<std::pair<std::string_view, std::string_view>> {
    if (source_segments.size() != current_segments.size()) {
        return std::nullopt;
    }

    std::optional<std::pair<std::string_view, std::string_view>> renamed;
    for (std::size_t index{}; index < source_segments.size(); ++index) {
        if (source_segments[index]->children.size() < 2) {
            return std::nullopt;
        }
        auto const& source_name{source_segments[index]->children[1].token.text};
        auto const& current_name{codegen::packed_segment_name(current_segments[index])};
        if (source_name == current_name) {
            continue;
        }
        if (renamed.has_value() ||
            !packed_segment_matches_except_name(*source_segments[index], current_segments[index])) {
            return std::nullopt;
        }
        renamed = std::pair{std::string_view{source_name}, std::string_view{current_name}};
    }

    if (!renamed.has_value()) {
        return std::nullopt;
    }
    auto const source_has_new_name{
        std::ranges::any_of(source_segments, [&](Form const* const segment) {
            return segment->children[1].token.text == renamed->second;
        })};
    auto const current_has_old_name{
        std::ranges::any_of(current_segments, [&](codegen::PackedSegmentSchema const& segment) {
            return codegen::packed_segment_name(segment) == renamed->first;
        })};
    if (source_has_new_name || current_has_old_name) {
        return std::nullopt;
    }
    return renamed;
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
    auto const renamed_segment{infer_single_packed_segment_rename(segments, schema.segments)};
    auto rendered_segments{std::string{}};
    auto append_row = [&](std::string row) {
        auto const preceding_newline{
            rendered_segments.empty() ? segments_begin > 0 && original[segments_begin - 1] == '\n'
                                      : rendered_segments.back() == '\n'};
        if (!preceding_newline && (row.empty() || row.front() != '\n')) {
            rendered_segments += '\n';
        }
        rendered_segments += row;
    };

    for (auto const& segment : schema.segments) {
        auto const head{std::holds_alternative<codegen::PackedFieldSchema>(segment)
                            ? std::string_view{"field"}
                            : std::string_view{"reserved"}};
        auto const& current_name{codegen::packed_segment_name(segment)};
        auto const renamed{renamed_segment.has_value() && renamed_segment->second == current_name};
        auto const key{SegmentKey{head, renamed ? renamed_segment->first : current_name}};
        auto const found{source_segments.find(key)};
        if (found == source_segments.end()) {
            append_row("    " + render_packed_segment(segment));
            continue;
        }

        std::vector<SourceReplacement> segment_replacements;
        if (renamed && !patch_source_form(found->second.form->children[1],
                                          std::string{current_name},
                                          original,
                                          segment_replacements)) {
            return std::nullopt;
        }
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
    Form const* relation{};
    for (auto const& child : parsed->children) {
        if (child.head() == "code") {
            codes.push_back(&child);
        } else if (child.head() == "relation") {
            if (relation != nullptr) {
                return std::nullopt;
            }
            relation = &child;
        }
    }
    for (auto const* code : codes) {
        if (code->children.size() < 2) {
            return std::nullopt;
        }
    }

    auto const properties{std::array<SourceProperty, 6>{
        std::pair{"signed", std::optional{schema.signedness ? "true" : "false"}},
        std::pair{"minimum", std::optional{format_packed_integer(schema.minimum_value)}},
        std::pair{"maximum", std::optional{format_packed_integer(schema.maximum_value)}},
        std::pair{"bit-width",
                  std::optional{schema.bit_width.has_value() ? std::to_string(*schema.bit_width)
                                                             : std::string{"auto"}}},
        std::pair{
            "cpp-emission",
            schema.cpp_emission != codegen::IntegerScalarCppEmission::none
                ? std::optional{std::string{integer_scalar_cpp_emission_name(schema.cpp_emission)}}
                : std::nullopt},
        std::pair{"cpp-type",
                  schema.cpp_type.has_value() ? std::optional{render_type_ref(*schema.cpp_type)}
                                              : std::nullopt}}};
    std::vector<SourceReplacement> replacements;
    if (!patch_source_properties(*parsed, 1, properties, "    ", original, replacements)) {
        return std::nullopt;
    }
    if (codes.empty() && !schema.named_codes.empty() && relation == nullptr &&
        schema.relationship.has_value()) {
        return std::nullopt;
    }
    if (!patch_source_named_code_rows(*parsed,
                                      codes,
                                      schema.named_codes,
                                      relation,
                                      "    ",
                                      "      ",
                                      original,
                                      replacements) ||
        !patch_source_optional_relation(
            *parsed, relation, schema.relationship, "    ", "      ", original, replacements)) {
        return std::nullopt;
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
    auto const properties{std::array<SourceProperty, 6>{
        std::pair{"signed", std::optional{schema.signedness ? "true" : "false"}},
        std::pair{"total-bits", std::optional{std::to_string(schema.total_bits)}},
        std::pair{"fractional-bits", std::optional{std::to_string(schema.fractional_bits)}},
        std::pair{"rounding",
                  std::optional{std::string{fixed_point_rounding_name(schema.rounding)}}},
        std::pair{"minimum", schema.minimum_value},
        std::pair{"maximum", schema.maximum_value}}};
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
    if constexpr (std::is_same_v<Child, codegen::RecordMemberSchema>) {
        if (child.relationship.has_value()) {
            output << "\n      " << render_semantic_relation(*child.relationship);
        }
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
    auto const renamed_child{infer_single_positional_rename<Child>(
        source_children, children, [](Form const& source, Child const& child) {
            if (source.children.size() < 3 ||
                !source_form_matches_rendered(source.children[2], render_type_ref(child.type)) ||
                !source_property_matches(source,
                                         2,
                                         "count",
                                         child.count.has_value()
                                             ? std::optional{std::to_string(*child.count)}
                                             : std::nullopt)) {
                return false;
            }
            if constexpr (!std::is_same_v<Child, codegen::RecordMemberSchema>) {
                return true;
            } else {
                Form const* source_relation{};
                for (auto const& nested : source.children) {
                    if (nested.head() != "relation") {
                        continue;
                    }
                    if (source_relation != nullptr) {
                        return false;
                    }
                    source_relation = &nested;
                }
                return child.relationship.has_value()
                         ? source_relation != nullptr &&
                               source_form_matches_rendered(
                                   *source_relation, render_semantic_relation(*child.relationship))
                         : source_relation == nullptr;
            }
        })};
    auto rendered_children{std::string{}};
    auto append_row = [&](std::string row) {
        auto const preceding_newline{
            rendered_children.empty() ? children_begin > 0 && original[children_begin - 1] == '\n'
                                      : rendered_children.back() == '\n'};
        if (!preceding_newline && (row.empty() || row.front() != '\n')) {
            rendered_children += '\n';
        }
        rendered_children += row;
    };

    for (auto const& child : children) {
        auto const renamed{renamed_child.has_value() && renamed_child->second == child.name};
        auto const found{
            source_by_name.find(renamed ? renamed_child->first : std::string_view{child.name})};
        if (found == source_by_name.end()) {
            append_row("    " + render_aggregate_child(child_head, child));
            continue;
        }

        std::vector<SourceReplacement> child_replacements;
        if (renamed &&
            !patch_source_form(
                found->second.form->children[1], child.name, original, child_replacements)) {
            return std::nullopt;
        }
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
        if constexpr (std::is_same_v<Child, codegen::RecordMemberSchema>) {
            Form const* source_relation{};
            for (auto const& nested : found->second.form->children) {
                if (nested.head() != "relation") {
                    continue;
                }
                if (source_relation != nullptr) {
                    return std::nullopt;
                }
                source_relation = &nested;
            }
            if (!patch_source_optional_relation(*found->second.form,
                                                source_relation,
                                                child.relationship,
                                                "      ",
                                                "        ",
                                                original,
                                                child_replacements)) {
                return std::nullopt;
            }
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
    auto const renamed_alternative{
        infer_single_positional_rename<codegen::TaggedUnionAlternativeSchema>(
            alternatives,
            schema.alternatives,
            [](Form const& source, codegen::TaggedUnionAlternativeSchema const& alternative) {
                return source.children.size() >= 3 &&
                       source_form_matches_rendered(source.children[2],
                                                    render_type_ref(alternative.type)) &&
                       source_property_matches(source, 2, "tag", std::optional{alternative.tag}) &&
                       source_property_matches(
                           source,
                           2,
                           "count",
                           alternative.count.has_value()
                               ? std::optional{std::to_string(*alternative.count)}
                               : std::nullopt);
            })};
    auto rendered_alternatives{std::string{}};
    auto append_row = [&](std::string row) {
        auto const preceding_newline{rendered_alternatives.empty()
                                         ? alternatives_begin > 0 &&
                                               original[alternatives_begin - 1] == '\n'
                                         : rendered_alternatives.back() == '\n'};
        if (!preceding_newline && (row.empty() || row.front() != '\n')) {
            rendered_alternatives += '\n';
        }
        rendered_alternatives += row;
    };

    for (auto const& alternative : schema.alternatives) {
        auto const renamed{renamed_alternative.has_value() &&
                           renamed_alternative->second == alternative.name};
        auto const found{source_by_name.find(renamed ? renamed_alternative->first
                                                     : std::string_view{alternative.name})};
        if (found == source_by_name.end()) {
            append_row("    " + render_tagged_union_alternative(alternative));
            continue;
        }

        std::vector<SourceReplacement> alternative_replacements;
        if (renamed && !patch_source_form(found->second.form->children[1],
                                          alternative.name,
                                          original,
                                          alternative_replacements)) {
            return std::nullopt;
        }
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

auto render_soa_member(codegen::SoaMemberSchema const& member) -> std::string {
    std::ostringstream output;
    output << "(member " << member.name << ' ' << soa_member_kind_name(member.kind) << ' '
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
    if (member.relationship.has_value()) {
        output << "\n      " << render_semantic_relation(*member.relationship);
    }
    output << ')';
    return output.str();
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
    if (!schema.vector_components.empty()) {
        output << "\n    :vector-components (";
        for (std::size_t index{}; index < schema.vector_components.size(); ++index) {
            if (index != 0) {
                output << ' ';
            }
            output << schema.vector_components[index];
        }
        output << ')';
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
        output << "\n    " << render_soa_member(member);
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

auto render_vector_soa(codegen::VectorSoaSchema const& schema) -> std::string {
    std::ostringstream output;
    output << "(vector-soa " << schema.name << "\n    :value-type "
           << render_type_ref(schema.value_type) << "\n    :components ";
    render_quoted_list(output, schema.components);
    if (!schema.equivalent_members.empty()) {
        output << "\n    :equivalent-members ";
        render_quoted_list(output, schema.equivalent_members);
    }
    if (schema.equivalent_constructor.has_value()) {
        output << "\n    :equivalent-constructor " << *schema.equivalent_constructor;
    }
    output << "\n    :equivalent-type " << render_type_ref(schema.equivalent_type);
    if (schema.export_specifier.has_value()) {
        output << "\n    :export-specifier " << *schema.export_specifier;
    }
    if (schema.fixed.has_value()) {
        output << "\n    (fixed " << schema.fixed->storage_name;
        if (!schema.fixed->containers.empty()) {
            output << " :containers ";
            render_quoted_list(output, schema.fixed->containers);
        }
        output << ')';
    }
    output << ')';
    return output.str();
}

auto render_homogeneous_layout(codegen::HomogeneousLayoutSchema const& schema) -> std::string {
    std::ostringstream output;
    output << "(layout " << schema.name << "\n    :components ";
    render_quoted_list(output, schema.components);
    if (!schema.input_members.empty()) {
        output << "\n    :input-members ";
        render_quoted_list(output, schema.input_members);
    }
    if (schema.export_specifier.has_value()) {
        output << "\n    :export-specifier " << *schema.export_specifier;
    }
    for (auto const& value : schema.value_types) {
        output << "\n    (value-type " << render_type_ref(value.type) << ' ' << value.suffix;
        if (value.equivalent_type.has_value()) {
            output << "\n      :equivalent-type " << render_type_ref(*value.equivalent_type);
        }
        if (!value.input_types.empty()) {
            output << "\n      :input-types (";
            for (std::size_t index{}; index < value.input_types.size(); ++index) {
                output << (index == 0 ? "" : " ") << render_type_ref(value.input_types[index]);
            }
            output << ')';
        }
        output << ')';
    }
    output << ')';
    return output.str();
}

auto render_static_table(codegen::StaticTableSchema const& schema) -> std::string {
    std::ostringstream output;
    output << "(table " << schema.name;
    if (schema.export_specifier.has_value()) {
        output << "\n    :export-specifier " << *schema.export_specifier;
    }
    for (auto const& row : schema.rows) {
        output << "\n    (row " << row.name << ')';
    }
    for (auto const& column : schema.columns) {
        output << "\n    (column " << column.name << ' ' << render_type_ref(column.type) << ')';
    }
    for (auto const& group : schema.groups) {
        output << "\n    (group " << group.name << ' ' << render_type_ref(group.type)
               << " :columns ";
        render_quoted_list(output, group.columns);
        output << ')';
    }
    output << ')';
    return output.str();
}

auto render_facade(codegen::FacadeSchema const& schema) -> std::string {
    std::ostringstream output;
    output << "(facade " << schema.name << ' ' << render_type_ref(schema.target_type) << ' '
           << schema.target_member_name;
    if (!schema.validation_lines.empty()) {
        output << "\n    :validation ";
        render_quoted_list(output, schema.validation_lines);
    }
    if (!schema.validation_dependencies.empty()) {
        output << "\n    :validation-dependencies ";
        render_quoted_list(output, schema.validation_dependencies);
    }
    if (schema.export_specifier.has_value()) {
        output << "\n    :export-specifier " << *schema.export_specifier;
    }
    if (schema.bind_access != "public") {
        output << "\n    :bind-access " << schema.bind_access;
    }
    if (schema.method_access != "public") {
        output << "\n    :method-access " << schema.method_access;
    }
    if (!schema.friends.empty()) {
        output << "\n    :friends ";
        render_quoted_list(output, schema.friends);
    }
    if (schema.friend_kind != "class") {
        output << "\n    :friend-kind " << schema.friend_kind;
    }
    if (schema.definitions_in_source) {
        output << "\n    :definitions-in-source true";
    }
    if (schema.reference_target) {
        output << "\n    :target-storage reference";
    }
    for (auto const& method : schema.methods) {
        output << "\n    (method " << method.name << ' ' << render_type_ref(method.return_type);
        if (method.is_const) {
            output << "\n      :const true";
        }
        if (method.is_noexcept) {
            output << "\n      :noexcept true";
        }
        if (method.target_name.has_value()) {
            output << "\n      :target-name " << *method.target_name;
        }
        for (auto const& parameter : method.parameters) {
            output << "\n      (parameter " << parameter.name << ' '
                   << render_type_ref(parameter.type);
            if (parameter.default_value.has_value()) {
                output << " :default " << quote(*parameter.default_value);
            }
            output << ')';
        }
        output << ')';
    }
    output << ')';
    return output.str();
}

template <typename>
inline constexpr bool unrendered_declaration_schema{false};

auto render_declaration_schema(codegen::DeclarationSchema const& declaration) -> std::string {
    return std::visit(
        [](auto const& value) -> std::string {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, codegen::EnumSchema>) {
                return render_enum(value);
            } else if constexpr (std::is_same_v<T, codegen::IntegerScalarSchema>) {
                return render_integer_scalar(value);
            } else if constexpr (std::is_same_v<T, codegen::LinearQuantizedSchema>) {
                return render_linear_quantized(value);
            } else if constexpr (std::is_same_v<T, codegen::IntegerVarintSchema>) {
                return render_integer_varint(value);
            } else if constexpr (std::is_same_v<T, codegen::FixedPointSchema>) {
                return render_fixed_point(value);
            } else if constexpr (std::is_same_v<T, codegen::OptionalSentinelSchema>) {
                return render_optional_sentinel(value);
            } else if constexpr (std::is_same_v<T, codegen::OptionalPresenceBitSchema>) {
                return render_optional_presence_bit(value);
            } else if constexpr (std::is_same_v<T, codegen::MiniFloatSchema>) {
                return render_mini_float(value);
            } else if constexpr (std::is_same_v<T, codegen::PackedValueSchema>) {
                return render_packed_value(value);
            } else if constexpr (std::is_same_v<T, codegen::RecordSchema>) {
                return render_record(value);
            } else if constexpr (std::is_same_v<T, codegen::UnionSchema>) {
                return render_union(value);
            } else if constexpr (std::is_same_v<T, codegen::TaggedUnionSchema>) {
                return render_tagged_union(value);
            } else if constexpr (std::is_same_v<T, codegen::SoaSchema>) {
                return render_soa(value);
            } else if constexpr (std::is_same_v<T, codegen::VectorSoaSchema>) {
                return render_vector_soa(value);
            } else if constexpr (std::is_same_v<T, codegen::HomogeneousLayoutSchema>) {
                return render_homogeneous_layout(value);
            } else if constexpr (std::is_same_v<T, codegen::StaticTableSchema>) {
                return render_static_table(value);
            } else if constexpr (std::is_same_v<T, codegen::FacadeSchema>) {
                return render_facade(value);
            } else {
                static_assert(unrendered_declaration_schema<T>);
            }
        },
        declaration);
}

auto render_editable_module(codegen::ModuleSchema const& schema) -> std::optional<std::string> {
    auto const* normal{std::get_if<codegen::NormalModuleSchema>(&schema)};
    if (normal == nullptr) {
        return std::nullopt;
    }
    auto const& module{*normal};
    auto const& settings{module.settings};
    std::ostringstream output;
    output << '(' << "module" << ' ' << settings.name << "\n  :header "
           << quote(settings.header.generic_string());
    if (settings.source.has_value()) {
        output << "\n  :source " << quote(settings.source->generic_string());
    }
    if (settings.header_include.has_value()) {
        output << "\n  :header-include " << quote(*settings.header_include);
    }
    if (settings.namespace_name.has_value()) {
        output << "\n  :namespace " << *settings.namespace_name;
    }
    if (!settings.include_order.empty()) {
        output << "\n  :include-order ";
        render_quoted_list(output, settings.include_order);
    }
    if (!settings.prelude_lines.empty()) {
        output << "\n  :prelude ";
        render_quoted_list(output, settings.prelude_lines);
    }

    if (module.enum_helper_namespace.has_value()) {
        output << "\n  :helper-namespace " << *module.enum_helper_namespace;
    }
    if (module.soa_backend == codegen::SoaBackend::standard_library) {
        output << "\n  :backend standard-library";
    }
    for (auto const& allocator : module.soa_array_allocators) {
        output << "\n  (array-allocator " << allocator.prefix << ' '
               << render_type_ref(allocator.allocator) << ')';
    }
    for (auto const& declaration : module.declarations) {
        output << "\n  " << render_declaration_schema(declaration);
    }
    output << ')';
    return output.str();
}

auto render_single_allocation_variant(codegen::SingleAllocationVariant const& variant)
    -> std::string {
    return "(variant " + variant.name + " " + render_type_ref(variant.allocator) + ")";
}

auto render_fixed_containers(std::span<std::string const> const containers) -> std::string {
    auto rendered{std::string{"("}};
    for (std::size_t index{}; index < containers.size(); ++index) {
        rendered += index == 0 ? "" : " ";
        rendered += containers[index];
    }
    rendered += ')';
    return rendered;
}

auto render_fixed_soa(codegen::FixedSoaSchema const& schema) -> std::string {
    auto rendered{"(fixed " + schema.storage_name};
    if (!schema.containers.empty()) {
        rendered += " :containers " + render_fixed_containers(schema.containers);
    }
    rendered += ')';
    return rendered;
}

auto render_mask_dimension(codegen::SoaMaskDimensionSchema const& dimension) -> std::string {
    return "(" + dimension.index_name + " " + quote(dimension.extent) + ")";
}

auto render_mask_dimensions(std::span<codegen::SoaMaskDimensionSchema const> const dimensions)
    -> std::string {
    auto rendered{std::string{}};
    if (!dimensions.empty()) {
        rendered = "(";
        for (std::size_t index{}; index < dimensions.size(); ++index) {
            rendered += index == 0 ? "" : " ";
            rendered += render_mask_dimension(dimensions[index]);
        }
        rendered += ')';
    }
    return rendered;
}

auto infer_single_mask_dimension_rename(
    std::span<Form const> const source_dimensions,
    std::span<codegen::SoaMaskDimensionSchema const> const current_dimensions)
    -> std::optional<std::pair<std::string_view, std::string_view>> {
    if (source_dimensions.size() != current_dimensions.size()) {
        return std::nullopt;
    }

    std::optional<std::pair<std::string_view, std::string_view>> renamed;
    for (std::size_t index{}; index < source_dimensions.size(); ++index) {
        if (!source_dimensions[index].is_list() || source_dimensions[index].children.size() != 2) {
            return std::nullopt;
        }
        auto const& source_name{source_dimensions[index].children[0].token.text};
        auto const& current{current_dimensions[index]};
        if (source_name == current.index_name) {
            continue;
        }
        if (renamed.has_value() ||
            !source_form_matches_rendered(source_dimensions[index].children[1],
                                          quote(current.extent))) {
            return std::nullopt;
        }
        renamed = std::pair{std::string_view{source_name}, std::string_view{current.index_name}};
    }

    if (!renamed.has_value()) {
        return std::nullopt;
    }
    auto const source_has_new_name{
        std::ranges::any_of(source_dimensions, [&](Form const& dimension) {
            return dimension.children[0].token.text == renamed->second;
        })};
    auto const current_has_old_name{
        std::ranges::find(
            current_dimensions, renamed->first, &codegen::SoaMaskDimensionSchema::index_name) !=
        current_dimensions.end()};
    return source_has_new_name || current_has_old_name ? std::nullopt : renamed;
}

auto patch_source_mask_dimensions(Form const& source_member,
                                  std::span<codegen::SoaMaskDimensionSchema const> const dimensions,
                                  std::string_view const original,
                                  std::vector<SourceReplacement>& replacements) -> bool {
    Form const* source_dimensions{};
    for (std::size_t index{4}; index + 1 < source_member.children.size(); ++index) {
        if (source_member.children[index].token.kind == codegen::sexpr::TokenKind::keyword &&
            source_member.children[index].token.text == "mask-dimensions") {
            source_dimensions = &source_member.children[index + 1];
            break;
        }
    }

    auto const rendered_dimensions{render_mask_dimensions(dimensions)};
    if (source_dimensions == nullptr || dimensions.empty()) {
        auto const properties{std::array<SourceProperty, 1>{std::pair{
            "mask-dimensions",
            rendered_dimensions.empty() ? std::nullopt : std::optional{rendered_dimensions}}}};
        return patch_source_properties(
            source_member, 3, properties, "      ", original, replacements);
    }
    if (!source_dimensions->is_list()) {
        return false;
    }
    for (auto const& dimension : source_dimensions->children) {
        if (!dimension.is_list() || dimension.children.size() != 2) {
            return false;
        }
    }

    auto const renamed_dimension{
        infer_single_mask_dimension_rename(source_dimensions->children, dimensions)};
    auto const rows_begin{source_dimensions->token.span.offset + 1};
    auto const multiline{
        original.substr(rows_begin, source_dimensions->closing.span.offset - rows_begin)
            .find('\n') != std::string_view::npos};
    if (!multiline) {
        if (source_dimensions->children.size() != dimensions.size()) {
            return patch_source_form(
                *source_dimensions, rendered_dimensions, original, replacements);
        }
        for (std::size_t index{}; index < dimensions.size(); ++index) {
            auto const& source_dimension{source_dimensions->children[index]};
            auto const& dimension{dimensions[index]};
            auto const same_name{source_dimension.children[0].token.text == dimension.index_name};
            auto const renamed{renamed_dimension.has_value() &&
                               renamed_dimension->first ==
                                   source_dimension.children[0].token.text &&
                               renamed_dimension->second == dimension.index_name};
            if (!same_name && !renamed) {
                return patch_source_form(
                    *source_dimensions, rendered_dimensions, original, replacements);
            }
            if ((renamed &&
                 !patch_source_form(
                     source_dimension.children[0], dimension.index_name, original, replacements)) ||
                !patch_source_form(source_dimension.children[1],
                                   quote(dimension.extent),
                                   original,
                                   replacements)) {
                return false;
            }
        }
        return true;
    }

    struct SourceDimension {
        Form const* form{};
        std::size_t begin{};
        std::size_t end{};
    };
    std::map<std::string_view, SourceDimension> source_by_name;
    auto row_begin{rows_begin};
    for (auto const& dimension : source_dimensions->children) {
        auto const row_end{
            source_form_line_end(dimension, original, source_dimensions->closing.span.offset)};
        if (!row_end.has_value() || row_begin > dimension.token.span.offset ||
            !source_by_name
                 .emplace(dimension.children[0].token.text,
                          SourceDimension{.form = &dimension, .begin = row_begin, .end = *row_end})
                 .second) {
            return false;
        }
        row_begin = *row_end;
    }

    auto rendered_rows{std::string{}};
    auto append_row = [&](std::string row) {
        auto const preceding_newline{rendered_rows.empty()
                                         ? rows_begin > 0 && original[rows_begin - 1] == '\n'
                                         : rendered_rows.back() == '\n'};
        if (!preceding_newline && (row.empty() || row.front() != '\n')) {
            rendered_rows += '\n';
        }
        rendered_rows += row;
    };
    for (auto const& dimension : dimensions) {
        auto const renamed{renamed_dimension.has_value() &&
                           renamed_dimension->second == dimension.index_name};
        auto const found{source_by_name.find(renamed ? renamed_dimension->first
                                                     : std::string_view{dimension.index_name})};
        if (found == source_by_name.end()) {
            append_row("        " + render_mask_dimension(dimension));
            continue;
        }

        std::vector<SourceReplacement> dimension_replacements;
        if ((renamed && !patch_source_form(found->second.form->children[0],
                                           dimension.index_name,
                                           original,
                                           dimension_replacements)) ||
            !patch_source_form(found->second.form->children[1],
                               quote(dimension.extent),
                               original,
                               dimension_replacements)) {
            return false;
        }
        auto rendered{apply_source_replacements_to_range(
            original, found->second.begin, found->second.end, std::move(dimension_replacements))};
        if (!rendered.has_value()) {
            return false;
        }
        append_row(std::move(*rendered));
    }
    replacements.push_back(
        {.begin = rows_begin, .end = row_begin, .text = std::move(rendered_rows)});
    return true;
}

enum class SourceListValueKind { atom, quoted_string };

auto render_source_list_value(std::string_view const value, SourceListValueKind const kind)
    -> std::string {
    return kind == SourceListValueKind::quoted_string ? quote(value) : std::string{value};
}

auto render_source_list_values(std::span<std::string const> const values,
                               SourceListValueKind const kind) -> std::string {
    auto rendered{std::string{"("}};
    for (std::size_t index{}; index < values.size(); ++index) {
        rendered += index == 0 ? "" : " ";
        rendered += render_source_list_value(values[index], kind);
    }
    rendered += ')';
    return rendered;
}

auto render_quoted_values(std::span<std::string const> const values) -> std::string {
    return render_source_list_values(values, SourceListValueKind::quoted_string);
}

auto infer_single_list_value_edit(std::span<Form const> const source_values,
                                  std::span<std::string const> const current_values)
    -> std::optional<std::pair<std::string_view, std::string_view>> {
    if (source_values.size() != current_values.size()) {
        return std::nullopt;
    }

    std::optional<std::pair<std::string_view, std::string_view>> edited;
    for (std::size_t index{}; index < source_values.size(); ++index) {
        auto const& source_value{source_values[index].token.text};
        auto const& current_value{current_values[index]};
        if (source_value == current_value) {
            continue;
        }
        if (edited.has_value()) {
            return std::nullopt;
        }
        edited = std::pair{std::string_view{source_value}, std::string_view{current_value}};
    }

    if (!edited.has_value()) {
        return std::nullopt;
    }
    auto const source_has_new_value{std::ranges::any_of(
        source_values, [&](Form const& value) { return value.token.text == edited->second; })};
    auto const current_has_old_value{std::ranges::find(current_values, edited->first) !=
                                     current_values.end()};
    return source_has_new_value || current_has_old_value ? std::nullopt : edited;
}

auto patch_source_scalar_list_property(Form const& source,
                                       std::size_t const positional_count,
                                       std::string_view const property_name,
                                       std::span<std::string const> const values,
                                       SourceListValueKind const value_kind,
                                       std::string_view const property_indentation,
                                       std::string_view const row_indentation,
                                       std::string_view const original,
                                       std::vector<SourceReplacement>& replacements) -> bool {
    Form const* source_values{};
    for (auto index{positional_count + 1}; index + 1 < source.children.size(); ++index) {
        if (source.children[index].token.kind == codegen::sexpr::TokenKind::keyword &&
            source.children[index].token.text == property_name) {
            source_values = &source.children[index + 1];
            break;
        }
    }

    auto const rendered_values{values.empty()
                                   ? std::optional<std::string>{}
                                   : std::optional{render_source_list_values(values, value_kind)}};
    if (source_values == nullptr || values.empty()) {
        auto const properties{
            std::array<SourceProperty, 1>{std::pair{property_name, rendered_values}}};
        return patch_source_properties(
            source, positional_count, properties, property_indentation, original, replacements);
    }
    auto const expected_token_kind{value_kind == SourceListValueKind::quoted_string
                                       ? codegen::sexpr::TokenKind::string
                                       : codegen::sexpr::TokenKind::atom};
    if (!source_values->is_list() ||
        std::ranges::any_of(source_values->children, [&](Form const& value) {
            return value.is_list() || value.token.kind != expected_token_kind;
        })) {
        return false;
    }

    std::set<std::string_view> unique_source_values;
    for (auto const& value : source_values->children) {
        if (!unique_source_values.insert(value.token.text).second) {
            return patch_source_form(*source_values, *rendered_values, original, replacements);
        }
    }
    std::set<std::string_view> unique_current_values;
    for (auto const& value : values) {
        if (!unique_current_values.insert(value).second) {
            return patch_source_form(*source_values, *rendered_values, original, replacements);
        }
    }

    auto const edited_value{infer_single_list_value_edit(source_values->children, values)};
    auto const rows_begin{source_values->token.span.offset + 1};
    auto const multiline{
        original.substr(rows_begin, source_values->closing.span.offset - rows_begin).find('\n') !=
        std::string_view::npos};
    if (!multiline) {
        if (source_values->children.size() != values.size()) {
            return patch_source_form(*source_values, *rendered_values, original, replacements);
        }
        for (std::size_t index{}; index < values.size(); ++index) {
            auto const& source_value{source_values->children[index]};
            auto const& current_value{values[index]};
            auto const same_value{source_value.token.text == current_value};
            auto const edited{edited_value.has_value() &&
                              edited_value->first == source_value.token.text &&
                              edited_value->second == current_value};
            if (!same_value && !edited) {
                return patch_source_form(*source_values, *rendered_values, original, replacements);
            }
            if (edited && !patch_source_form(source_value,
                                             render_source_list_value(current_value, value_kind),
                                             original,
                                             replacements)) {
                return false;
            }
        }
        return true;
    }

    struct SourceValue {
        Form const* form{};
        std::size_t begin{};
        std::size_t end{};
    };
    std::map<std::string_view, SourceValue> source_by_value;
    auto row_begin{rows_begin};
    for (auto const& value : source_values->children) {
        auto const row_end{
            source_form_line_end(value, original, source_values->closing.span.offset)};
        if (!row_end.has_value() || row_begin > value.token.span.offset ||
            !source_by_value
                 .emplace(value.token.text,
                          SourceValue{.form = &value, .begin = row_begin, .end = *row_end})
                 .second) {
            return patch_source_form(*source_values, *rendered_values, original, replacements);
        }
        row_begin = *row_end;
    }

    auto rendered_rows{std::string{}};
    auto append_row = [&](std::string row) {
        auto const preceding_newline{rendered_rows.empty()
                                         ? rows_begin > 0 && original[rows_begin - 1] == '\n'
                                         : rendered_rows.back() == '\n'};
        if (!preceding_newline && (row.empty() || row.front() != '\n')) {
            rendered_rows += '\n';
        }
        rendered_rows += row;
    };
    for (auto const& value : values) {
        auto const edited{edited_value.has_value() && edited_value->second == value};
        auto const found{
            source_by_value.find(edited ? edited_value->first : std::string_view{value})};
        if (found == source_by_value.end()) {
            append_row(std::string{row_indentation} + render_source_list_value(value, value_kind));
            continue;
        }

        std::vector<SourceReplacement> value_replacements;
        if (edited && !patch_source_form(*found->second.form,
                                         render_source_list_value(value, value_kind),
                                         original,
                                         value_replacements)) {
            return false;
        }
        auto rendered{apply_source_replacements_to_range(
            original, found->second.begin, found->second.end, std::move(value_replacements))};
        if (!rendered.has_value()) {
            return false;
        }
        append_row(std::move(*rendered));
    }
    replacements.push_back(
        {.begin = rows_begin, .end = row_begin, .text = std::move(rendered_rows)});
    return true;
}

auto patch_source_quoted_list_property(Form const& source,
                                       std::size_t const positional_count,
                                       std::string_view const property_name,
                                       std::span<std::string const> const values,
                                       std::string_view const property_indentation,
                                       std::string_view const row_indentation,
                                       std::string_view const original,
                                       std::vector<SourceReplacement>& replacements) -> bool {
    return patch_source_scalar_list_property(source,
                                             positional_count,
                                             property_name,
                                             values,
                                             SourceListValueKind::quoted_string,
                                             property_indentation,
                                             row_indentation,
                                             original,
                                             replacements);
}

auto patch_source_atom_list_property(Form const& source,
                                     std::size_t const positional_count,
                                     std::string_view const property_name,
                                     std::span<std::string const> const values,
                                     std::string_view const property_indentation,
                                     std::string_view const row_indentation,
                                     std::string_view const original,
                                     std::vector<SourceReplacement>& replacements) -> bool {
    return patch_source_scalar_list_property(source,
                                             positional_count,
                                             property_name,
                                             values,
                                             SourceListValueKind::atom,
                                             property_indentation,
                                             row_indentation,
                                             original,
                                             replacements);
}

auto infer_single_fixed_container_rename(std::span<Form const> const source_containers,
                                         std::span<std::string const> const current_containers)
    -> std::optional<std::pair<std::string_view, std::string_view>> {
    if (source_containers.size() != current_containers.size()) {
        return std::nullopt;
    }

    std::optional<std::pair<std::string_view, std::string_view>> renamed;
    for (std::size_t index{}; index < source_containers.size(); ++index) {
        auto const& source_name{source_containers[index].token.text};
        auto const& current_name{current_containers[index]};
        if (source_name == current_name) {
            continue;
        }
        if (renamed.has_value()) {
            return std::nullopt;
        }
        renamed = std::pair{std::string_view{source_name}, std::string_view{current_name}};
    }

    if (!renamed.has_value()) {
        return std::nullopt;
    }
    auto const source_has_new_name{
        std::ranges::any_of(source_containers, [&](Form const& container) {
            return container.token.text == renamed->second;
        })};
    auto const current_has_old_name{std::ranges::find(current_containers, renamed->first) !=
                                    current_containers.end()};
    return source_has_new_name || current_has_old_name ? std::nullopt : renamed;
}

auto patch_source_fixed_soa(codegen::FixedSoaSchema const& schema,
                            Form const& source,
                            std::string_view const original,
                            std::vector<SourceReplacement>& replacements) -> bool {
    if (source.children.size() < 2 ||
        !patch_source_form(source.children[1], schema.storage_name, original, replacements)) {
        return false;
    }

    Form const* source_containers{};
    for (std::size_t index{2}; index + 1 < source.children.size(); ++index) {
        if (source.children[index].token.kind == codegen::sexpr::TokenKind::keyword &&
            source.children[index].token.text == "containers") {
            source_containers = &source.children[index + 1];
            break;
        }
    }

    auto const rendered_containers{schema.containers.empty()
                                       ? std::optional<std::string>{}
                                       : std::optional{render_fixed_containers(schema.containers)}};
    if (source_containers == nullptr || schema.containers.empty()) {
        auto const properties{
            std::array<SourceProperty, 1>{std::pair{"containers", rendered_containers}}};
        return patch_source_properties(source, 1, properties, "      ", original, replacements);
    }
    if (!source_containers->is_list()) {
        return false;
    }

    auto const renamed_container{
        infer_single_fixed_container_rename(source_containers->children, schema.containers)};
    auto const rows_begin{source_containers->token.span.offset + 1};
    auto const multiline{
        original.substr(rows_begin, source_containers->closing.span.offset - rows_begin)
            .find('\n') != std::string_view::npos};
    if (!multiline) {
        if (renamed_container.has_value()) {
            for (auto const& container : source_containers->children) {
                if (container.token.text == renamed_container->first) {
                    return patch_source_form(
                        container, std::string{renamed_container->second}, original, replacements);
                }
            }
        }
        return patch_source_form(*source_containers, *rendered_containers, original, replacements);
    }

    struct SourceContainer {
        Form const* form{};
        std::size_t begin{};
        std::size_t end{};
    };
    std::map<std::string_view, SourceContainer> source_by_name;
    auto row_begin{rows_begin};
    for (auto const& container : source_containers->children) {
        auto const row_end{
            source_form_line_end(container, original, source_containers->closing.span.offset)};
        if (!row_end.has_value() || row_begin > container.token.span.offset ||
            !source_by_name
                 .emplace(container.token.text,
                          SourceContainer{.form = &container, .begin = row_begin, .end = *row_end})
                 .second) {
            return false;
        }
        row_begin = *row_end;
    }

    auto rendered_rows{std::string{}};
    auto append_row = [&](std::string row) {
        auto const preceding_newline{rendered_rows.empty()
                                         ? rows_begin > 0 && original[rows_begin - 1] == '\n'
                                         : rendered_rows.back() == '\n'};
        if (!preceding_newline && (row.empty() || row.front() != '\n')) {
            rendered_rows += '\n';
        }
        rendered_rows += row;
    };
    for (auto const& container : schema.containers) {
        auto const renamed{renamed_container.has_value() && renamed_container->second == container};
        auto const found{
            source_by_name.find(renamed ? renamed_container->first : std::string_view{container})};
        if (found == source_by_name.end()) {
            append_row("        " + container);
            continue;
        }

        std::vector<SourceReplacement> container_replacements;
        if (renamed &&
            !patch_source_form(*found->second.form, container, original, container_replacements)) {
            return false;
        }
        auto rendered{apply_source_replacements_to_range(
            original, found->second.begin, found->second.end, std::move(container_replacements))};
        if (!rendered.has_value()) {
            return false;
        }
        append_row(std::move(*rendered));
    }
    replacements.push_back(
        {.begin = rows_begin, .end = row_begin, .text = std::move(rendered_rows)});
    return true;
}

auto render_single_allocation(codegen::SoaSchema const& schema) -> std::string {
    std::ostringstream rendered;
    rendered << "(single-allocation " << *schema.single_allocation;
    for (auto const& variant : schema.single_allocation_variants) {
        rendered << "\n      " << render_single_allocation_variant(variant);
    }
    rendered << ')';
    return std::move(rendered).str();
}

auto patch_source_single_allocation(codegen::SoaSchema const& schema,
                                    Form const& source,
                                    std::string_view const original,
                                    std::vector<SourceReplacement>& replacements) -> bool {
    if (!schema.single_allocation.has_value() || source.children.size() < 2 ||
        !patch_source_form(source.children[1], *schema.single_allocation, original, replacements)) {
        return false;
    }

    std::vector<Form const*> variants;
    for (auto const& child : source.children) {
        if (child.head() == "variant") {
            if (child.children.size() < 3) {
                return false;
            }
            variants.push_back(&child);
        }
    }

    if (variants.empty()) {
        if (schema.single_allocation_variants.empty()) {
            return true;
        }

        auto rendered{std::string{}};
        for (auto const& variant : schema.single_allocation_variants) {
            rendered += "\n      " + render_single_allocation_variant(variant);
        }
        replacements.push_back({.begin = source.closing.span.offset,
                                .end = source.closing.span.offset,
                                .text = std::move(rendered)});
        return true;
    }

    auto const first_variant_offset{variants.front()->token.span.offset};
    auto variants_begin{std::size_t{}};
    for (auto const& child : source.children) {
        if (child.token.span.offset >= first_variant_offset) {
            continue;
        }
        auto const child_end{source_form_line_end(child, original, first_variant_offset)};
        if (!child_end.has_value()) {
            return false;
        }
        variants_begin = (std::max)(variants_begin, *child_end);
    }
    if (variants_begin > first_variant_offset) {
        return false;
    }

    struct SourceVariant {
        Form const* form{};
        std::size_t begin{};
        std::size_t end{};
    };
    std::map<std::string_view, SourceVariant> source_by_name;
    auto row_begin{variants_begin};
    for (auto const* variant : variants) {
        auto const row_end{source_form_line_end(*variant, original, source.closing.span.offset)};
        if (!row_end.has_value() || row_begin > variant->token.span.offset ||
            *row_end < variant->closing.span.offset + 1 ||
            !source_by_name
                 .emplace(variant->children[1].token.text,
                          SourceVariant{.form = variant, .begin = row_begin, .end = *row_end})
                 .second) {
            return false;
        }
        row_begin = *row_end;
    }

    auto const variants_end{row_begin};
    auto const renamed_variant{infer_single_positional_rename<codegen::SingleAllocationVariant>(
        variants,
        schema.single_allocation_variants,
        [](Form const& source_variant, codegen::SingleAllocationVariant const& variant) {
            return source_variant.children.size() >= 3 &&
                   source_form_matches_rendered(source_variant.children[2],
                                                render_type_ref(variant.allocator));
        })};
    auto rendered_variants{std::string{}};
    auto append_row = [&](std::string row) {
        auto const preceding_newline{
            rendered_variants.empty() ? variants_begin > 0 && original[variants_begin - 1] == '\n'
                                      : rendered_variants.back() == '\n'};
        if (!preceding_newline && (row.empty() || row.front() != '\n')) {
            rendered_variants += '\n';
        }
        rendered_variants += row;
    };

    for (auto const& variant : schema.single_allocation_variants) {
        auto const renamed{renamed_variant.has_value() && renamed_variant->second == variant.name};
        auto const found{
            source_by_name.find(renamed ? renamed_variant->first : std::string_view{variant.name})};
        if (found == source_by_name.end()) {
            append_row("      " + render_single_allocation_variant(variant));
            continue;
        }

        std::vector<SourceReplacement> variant_replacements;
        if ((renamed &&
             !patch_source_form(
                 found->second.form->children[1], variant.name, original, variant_replacements)) ||
            !patch_source_form(found->second.form->children[2],
                               render_type_ref(variant.allocator),
                               original,
                               variant_replacements)) {
            return false;
        }
        auto rendered{apply_source_replacements_to_range(
            original, found->second.begin, found->second.end, std::move(variant_replacements))};
        if (!rendered.has_value()) {
            return false;
        }
        append_row(std::move(*rendered));
    }
    replacements.push_back(
        {.begin = variants_begin, .end = variants_end, .text = std::move(rendered_variants)});
    return true;
}

auto try_render_source_preserved_soa(codegen::SoaSchema const& schema,
                                     std::string_view const original)
    -> std::optional<std::string> {
    if (!schema.mutable_view_functions.empty() || schema.array_allocator.has_value() ||
        schema.single_allocation_allocator.has_value()) {
        return std::nullopt;
    }
    auto parsed{parse_owned_source_declaration(original, "struct")};
    if (!parsed.has_value()) {
        return std::nullopt;
    }

    std::vector<SourceReplacement> replacements;
    if (!patch_source_form(parsed->children[1], schema.name, original, replacements)) {
        return std::nullopt;
    }

    std::vector<Form const*> members;
    std::vector<Form const*> functions;
    Form const* fixed{};
    Form const* single_allocation{};
    auto members_limit{parsed->closing.span.offset};
    auto functions_limit{parsed->closing.span.offset};
    auto found_member{false};
    auto member_region_ended{false};
    auto found_function{false};
    auto function_region_ended{false};
    for (auto const& child : parsed->children) {
        if (child.head() == "member") {
            if (member_region_ended || child.children.size() < 4) {
                return std::nullopt;
            }
            found_member = true;
            members.push_back(&child);
        } else {
            if (found_member && !member_region_ended) {
                member_region_ended = true;
                members_limit = child.token.span.offset;
            }
            if (child.head() == "function") {
                if (function_region_ended || child.children.size() < 3) {
                    return std::nullopt;
                }
                found_function = true;
                functions.push_back(&child);
            } else {
                if (found_function && !function_region_ended) {
                    function_region_ended = true;
                    functions_limit = child.token.span.offset;
                }
                if (child.head() == "fixed") {
                    fixed = &child;
                } else if (child.head() == "single-allocation") {
                    single_allocation = &child;
                }
            }
        }
    }
    std::string trailing_forms;
    if (fixed != nullptr && schema.fixed.has_value()) {
        if (!patch_source_fixed_soa(*schema.fixed, *fixed, original, replacements)) {
            return std::nullopt;
        }
    } else if (fixed != nullptr) {
        replacements.push_back(
            {.begin = fixed->token.span.offset, .end = fixed->closing.span.offset + 1, .text = {}});
    } else if (schema.fixed.has_value()) {
        auto rendered{render_fixed_soa(*schema.fixed)};
        if (single_allocation != nullptr) {
            replacements.push_back({.begin = single_allocation->token.span.offset,
                                    .end = single_allocation->token.span.offset,
                                    .text = std::move(rendered) + "\n    "});
        } else {
            trailing_forms += "\n    " + std::move(rendered);
        }
    }
    if (single_allocation != nullptr && schema.single_allocation.has_value()) {
        if (!patch_source_single_allocation(schema, *single_allocation, original, replacements)) {
            return std::nullopt;
        }
    } else if (single_allocation != nullptr) {
        replacements.push_back({.begin = single_allocation->token.span.offset,
                                .end = single_allocation->closing.span.offset + 1,
                                .text = {}});
    } else if (schema.single_allocation.has_value()) {
        auto rendered{render_single_allocation(schema)};
        trailing_forms += "\n    " + std::move(rendered);
    }
    if (!trailing_forms.empty()) {
        replacements.push_back({.begin = parsed->closing.span.offset,
                                .end = parsed->closing.span.offset,
                                .text = std::move(trailing_forms)});
    }

    std::vector<std::string> operation_names;
    operation_names.reserve(schema.operations.size());
    for (auto const operation : schema.operations) {
        operation_names.emplace_back(storage_operation_name(operation));
    }
    auto source_operations_use_all{false};
    for (std::size_t index{2}; index + 1 < parsed->children.size(); ++index) {
        if (parsed->children[index].token.kind != codegen::sexpr::TokenKind::keyword ||
            parsed->children[index].token.text != "operations") {
            continue;
        }
        auto const& value{parsed->children[index + 1]};
        source_operations_use_all =
            value.is_list() && value.children.size() == 1 &&
            value.children.front().token.kind == codegen::sexpr::TokenKind::atom &&
            value.children.front().token.text == "all";
        break;
    }
    auto const preserve_all_operations{source_operations_use_all &&
                                       schema.operations == codegen::all_storage_operations()};
    auto const properties{std::array<SourceProperty, 8>{
        std::pair{"view-name", schema.view_name},
        std::pair{"const-view-name", schema.const_view_name},
        std::pair{"export-specifier", schema.export_specifier},
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
    if (!patch_source_properties(*parsed, 1, properties, "    ", original, replacements) ||
        !patch_source_atom_list_property(*parsed,
                                         1,
                                         "vector-components",
                                         schema.vector_components,
                                         "    ",
                                         "      ",
                                         original,
                                         replacements) ||
        (!preserve_all_operations && !patch_source_atom_list_property(*parsed,
                                                                      1,
                                                                      "operations",
                                                                      operation_names,
                                                                      "    ",
                                                                      "      ",
                                                                      original,
                                                                      replacements)) ||
        !patch_source_quoted_list_property(*parsed,
                                           1,
                                           "using-declarations",
                                           schema.using_declarations,
                                           "    ",
                                           "      ",
                                           original,
                                           replacements)) {
        return std::nullopt;
    }

    if (members.empty()) {
        return schema.members.empty() ? apply_source_replacements(original, std::move(replacements))
                                      : std::nullopt;
    }

    auto const first_member_offset{members.front()->token.span.offset};
    auto members_begin{std::size_t{}};
    for (auto const& child : parsed->children) {
        if (child.token.span.offset >= first_member_offset) {
            continue;
        }
        auto const child_end{source_form_line_end(child, original, first_member_offset)};
        if (!child_end.has_value()) {
            return std::nullopt;
        }
        members_begin = (std::max)(members_begin, *child_end);
    }
    if (members_begin > first_member_offset || members_limit < first_member_offset) {
        return std::nullopt;
    }

    struct SourceMember {
        Form const* form{};
        std::size_t begin{};
        std::size_t end{};
    };
    std::map<std::string_view, SourceMember> source_by_name;
    auto row_begin{members_begin};
    for (auto const* member : members) {
        auto const row_end{source_form_line_end(*member, original, members_limit)};
        if (!row_end.has_value() || row_begin > member->token.span.offset ||
            *row_end < member->closing.span.offset + 1) {
            return std::nullopt;
        }
        if (!source_by_name
                 .emplace(member->children[1].token.text,
                          SourceMember{.form = member, .begin = row_begin, .end = *row_end})
                 .second) {
            return std::nullopt;
        }
        row_begin = *row_end;
    }

    auto const members_end{row_begin};
    auto const renamed_member{infer_single_positional_rename<codegen::SoaMemberSchema>(
        members, schema.members, [&](Form const& source, codegen::SoaMemberSchema const& member) {
            auto const dimensions{render_mask_dimensions(member.mask_dimensions)};
            if (source.children.size() < 4 ||
                !source_form_matches_rendered(source.children[2],
                                              soa_member_kind_name(member.kind)) ||
                !source_form_matches_rendered(source.children[3], render_type_ref(member.type)) ||
                !source_property_matches(source, 3, "fixed-schema", member.fixed_schema) ||
                !source_property_matches(source, 3, "nested-schema", member.nested_schema) ||
                !source_property_matches(source,
                                         3,
                                         "mask-field",
                                         member.mask_field ? std::optional<std::string>{"true"}
                                                           : std::nullopt) ||
                !source_property_matches(source,
                                         3,
                                         "mask-dimensions",
                                         dimensions.empty() ? std::nullopt
                                                            : std::optional{dimensions})) {
                return false;
            }

            Form const* source_relation{};
            for (auto const& nested : source.children) {
                if (nested.head() != "relation") {
                    continue;
                }
                if (source_relation != nullptr) {
                    return false;
                }
                source_relation = &nested;
            }
            return member.relationship.has_value()
                     ? source_relation != nullptr &&
                           source_form_matches_rendered(
                               *source_relation, render_semantic_relation(*member.relationship))
                     : source_relation == nullptr;
        })};
    auto rendered_members{std::string{}};
    auto append_row = [&](std::string row) {
        auto const preceding_newline{rendered_members.empty()
                                         ? members_begin > 0 && original[members_begin - 1] == '\n'
                                         : rendered_members.back() == '\n'};
        if (!preceding_newline && (row.empty() || row.front() != '\n')) {
            rendered_members += '\n';
        }
        rendered_members += row;
    };

    for (auto const& member : schema.members) {
        auto const renamed{renamed_member.has_value() && renamed_member->second == member.name};
        auto const found{
            source_by_name.find(renamed ? renamed_member->first : std::string_view{member.name})};
        if (found == source_by_name.end()) {
            append_row("    " + render_soa_member(member));
            continue;
        }

        std::vector<SourceReplacement> member_replacements;
        if (renamed &&
            !patch_source_form(
                found->second.form->children[1], member.name, original, member_replacements)) {
            return std::nullopt;
        }
        if (!patch_source_form(found->second.form->children[2],
                               std::string{soa_member_kind_name(member.kind)},
                               original,
                               member_replacements) ||
            !patch_source_form(found->second.form->children[3],
                               render_type_ref(member.type),
                               original,
                               member_replacements)) {
            return std::nullopt;
        }
        auto const member_properties{std::array<SourceProperty, 3>{
            std::pair{"fixed-schema", member.fixed_schema},
            std::pair{"nested-schema", member.nested_schema},
            std::pair{"mask-field",
                      member.mask_field ? std::optional<std::string>{"true"} : std::nullopt}}};
        if (!patch_source_properties(*found->second.form,
                                     3,
                                     member_properties,
                                     "      ",
                                     original,
                                     member_replacements) ||
            !patch_source_mask_dimensions(
                *found->second.form, member.mask_dimensions, original, member_replacements)) {
            return std::nullopt;
        }
        Form const* source_relation{};
        for (auto const& nested : found->second.form->children) {
            if (nested.head() != "relation") {
                continue;
            }
            if (source_relation != nullptr) {
                return std::nullopt;
            }
            source_relation = &nested;
        }
        if (!patch_source_optional_relation(*found->second.form,
                                            source_relation,
                                            member.relationship,
                                            "      ",
                                            "        ",
                                            original,
                                            member_replacements)) {
            return std::nullopt;
        }
        auto rendered{apply_source_replacements_to_range(
            original, found->second.begin, found->second.end, std::move(member_replacements))};
        if (!rendered.has_value()) {
            return std::nullopt;
        }
        append_row(std::move(*rendered));
    }
    replacements.push_back(
        {.begin = members_begin, .end = members_end, .text = std::move(rendered_members)});

    auto source_property = [](Form const& form, std::string_view const name) -> Form const* {
        for (std::size_t index{3}; index + 1 < form.children.size(); ++index) {
            if (form.children[index].token.kind == codegen::sexpr::TokenKind::keyword &&
                form.children[index].token.text == name) {
                return &form.children[index + 1];
            }
        }
        return nullptr;
    };
    auto property_matches = [&](Form const& form,
                                std::string_view const name,
                                std::optional<std::string> const& expected) {
        auto const* value{source_property(form, name)};
        return expected.has_value()
                 ? value != nullptr && source_form_matches_rendered(*value, *expected)
                 : value == nullptr;
    };
    auto rendered_function_properties = [&](codegen::FunctionSchema const& function) {
        auto const body{function.body_lines.empty()
                            ? std::nullopt
                            : std::optional{render_quoted_values(function.body_lines)}};
        auto const dependencies{function.dependencies.empty()
                                    ? std::nullopt
                                    : std::optional{render_quoted_values(function.dependencies)}};
        return std::array<SourceProperty, 10>{
            std::pair{"body", body},
            std::pair{"dependencies", dependencies},
            std::pair{"trailing-return-type",
                      function.trailing_return_type.has_value()
                          ? std::optional{render_type_ref(*function.trailing_return_type)}
                          : std::nullopt},
            std::pair{"const",
                      function.is_const ? std::optional<std::string>{"true"} : std::nullopt},
            std::pair{"noexcept",
                      function.is_noexcept ? std::optional<std::string>{"true"} : std::nullopt},
            std::pair{"static",
                      function.is_static ? std::optional<std::string>{"true"} : std::nullopt},
            std::pair{"inline",
                      function.is_inline ? std::optional<std::string>{"true"} : std::nullopt},
            std::pair{"definition-in-source",
                      function.definition_in_source ? std::optional<std::string>{"true"}
                                                    : std::nullopt},
            std::pair{"template-parameters",
                      function.template_parameters.has_value()
                          ? std::optional{quote(*function.template_parameters)}
                          : std::nullopt},
            std::pair{"requires",
                      function.requires_clause.has_value()
                          ? std::optional{quote(*function.requires_clause)}
                          : std::nullopt}};
    };
    auto body_property_matches = [&](Form const& source, codegen::FunctionSchema const& function) {
        auto const* source_body{source_property(source, "body")};
        if (source_body == nullptr ||
            source_body->token.kind != codegen::sexpr::TokenKind::raw_literal) {
            auto const body{function.body_lines.empty()
                                ? std::nullopt
                                : std::optional{render_quoted_values(function.body_lines)}};
            return property_matches(source, "body", body);
        }
        return source_body->token.tag == "cpp" &&
               (function.body_lines.empty()
                    ? source_body->token.text.empty()
                    : function.body_lines.size() == 1 &&
                          function.body_lines.front() == source_body->token.text);
    };
    auto parameter_fields_match_except_name = [&](Form const& source,
                                                  codegen::ParameterSchema const& parameter) {
        auto const expected_default{parameter.default_value.has_value()
                                        ? std::optional{quote(*parameter.default_value)}
                                        : std::nullopt};
        return source.children.size() >= 3 &&
               source_form_matches_rendered(source.children[2], render_type_ref(parameter.type)) &&
               property_matches(source, "default", expected_default);
    };
    auto function_fields_match_except_name = [&](Form const& source,
                                                 codegen::FunctionSchema const& function) {
        if (source.children.size() < 3 ||
            !source_form_matches_rendered(source.children[2],
                                          render_type_ref(function.return_type))) {
            return false;
        }
        auto const properties{rendered_function_properties(function)};
        auto properties_match{body_property_matches(source, function)};
        for (std::size_t index{1}; index < properties.size(); ++index) {
            properties_match =
                properties_match &&
                property_matches(source, properties[index].first, properties[index].second);
        }
        if (!properties_match) {
            return false;
        }

        std::vector<Form const*> source_parameters;
        for (auto const& child : source.children) {
            if (child.head() == "parameter") {
                source_parameters.push_back(&child);
            }
        }
        if (source_parameters.size() != function.parameters.size()) {
            return false;
        }
        auto renamed_parameters{std::size_t{}};
        for (std::size_t index{}; index < source_parameters.size(); ++index) {
            if (source_parameters[index]->children.size() < 2 ||
                !parameter_fields_match_except_name(*source_parameters[index],
                                                    function.parameters[index])) {
                return false;
            }
            if (source_parameters[index]->children[1].token.text !=
                function.parameters[index].name) {
                ++renamed_parameters;
            }
        }
        return renamed_parameters <= 1;
    };

    auto render_parameter_row = [](codegen::ParameterSchema const& parameter) {
        std::ostringstream rendered;
        rendered << "      (parameter " << parameter.name << ' ' << render_type_ref(parameter.type);
        if (parameter.default_value.has_value()) {
            rendered << " :default " << quote(*parameter.default_value);
        }
        rendered << ')';
        return std::move(rendered).str();
    };
    auto patch_function_parameters = [&](Form const& source,
                                         codegen::FunctionSchema const& function,
                                         std::vector<SourceReplacement>& function_replacements) {
        std::vector<Form const*> source_parameters;
        for (auto const& child : source.children) {
            if (child.head() == "parameter") {
                if (child.children.size() < 3) {
                    return false;
                }
                source_parameters.push_back(&child);
            }
        }

        if (source_parameters.empty()) {
            if (function.parameters.empty()) {
                return true;
            }
            std::string additions;
            for (auto const& parameter : function.parameters) {
                additions += "\n" + render_parameter_row(parameter);
            }
            auto const existing_insertion{
                std::ranges::find_if(function_replacements, [&](auto const& replacement) {
                    return replacement.begin == source.closing.span.offset &&
                           replacement.end == source.closing.span.offset;
                })};
            if (existing_insertion == function_replacements.end()) {
                function_replacements.push_back({.begin = source.closing.span.offset,
                                                 .end = source.closing.span.offset,
                                                 .text = std::move(additions)});
            } else {
                existing_insertion->text += additions;
            }
            return true;
        }

        auto const first_parameter_offset{source_parameters.front()->token.span.offset};
        auto parameters_begin{source.token.span.offset};
        for (auto const& child : source.children) {
            if (child.token.span.offset >= first_parameter_offset) {
                continue;
            }
            auto const child_end{source_form_line_end(child, original, first_parameter_offset)};
            if (!child_end.has_value()) {
                return false;
            }
            parameters_begin = (std::max)(parameters_begin, *child_end);
        }
        if (parameters_begin > first_parameter_offset) {
            return false;
        }

        struct SourceParameter {
            Form const* form{};
            std::size_t begin{};
            std::size_t end{};
        };
        std::map<std::string_view, SourceParameter> source_by_name;
        auto row_begin{parameters_begin};
        for (auto const* parameter : source_parameters) {
            auto const row_end{
                source_form_line_end(*parameter, original, source.closing.span.offset)};
            if (!row_end.has_value() || row_begin > parameter->token.span.offset ||
                *row_end < parameter->closing.span.offset + 1 ||
                !source_by_name
                     .emplace(
                         parameter->children[1].token.text,
                         SourceParameter{.form = parameter, .begin = row_begin, .end = *row_end})
                     .second) {
                return false;
            }
            row_begin = *row_end;
        }

        auto const parameters_end{row_begin};
        std::set<std::string> schema_names;
        std::vector<SourceParameter const*> source_for_parameter(function.parameters.size());
        std::set<std::string_view> matched_source_names;
        for (std::size_t index{}; index < function.parameters.size(); ++index) {
            auto const& parameter{function.parameters[index]};
            if (!schema_names.insert(parameter.name).second) {
                return false;
            }
            auto const found{source_by_name.find(parameter.name)};
            if (found != source_by_name.end()) {
                source_for_parameter[index] = &found->second;
                matched_source_names.insert(found->first);
            }
        }
        std::vector<std::size_t> unmatched_schema_indices;
        for (std::size_t index{}; index < source_for_parameter.size(); ++index) {
            if (source_for_parameter[index] == nullptr) {
                unmatched_schema_indices.push_back(index);
            }
        }
        std::vector<SourceParameter const*> unmatched_source_parameters;
        for (auto const& [name, source_parameter] : source_by_name) {
            if (!matched_source_names.contains(name)) {
                unmatched_source_parameters.push_back(&source_parameter);
            }
        }
        if (function.parameters.size() == source_parameters.size() &&
            unmatched_schema_indices.size() == 1 && unmatched_source_parameters.size() == 1) {
            auto const index{unmatched_schema_indices.front()};
            if (parameter_fields_match_except_name(*unmatched_source_parameters.front()->form,
                                                   function.parameters[index])) {
                source_for_parameter[index] = unmatched_source_parameters.front();
            }
        }

        std::string rendered_parameters;
        auto append_parameter = [&](std::string row) {
            auto const preceding_newline{rendered_parameters.empty()
                                             ? parameters_begin > 0 &&
                                                   original[parameters_begin - 1] == '\n'
                                             : rendered_parameters.back() == '\n'};
            if (!preceding_newline && (row.empty() || row.front() != '\n')) {
                rendered_parameters += '\n';
            }
            rendered_parameters += row;
        };
        for (std::size_t index{}; index < function.parameters.size(); ++index) {
            auto const& parameter{function.parameters[index]};
            auto const* found{source_for_parameter[index]};
            if (found == nullptr) {
                append_parameter(render_parameter_row(parameter));
                continue;
            }

            std::vector<SourceReplacement> parameter_replacements;
            if (!patch_source_form(
                    found->form->children[1], parameter.name, original, parameter_replacements) ||
                !patch_source_form(found->form->children[2],
                                   render_type_ref(parameter.type),
                                   original,
                                   parameter_replacements)) {
                return false;
            }
            auto const parameter_properties{std::array<SourceProperty, 1>{std::pair{
                "default",
                parameter.default_value.has_value() ? std::optional{quote(*parameter.default_value)}
                                                    : std::nullopt}}};
            if (!patch_source_properties(*found->form,
                                         2,
                                         parameter_properties,
                                         "        ",
                                         original,
                                         parameter_replacements)) {
                return false;
            }
            auto rendered{apply_source_replacements_to_range(
                original, found->begin, found->end, std::move(parameter_replacements))};
            if (!rendered.has_value()) {
                return false;
            }
            append_parameter(std::move(*rendered));
        }
        function_replacements.push_back({.begin = parameters_begin,
                                         .end = parameters_end,
                                         .text = std::move(rendered_parameters)});
        return true;
    };

    auto render_function_row = [](codegen::FunctionSchema const& function) {
        std::ostringstream rendered;
        render_function(rendered, function, "    ");
        return std::move(rendered).str();
    };
    if (functions.empty()) {
        if (!schema.functions.empty()) {
            std::string rendered_functions;
            for (auto const& function : schema.functions) {
                rendered_functions += "\n" + render_function_row(function);
            }
            replacements.push_back(
                {.begin = members_end, .end = members_end, .text = std::move(rendered_functions)});
        }
    } else {
        auto const functions_begin{members_end};
        if (functions_begin > functions.front()->token.span.offset ||
            functions_limit < functions.front()->token.span.offset) {
            return std::nullopt;
        }

        struct SourceFunction {
            Form const* form{};
            std::size_t begin{};
            std::size_t end{};
        };
        std::map<std::string_view, SourceFunction> source_by_name;
        auto row_begin{functions_begin};
        for (auto const* function : functions) {
            auto const row_end{source_form_line_end(*function, original, functions_limit)};
            if (!row_end.has_value() || row_begin > function->token.span.offset ||
                *row_end < function->closing.span.offset + 1 ||
                !source_by_name
                     .emplace(function->children[1].token.text,
                              SourceFunction{.form = function, .begin = row_begin, .end = *row_end})
                     .second) {
                return std::nullopt;
            }
            row_begin = *row_end;
        }

        auto const functions_end{row_begin};
        std::set<std::string> schema_names;
        std::vector<SourceFunction const*> source_for_function(schema.functions.size());
        std::set<std::string_view> matched_source_names;
        for (std::size_t index{}; index < schema.functions.size(); ++index) {
            auto const& function{schema.functions[index]};
            if (!schema_names.insert(function.name).second) {
                return std::nullopt;
            }
            auto const found{source_by_name.find(function.name)};
            if (found != source_by_name.end()) {
                source_for_function[index] = &found->second;
                matched_source_names.insert(found->first);
            }
        }
        std::vector<std::size_t> unmatched_schema_indices;
        for (std::size_t index{}; index < source_for_function.size(); ++index) {
            if (source_for_function[index] == nullptr) {
                unmatched_schema_indices.push_back(index);
            }
        }
        std::vector<SourceFunction const*> unmatched_source_functions;
        for (auto const& [name, source_function] : source_by_name) {
            if (!matched_source_names.contains(name)) {
                unmatched_source_functions.push_back(&source_function);
            }
        }
        if (schema.functions.size() == functions.size() && unmatched_schema_indices.size() == 1 &&
            unmatched_source_functions.size() == 1) {
            auto const index{unmatched_schema_indices.front()};
            if (function_fields_match_except_name(*unmatched_source_functions.front()->form,
                                                  schema.functions[index])) {
                source_for_function[index] = unmatched_source_functions.front();
            }
        }

        std::string rendered_functions;
        auto append_function = [&](std::string row) {
            auto const preceding_newline{rendered_functions.empty()
                                             ? functions_begin > 0 &&
                                                   original[functions_begin - 1] == '\n'
                                             : rendered_functions.back() == '\n'};
            if (!preceding_newline && (row.empty() || row.front() != '\n')) {
                rendered_functions += '\n';
            }
            rendered_functions += row;
        };
        for (std::size_t index{}; index < schema.functions.size(); ++index) {
            auto const& function{schema.functions[index]};
            auto const* found{source_for_function[index]};
            if (found == nullptr) {
                append_function(render_function_row(function));
                continue;
            }
            std::vector<SourceReplacement> function_replacements;
            if (!patch_source_form(
                    found->form->children[1], function.name, original, function_replacements) ||
                !patch_source_form(found->form->children[2],
                                   render_type_ref(function.return_type),
                                   original,
                                   function_replacements)) {
                return std::nullopt;
            }
            auto function_properties{rendered_function_properties(function)};
            auto const* source_body{source_property(*found->form, "body")};
            auto const* source_dependencies{source_property(*found->form, "dependencies")};
            auto const patch_body_rows{source_body != nullptr && source_body->is_list() &&
                                       !function.body_lines.empty()};
            auto const patch_dependency_rows{source_dependencies != nullptr &&
                                             source_dependencies->is_list() &&
                                             !function.dependencies.empty()};
            auto retain_source_property = [&](Form const& property,
                                              std::optional<std::string>& rendered) {
                auto const property_end{source_form_end(property, original)};
                if (!property_end.has_value()) {
                    return false;
                }
                rendered = std::string{original.substr(property.token.span.offset,
                                                       *property_end - property.token.span.offset)};
                return true;
            };
            if (source_body != nullptr &&
                source_body->token.kind == codegen::sexpr::TokenKind::raw_literal &&
                body_property_matches(*found->form, function)) {
                if (!retain_source_property(*source_body, function_properties[0].second)) {
                    return std::nullopt;
                }
            } else if (patch_body_rows &&
                       !retain_source_property(*source_body, function_properties[0].second)) {
                return std::nullopt;
            }
            if (patch_dependency_rows &&
                !retain_source_property(*source_dependencies, function_properties[1].second)) {
                return std::nullopt;
            }
            if (!patch_source_properties(*found->form,
                                         2,
                                         function_properties,
                                         "      ",
                                         original,
                                         function_replacements)) {
                return std::nullopt;
            }
            if ((patch_body_rows && !patch_source_quoted_list_property(*found->form,
                                                                       2,
                                                                       "body",
                                                                       function.body_lines,
                                                                       "      ",
                                                                       "        ",
                                                                       original,
                                                                       function_replacements)) ||
                (patch_dependency_rows &&
                 !patch_source_quoted_list_property(*found->form,
                                                    2,
                                                    "dependencies",
                                                    function.dependencies,
                                                    "      ",
                                                    "        ",
                                                    original,
                                                    function_replacements))) {
                return std::nullopt;
            }
            if (!patch_function_parameters(*found->form, function, function_replacements)) {
                return std::nullopt;
            }
            auto rendered{apply_source_replacements_to_range(
                original, found->begin, found->end, std::move(function_replacements))};
            if (!rendered.has_value()) {
                return std::nullopt;
            }
            append_function(std::move(*rendered));
        }
        replacements.push_back({.begin = functions_begin,
                                .end = functions_end,
                                .text = std::move(rendered_functions)});
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

auto declaration_count(codegen::ModuleSchema const& module) -> std::size_t {
    auto const* normal{std::get_if<codegen::NormalModuleSchema>(&module)};
    return normal == nullptr ? 0 : normal->declarations.size();
}

template <typename Function>
void for_each_declaration(codegen::Manifest const& manifest, Function&& function) {
    for (std::size_t module_index{}; module_index < manifest.modules.size(); ++module_index) {
        auto const* module{
            std::get_if<codegen::NormalModuleSchema>(&manifest.modules[module_index])};
        if (module == nullptr) {
            continue;
        }
        for (std::size_t index{}; index < module->declarations.size(); ++index) {
            function(module_index,
                     index,
                     module->settings,
                     codegen::declaration_name(module->declarations[index]));
        }
    }
}

void refresh_declaration_locations(codegen::Manifest const& manifest,
                                   TypeGraph const& types,
                                   std::vector<DeclarationInfo>& declarations) {
    std::set<DeclarationId> located;
    for_each_declaration(
        manifest,
        [&](std::size_t const module_index,
            std::size_t const declaration_index,
            codegen::ModuleSettings const& settings,
            std::string const& name) {
            auto const declaration{
                std::ranges::find_if(declarations, [&](DeclarationInfo const& candidate) {
                    return candidate.module_index == module_index &&
                           candidate.identity.name == name;
                })};
            if (declaration == declarations.end() || !located.insert(declaration->id).second) {
                throw std::logic_error{"Cannot reconcile declaration locations after module move"};
            }
            auto const type{types.find_declared(settings.name, name)};
            declaration->identity =
                type.has_value()
                    ? types.type(*type).identity
                    : TypeIdentity{.origin = TypeOrigin::declaration,
                                   .module_name = settings.name,
                                   .namespace_name = settings.namespace_name.value_or(""),
                                   .name = name};
            declaration->declaration_index = declaration_index;
        });
    if (located.size() != declarations.size()) {
        throw std::logic_error{"Cannot reconcile every declaration after module move"};
    }
}

enum class LocalSoaReferencePolicy { ignore, rename, reject };

void validate_relocated_references(codegen::Manifest const& manifest,
                                   TypeGraph const& previous,
                                   TypeGraph const& candidate,
                                   TypeIdentity const& old_owner,
                                   TypeIdentity const& new_owner) {
    auto const old_types{previous.types_for_declaration(old_owner)};
    auto const new_types{candidate.types_for_declaration(new_owner)};
    if (old_types.size() != new_types.size()) {
        throw std::invalid_argument{"Rename/move changed the declaration's owned types"};
    }

    // Rename and move preserve the members and order of a declaration's generated family.
    auto expected_binding = [&](TypeId const old_type) -> std::optional<TypeId> {
        auto const owned{std::ranges::find(old_types, old_type)};
        if (owned != old_types.end()) {
            return new_types[static_cast<std::size_t>(owned - old_types.begin())];
        }
        return candidate.find(previous.type(old_type).identity);
    };
    for (auto const& use : previous.type_uses()) {
        if (previous.type(use.target.type).identity.origin != TypeOrigin::declaration) {
            continue;
        }
        auto const owner{use.declaration == old_owner ? std::optional{new_owner} : use.declaration};
        auto const expected{expected_binding(use.target.type)};
        auto const preserved{std::ranges::any_of(candidate.type_uses(), [&](auto const& updated) {
            return updated.declaration == owner && updated.role == use.role &&
                   (owner.has_value() || updated.module_name == use.module_name) &&
                   updated.target.type == expected;
        })};
        if (!preserved) {
            throw std::invalid_argument{"Cannot preserve semantic reference to '" +
                                        previous.type(use.target.type).cpp_spelling + "' from '" +
                                        use.module_name + " / " + use.role + "' after rename/move"};
        }
    }
    for (auto const& [name, registration] : manifest.types) {
        static_cast<void>(registration);
        auto const binding{previous.find_registered(name)};
        if (binding.has_value() &&
            previous.type(*binding).identity.origin == TypeOrigin::declaration &&
            candidate.find_registered(name) != expected_binding(*binding)) {
            throw std::invalid_argument{"Cannot preserve registered semantic reference '@" + name +
                                        "' after rename/move"};
        }
    }
}

void repair_semantic_references(codegen::Manifest& manifest,
                                TypeGraph const& types,
                                std::span<DeclarationInfo const> const declarations,
                                TypeId const target,
                                std::string const& new_spelling,
                                std::size_t const target_module_index,
                                std::string_view const old_name,
                                LocalSoaReferencePolicy const local_soa_policy,
                                std::string_view const new_local_name,
                                std::size_t const destination_module_index) {
    auto const& target_node{types.type(target)};
    auto const target_owner{target_node.owning_declaration.value_or(target_node.identity)};
    auto const requires_local_spelling{std::ranges::any_of(types.types(), [&](auto const& node) {
        return node.identity.origin == TypeOrigin::declaration &&
               node.identity != types.type(target).identity && node.cpp_spelling == new_spelling;
    })};
    auto repair_ref = [&](codegen::TypeRef& reference,
                          TypeId const resolved,
                          std::size_t const user_module_index) {
        if (resolved != target) {
            return;
        }
        if (reference.name.starts_with('@')) {
            if (new_spelling == types.type(target).cpp_spelling) {
                return;
            }
            throw std::invalid_argument{"Cannot repair registered semantic reference '" +
                                        reference.name + "' to '" + std::string{old_name} + "'"};
        }
        reference.name =
            user_module_index == destination_module_index &&
                    (reference.name.find("::") == std::string::npos || requires_local_spelling)
                ? std::string{new_local_name}
                : new_spelling;
    };

    for (auto const& user_info : declarations) {
        auto* normal{
            std::get_if<codegen::NormalModuleSchema>(&manifest.modules[user_info.module_index])};
        if (normal == nullptr) {
            continue;
        }
        auto& declaration{normal->declarations.at(user_info.declaration_index)};
        auto const effective_user_module_index{
            user_info.identity == target_owner ? destination_module_index : user_info.module_index};
        codegen::visit_type_references(
            declaration, [&](std::string const& role, codegen::TypeRef& reference) {
                auto const found{std::ranges::find_if(types.type_uses(), [&](auto const& use) {
                    return use.declaration == user_info.identity && use.role == role;
                })};
                if (found != types.type_uses().end()) {
                    repair_ref(reference, found->target.type, effective_user_module_index);
                }
            });

        auto* soa{std::get_if<codegen::SoaSchema>(&declaration)};
        if (soa == nullptr || !std::holds_alternative<SoaType>(types.type(target).definition)) {
            continue;
        }
        auto const user_type{types.find(user_info.identity)};
        auto const& resolved{std::get<SoaType>(types.type(*user_type).definition)};
        for (std::size_t index{}; index < soa->members.size(); ++index) {
            auto& member{soa->members[index]};
            auto const nested_reference{resolved.columns[index].nested_type == target &&
                                        member.nested_schema.has_value()};
            auto const fixed_reference{user_info.module_index == target_module_index &&
                                       member.fixed_schema == old_name};
            if ((nested_reference || fixed_reference) &&
                local_soa_policy == LocalSoaReferencePolicy::reject) {
                throw std::invalid_argument{
                    "Cannot move SoA '" + std::string{old_name} +
                    "' across modules while module-local nested/fixed schema references exist"};
            }
            if (local_soa_policy == LocalSoaReferencePolicy::rename) {
                if (nested_reference) {
                    member.nested_schema = new_local_name;
                }
                if (fixed_reference) {
                    member.fixed_schema = new_local_name;
                }
            }
        }
    }
    for (auto const& use : types.type_uses()) {
        if (!use.declaration.has_value() && use.target.type == target &&
            (new_spelling != types.type(target).cpp_spelling ||
             use.target.cpp_type.spelling != new_spelling)) {
            throw std::invalid_argument{
                "Cannot rename or move a type referenced by module configuration: " +
                use.module_name + " / " + use.role};
        }
    }
}

template <typename Schema>
auto normal_schema_at(codegen::Manifest const& manifest, DeclarationInfo const& info)
    -> Schema const* {
    auto const* module{
        std::get_if<codegen::NormalModuleSchema>(&manifest.modules[info.module_index])};
    if (module == nullptr || info.declaration_index >= module->declarations.size()) {
        return nullptr;
    }
    return std::get_if<Schema>(&module->declarations[info.declaration_index]);
}

template <typename Schema>
auto normal_schema_at(codegen::Manifest& manifest, DeclarationInfo const& info) -> Schema* {
    auto* module{std::get_if<codegen::NormalModuleSchema>(&manifest.modules[info.module_index])};
    if (module == nullptr || info.declaration_index >= module->declarations.size()) {
        return nullptr;
    }
    return std::get_if<Schema>(&module->declarations[info.declaration_index]);
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

auto EditableSchemaDocument::registered_type_source(std::string_view const name) const
    -> std::optional<SourceRange> {
    auto const found{registered_type_sources_.find(name)};
    return found == registered_type_sources_.end() ? std::nullopt : std::optional{found->second};
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
    return info == nullptr ? nullptr : normal_schema_at<codegen::EnumSchema>(manifest_, *info);
}

auto EditableSchemaDocument::packed_value_schema(DeclarationId const declaration_id) const
    -> codegen::PackedValueSchema const* {
    auto const* info{declaration(declaration_id)};
    return info == nullptr ? nullptr
                           : normal_schema_at<codegen::PackedValueSchema>(manifest_, *info);
}

auto EditableSchemaDocument::integer_scalar_schema(DeclarationId const declaration_id) const
    -> codegen::IntegerScalarSchema const* {
    auto const* info{declaration(declaration_id)};
    return info == nullptr ? nullptr
                           : normal_schema_at<codegen::IntegerScalarSchema>(manifest_, *info);
}

auto EditableSchemaDocument::linear_quantized_schema(DeclarationId const declaration_id) const
    -> codegen::LinearQuantizedSchema const* {
    auto const* info{declaration(declaration_id)};
    return info == nullptr ? nullptr
                           : normal_schema_at<codegen::LinearQuantizedSchema>(manifest_, *info);
}

auto EditableSchemaDocument::integer_varint_schema(DeclarationId const declaration_id) const
    -> codegen::IntegerVarintSchema const* {
    auto const* info{declaration(declaration_id)};
    return info == nullptr ? nullptr
                           : normal_schema_at<codegen::IntegerVarintSchema>(manifest_, *info);
}

auto EditableSchemaDocument::fixed_point_schema(DeclarationId const declaration_id) const
    -> codegen::FixedPointSchema const* {
    auto const* info{declaration(declaration_id)};
    return info == nullptr ? nullptr
                           : normal_schema_at<codegen::FixedPointSchema>(manifest_, *info);
}

auto EditableSchemaDocument::optional_sentinel_schema(DeclarationId const declaration_id) const
    -> codegen::OptionalSentinelSchema const* {
    auto const* info{declaration(declaration_id)};
    return info == nullptr ? nullptr
                           : normal_schema_at<codegen::OptionalSentinelSchema>(manifest_, *info);
}

auto EditableSchemaDocument::optional_presence_bit_schema(DeclarationId const declaration_id) const
    -> codegen::OptionalPresenceBitSchema const* {
    auto const* info{declaration(declaration_id)};
    return info == nullptr ? nullptr
                           : normal_schema_at<codegen::OptionalPresenceBitSchema>(manifest_, *info);
}

auto EditableSchemaDocument::mini_float_schema(DeclarationId const declaration_id) const
    -> codegen::MiniFloatSchema const* {
    auto const* info{declaration(declaration_id)};
    return info == nullptr ? nullptr : normal_schema_at<codegen::MiniFloatSchema>(manifest_, *info);
}

auto EditableSchemaDocument::record_schema(DeclarationId const declaration_id) const
    -> codegen::RecordSchema const* {
    auto const* info{declaration(declaration_id)};
    return info == nullptr ? nullptr : normal_schema_at<codegen::RecordSchema>(manifest_, *info);
}

auto EditableSchemaDocument::union_schema(DeclarationId const declaration_id) const
    -> codegen::UnionSchema const* {
    auto const* info{declaration(declaration_id)};
    return info == nullptr ? nullptr : normal_schema_at<codegen::UnionSchema>(manifest_, *info);
}

auto EditableSchemaDocument::tagged_union_schema(DeclarationId const declaration_id) const
    -> codegen::TaggedUnionSchema const* {
    auto const* info{declaration(declaration_id)};
    return info == nullptr ? nullptr
                           : normal_schema_at<codegen::TaggedUnionSchema>(manifest_, *info);
}

auto EditableSchemaDocument::soa_schema(DeclarationId const declaration_id) const
    -> codegen::SoaSchema const* {
    auto const* info{declaration(declaration_id)};
    return info == nullptr ? nullptr : normal_schema_at<codegen::SoaSchema>(manifest_, *info);
}

auto EditableSchemaDocument::unique_soa_generated_type_name(DeclarationId const declaration_id,
                                                            std::string const& base) const
    -> std::expected<std::string, SchemaEditError> {
    auto const* info{declaration(declaration_id)};
    if (info == nullptr || soa_schema(declaration_id) == nullptr) {
        return std::unexpected{SchemaEditError{"Unknown SoA declaration"}};
    }
    auto const occupied{std::visit(
        [](auto const& module) -> std::set<std::string> {
            using T = std::decay_t<decltype(module)>;
            if constexpr (std::is_same_v<T, codegen::NormalModuleSchema>) {
                return module_generated_type_names(module);
            } else {
                return {};
            }
        },
        manifest_.modules[info->module_index])};
    auto candidate{base};
    for (auto suffix{std::size_t{2}}; occupied.contains(candidate); ++suffix) {
        candidate = base + std::to_string(suffix);
    }
    return candidate;
}

auto EditableSchemaDocument::unique_soa_storage_owner_name(DeclarationId const declaration_id,
                                                           std::string const& base) const
    -> std::expected<std::string, SchemaEditError> {
    auto const* info{declaration(declaration_id)};
    if (info == nullptr || soa_schema(declaration_id) == nullptr) {
        return std::unexpected{SchemaEditError{"Unknown SoA declaration"}};
    }
    auto const occupied{std::visit(
        [](auto const& module) -> std::set<std::string> {
            using T = std::decay_t<decltype(module)>;
            if constexpr (std::is_same_v<T, codegen::NormalModuleSchema>) {
                return module_generated_type_names(module);
            } else {
                return {};
            }
        },
        manifest_.modules[info->module_index])};
    auto candidate{base};
    for (auto suffix{std::size_t{2}};
         occupied.contains(candidate) || occupied.contains(candidate + "Storage");
         ++suffix) {
        candidate = base + std::to_string(suffix);
    }
    return candidate;
}

auto EditableSchemaDocument::prepare_soa_duplicate(DeclarationId const declaration_id) const
    -> std::expected<codegen::SoaSchema, SchemaEditError> {
    auto const* info{declaration(declaration_id)};
    auto const* source{soa_schema(declaration_id)};
    if (info == nullptr || source == nullptr) {
        return std::unexpected{SchemaEditError{"Unknown SoA declaration"}};
    }
    auto occupied{std::visit(
        [](auto const& module) -> std::set<std::string> {
            using T = std::decay_t<decltype(module)>;
            if constexpr (std::is_same_v<T, codegen::NormalModuleSchema>) {
                return module_generated_type_names(module);
            } else {
                return {};
            }
        },
        manifest_.modules[info->module_index])};

    auto const numbered_candidate = [](std::string const& base, std::size_t const suffix) {
        return suffix == 1 ? base : base + std::to_string(suffix);
    };
    auto reserve_unique = [&](std::string const& base) {
        for (auto suffix{std::size_t{1}};; ++suffix) {
            auto candidate{numbered_candidate(base, suffix)};
            if (occupied.insert(candidate).second) {
                return candidate;
            }
        }
    };
    auto reserve_unique_group = [&](std::string const& base,
                                    std::initializer_list<std::string_view> const endings) {
        for (auto suffix{std::size_t{1}};; ++suffix) {
            auto candidate{numbered_candidate(base, suffix)};
            auto const available{std::ranges::all_of(endings, [&](std::string_view const ending) {
                return !occupied.contains(candidate + std::string{ending});
            })};
            if (!available) {
                continue;
            }
            for (auto const ending : endings) {
                occupied.insert(candidate + std::string{ending});
            }
            return candidate;
        }
    };

    auto copy{*source};
    auto schema_name_available = [&](std::string const& candidate) {
        if (occupied.contains(candidate)) {
            return false;
        }
        if (!source->view_name.has_value() && occupied.contains(candidate + "View")) {
            return false;
        }
        if (!source->const_view_name.has_value() && occupied.contains(candidate + "ConstView")) {
            return false;
        }
        if (source->single_allocation.has_value() &&
            (occupied.contains(candidate + "SingleLayout") ||
             occupied.contains(candidate + "SingleView") ||
             occupied.contains(candidate + "SingleConstView"))) {
            return false;
        }
        return std::ranges::none_of(
            source->functions,
            [&](codegen::FunctionSchema const& function) { return function.name == candidate; });
    };
    auto const schema_name_base{source->name + "_copy"};
    for (auto suffix{std::size_t{1}};; ++suffix) {
        auto candidate{numbered_candidate(schema_name_base, suffix)};
        if (!schema_name_available(candidate)) {
            continue;
        }
        copy.name = std::move(candidate);
        occupied.insert(copy.name);
        if (!source->view_name.has_value()) {
            occupied.insert(copy.name + "View");
        }
        if (!source->const_view_name.has_value()) {
            occupied.insert(copy.name + "ConstView");
        }
        if (source->single_allocation.has_value()) {
            occupied.insert(copy.name + "SingleLayout");
            occupied.insert(copy.name + "SingleView");
            occupied.insert(copy.name + "SingleConstView");
        }
        break;
    }

    auto duplicate_helper_base = [&](std::string const& original) {
        if (original.starts_with(source->name)) {
            return copy.name + original.substr(source->name.size());
        }
        return original + "_copy";
    };
    if (source->view_name.has_value()) {
        copy.view_name = reserve_unique(duplicate_helper_base(*source->view_name));
    }
    if (source->const_view_name.has_value()) {
        copy.const_view_name = reserve_unique(duplicate_helper_base(*source->const_view_name));
    }
    if (source->field_mask_name.has_value()) {
        auto const old_mask_name{*source->field_mask_name};
        copy.field_mask_name = reserve_unique(duplicate_helper_base(old_mask_name));
        copy.field_enum_name = reserve_unique(duplicate_helper_base(*source->field_enum_name));
        for (auto& member : copy.members) {
            if (member.type.name == old_mask_name) {
                member.type.name = *copy.field_mask_name;
            }
        }
    }
    if (source->fixed.has_value()) {
        copy.fixed->storage_name =
            reserve_unique(duplicate_helper_base(source->fixed->storage_name));
        for (auto& container : copy.fixed->containers) {
            container = reserve_unique(duplicate_helper_base(container));
        }
    }
    if (source->single_allocation.has_value()) {
        copy.single_allocation = reserve_unique_group(
            duplicate_helper_base(*source->single_allocation), {"", "Storage"});
        for (auto& variant : copy.single_allocation_variants) {
            variant.name =
                reserve_unique_group(duplicate_helper_base(variant.name), {"", "Storage"});
        }
    }

    return copy;
}

auto EditableSchemaDocument::allocate_declaration_id() -> DeclarationId {
    return DeclarationId{next_declaration_id_++};
}

auto EditableSchemaDocument::apply(SchemaEditCommand command)
    -> std::expected<bool, SchemaEditError> {
    std::optional<SchemaEditCommand> normalized;
    std::visit(
        [&](auto const& edit) {
            using Edit = std::decay_t<decltype(edit)>;
            if constexpr (std::is_same_v<Edit, CreateModule>) {
                normalized = CreateModule{.source_file_index = edit.source_file_index,
                                          .schema = edit.schema};
            } else if constexpr (requires {
                                     edit.schema;
                                     edit.declaration;
                                 }) {
                using Schema = std::decay_t<decltype(edit.schema)>;
                if constexpr (std::is_constructible_v<codegen::DeclarationSchema, Schema> &&
                              !std::is_same_v<Schema, codegen::DeclarationSchema>) {
                    if constexpr (requires {
                                      edit.module_index;
                                      edit.insertion_index;
                                  }) {
                        if (edit.module_index < manifest_.modules.size() &&
                            std::holds_alternative<codegen::NormalModuleSchema>(
                                manifest_.modules[edit.module_index])) {
                            normalized = CreateDeclaration{.declaration = edit.declaration,
                                                           .module_index = edit.module_index,
                                                           .schema = edit.schema,
                                                           .insertion_index = edit.insertion_index};
                        }
                    } else {
                        auto const* info{declaration(edit.declaration)};
                        if (info != nullptr && std::holds_alternative<codegen::NormalModuleSchema>(
                                                   manifest_.modules[info->module_index])) {
                            normalized = ReplaceDeclaration{.declaration = edit.declaration,
                                                            .schema = edit.schema};
                        }
                    }
                }
            } else if constexpr (
                requires { edit.declaration; } && !requires { edit.module_index; } &&
                !requires { edit.new_name; } && !std::is_same_v<Edit, DeleteDeclaration>) {
                auto const* info{declaration(edit.declaration)};
                if (info != nullptr && std::holds_alternative<codegen::NormalModuleSchema>(
                                           manifest_.modules[info->module_index])) {
                    normalized = DeleteDeclaration{edit.declaration};
                }
            }
        },
        command);
    if (normalized.has_value()) {
        command = std::move(*normalized);
    }
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
                } else if constexpr (requires { edit.declaration; }) {
                    touched.insert(edit.declaration);
                    if constexpr (std::is_same_v<std::decay_t<decltype(edit)>, RenameDeclaration> ||
                                  std::is_same_v<std::decay_t<decltype(edit)>, MoveDeclaration>) {
                        auto const* renamed{declaration(edit.declaration)};
                        if (renamed != nullptr) {
                            auto const owned_types{types_.types_for_declaration(renamed->identity)};
                            for (auto const& use : types_.type_uses()) {
                                if (use.declaration.has_value() &&
                                    std::ranges::find(owned_types, use.target.type) !=
                                        owned_types.end()) {
                                    if (auto const owner{find_declaration(*use.declaration)}) {
                                        touched.insert(*owner);
                                    }
                                }
                            }
                            auto const type{types_.find(renamed->identity)};
                            if (type.has_value()) {
                                for (auto const user : types_.users_of(*type)) {
                                    auto const user_declaration{
                                        find_declaration(types_.type(user).identity)};
                                    if (user_declaration.has_value()) {
                                        touched.insert(*user_declaration);
                                    }
                                }
                                if (std::holds_alternative<SoaType>(
                                        types_.type(*type).definition) &&
                                    soa_schema(edit.declaration) != nullptr) {
                                    for (auto const& candidate : declarations_) {
                                        if (candidate.module_index != renamed->module_index) {
                                            continue;
                                        }
                                        auto const* candidate_schema{soa_schema(candidate.id)};
                                        if (candidate_schema == nullptr) {
                                            continue;
                                        }
                                        auto const references_renamed{std::ranges::any_of(
                                            candidate_schema->members, [&](auto const& member) {
                                                return member.nested_schema ==
                                                           renamed->identity.name ||
                                                       member.fixed_schema ==
                                                           renamed->identity.name;
                                            })};
                                        if (references_renamed) {
                                            touched.insert(candidate.id);
                                        }
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
    auto covered_by_deleted_module = [&](SourceRange const& range) {
        return std::ranges::any_of(deleted_module_source_ranges_, [&](SourceRange const& module) {
            return module.source_file_index == range.source_file_index &&
                   module.begin_offset <= range.begin_offset &&
                   range.end_offset <= module.end_offset;
        });
    };
    for (auto const& range : deleted_module_source_ranges_) {
        replacements[range.source_file_index].push_back(
            {.begin = range.begin_offset, .end = range.end_offset, .text = {}});
    }
    auto render_source_aware = [&](DeclarationId const id,
                                   auto const& schema,
                                   auto const preserve,
                                   auto const canonical) -> std::optional<std::string> {
        auto const* info{declaration(id)};
        auto range{info == nullptr ? std::optional<SourceRange>{} : info->source};
        if (!range.has_value()) {
            auto const tombstone{source_tombstones_.find(id)};
            if (tombstone != source_tombstones_.end()) {
                range = tombstone->second;
            }
        }
        if (range.has_value()) {
            auto const& source{source_files_[range->source_file_index].text};
            auto const original{std::string_view{source}.substr(
                range->begin_offset, range->end_offset - range->begin_offset)};
            if (auto preserved{preserve(schema, original)}) {
                return preserved;
            }
            if (auto renamed{try_patch_owned_declaration_name(original, schema.name)}) {
                if (auto preserved{preserve(schema, *renamed)}) {
                    return preserved;
                }
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
        auto const* info{declaration(id)};
        if (info == nullptr || info->module_index >= manifest_.modules.size()) {
            return std::nullopt;
        }
        auto const* module{
            std::get_if<codegen::NormalModuleSchema>(&manifest_.modules[info->module_index])};
        if (module == nullptr || info->declaration_index >= module->declarations.size()) {
            return std::nullopt;
        }
        return render_declaration_schema(module->declarations[info->declaration_index]);
    };
    for (auto const id : touched) {
        auto const* info{declaration(id)};
        if (info == nullptr) {
            auto const tombstone{source_tombstones_.find(id)};
            if (tombstone != source_tombstones_.end() &&
                !covered_by_deleted_module(tombstone->second)) {
                auto const& source{tombstone->second};
                replacements[source.source_file_index].push_back(
                    {.begin = source.begin_offset, .end = source.end_offset, .text = {}});
            }
            continue;
        }

        auto const rendered{render_declaration(id)};
        if (!rendered.has_value()) {
            return std::unexpected{SchemaEditError{"Touched declaration cannot be serialized"}};
        }
        if (info->source.has_value() && !covered_by_deleted_module(*info->source)) {
            replacements[info->source->source_file_index].push_back(
                {.begin = info->source->begin_offset,
                 .end = info->source->end_offset,
                 .text = *rendered});
        } else {
            auto const tombstone{source_tombstones_.find(id)};
            if (tombstone != source_tombstones_.end() &&
                !covered_by_deleted_module(tombstone->second)) {
                auto const& source{tombstone->second};
                replacements[source.source_file_index].push_back(
                    {.begin = source.begin_offset, .end = source.end_offset, .text = {}});
            }
            insertions[info->module_index].insert(id);
        }
    }
    struct InsertionBoundary {
        std::size_t source_file_index{};
        std::size_t offset{};
        bool before_declaration{};

        auto operator<=>(InsertionBoundary const&) const = default;
    };
    std::map<InsertionBoundary, std::vector<DeclarationInfo const*>> bounded_insertions;
    for (auto const& [module_index, ids] : insertions) {

        if (pending_module_sources_.contains(module_index)) {
            continue;
        }
        if (module_index >= module_source_ranges_.size() ||
            !module_source_ranges_[module_index].has_value()) {
            return std::unexpected{SchemaEditError{"New declaration's module has no source range"}};
        }
        for (auto const& declaration_info : declarations_) {
            if (declaration_info.module_index != module_index ||
                !ids.contains(declaration_info.id)) {
                continue;
            }

            DeclarationInfo const* next{};
            for (auto const& candidate : declarations_) {
                if (candidate.module_index == module_index && candidate.source.has_value() &&
                    candidate.declaration_index > declaration_info.declaration_index &&
                    (next == nullptr || candidate.declaration_index < next->declaration_index)) {
                    next = &candidate;
                }
            }
            if (next != nullptr) {
                auto const& source{source_files_[next->source->source_file_index].text};
                auto const line_break{next->source->begin_offset == 0
                                          ? std::string::npos
                                          : source.rfind('\n', next->source->begin_offset - 1)};
                auto const line_begin{line_break == std::string::npos ? 0 : line_break + 1};
                bounded_insertions[{.source_file_index = next->source->source_file_index,
                                    .offset = line_begin,
                                    .before_declaration = true}]
                    .push_back(&declaration_info);
            } else {
                auto const& module_range{*module_source_ranges_[module_index]};
                bounded_insertions[{.source_file_index = module_range.source_file_index,
                                    .offset = module_range.end_offset - 1,
                                    .before_declaration = false}]
                    .push_back(&declaration_info);
            }
        }
    }
    for (auto& [boundary, declarations] : bounded_insertions) {
        std::ranges::sort(declarations, {}, &DeclarationInfo::declaration_index);
        std::string insertion;
        for (auto const* declaration_info : declarations) {
            auto const rendered{render_declaration(declaration_info->id)};
            if (!rendered.has_value()) {
                return std::unexpected{
                    SchemaEditError{"New declaration kind cannot be serialized"}};
            }
            insertion += boundary.before_declaration ? "  " + *rendered + "\n" : "\n  " + *rendered;
        }
        replacements[boundary.source_file_index].push_back(
            {.begin = boundary.offset, .end = boundary.offset, .text = std::move(insertion)});
    }
    std::map<std::size_t, std::string> pending_module_text;
    for (auto const& [module_index, source_file_index] : pending_module_sources_) {
        if (module_index >= manifest_.modules.size() || source_file_index >= source_files_.size()) {
            return std::unexpected{SchemaEditError{"Invalid pending module source ownership"}};
        }
        auto rendered{render_editable_module(manifest_.modules[module_index])};
        if (!rendered.has_value()) {
            return std::unexpected{SchemaEditError{"New module kind cannot be serialized"}};
        }
        auto& insertion{pending_module_text[source_file_index]};
        auto const& source{source_files_[source_file_index].text};
        if (insertion.empty() && !source.empty()) {
            insertion = source.ends_with('\n') ? "\n" : "\n\n";
        }
        insertion += std::move(*rendered) + "\n\n";
    }
    for (auto& [source_file_index, insertion] : pending_module_text) {
        insertion.pop_back();
        auto const& source{source_files_[source_file_index].text};
        replacements[source_file_index].push_back(
            {.begin = source.size(), .end = source.size(), .text = std::move(insertion)});
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
        if (updated == source_files_[source_index].text) {
            continue;
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
    for_each_declaration(
        manifest_,
        [&](std::size_t const module_index,
            std::size_t const declaration_index,
            codegen::ModuleSettings const& settings,
            std::string const& name) {
            auto const type{types_.find_declared(settings.name, name)};
            auto identity{type.has_value()
                              ? types_.type(*type).identity
                              : TypeIdentity{.origin = TypeOrigin::declaration,
                                             .module_name = settings.name,
                                             .namespace_name = settings.namespace_name.value_or(""),
                                             .name = name}};
            auto source{source_index < source_ranges.size() ? source_ranges[source_index]
                                                            : std::nullopt};
            declarations_.push_back({.id = DeclarationId{next_id++},
                                     .identity = std::move(identity),
                                     .module_index = module_index,
                                     .declaration_index = declaration_index,
                                     .source = source});
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
        auto const owned_types{types_.types_for_declaration(info.identity)};
        for (auto const& [registered_name, cpp_type] : manifest_.types) {
            static_cast<void>(cpp_type);
            auto const registered{types_.find_registered(registered_name)};
            if (registered.has_value() &&
                std::ranges::find(owned_types, *registered) != owned_types.end()) {
                return SchemaEditError{"Cannot delete declaration '" + info.identity.name +
                                       "'; it is registered as '@" + registered_name + "'"};
            }
        }

        for (auto const& use : types_.type_uses()) {
            if (use.declaration != info.identity &&
                std::ranges::find(owned_types, use.target.type) != owned_types.end()) {
                return SchemaEditError{
                    "Cannot delete declaration '" + info.identity.name + "'; it is used by '" +
                    use.module_name + " / " +
                    (use.declaration.has_value() ? use.declaration->name : use.role) + "'"};
            }
        }
        for (auto const owned : owned_types) {
            for (auto const user : types_.users_of(owned)) {
                auto const& node{types_.type(user)};
                if (node.owning_declaration.value_or(node.identity) != info.identity) {
                    return SchemaEditError{"Cannot delete declaration '" + info.identity.name +
                                           "'; it is used by '" + node.cpp_spelling + "'"};
                }
            }
        }
        return std::nullopt;
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
            if constexpr (std::is_same_v<Edit, CreateDeclaration>) {
                if (!edit.declaration.valid() || declaration(edit.declaration) != nullptr) {
                    return std::unexpected{SchemaEditError{"New declaration requires a unique id"}};
                }
                if (edit.module_index >= manifest_.modules.size()) {
                    return std::unexpected{SchemaEditError{"Unknown module index"}};
                }
                auto candidate{manifest_};
                auto* module{std::get_if<codegen::NormalModuleSchema>(
                    &candidate.modules[edit.module_index])};
                if (module == nullptr) {
                    return std::unexpected{SchemaEditError{"Declarations require a normal module"}};
                }
                auto const insertion{edit.insertion_index.value_or(module->declarations.size())};
                if (insertion > module->declarations.size()) {
                    return std::unexpected{SchemaEditError{"Invalid declaration insertion index"}};
                }
                module->declarations.insert(module->declarations.begin() +
                                                static_cast<std::ptrdiff_t>(insertion),
                                            edit.schema);
                TypeGraph candidate_types;
                try {
                    codegen::validate_manifest(candidate);
                    candidate_types = resolve_type_graph(candidate);
                } catch (std::exception const& error) {
                    return std::unexpected{SchemaEditError{error.what()}};
                }
                auto const& name{codegen::declaration_name(edit.schema)};
                auto const found{candidate_types.find_declared(module->settings.name, name)};
                auto const identity{
                    found.has_value()
                        ? candidate_types.type(*found).identity
                        : TypeIdentity{.origin = TypeOrigin::declaration,
                                       .module_name = module->settings.name,
                                       .namespace_name =
                                           module->settings.namespace_name.value_or(""),
                                       .name = name}};
                for (auto& info : declarations_) {
                    if (info.module_index == edit.module_index &&
                        info.declaration_index >= insertion) {
                        ++info.declaration_index;
                    }
                }
                declarations_.push_back({.id = edit.declaration,
                                         .identity = identity,
                                         .module_index = edit.module_index,
                                         .declaration_index = insertion,
                                         .source = creation_source(edit.declaration)});
                manifest_ = std::move(candidate);
                types_ = std::move(candidate_types);
                return SchemaEditCommand{DeleteDeclaration{edit.declaration}};
            } else if constexpr (std::is_same_v<Edit, ReplaceDeclaration>) {
                auto const* info{declaration(edit.declaration)};
                if (info == nullptr) {
                    return std::unexpected{SchemaEditError{"Unknown declaration"}};
                }
                auto candidate{manifest_};
                auto* module{std::get_if<codegen::NormalModuleSchema>(
                    &candidate.modules[info->module_index])};
                if (module == nullptr || info->declaration_index >= module->declarations.size()) {
                    return std::unexpected{
                        SchemaEditError{"Declaration is not in a normal module"}};
                }
                auto& current{module->declarations[info->declaration_index]};
                if (current.index() != edit.schema.index()) {
                    return std::unexpected{SchemaEditError{"Replacement declaration kind differs"}};
                }
                if (codegen::declaration_name(current) != codegen::declaration_name(edit.schema)) {
                    return std::unexpected{
                        SchemaEditError{"Use RenameDeclaration to change a name"}};
                }
                auto previous{current};
                current = edit.schema;
                TypeGraph candidate_types;
                try {
                    codegen::validate_manifest(candidate);
                    candidate_types = resolve_type_graph(candidate);
                    for (auto const owned : types_.types_for_declaration(info->identity)) {
                        if (candidate_types.find(types_.type(owned).identity).has_value()) {
                            continue;
                        }
                        for (auto const& [name, registration] : manifest_.types) {
                            static_cast<void>(registration);
                            if (types_.find_registered(name) == owned) {
                                throw std::invalid_argument{
                                    "Cannot remove or rename generated type '" +
                                    types_.type(owned).cpp_spelling + "'; it is registered as '@" +
                                    name + "'"};
                            }
                        }
                        codegen::visit_type_references(
                            current,
                            [&](std::string const& role, codegen::TypeRef const& reference) {
                                if (types_.find_reference(reference, info->identity.module_name) ==
                                    owned) {
                                    throw std::invalid_argument{
                                        "Cannot remove or rename generated type '" +
                                        types_.type(owned).cpp_spelling + "'; it is used by '" +
                                        info->identity.name + " / " + role + "'"};
                                }
                            });
                        for (auto const& use : types_.type_uses()) {
                            if (use.target.type == owned && use.declaration != info->identity) {
                                throw std::invalid_argument{
                                    "Cannot remove or rename generated type '" +
                                    types_.type(owned).cpp_spelling + "'; it is used by '" +
                                    use.module_name + " / " +
                                    (use.declaration.has_value() ? use.declaration->name
                                                                 : use.role) +
                                    "'"};
                            }
                        }
                    }
                } catch (std::exception const& error) {
                    return std::unexpected{SchemaEditError{error.what()}};
                }
                manifest_ = std::move(candidate);
                types_ = std::move(candidate_types);
                return SchemaEditCommand{ReplaceDeclaration{edit.declaration, std::move(previous)}};
            } else if constexpr (std::is_same_v<Edit, DeleteDeclaration>) {
                auto const* found{declaration(edit.declaration)};
                if (found == nullptr) {
                    return std::unexpected{SchemaEditError{"Unknown declaration"}};
                }
                if (auto const blocker{deletion_blocker(*found)}; blocker.has_value()) {
                    return std::unexpected{*blocker};
                }
                auto const info{*found};
                auto candidate{manifest_};
                auto* module{std::get_if<codegen::NormalModuleSchema>(
                    &candidate.modules[info.module_index])};
                if (module == nullptr || info.declaration_index >= module->declarations.size()) {
                    return std::unexpected{
                        SchemaEditError{"Declaration is not in a normal module"}};
                }
                auto schema{module->declarations[info.declaration_index]};
                module->declarations.erase(module->declarations.begin() +
                                           static_cast<std::ptrdiff_t>(info.declaration_index));
                TypeGraph candidate_types;
                try {
                    codegen::validate_manifest(candidate);
                    candidate_types = resolve_type_graph(candidate);
                } catch (std::exception const& error) {
                    return std::unexpected{SchemaEditError{error.what()}};
                }
                remember_source_tombstone(info);
                declarations_.erase(
                    std::ranges::find(declarations_, edit.declaration, &DeclarationInfo::id));
                for (auto& existing : declarations_) {
                    if (existing.module_index == info.module_index &&
                        existing.declaration_index > info.declaration_index) {
                        --existing.declaration_index;
                    }
                }
                manifest_ = std::move(candidate);
                types_ = std::move(candidate_types);
                return SchemaEditCommand{
                    CreateDeclaration{.declaration = edit.declaration,
                                      .module_index = info.module_index,
                                      .schema = std::move(schema),
                                      .insertion_index = info.declaration_index}};
            } else if constexpr (std::is_same_v<Edit, CreateModule>) {
                if (edit.source_file_index >= source_files_.size() ||
                    source_files_[edit.source_file_index].kind != SchemaSourceKind::module ||
                    std::ranges::find(module_paths_, source_files_[edit.source_file_index].path) ==
                        module_paths_.end()) {
                    return std::unexpected{
                        SchemaEditError{"New modules require an existing loaded module source"}};
                }
                if (!render_editable_module(edit.schema).has_value()) {
                    return std::unexpected{SchemaEditError{"Module kind is not editable"}};
                }
                if (declaration_count(edit.schema) != 0) {
                    return std::unexpected{
                        SchemaEditError{"CreateModule requires an empty module schema"}};
                }

                auto const module_index{manifest_.modules.size()};
                manifest_.modules.push_back(edit.schema);
                module_source_ranges_.push_back(std::nullopt);

                pending_module_sources_.emplace(module_index, edit.source_file_index);
                try {
                    codegen::validate_manifest(manifest_);
                    types_ = resolve_type_graph(manifest_);
                } catch (std::exception const& error) {
                    pending_module_sources_.erase(module_index);
                    module_source_ranges_.pop_back();

                    manifest_.modules.pop_back();
                    return std::unexpected{SchemaEditError{error.what()}};
                }
                return SchemaEditCommand{DeleteModule{.module_index = module_index}};
            } else if constexpr (std::is_same_v<Edit, DeleteModule>) {
                if (edit.module_index >= manifest_.modules.size()) {
                    return std::unexpected{SchemaEditError{"Unknown module index"}};
                }
                if (manifest_.modules.size() == 1) {
                    return std::unexpected{
                        SchemaEditError{"The project must retain at least one module"}};
                }

                auto const source{module_source_ranges_[edit.module_index]};
                auto const pending{pending_module_sources_.find(edit.module_index)};
                if (!source.has_value() && pending == pending_module_sources_.end()) {
                    return std::unexpected{
                        SchemaEditError{"Module has no editable source ownership"}};
                }

                auto const& module_name{std::visit(
                    [](auto const& module) -> std::string const& { return module.settings.name; },
                    manifest_.modules[edit.module_index])};
                for (auto const& info : declarations_) {
                    if (info.module_index != edit.module_index) {
                        continue;
                    }
                    auto const owned_types{types_.types_for_declaration(info.identity)};
                    for (auto const& use : types_.type_uses()) {
                        if (use.module_name != module_name &&
                            std::ranges::find(owned_types, use.target.type) != owned_types.end()) {
                            return std::unexpected{SchemaEditError{
                                "Cannot delete module '" + module_name + "'; declaration '" +
                                info.identity.name + "' is used by '" + use.module_name + " / " +
                                (use.declaration.has_value() ? use.declaration->name : use.role) +
                                "'"}};
                        }
                    }
                    for (auto const& [registered_name, registered_schema] : manifest_.types) {
                        static_cast<void>(registered_schema);
                        auto const registered{types_.find_registered(registered_name)};
                        if (registered.has_value() &&
                            std::ranges::find(owned_types, *registered) != owned_types.end()) {
                            return std::unexpected{SchemaEditError{
                                "Cannot delete module '" + module_name +
                                "'; it contains a type registered as '@" + registered_name + "'"}};
                        }
                    }
                    auto const type{types_.find(info.identity)};
                    if (!type.has_value()) {
                        auto const* normal{std::get_if<codegen::NormalModuleSchema>(
                            &manifest_.modules[edit.module_index])};
                        if (normal != nullptr &&
                            !codegen::has_primary_semantic_type(
                                normal->declarations[info.declaration_index])) {
                            continue;
                        }
                        return std::unexpected{
                            SchemaEditError{"Module declaration is missing from the type graph"}};
                    }
                    for (auto const& [registered_name, cpp_type] : manifest_.types) {
                        static_cast<void>(cpp_type);
                        if (types_.find_registered(registered_name) == type) {
                            return std::unexpected{
                                SchemaEditError{"Cannot delete module '" + module_name +
                                                "'; declaration '" + info.identity.name +
                                                "' is registered as '@" + registered_name + "'"}};
                        }
                    }
                    for (auto const user : types_.users_of(*type)) {
                        auto const& user_node{types_.type(user)};
                        if (user_node.identity.module_name != module_name) {
                            return std::unexpected{
                                SchemaEditError{"Cannot delete module '" + module_name +
                                                "'; declaration '" + info.identity.name +
                                                "' is used by '" + user_node.cpp_spelling + "'"}};
                        }
                    }
                }

                auto candidate{manifest_};
                candidate.modules.erase(candidate.modules.begin() +
                                        static_cast<std::ptrdiff_t>(edit.module_index));
                TypeGraph candidate_types;
                try {
                    codegen::validate_manifest(candidate);
                    candidate_types = resolve_type_graph(candidate);
                } catch (std::exception const& error) {
                    return std::unexpected{SchemaEditError{error.what()}};
                }

                RestoreModule inverse{.module_index = edit.module_index,
                                      .schema = manifest_.modules[edit.module_index],
                                      .source = source,
                                      .pending_source_file_index =
                                          pending == pending_module_sources_.end()
                                              ? std::nullopt
                                              : std::optional{pending->second}};
                for (auto const& info : declarations_) {
                    if (info.module_index == edit.module_index) {
                        inverse.declarations.push_back(info);
                    }
                }

                manifest_ = std::move(candidate);
                types_ = std::move(candidate_types);
                module_source_ranges_.erase(module_source_ranges_.begin() +
                                            static_cast<std::ptrdiff_t>(edit.module_index));

                if (source.has_value()) {
                    deleted_module_source_ranges_.push_back(*source);
                }
                std::erase_if(declarations_, [&](DeclarationInfo const& info) {
                    return info.module_index == edit.module_index;
                });
                for (auto& info : declarations_) {
                    if (info.module_index > edit.module_index) {
                        --info.module_index;
                    }
                }
                auto shifted_pending{std::map<std::size_t, std::size_t>{}};
                for (auto const& [index, source_file_index] : pending_module_sources_) {
                    if (index != edit.module_index) {
                        shifted_pending.emplace(index > edit.module_index ? index - 1 : index,
                                                source_file_index);
                    }
                }
                pending_module_sources_ = std::move(shifted_pending);
                return SchemaEditCommand{std::move(inverse)};
            } else if constexpr (std::is_same_v<Edit, RestoreModule>) {
                if (edit.module_index > manifest_.modules.size()) {
                    return std::unexpected{SchemaEditError{"Unknown module insertion index"}};
                }
                if (edit.source.has_value() == edit.pending_source_file_index.has_value()) {
                    return std::unexpected{
                        SchemaEditError{"Module restore has invalid source ownership"}};
                }
                auto deleted_range{deleted_module_source_ranges_.end()};
                if (edit.source.has_value()) {
                    deleted_range = std::ranges::find_if(
                        deleted_module_source_ranges_,
                        [&](SourceRange const& range) { return range == *edit.source; });
                    if (deleted_range == deleted_module_source_ranges_.end()) {
                        return std::unexpected{
                            SchemaEditError{"Deleted module source range is missing"}};
                    }
                }

                auto candidate{manifest_};
                candidate.modules.insert(candidate.modules.begin() +
                                             static_cast<std::ptrdiff_t>(edit.module_index),
                                         edit.schema);
                TypeGraph candidate_types;
                try {
                    codegen::validate_manifest(candidate);
                    candidate_types = resolve_type_graph(candidate);
                } catch (std::exception const& error) {
                    return std::unexpected{SchemaEditError{error.what()}};
                }

                manifest_ = std::move(candidate);
                types_ = std::move(candidate_types);
                module_source_ranges_.insert(module_source_ranges_.begin() +
                                                 static_cast<std::ptrdiff_t>(edit.module_index),
                                             edit.source);

                if (edit.source.has_value()) {
                    deleted_module_source_ranges_.erase(deleted_range);
                }
                for (auto& info : declarations_) {
                    if (info.module_index >= edit.module_index) {
                        ++info.module_index;
                    }
                }
                declarations_.insert(
                    declarations_.end(), edit.declarations.begin(), edit.declarations.end());
                std::ranges::sort(
                    declarations_, [](DeclarationInfo const& first, DeclarationInfo const& second) {
                        return std::pair{first.module_index, first.declaration_index} <
                               std::pair{second.module_index, second.declaration_index};
                    });
                auto shifted_pending{std::map<std::size_t, std::size_t>{}};
                for (auto const& [index, source_file_index] : pending_module_sources_) {
                    shifted_pending.emplace(index >= edit.module_index ? index + 1 : index,
                                            source_file_index);
                }
                if (edit.pending_source_file_index.has_value()) {
                    shifted_pending.emplace(edit.module_index, *edit.pending_source_file_index);
                }
                pending_module_sources_ = std::move(shifted_pending);
                return SchemaEditCommand{DeleteModule{.module_index = edit.module_index}};
            } else if constexpr (std::is_same_v<Edit, MoveDeclaration>) {
                auto const declaration_it{
                    std::ranges::find(declarations_, edit.declaration, &DeclarationInfo::id)};
                if (declaration_it == declarations_.end()) {
                    return std::unexpected{SchemaEditError{"Unknown declaration"}};
                }
                if (edit.module_index >= manifest_.modules.size()) {
                    return std::unexpected{SchemaEditError{"Unknown destination module index"}};
                }
                if (edit.module_index == declaration_it->module_index) {
                    return std::nullopt;
                }
                if (declaration_it->source.has_value() &&
                    pending_module_sources_.contains(edit.module_index)) {
                    return std::unexpected{SchemaEditError{
                        "Save a newly created module before moving source-backed declarations into "
                        "it"}};
                }

                auto const& source_settings{std::visit(
                    [](auto const& module) -> codegen::ModuleSettings const& {
                        return module.settings;
                    },
                    manifest_.modules[declaration_it->module_index])};
                auto const& destination_settings{std::visit(
                    [](auto const& module) -> codegen::ModuleSettings const& {
                        return module.settings;
                    },
                    manifest_.modules[edit.module_index])};

                auto restored_source{std::optional<SourceRange>{}};
                if (edit.restore_source_ownership) {
                    auto const tombstone{source_tombstones_.find(edit.declaration)};
                    if (tombstone == source_tombstones_.end()) {
                        return std::unexpected{SchemaEditError{
                            "Moved declaration has no source ownership to restore"}};
                    }
                    restored_source = tombstone->second;
                }

                auto const previous_manifest{manifest_};
                auto const previous_declarations{declarations_};
                auto const previous_tombstones{source_tombstones_};
                // Snapshot for undo before declarations_ is modified.
                // NOLINTNEXTLINE(performance-unnecessary-copy-initialization)
                auto const original_info{*declaration_it};
                auto const target{types_.find(original_info.identity)};
                auto const owned_types{types_.types_for_declaration(original_info.identity)};
                if (!target.has_value()) {
                    auto const* normal{std::get_if<codegen::NormalModuleSchema>(
                        &manifest_.modules[original_info.module_index])};
                    if (normal == nullptr ||
                        codegen::has_primary_semantic_type(
                            normal->declarations[original_info.declaration_index])) {
                        return std::unexpected{
                            SchemaEditError{"Declaration is missing from the type graph"}};
                    }
                }
                auto const namespace_changed{source_settings.namespace_name !=
                                             destination_settings.namespace_name};
                if (namespace_changed) {
                    for (auto const& [registered_name, cpp_type] : manifest_.types) {
                        static_cast<void>(cpp_type);
                        auto const registered{types_.find_registered(registered_name)};
                        if (registered.has_value() &&
                            std::ranges::find(owned_types, *registered) != owned_types.end()) {
                            return std::unexpected{SchemaEditError{
                                "Moving a declaration registered as '@" + registered_name +
                                "' across namespaces requires source-aware types-registry "
                                "editing"}};
                        }
                    }
                }
                try {
                    auto& moving{std::get<codegen::NormalModuleSchema>(
                                     manifest_.modules[original_info.module_index])
                                     .declarations.at(original_info.declaration_index)};
                    if (auto const* soa{std::get_if<codegen::SoaSchema>(&moving)};
                        soa != nullptr && std::ranges::any_of(soa->members, [](auto const& member) {
                            return member.nested_schema.has_value() ||
                                   member.fixed_schema.has_value();
                        })) {
                        throw std::invalid_argument{
                            "Cannot move a SoA across modules while module-local nested/fixed "
                            "schema references exist"};
                    }
                    codegen::visit_type_references(
                        moving, [&](std::string const&, codegen::TypeRef& reference) {
                            auto const binding{types_.find_reference(
                                reference, original_info.identity.module_name)};
                            if (reference.name.starts_with('@') || !binding.has_value() ||
                                std::ranges::find(owned_types, *binding) != owned_types.end()) {
                                return;
                            }
                            auto const& node{types_.type(*binding)};
                            reference.name = node.identity.module_name == destination_settings.name
                                               ? node.identity.name
                                               : node.cpp_spelling;
                        });
                    for (auto const owned : owned_types) {
                        auto const& name{types_.type(owned).identity.name};
                        auto const new_spelling{destination_settings.namespace_name.has_value()
                                                    ? *destination_settings.namespace_name +
                                                          "::" + name
                                                    : name};
                        repair_semantic_references(manifest_,
                                                   types_,
                                                   declarations_,
                                                   owned,
                                                   new_spelling,
                                                   original_info.module_index,
                                                   original_info.identity.name,
                                                   LocalSoaReferencePolicy::reject,
                                                   name,
                                                   edit.module_index);
                    }
                } catch (std::exception const& error) {
                    manifest_ = previous_manifest;
                    return std::unexpected{SchemaEditError{error.what()}};
                }
                auto move_schema = [&]<typename Schema>(std::vector<Schema>& source,
                                                        std::size_t const source_index,
                                                        std::vector<Schema>& destination)
                    -> std::expected<std::size_t, SchemaEditError> {
                    if (source_index >= source.size()) {
                        return std::unexpected{
                            SchemaEditError{"Declaration has an invalid source position"}};
                    }
                    auto const insertion_index{edit.insertion_index.value_or(destination.size())};
                    if (insertion_index > destination.size()) {
                        return std::unexpected{
                            SchemaEditError{"Invalid destination insertion index"}};
                    }
                    auto schema{std::move(source[source_index])};
                    source.erase(source.begin() + static_cast<std::ptrdiff_t>(source_index));
                    destination.insert(destination.begin() +
                                           static_cast<std::ptrdiff_t>(insertion_index),
                                       std::move(schema));
                    return source_index;
                };

                auto moved{std::expected<std::size_t, SchemaEditError>{
                    std::unexpected{SchemaEditError{"Destination module is incompatible"}}}};
                auto& source_module{manifest_.modules[original_info.module_index]};
                auto& destination_module{manifest_.modules[edit.module_index]};
                if (auto* source{std::get_if<codegen::NormalModuleSchema>(&source_module)}) {
                    auto* destination{
                        std::get_if<codegen::NormalModuleSchema>(&destination_module)};
                    if (destination != nullptr) {
                        moved = move_schema(source->declarations,
                                            original_info.declaration_index,
                                            destination->declarations);
                    }
                }
                if (!moved.has_value()) {
                    manifest_ = previous_manifest;
                    return std::unexpected{std::move(moved.error())};
                }

                remember_source_tombstone(original_info);
                declaration_it->module_index = edit.module_index;
                declaration_it->source = restored_source;
                try {
                    codegen::validate_manifest(manifest_);
                    auto resolved{resolve_type_graph(manifest_)};
                    refresh_declaration_locations(manifest_, resolved, declarations_);
                    validate_relocated_references(manifest_,
                                                  types_,
                                                  resolved,
                                                  original_info.identity,
                                                  declaration_it->identity);
                    types_ = std::move(resolved);
                } catch (std::exception const& error) {
                    manifest_ = previous_manifest;
                    declarations_ = previous_declarations;
                    source_tombstones_ = previous_tombstones;
                    return std::unexpected{SchemaEditError{error.what()}};
                }
                return SchemaEditCommand{
                    MoveDeclaration{.declaration = edit.declaration,
                                    .module_index = original_info.module_index,
                                    .insertion_index = *moved,
                                    .restore_source_ownership = original_info.source.has_value()}};
            } else if constexpr (std::is_same_v<Edit, SetEnumeratorDisplayName>) {
                auto const* info{declaration(edit.enum_declaration)};
                if (info == nullptr) {
                    return std::unexpected{SchemaEditError{"Unknown declaration id"}};
                }
                auto* schema{normal_schema_at<codegen::EnumSchema>(manifest_, *info)};

                if (schema == nullptr) {
                    return std::unexpected{
                        SchemaEditError{"Display names can only be edited on enum declarations"}};
                }
                auto value{std::ranges::find(
                    schema->values, edit.enumerator_name, &codegen::EnumeratorSchema::name)};
                if (value == schema->values.end()) {
                    return std::unexpected{SchemaEditError{"Enum '" + schema->name +
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
                auto* schema{normal_schema_at<codegen::EnumSchema>(manifest_, *info)};

                if (schema == nullptr) {
                    return std::unexpected{
                        SchemaEditError{"Enumerator names can only be edited on enums"}};
                }
                auto value{std::ranges::find(
                    schema->values, edit.current_name, &codegen::EnumeratorSchema::name)};
                if (value == schema->values.end()) {
                    return std::unexpected{SchemaEditError{"Enum '" + schema->name +
                                                           "' has no enumerator named '" +
                                                           edit.current_name + "'"}};
                }
                if (edit.current_name == edit.new_name) {
                    return std::nullopt;
                }

                auto const previous_count{schema->count};
                value->name = edit.new_name;
                if (schema->count == edit.current_name) {
                    schema->count = edit.new_name;
                }
                try {
                    auto resolved{resolve_type_graph(manifest_)};
                    types_ = std::move(resolved);
                } catch (std::exception const& error) {
                    value->name = edit.current_name;
                    schema->count = previous_count;
                    return std::unexpected{SchemaEditError{error.what()}};
                }
                return SchemaEditCommand{
                    SetEnumeratorName{.enum_declaration = edit.enum_declaration,
                                      .current_name = edit.new_name,
                                      .new_name = edit.current_name}};
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
                auto const owned_types{types_.types_for_declaration(declaration_it->identity)};
                if (owned_types.empty()) {
                    return std::unexpected{
                        SchemaEditError{"Declaration is missing from the type graph"}};
                }
                if (target.has_value() &&
                    std::holds_alternative<SoaType>(types_.type(*target).definition) &&
                    soa_schema(edit.declaration) == nullptr) {
                    return std::unexpected{
                        SchemaEditError{"Only struct SoA declarations can be renamed"}};
                }
                for (auto const& [registered_name, cpp_type] : manifest_.types) {
                    static_cast<void>(cpp_type);
                    auto const registered{types_.find_registered(registered_name)};
                    if (registered.has_value() &&
                        std::ranges::find(owned_types, *registered) != owned_types.end()) {
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
                try {
                    for (auto const owned : owned_types) {
                        auto const& node{types_.type(owned)};
                        auto const generated_name{
                            node.owning_declaration.has_value()
                                ? "F" + edit.new_name +
                                      node.identity.name.substr(1 + old_name.size())
                                : edit.new_name};
                        auto const spelling{target_settings.namespace_name.has_value()
                                                ? *target_settings.namespace_name +
                                                      "::" + generated_name
                                                : generated_name};
                        repair_semantic_references(manifest_,
                                                   types_,
                                                   declarations_,
                                                   owned,
                                                   spelling,
                                                   declaration_it->module_index,
                                                   old_name,
                                                   LocalSoaReferencePolicy::rename,
                                                   generated_name,
                                                   declaration_it->module_index);
                    }
                    auto& target_module{manifest_.modules[declaration_it->module_index]};
                    auto& normal{std::get<codegen::NormalModuleSchema>(target_module)};
                    auto& schema{normal.declarations.at(declaration_it->declaration_index)};
                    std::visit([&](auto& value) { value.name = edit.new_name; }, schema);
                    declaration_it->identity.name = edit.new_name;
                    codegen::validate_manifest(manifest_);
                    auto resolved{resolve_type_graph(manifest_)};
                    auto old_identity{declaration_it->identity};
                    old_identity.name = old_name;
                    validate_relocated_references(
                        manifest_, types_, resolved, old_identity, declaration_it->identity);
                    types_ = std::move(resolved);
                } catch (std::exception const& error) {
                    manifest_ = previous_manifest;
                    declarations_ = previous_declarations;
                    return std::unexpected{SchemaEditError{error.what()}};
                }
                return SchemaEditCommand{
                    RenameDeclaration{.declaration = edit.declaration, .new_name = old_name}};
            } else {
                return std::unexpected{
                    SchemaEditError{"Declaration command requires a normal module"}};
            }
        },
        command);
}

auto load_editable_schema_document(std::filesystem::path const& types_path,
                                   std::span<std::filesystem::path const> const module_paths)
    -> EditableSchemaDocument {
    auto registry{codegen::load_type_registry(types_path)};
    auto manifest{codegen::load_sources(registry, module_paths)};
    std::vector<SchemaSourceFile> sources;
    for (auto& source : registry.sources) {
        sources.push_back({.path = std::move(source.path),
                           .text = std::move(source.text),
                           .kind = SchemaSourceKind::type_registry,
                           .includes = std::move(source.includes)});
    }

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
            if (auto const* normal{std::get_if<codegen::NormalModuleSchema>(&module)}) {

                auto declaration_index{std::size_t{}};
                for (auto const& child : form.children) {
                    if (declaration_index == normal->declarations.size()) {
                        break;
                    }
                    auto const& expected{normal->declarations[declaration_index]};
                    if (child.head() != codegen::declaration_head(expected)) {
                        continue;
                    }
                    if (child.children.size() < 2 ||
                        child.children[1].token.text != codegen::declaration_name(expected)) {
                        throw std::logic_error{"Source declaration does not match loaded module"};
                    }
                    declaration_ranges.push_back(form_range(child, source_file_index));
                    ++declaration_index;
                }
                if (declaration_index != expected_count) {
                    throw std::logic_error{"Source declaration count does not match loaded module"};
                }
                continue;
            }
            throw std::logic_error{"Noncanonical module has declarations"};
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
    for (auto const& [name, range] : registry.declarations) {
        result.registered_type_sources_.emplace(name,
                                                SourceRange{range.source_file_index,
                                                            range.begin_offset,
                                                            range.end_offset,
                                                            range.line,
                                                            range.column});
    }
    return result;
}

} // namespace lispb::schema
