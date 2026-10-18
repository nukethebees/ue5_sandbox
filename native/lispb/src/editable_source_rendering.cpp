#include "editable_source_rendering.h"

#include <codegen/ast/access.h>
#include <codegen/ast/record_kind.h>
#include <codegen/schema.h>
#include <codegen/schema/declaration_handle.h>
#include <codegen/schema/declaration_schema.h>
#include <codegen/schema/declarations.h>
#include <codegen/schema/enum_conversion.h>
#include <codegen/schema/enum_reflection.h>
#include <codegen/schema/enum_schemas.h>
#include <codegen/schema/enum_unreal_projections.h>
#include <codegen/schema/enumerator_schemas.h>
#include <codegen/schema/facade_method_schemas.h>
#include <codegen/schema/facade_schemas.h>
#include <codegen/schema/fixed_point_rounding.h>
#include <codegen/schema/fixed_point_schemas.h>
#include <codegen/schema/fixed_soa_schemas.h>
#include <codegen/schema/function_schemas.h>
#include <codegen/schema/homogeneous_layout_schemas.h>
#include <codegen/schema/homogeneous_value_schemas.h>
#include <codegen/schema/indices.h>
#include <codegen/schema/integer_scalar_cpp_emission.h>
#include <codegen/schema/integer_scalar_schemas.h>
#include <codegen/schema/integer_varint_encoding.h>
#include <codegen/schema/integer_varint_schemas.h>
#include <codegen/schema/linear_quantized_schemas.h>
#include <codegen/schema/mini_float_schemas.h>
#include <codegen/schema/module_handle.h>
#include <codegen/schema/module_settings_store.h>
#include <codegen/schema/native_enum_helper.h>
#include <codegen/schema/normal_module_schemas.h>
#include <codegen/schema/optional_presence_bit_schemas.h>
#include <codegen/schema/optional_sentinel_schemas.h>
#include <codegen/schema/packed_bit_order.h>
#include <codegen/schema/packed_byte_order.h>
#include <codegen/schema/packed_field_kind.h>
#include <codegen/schema/packed_field_schemas.h>
#include <codegen/schema/packed_integer_value.h>
#include <codegen/schema/packed_named_code_schemas.h>
#include <codegen/schema/packed_reserved_bits_schemas.h>
#include <codegen/schema/packed_segment_id.h>
#include <codegen/schema/packed_segment_kind.h>
#include <codegen/schema/packed_value_schemas.h>
#include <codegen/schema/parameter_schemas.h>
#include <codegen/schema/quantization_clipping.h>
#include <codegen/schema/record_comparison.h>
#include <codegen/schema/record_member_schemas.h>
#include <codegen/schema/record_schemas.h>
#include <codegen/schema/schema_range.h>
#include <codegen/schema/semantic_relation_kind.h>
#include <codegen/schema/semantic_relation_schemas.h>
#include <codegen/schema/semantic_relation_unit.h>
#include <codegen/schema/soa_allocators.h>
#include <codegen/schema/soa_backend.h>
#include <codegen/schema/soa_mask_dimension_schemas.h>
#include <codegen/schema/soa_member_kind.h>
#include <codegen/schema/soa_member_schemas.h>
#include <codegen/schema/soa_schemas.h>
#include <codegen/schema/soa_storage.h>
#include <codegen/schema/static_table_column_schemas.h>
#include <codegen/schema/static_table_group_schemas.h>
#include <codegen/schema/static_table_row_schemas.h>
#include <codegen/schema/static_table_schemas.h>
#include <codegen/schema/storage_operation.h>
#include <codegen/schema/tagged_union_alternative_schemas.h>
#include <codegen/schema/tagged_union_schemas.h>
#include <codegen/schema/type_refs.h>
#include <codegen/schema/type_registry.h>
#include <codegen/schema/union_alternative_schemas.h>
#include <codegen/schema/union_schemas.h>
#include <codegen/schema/vector_soa_schemas.h>
#include <codegen/sexpr/reader.h>
#include <codegen/sexpr/sources.h>
#include <codegen/sexpr/syntax.h>
#include <codegen/sexpr/token_kind.h>
#include <lispb/compiler_storage.h>
#include <lispb/indices.h>
#include <lispb/schema/source_range.h>
#include <lispb/strings.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <ranges>
#include <set>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace lispb::schema::detail {
namespace {

using codegen::sexpr::FormId;
class SourceRenderer {
  public:
    explicit SourceRenderer(CompilerStorage& storage)
        : storage_{storage}
        , store_{storage.declarations} {}

    auto text(StringId value) const -> std::string_view { return storage_.strings.view(value); }
    auto text(IdentifierId value) const -> std::string_view { return storage_.strings.view(value); }
    auto quote(StringId value) -> std::string { return quote(text(value)); }
    template <typename Id>
    auto optional_text(std::optional<Id> value) const -> std::optional<std::string> {
        if (!value) {
            return std::nullopt;
        }
        return std::string{text(*value)};
    }
    static auto access_keyword(codegen::Access access) -> std::string_view {
        switch (access) {
            case codegen::Access::public_:
                return "public";
            case codegen::Access::private_:
                return "private";
            case codegen::Access::protected_:
                return "protected";
        }
        std::unreachable();
    }
    auto segment_name(codegen::PackedSegmentId id) const -> std::string_view {
        return text(id.kind() == codegen::PackedSegmentKind::field
                        ? store_.packed_fields.name[store_.packed_fields.index(id)]
                        : store_.packed_reserved_bits.name[store_.packed_reserved_bits.index(id)]);
    }
    template <typename Id>
    auto items(codegen::SchemaRange<Id> range) const {
        return store_.items(range);
    }
    template <typename Id>
    auto texts(codegen::SchemaRange<Id> range) const {
        return store_.items(range) | std::views::transform([this](Id id) { return text(id); });
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

    auto render_type_ref(codegen::TypeRefId id) -> std::string {
        auto const type{id};
        if (text(store_.type_refs.suffix[store_.type_refs.index(type)]).empty() &&
            !store_.type_refs.nested[store_.type_refs.index(type)].has_value()) {
            return std::string{text(store_.type_refs.name[store_.type_refs.index(type)])};
        }
        auto result{std::string{"(type-ref "} +
                    std::string{text(store_.type_refs.name[store_.type_refs.index(type)])}};
        if (!text(store_.type_refs.suffix[store_.type_refs.index(type)]).empty()) {
            result +=
                " :suffix " + quote(text(store_.type_refs.suffix[store_.type_refs.index(type)]));
        }
        if (store_.type_refs.nested[store_.type_refs.index(type)].has_value()) {
            result +=
                " :nested " + quote(text(*store_.type_refs.nested[store_.type_refs.index(type)]));
        }
        return result + ')';
    }

    auto render_enum_value(lispb::codegen::EnumeratorId const& value) -> std::string {
        std::ostringstream output;
        output << "(value " << text(store_.enumerators.name[store_.enumerators.index(value)]);
        if (store_.enumerators.initializer[store_.enumerators.index(value)].has_value()) {
            output << "\n      :value "
                   << quote(text(*store_.enumerators.initializer[store_.enumerators.index(value)]));
        }
        if (store_.enumerators.display_name[store_.enumerators.index(value)].has_value()) {
            output << "\n      :display-name "
                   << quote(
                          text(*store_.enumerators.display_name[store_.enumerators.index(value)]));
        }
        if (store_.enumerators.hidden[store_.enumerators.index(value)]) {
            output << "\n      :hidden true";
        }
        if (store_.enumerators.serialized_name[store_.enumerators.index(value)].has_value()) {
            output << "\n      :serialized-name "
                   << quote(text(
                          *store_.enumerators.serialized_name[store_.enumerators.index(value)]));
        }
        if (store_.enumerators.sentinel[store_.enumerators.index(value)]) {
            output << "\n      :sentinel true";
        }
        output << ')';
        return output.str();
    }

    auto render_enum_unreal_projection(lispb::codegen::EnumUnrealProjectionId const& projection)
        -> std::string {
        std::ostringstream output;
        output
            << "(unreal-projection "
            << text(store_.enum_unreal_projections
                        .name[store_.enum_unreal_projections.index(projection)])
            << "\n      :header "
            << quote(text(store_.enum_unreal_projections
                              .header[store_.enum_unreal_projections.index(projection)]))
            << "\n      :header-include "
            << quote(text(store_.enum_unreal_projections
                              .header_include[store_.enum_unreal_projections.index(projection)]))
            << "\n      :conversion-header "
            << quote(text(store_.enum_unreal_projections
                              .conversion_header[store_.enum_unreal_projections.index(projection)]))
            << "\n      :native-header-include "
            << quote(text(
                   store_.enum_unreal_projections
                       .native_header_include[store_.enum_unreal_projections.index(projection)]));
        if (store_.enum_unreal_projections
                .enum_array[store_.enum_unreal_projections.index(projection)]) {
            output << "\n      :enum-array true";
        }
        if (store_.enum_unreal_projections
                .reflection[store_.enum_unreal_projections.index(projection)] !=
            codegen::EnumReflection::uenum) {
            output << "\n      :reflection "
                   << codegen::enum_reflection_name(
                          store_.enum_unreal_projections
                              .reflection[store_.enum_unreal_projections.index(projection)]);
        }
        output << ')';
        return output.str();
    }

    auto render_enum(lispb::codegen::DeclarationHandle const& schema) -> std::string {
        std::ostringstream output;
        output << "(enum " << text(store_.enums.name[store_.enums.index(schema)]);
        if (store_.enums.underlying_type[store_.enums.index(schema)].has_value()) {
            output << ' '
                   << render_type_ref(*store_.enums.underlying_type[store_.enums.index(schema)]);
        }
        if (store_.enums.bit_width[store_.enums.index(schema)].has_value()) {
            output << "\n    :bit-width "
                   << static_cast<unsigned>(*store_.enums.bit_width[store_.enums.index(schema)]);
        }
        if (store_.enums.signedness[store_.enums.index(schema)] != EnumSignedness::unspecified) {
            output << "\n    :signed "
                   << (store_.enums.signedness[store_.enums.index(schema)] ==
                               EnumSignedness::signed_
                           ? "true"
                           : "false");
        }
        if (store_.enums.reflection[store_.enums.index(schema)] != codegen::EnumReflection::none) {
            output << "\n    :reflection "
                   << codegen::enum_reflection_name(
                          store_.enums.reflection[store_.enums.index(schema)]);
        }
        if (store_.enums.enum_array[store_.enums.index(schema)]) {
            output << "\n    :enum-array true";
        }
        if (store_.enums.count[store_.enums.index(schema)].has_value()) {
            output << "\n    :count " << text(*store_.enums.count[store_.enums.index(schema)]);
        }
        if (!items(store_.enums.conversions[store_.enums.index(schema)]).empty()) {
            output << "\n    :conversions (";
            for (std::size_t index{};
                 index < items(store_.enums.conversions[store_.enums.index(schema)]).size();
                 ++index) {
                output << (index == 0 ? "" : " ")
                       << codegen::enum_conversion_name(
                              items(store_.enums.conversions[store_.enums.index(schema)])[index]);
            }
            output << ')';
        }
        if (store_.enums.export_specifier[store_.enums.index(schema)].value != 0) {
            output << "\n    :export-specifier "
                   << text(store_.enums.export_specifier[store_.enums.index(schema)]);
        }
        if (store_.enums.native_api[store_.enums.index(schema)]) {
            output << "\n    :native-api true";
        }
        if (!items(store_.enums.helpers[store_.enums.index(schema)]).empty()) {
            output << "\n    :helpers (";
            for (std::size_t index{};
                 index < items(store_.enums.helpers[store_.enums.index(schema)]).size();
                 ++index) {
                output << (index == 0 ? "" : " ")
                       << codegen::native_enum_helper_name(
                              items(store_.enums.helpers[store_.enums.index(schema)])[index]);
            }
            output << ')';
        }
        for (auto const& value : items(store_.enums.values[store_.enums.index(schema)])) {
            output << "\n    " << render_enum_value(value);
        }
        if (store_.enums.unreal_projection[store_.enums.index(schema)].has_value()) {
            output << "\n    "
                   << render_enum_unreal_projection(
                          *store_.enums.unreal_projection[store_.enums.index(schema)]);
        }
        output << ')';
        return output.str();
    }

    auto source_form_end(FormId const form, std::string_view) -> std::optional<std::size_t> {
        return storage_.syntax.span(form).end_offset;
    }

    auto source_form_matches_rendered(FormId const form, std::string_view const rendered) -> bool {
        if (!storage_.syntax.is_list(form)) {
            if (storage_.syntax.kind(form) == codegen::sexpr::TokenKind::string) {
                return rendered == storage_.syntax.text(form) ||
                       rendered == quote(storage_.syntax.text(form));
            }
            return rendered == storage_.syntax.text(form);
        }
        std::size_t cursor{};
        auto skip_space = [&] {
            while (cursor < rendered.size() &&
                   std::isspace(static_cast<unsigned char>(rendered[cursor]))) {
                ++cursor;
            }
        };
        auto match = [&](auto&& self, FormId const source) -> bool {
            skip_space();
            if (storage_.syntax.is_list(source)) {
                if (cursor == rendered.size() || rendered[cursor++] != '(') {
                    return false;
                }
                for (auto const child : storage_.syntax.children(source)) {
                    if (!self(self, child)) {
                        return false;
                    }
                }
                skip_space();
                return cursor < rendered.size() && rendered[cursor++] == ')';
            }
            auto const token{storage_.syntax.token(source)};
            auto const expected{storage_.syntax.kind(token) == codegen::sexpr::TokenKind::string
                                    ? quote(storage_.syntax.text(token))
                                : storage_.syntax.kind(token) == codegen::sexpr::TokenKind::keyword
                                    ? ":" + std::string{storage_.syntax.text(token)}
                                    : std::string{storage_.syntax.text(token)}};
            if (!rendered.substr(cursor).starts_with(expected)) {
                return false;
            }
            cursor += expected.size();
            return cursor == rendered.size() || rendered[cursor] == ')' ||
                   std::isspace(static_cast<unsigned char>(rendered[cursor]));
        };
        if (!match(match, form)) {
            return false;
        }
        skip_space();
        return cursor == rendered.size();
    }

    struct SourceReplacement {
        std::size_t begin{};
        std::size_t end{};
        std::string text;
    };

    using SourceProperty = std::pair<std::string_view, std::optional<std::string>>;

    auto replace_source_form(FormId const target,
                             std::string text,
                             std::string_view const original,
                             std::vector<SourceReplacement>& replacements) -> bool {
        auto const end{source_form_end(target, original)};
        if (!end.has_value()) {
            return false;
        }
        replacements.push_back({.begin = storage_.syntax.token_span(target).offset,
                                .end = *end,
                                .text = std::move(text)});
        return true;
    }

    auto patch_source_form(FormId const target,
                           std::string_view const rendered,
                           std::string_view const original,
                           std::vector<SourceReplacement>& replacements) -> bool {
        auto const span{storage_.syntax.span(target)};
        return original.substr(span.offset, span.end_offset - span.offset) == rendered ||
               source_form_matches_rendered(target, rendered) ||
               replace_source_form(target, std::string{rendered}, original, replacements);
    }

    auto patch_source_properties(FormId const target,
                                 std::size_t const positional_count,
                                 std::span<SourceProperty const> properties,
                                 std::string_view const indentation,
                                 std::string_view const original,
                                 std::vector<SourceReplacement>& replacements) -> bool {
        std::map<std::string_view, std::pair<FormId, FormId>> existing;
        for (auto index{positional_count + 1}; index < storage_.syntax.children(target).size();) {
            auto const& child{storage_.syntax.children(target)[index]};
            if (storage_.syntax.kind(child) == codegen::sexpr::TokenKind::keyword &&
                index + 1 < storage_.syntax.children(target).size()) {
                existing.emplace(storage_.syntax.text(child),
                                 std::pair{child, storage_.syntax.children(target)[index + 1]});
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
                if (!patch_source_form(existing_value, *value, original, replacements)) {
                    return false;
                }
            } else {
                auto const keyword_end{source_form_end(keyword, original)};
                auto const value_end{source_form_end(existing_value, original)};
                if (!keyword_end.has_value() || !value_end.has_value()) {
                    return false;
                }
                replacements.push_back({.begin = storage_.syntax.token_span(keyword).offset,
                                        .end = *keyword_end,
                                        .text = {}});
                replacements.push_back({.begin = storage_.syntax.token_span(existing_value).offset,
                                        .end = *value_end,
                                        .text = {}});
            }
        }
        if (!additions.empty()) {
            replacements.push_back({.begin = storage_.syntax.closing_span(target).offset,
                                    .end = storage_.syntax.closing_span(target).offset,
                                    .text = std::move(additions)});
        }
        return true;
    }

    auto source_property_matches(FormId const target,
                                 std::size_t const positional_count,
                                 std::string_view const name,
                                 std::optional<std::string> const& expected) -> bool {
        for (auto index{positional_count + 1}; index + 1 < storage_.syntax.children(target).size();
             ++index) {
            if (storage_.syntax.kind(storage_.syntax.children(target)[index]) !=
                    codegen::sexpr::TokenKind::keyword ||
                storage_.syntax.text(storage_.syntax.children(target)[index]) != name) {
                continue;
            }
            return expected.has_value() &&
                   source_form_matches_rendered(storage_.syntax.children(target)[index + 1],
                                                *expected);
        }
        return !expected.has_value();
    }

    auto apply_replacements(std::string_view const original,
                            std::vector<SourceReplacement> replacements)
        -> std::optional<std::string> {
        std::ranges::sort(replacements,
                          [&](SourceReplacement const& left, SourceReplacement const& right) {
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
                replacement.begin, replacement.end - replacement.begin, replacement.text);
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
        return apply_replacements(original.substr(begin, end - begin), std::move(replacements));
    }

    auto apply_source_replacements(std::string_view const original,
                                   std::vector<SourceReplacement> replacements)
        -> std::optional<std::string> {
        auto const form{original_->form};
        auto const name{storage_.syntax.children(form)[1]};
        if (storage_.syntax.text(name) != text(current_name_)) {
            replacements.push_back({storage_.syntax.span(name).offset,
                                    storage_.syntax.span(name).end_offset,
                                    std::string{text(current_name_)}});
        }
        return apply_source_replacements_to_range(original,
                                                  storage_.syntax.span(form).offset,
                                                  storage_.syntax.span(form).end_offset,
                                                  std::move(replacements));
    }

    auto source_form_line_end(FormId const form,
                              std::string_view const source,
                              std::size_t const limit) -> std::optional<std::size_t> {
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

    auto source_declaration(std::string_view const head) const -> std::optional<FormId> {
        auto const form{original_->form};
        return storage_.syntax.head(form) == head ? std::optional{form} : std::nullopt;
    }

    template <typename FieldsMatch>
    auto infer_single_positional_rename(auto const& columns,
                                        std::span<FormId const> const source_rows,
                                        auto const& current_rows,
                                        FieldsMatch&& fields_match)
        -> std::optional<std::pair<std::string_view, std::string_view>> {
        if (source_rows.size() != current_rows.size()) {
            return std::nullopt;
        }

        std::optional<std::pair<std::string_view, std::string_view>> renamed;
        for (std::size_t index{}; index < source_rows.size(); ++index) {
            if (storage_.syntax.children(source_rows[index]).size() < 2) {
                return std::nullopt;
            }
            auto const& source_name{
                storage_.syntax.text(storage_.syntax.children(source_rows[index])[1])};
            auto const& current_name{text(columns.name[columns.index(current_rows[index])])};
            if (source_name == current_name) {
                continue;
            }
            if (renamed.has_value()) {
                return std::nullopt;
            }
            if (!fields_match(source_rows[index], current_rows[index])) {
                return std::nullopt;
            }
            renamed = std::pair{std::string_view{source_name}, std::string_view{current_name}};
        }

        if (!renamed.has_value()) {
            return std::nullopt;
        }
        auto const source_has_new_name{std::ranges::any_of(source_rows, [&](FormId const row) {
            return storage_.syntax.text(storage_.syntax.children(row)[1]) == renamed->second;
        })};
        auto const current_has_old_name{
            std::ranges::find(current_rows, renamed->first, [&](auto const child) {
                return text(columns.name[columns.index(child)]);
            }) != current_rows.end()};
        if (source_has_new_name || current_has_old_name) {
            return std::nullopt;
        }
        return renamed;
    }

    auto try_render_source_preserved_enum(lispb::codegen::DeclarationHandle const& schema,
                                          std::string_view const original)
        -> std::optional<std::string> {
        auto parsed{source_declaration("enum")};
        if (!parsed.has_value()) {
            return std::nullopt;
        }
        auto const& form{*parsed};
        if (storage_.syntax.children(form).size() < 2) {
            return std::nullopt;
        }
        auto const source_has_underlying{
            storage_.syntax.children(form).size() > 2 &&
            storage_.syntax.kind(storage_.syntax.children(form)[2]) !=
                codegen::sexpr::TokenKind::keyword &&
            (!storage_.syntax.is_list(storage_.syntax.children(form)[2]) ||
             storage_.syntax.head(storage_.syntax.children(form)[2]) == "type-ref")};
        if (store_.enums.underlying_type[store_.enums.index(schema)].has_value() &&
            (!text(store_.type_refs.suffix[store_.type_refs.index(
                       *store_.enums.underlying_type[store_.enums.index(schema)])])
                  .empty() ||
             store_.type_refs
                 .nested[store_.type_refs.index(
                     *store_.enums.underlying_type[store_.enums.index(schema)])]
                 .has_value())) {
            return std::nullopt;
        }

        std::vector<FormId> values;
        std::optional<FormId> source_projection{};
        for (auto const& child : storage_.syntax.children(form)) {
            if (storage_.syntax.head(child) == "unreal-projection") {
                if (source_projection.has_value()) {
                    return std::nullopt;
                }
                source_projection = child;
            }
            if (storage_.syntax.head(child) == "value") {
                values.push_back(child);
            }
        }
        for (auto const& value : values) {
            if (storage_.syntax.children(value).size() < 2) {
                return std::nullopt;
            }
        }

        std::vector<SourceReplacement> replacements;

        if (source_projection.has_value() &&
            store_.enums.unreal_projection[store_.enums.index(schema)].has_value()) {
            auto const& projection{*store_.enums.unreal_projection[store_.enums.index(schema)]};
            if (storage_.syntax.children(*source_projection).size() < 2 ||
                !patch_source_form(
                    storage_.syntax.children(*source_projection)[1],
                    text(store_.enum_unreal_projections
                             .name[store_.enum_unreal_projections.index(projection)]),
                    original,
                    replacements)) {
                return std::nullopt;
            }
            auto const projection_properties{std::array<SourceProperty, 6>{
                std::pair{"enum-array",
                          store_.enum_unreal_projections
                                  .enum_array[store_.enum_unreal_projections.index(projection)]
                              ? std::optional<std::string>{"true"}
                              : std::nullopt},
                std::pair{"header",
                          std::optional{quote(text(
                              store_.enum_unreal_projections
                                  .header[store_.enum_unreal_projections.index(projection)]))}},
                std::pair{
                    "header-include",
                    std::optional{quote(
                        store_.enum_unreal_projections
                            .header_include[store_.enum_unreal_projections.index(projection)])}},
                std::pair{"conversion-header",
                          std::optional{
                              quote(text(store_.enum_unreal_projections.conversion_header
                                             [store_.enum_unreal_projections.index(projection)]))}},
                std::pair{
                    "native-header-include",
                    std::optional{quote(store_.enum_unreal_projections.native_header_include
                                            [store_.enum_unreal_projections.index(projection)])}},
                std::pair{
                    "reflection",
                    store_.enum_unreal_projections.reflection[store_.enum_unreal_projections.index(
                        projection)] != codegen::EnumReflection::uenum
                        ? std::optional{std::string{codegen::enum_reflection_name(
                              store_.enum_unreal_projections
                                  .reflection[store_.enum_unreal_projections.index(projection)])}}
                        : std::nullopt}}};
            if (!patch_source_properties(*source_projection,
                                         1,
                                         projection_properties,
                                         "      ",
                                         original,
                                         replacements)) {
                return std::nullopt;
            }
        } else if (source_projection.has_value()) {
            auto begin{storage_.syntax.token_span(*source_projection).offset};
            while (begin > 0 && original[begin - 1] != '\n' &&
                   std::isspace(static_cast<unsigned char>(original[begin - 1])) != 0) {
                --begin;
            }
            auto const end{source_form_line_end(
                *source_projection, original, storage_.syntax.closing_span(form).offset)};
            if (!end.has_value()) {
                return std::nullopt;
            }
            replacements.push_back({.begin = begin, .end = *end, .text = {}});
        } else if (store_.enums.unreal_projection[store_.enums.index(schema)].has_value()) {
            replacements.push_back(
                {.begin = storage_.syntax.closing_span(form).offset,
                 .end = storage_.syntax.closing_span(form).offset,
                 .text =
                     "\n    " + render_enum_unreal_projection(
                                    *store_.enums.unreal_projection[store_.enums.index(schema)])});
        }

        if (source_has_underlying &&
            store_.enums.underlying_type[store_.enums.index(schema)].has_value()) {
            auto const& underlying{storage_.syntax.children(form)[2]};
            if (storage_.syntax.text(underlying) !=
                    text(store_.type_refs.name[store_.type_refs.index(
                        *store_.enums.underlying_type[store_.enums.index(schema)])]) &&
                !replace_source_form(
                    underlying,
                    std::string{text(store_.type_refs.name[store_.type_refs.index(
                        *store_.enums.underlying_type[store_.enums.index(schema)])])},
                    original,
                    replacements)) {
                return std::nullopt;
            }
        } else if (source_has_underlying) {
            auto const& underlying{storage_.syntax.children(form)[2]};
            auto const underlying_end{source_form_end(underlying, original)};
            if (!underlying_end.has_value()) {
                return std::nullopt;
            }
            replacements.push_back({.begin = storage_.syntax.token_span(underlying).offset,
                                    .end = *underlying_end,
                                    .text = {}});
        } else if (store_.enums.underlying_type[store_.enums.index(schema)].has_value()) {
            auto const name_end{source_form_end(storage_.syntax.children(form)[1], original)};
            if (!name_end.has_value()) {
                return std::nullopt;
            }
            replacements.push_back(
                {.begin = *name_end,
                 .end = *name_end,
                 .text = " " + std::string{text(store_.type_refs.name[store_.type_refs.index(
                                   *store_.enums.underlying_type[store_.enums.index(schema)])])}});
        }

        std::vector<std::string> helper_names;
        for (auto const helper : items(store_.enums.helpers[store_.enums.index(schema)])) {
            helper_names.emplace_back(codegen::native_enum_helper_name(helper));
        }
        std::vector<std::string> conversion_names;
        conversion_names.reserve(
            items(store_.enums.conversions[store_.enums.index(schema)]).size());
        for (auto const conversion : items(store_.enums.conversions[store_.enums.index(schema)])) {
            conversion_names.emplace_back(codegen::enum_conversion_name(conversion));
        }
        auto const export_specifier{store_.enums.export_specifier[store_.enums.index(schema)]};
        auto const enum_properties{std::array<SourceProperty, 7>{
            std::pair{"bit-width",
                      store_.enums.bit_width[store_.enums.index(schema)].has_value()
                          ? std::optional{std::to_string(
                                *store_.enums.bit_width[store_.enums.index(schema)])}
                          : std::nullopt},
            std::pair{"signed",
                      store_.enums.signedness[store_.enums.index(schema)] !=
                              EnumSignedness::unspecified
                          ? std::optional{store_.enums.signedness[store_.enums.index(schema)] ==
                                                  EnumSignedness::signed_
                                              ? "true"
                                              : "false"}
                          : std::nullopt},
            std::pair{"reflection",
                      store_.enums.reflection[store_.enums.index(schema)] !=
                              codegen::EnumReflection::none
                          ? std::optional{std::string{codegen::enum_reflection_name(
                                store_.enums.reflection[store_.enums.index(schema)])}}
                          : std::nullopt},
            std::pair{"enum-array",
                      store_.enums.enum_array[store_.enums.index(schema)]
                          ? std::optional<std::string>{"true"}
                          : std::nullopt},
            std::pair{"count", optional_text(store_.enums.count[store_.enums.index(schema)])},
            std::pair{"export-specifier",
                      export_specifier.value != 0
                          ? std::optional{std::string{text(export_specifier)}}
                          : std::nullopt},
            std::pair{"native-api",
                      store_.enums.native_api[store_.enums.index(schema)]
                          ? std::optional<std::string>{"true"}
                          : std::nullopt}}};
        auto const positional_count{source_has_underlying ? 2U : 1U};
        if (!patch_source_properties(
                form, positional_count, enum_properties, "    ", original, replacements) ||
            !patch_source_atom_list_property(form,
                                             positional_count,
                                             "helpers",
                                             helper_names,
                                             "    ",
                                             "      ",
                                             original,
                                             replacements) ||
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
            return items(store_.enums.values[store_.enums.index(schema)]).empty()
                     ? apply_source_replacements(original, std::move(replacements))
                     : std::nullopt;
        }

        auto const first_value_offset{storage_.syntax.token_span(values.front()).offset};
        for (auto const& child : storage_.syntax.children(form)) {
            if (storage_.syntax.token_span(child).offset > first_value_offset &&
                storage_.syntax.head(child) != "value" &&
                storage_.syntax.head(child) != "unreal-projection") {
                return std::nullopt;
            }
        }

        auto values_begin{std::size_t{}};
        for (auto const& child : storage_.syntax.children(form)) {
            if (storage_.syntax.token_span(child).offset >= first_value_offset) {
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
            FormId form;
            std::size_t begin{};
            std::size_t end{};
        };
        std::map<std::string_view, SourceValue> source_values;
        auto row_begin{std::size_t{values_begin}};
        for (auto const& value : values) {
            auto const row_end{
                source_form_line_end(value, original, storage_.syntax.closing_span(form).offset)};
            if (!row_end.has_value() || row_begin > storage_.syntax.token_span(value).offset ||
                *row_end < storage_.syntax.closing_span(value).offset + 1) {
                return std::nullopt;
            }
            if (!source_values
                     .emplace(storage_.syntax.text(storage_.syntax.children(value)[1]),
                              SourceValue{.form = value, .begin = row_begin, .end = *row_end})
                     .second) {
                return std::nullopt;
            }
            row_begin = *row_end;
        }

        auto const values_end{row_begin};
        auto const renamed_value{infer_single_positional_rename(
            store_.enumerators,
            values,
            items(store_.enums.values[store_.enums.index(schema)]),
            [&](FormId const source, lispb::codegen::EnumeratorId const& value) {
                return source_property_matches(
                           source,
                           1,
                           "value",
                           store_.enumerators.initializer[store_.enumerators.index(value)]
                               .transform([&](auto value) { return quote(value); })) &&
                       source_property_matches(
                           source,
                           1,
                           "display-name",
                           store_.enumerators.display_name[store_.enumerators.index(value)]
                               .transform([&](auto value) { return quote(value); })) &&
                       source_property_matches(
                           source,
                           1,
                           "hidden",
                           store_.enumerators.hidden[store_.enumerators.index(value)]
                               ? std::optional<std::string>{"true"}
                               : std::nullopt) &&
                       source_property_matches(
                           source,
                           1,
                           "serialized-name",
                           store_.enumerators.serialized_name[store_.enumerators.index(value)]
                               .transform([&](auto value) { return quote(value); })) &&
                       source_property_matches(
                           source,
                           1,
                           "sentinel",
                           store_.enumerators.sentinel[store_.enumerators.index(value)]
                               ? std::optional<std::string>{"true"}
                               : std::nullopt);
            })};
        auto rendered_values{std::string{}};
        auto append_row = [&](std::string row) {
            auto const preceding_newline{
                rendered_values.empty() ? values_begin > 0 && original[values_begin - 1] == '\n'
                                        : rendered_values.back() == '\n'};
            if (!preceding_newline && (row.empty() || row.front() != '\n')) {
                rendered_values += '\n';
            }
            rendered_values += row;
        };

        for (auto const& value : items(store_.enums.values[store_.enums.index(schema)])) {
            auto const renamed{renamed_value.has_value() &&
                               renamed_value->second ==
                                   text(store_.enumerators.name[store_.enumerators.index(value)])};
            auto const found{source_values.find(
                renamed ? renamed_value->first
                        : text(store_.enumerators.name[store_.enumerators.index(value)]))};
            if (found == source_values.end()) {
                append_row("    " + render_enum_value(value));
                continue;
            }

            auto const value_properties{std::array<SourceProperty, 5>{
                std::pair{"value",
                          store_.enumerators.initializer[store_.enumerators.index(value)].transform(
                              [&](auto value) { return quote(value); })},
                std::pair{"display-name",
                          store_.enumerators.display_name[store_.enumerators.index(value)]
                              .transform([&](auto value) { return quote(value); })},
                std::pair{"hidden",
                          store_.enumerators.hidden[store_.enumerators.index(value)]
                              ? std::optional<std::string>{"true"}
                              : std::nullopt},
                std::pair{"serialized-name",
                          store_.enumerators.serialized_name[store_.enumerators.index(value)]
                              .transform([&](auto value) { return quote(value); })},
                std::pair{"sentinel",
                          store_.enumerators.sentinel[store_.enumerators.index(value)]
                              ? std::optional<std::string>{"true"}
                              : std::nullopt}}};
            std::vector<SourceReplacement> value_replacements;
            if (renamed &&
                !patch_source_form(storage_.syntax.children(found->second.form)[1],
                                   text(store_.enumerators.name[store_.enumerators.index(value)]),
                                   original,
                                   value_replacements)) {
                return std::nullopt;
            }
            if (!patch_source_properties(found->second.form,
                                         1,
                                         value_properties,
                                         "      ",
                                         original,
                                         value_replacements)) {
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

    void render_quoted_list(std::ostringstream& output, auto const& values) {
        output << '(';
        for (std::size_t index{}; index < values.size(); ++index) {
            output << (index == 0 ? "" : " ") << quote(values[index]);
        }
        output << ')';
    }

    void render_function(std::ostringstream& output,
                         lispb::codegen::FunctionSchemaId const& function,
                         std::string_view const indent,
                         std::string_view const kind = "function") {
        output << indent << "(" << kind << " "
               << text(store_.functions.name[store_.functions.index(function)]) << ' '
               << render_type_ref(store_.functions.return_type[store_.functions.index(function)]);
        if (!texts(store_.functions.body_lines[store_.functions.index(function)]).empty()) {
            output << "\n" << indent << "  :body ";
            render_quoted_list(
                output, texts(store_.functions.body_lines[store_.functions.index(function)]));
        }
        if (!texts(store_.functions.dependencies[store_.functions.index(function)]).empty()) {
            output << "\n" << indent << "  :dependencies ";
            render_quoted_list(
                output, texts(store_.functions.dependencies[store_.functions.index(function)]));
        }
        if (store_.functions.trailing_return_type[store_.functions.index(function)].has_value()) {
            output << "\n"
                   << indent << "  :trailing-return-type "
                   << render_type_ref(
                          *store_.functions.trailing_return_type[store_.functions.index(function)]);
        }
        if (store_.functions.is_const[store_.functions.index(function)]) {
            output << "\n" << indent << "  :const true";
        }
        if (store_.functions.is_noexcept[store_.functions.index(function)]) {
            output << "\n" << indent << "  :noexcept true";
        }
        if (store_.functions.is_static[store_.functions.index(function)]) {
            output << "\n" << indent << "  :static true";
        }
        if (store_.functions.is_constexpr[store_.functions.index(function)]) {
            output << "\n" << indent << "  :constexpr true";
        }
        if (store_.functions.is_nodiscard[store_.functions.index(function)]) {
            output << "\n" << indent << "  :nodiscard true";
        }
        if (store_.functions.is_inline[store_.functions.index(function)]) {
            output << "\n" << indent << "  :inline true";
        }
        if (store_.functions.definition_in_source[store_.functions.index(function)]) {
            output << "\n" << indent << "  :definition-in-source true";
        }
        if (store_.functions.template_parameters[store_.functions.index(function)].has_value()) {
            output << "\n"
                   << indent << "  :template-parameters "
                   << quote(text(
                          *store_.functions.template_parameters[store_.functions.index(function)]));
        }
        if (store_.functions.requires_clause[store_.functions.index(function)].has_value()) {
            output << "\n"
                   << indent << "  :requires "
                   << quote(text(
                          *store_.functions.requires_clause[store_.functions.index(function)]));
        }
        for (auto const& parameter :
             items(store_.functions.parameters[store_.functions.index(function)])) {
            output << "\n"
                   << indent << "  (parameter "
                   << text(store_.parameters.name[store_.parameters.index(parameter)]) << ' '
                   << render_type_ref(store_.parameters.type[store_.parameters.index(parameter)]);
            if (store_.parameters.default_value[store_.parameters.index(parameter)].has_value()) {
                output << " :default "
                       << quote(
                              *store_.parameters.default_value[store_.parameters.index(parameter)]);
            }
            output << ')';
        }
        output << ')';
    }

    auto render_semantic_relation(lispb::codegen::SemanticRelationId const& relationship)
        -> std::string {
        std::ostringstream output;
        output << "(relation "
               << semantic_relation_kind_name(
                      store_.semantic_relations.kind[store_.semantic_relations.index(relationship)])
               << ' '
               << render_type_ref(store_.semantic_relations
                                      .target[store_.semantic_relations.index(relationship)]);
        if (store_.semantic_relations.unit[store_.semantic_relations.index(relationship)]
                .has_value()) {
            output << " :unit "
                   << semantic_relation_unit_name(
                          *store_.semantic_relations
                               .unit[store_.semantic_relations.index(relationship)]);
        }
        output << ')';
        return output.str();
    }

    auto render_packed_segment(codegen::PackedSegmentId const segment) -> std::string {
        std::ostringstream output;
        [&]() {
            auto const value{segment};
            switch (value.kind()) {
                case lispb::codegen::PackedSegmentKind::field: {

                    output << "(field "
                           << text(store_.packed_fields.name[store_.packed_fields.index(value)])
                           << ' '
                           << render_type_ref(
                                  store_.packed_fields.type[store_.packed_fields.index(value)])
                           << " :bits ";
                    if (store_.packed_fields.bits[store_.packed_fields.index(value)].has_value()) {
                        output << *store_.packed_fields.bits[store_.packed_fields.index(value)];
                    } else {
                        output << "auto";
                    }
                    if (store_.packed_fields.kind[store_.packed_fields.index(value)] !=
                        codegen::PackedFieldKind::unsigned_integer) {
                        output << " :kind "
                               << packed_field_kind_name(
                                      store_.packed_fields.kind[store_.packed_fields.index(value)]);
                    }
                    if (store_.packed_fields.range_helper[store_.packed_fields.index(value)]) {
                        output << " :range-helper true";
                    }
                    if (store_.packed_fields.default_value[store_.packed_fields.index(value)]
                            .has_value()) {
                        output << " :default "
                               << format_packed_integer(
                                      *store_.packed_fields
                                           .default_value[store_.packed_fields.index(value)]);
                    }
                    if (store_.packed_fields.minimum_value[store_.packed_fields.index(value)]
                            .has_value()) {
                        output << " :minimum "
                               << format_packed_integer(
                                      *store_.packed_fields
                                           .minimum_value[store_.packed_fields.index(value)]);
                    }
                    if (store_.packed_fields.maximum_value[store_.packed_fields.index(value)]
                            .has_value()) {
                        output << " :maximum "
                               << format_packed_integer(
                                      *store_.packed_fields
                                           .maximum_value[store_.packed_fields.index(value)]);
                    }
                    for (auto const code : items(
                             store_.packed_fields.named_codes[store_.packed_fields.index(value)])) {
                        output << "\n      " << render_named_code(code);
                    }
                    if (store_.packed_fields.relationship[store_.packed_fields.index(value)]
                            .has_value()) {
                        output << "\n      "
                               << render_semantic_relation(
                                      *store_.packed_fields
                                           .relationship[store_.packed_fields.index(value)]);
                    }
                    break;
                }
                case lispb::codegen::PackedSegmentKind::reserved: {

                    output << "(reserved "
                           << text(store_.packed_reserved_bits
                                       .name[store_.packed_reserved_bits.index(value)])
                           << " :bits "
                           << store_.packed_reserved_bits
                                  .bits[store_.packed_reserved_bits.index(value)];
                    break;
                }
                default:
                    std::unreachable();
            }
            output << ')';
        }();
        return output.str();
    }

    auto render_packed_value(lispb::codegen::DeclarationHandle const& schema) -> std::string {
        std::ostringstream output;
        output << "(packed-value "
               << text(store_.packed_values.name[store_.packed_values.index(schema)])
               << "\n    :storage "
               << render_type_ref(
                      store_.packed_values.storage_type[store_.packed_values.index(schema)]);
        if (store_.packed_values.byte_order[store_.packed_values.index(schema)].has_value()) {
            output << "\n    :byte-order "
                   << packed_byte_order_name(
                          *store_.packed_values.byte_order[store_.packed_values.index(schema)]);
        }
        if (store_.packed_values.bit_order[store_.packed_values.index(schema)].has_value()) {
            output << "\n    :bit-order "
                   << packed_bit_order_name(
                          *store_.packed_values.bit_order[store_.packed_values.index(schema)]);
        }
        if (store_.packed_values.invalid_value[store_.packed_values.index(schema)].has_value()) {
            output << "\n    :invalid-value "
                   << *store_.packed_values.invalid_value[store_.packed_values.index(schema)];
        }
        if (store_.packed_values.export_specifier[store_.packed_values.index(schema)].value != 0) {
            output
                << "\n    :export-specifier "
                << text(store_.packed_values.export_specifier[store_.packed_values.index(schema)]);
        }
        if (store_.packed_values.mutable_value[store_.packed_values.index(schema)]) {
            output << "\n    :mutable true";
        }
        for (auto const& segment :
             items(store_.packed_values.segments[store_.packed_values.index(schema)])) {
            output << "\n    " << render_packed_segment(segment);
        }
        output << ')';
        return output.str();
    }

    auto render_named_code(lispb::codegen::PackedNamedCodeId const& code) -> std::string {
        std::ostringstream output;
        output << "(code "
               << text(store_.packed_named_codes.name[store_.packed_named_codes.index(code)])
               << " :value "
               << format_packed_integer(
                      store_.packed_named_codes.value[store_.packed_named_codes.index(code)]);
        if (store_.packed_named_codes.sentinel[store_.packed_named_codes.index(code)]) {
            output << " :sentinel true";
        }
        output << ')';
        return output.str();
    }

    auto render_integer_scalar(lispb::codegen::DeclarationHandle const& schema) -> std::string {
        std::ostringstream output;
        output << "(integer-scalar "
               << text(store_.integer_scalars.name[store_.integer_scalars.index(schema)])
               << "\n    :signed "
               << (store_.integer_scalars.signedness[store_.integer_scalars.index(schema)]
                       ? "true"
                       : "false")
               << "\n    :minimum "
               << format_packed_integer(
                      store_.integer_scalars.minimum_value[store_.integer_scalars.index(schema)])
               << "\n    :maximum "
               << format_packed_integer(
                      store_.integer_scalars.maximum_value[store_.integer_scalars.index(schema)])
               << "\n    :bit-width ";
        if (store_.integer_scalars.bit_width[store_.integer_scalars.index(schema)].has_value()) {
            output << *store_.integer_scalars.bit_width[store_.integer_scalars.index(schema)];
        } else {
            output << "auto";
        }
        if (store_.integer_scalars.cpp_emission[store_.integer_scalars.index(schema)] !=
            codegen::IntegerScalarCppEmission::none) {
            output
                << "\n    :cpp-emission "
                << integer_scalar_cpp_emission_name(
                       store_.integer_scalars.cpp_emission[store_.integer_scalars.index(schema)]);
        }
        if (store_.integer_scalars.cpp_type[store_.integer_scalars.index(schema)].has_value()) {
            output << "\n    :cpp-type "
                   << render_type_ref(
                          *store_.integer_scalars.cpp_type[store_.integer_scalars.index(schema)]);
        }
        for (auto const& code :
             items(store_.integer_scalars.named_codes[store_.integer_scalars.index(schema)])) {
            output << "\n    " << render_named_code(code);
        }
        if (store_.integer_scalars.relationship[store_.integer_scalars.index(schema)].has_value()) {
            output
                << "\n    "
                << render_semantic_relation(
                       *store_.integer_scalars.relationship[store_.integer_scalars.index(schema)]);
        }
        output << ')';
        return output.str();
    }

    auto render_linear_quantized(lispb::codegen::DeclarationHandle const& schema) -> std::string {
        std::ostringstream output;
        output << "(linear-quantized "
               << text(store_.linear_quantizeds.name[store_.linear_quantizeds.index(schema)])
               << "\n    :source "
               << render_type_ref(
                      store_.linear_quantizeds.source[store_.linear_quantizeds.index(schema)])
               << "\n    :bits "
               << store_.linear_quantizeds.bit_width[store_.linear_quantizeds.index(schema)]
               << "\n    :reserved-codes "
               << store_.linear_quantizeds.reserved_codes[store_.linear_quantizeds.index(schema)]
               << "\n    :clipping "
               << quantization_clipping_name(
                      store_.linear_quantizeds.clipping[store_.linear_quantizeds.index(schema)])
               << ')';
        return output.str();
    }

    auto render_integer_varint(lispb::codegen::DeclarationHandle const& schema) -> std::string {
        std::ostringstream output;
        output << "(integer-varint "
               << text(store_.integer_varints.name[store_.integer_varints.index(schema)])
               << "\n    :source "
               << render_type_ref(
                      store_.integer_varints.source[store_.integer_varints.index(schema)])
               << "\n    :encoding "
               << integer_varint_encoding_name(
                      store_.integer_varints.encoding[store_.integer_varints.index(schema)])
               << ')';
        return output.str();
    }

    auto render_fixed_point(lispb::codegen::DeclarationHandle const& schema) -> std::string {
        std::ostringstream output;
        output << "(fixed-point "
               << text(store_.fixed_points.name[store_.fixed_points.index(schema)])
               << "\n    :signed "
               << (store_.fixed_points.signedness[store_.fixed_points.index(schema)] ? "true"
                                                                                     : "false")
               << "\n    :total-bits "
               << store_.fixed_points.total_bits[store_.fixed_points.index(schema)]
               << "\n    :fractional-bits "
               << store_.fixed_points.fractional_bits[store_.fixed_points.index(schema)]
               << "\n    :rounding "
               << fixed_point_rounding_name(
                      store_.fixed_points.rounding[store_.fixed_points.index(schema)]);
        if (store_.fixed_points.minimum_value[store_.fixed_points.index(schema)].has_value()) {
            output << "\n    :minimum "
                   << text(*store_.fixed_points.minimum_value[store_.fixed_points.index(schema)]);
        }
        if (store_.fixed_points.maximum_value[store_.fixed_points.index(schema)].has_value()) {
            output << "\n    :maximum "
                   << text(*store_.fixed_points.maximum_value[store_.fixed_points.index(schema)]);
        }
        output << ')';
        return output.str();
    }

    auto render_mini_float(lispb::codegen::DeclarationHandle const& schema) -> std::string {
        std::ostringstream output;
        output << "(mini-float " << text(store_.mini_floats.name[store_.mini_floats.index(schema)])
               << "\n    :sign-bits "
               << store_.mini_floats.sign_bits[store_.mini_floats.index(schema)]
               << "\n    :exponent-bits "
               << store_.mini_floats.exponent_bits[store_.mini_floats.index(schema)]
               << "\n    :significand-bits "
               << store_.mini_floats.significand_bits[store_.mini_floats.index(schema)]
               << "\n    :bias "
               << store_.mini_floats.exponent_bias[store_.mini_floats.index(schema)] << ')';
        return output.str();
    }

    auto render_optional_sentinel(lispb::codegen::DeclarationHandle const& schema) -> std::string {
        std::ostringstream output;
        output << "(optional-sentinel "
               << text(store_.optional_sentinels.name[store_.optional_sentinels.index(schema)])
               << "\n    :source "
               << render_type_ref(
                      store_.optional_sentinels.source[store_.optional_sentinels.index(schema)])
               << "\n    :sentinel "
               << text(store_.optional_sentinels.sentinel[store_.optional_sentinels.index(schema)])
               << ')';
        return output.str();
    }

    auto render_optional_presence_bit(lispb::codegen::DeclarationHandle const& schema)
        -> std::string {
        std::ostringstream output;
        output
            << "(optional-presence-bit "
            << text(store_.optional_presence_bits.name[store_.optional_presence_bits.index(schema)])
            << "\n    :source "
            << render_type_ref(store_.optional_presence_bits
                                   .source[store_.optional_presence_bits.index(schema)])
            << ')';
        return output.str();
    }

    auto patch_source_named_code_rows(FormId const parent,
                                      std::span<FormId const> const source_code_forms,
                                      std::span<codegen::PackedNamedCodeId const> const codes,
                                      std::optional<FormId> const trailing_form,
                                      std::string_view const indentation,
                                      std::string_view const property_indentation,
                                      std::string_view const original,
                                      std::vector<SourceReplacement>& replacements) -> bool {
        if (source_code_forms.empty()) {
            if (codes.empty()) {
                return true;
            }

            auto insertion_begin{std::size_t{storage_.syntax.closing_span(parent).offset}};
            if (trailing_form.has_value()) {
                insertion_begin = 0;
                for (auto const& child : storage_.syntax.children(parent)) {
                    if (storage_.syntax.token_span(child).offset >=
                        storage_.syntax.token_span(*trailing_form).offset) {
                        continue;
                    }
                    auto const child_end{source_form_line_end(
                        child, original, storage_.syntax.token_span(*trailing_form).offset)};
                    if (!child_end.has_value()) {
                        return false;
                    }
                    insertion_begin = (std::max)(insertion_begin, *child_end);
                }
                if (insertion_begin > storage_.syntax.token_span(*trailing_form).offset) {
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
            if (trailing_form.has_value() && rendered_codes.back() != '\n') {
                rendered_codes += '\n';
            }
            replacements.push_back({.begin = insertion_begin,
                                    .end = insertion_begin,
                                    .text = std::move(rendered_codes)});
            return true;
        }

        auto const first_code_offset{storage_.syntax.token_span(source_code_forms.front()).offset};
        if (trailing_form.has_value() &&
            storage_.syntax.token_span(*trailing_form).offset < first_code_offset) {
            return false;
        }
        for (auto const& child : storage_.syntax.children(parent)) {
            if (storage_.syntax.token_span(child).offset > first_code_offset &&
                storage_.syntax.head(child) != "code" &&
                (!trailing_form.has_value() || child != *trailing_form)) {
                return false;
            }
        }

        auto codes_begin{std::size_t{}};
        for (auto const& child : storage_.syntax.children(parent)) {
            if (storage_.syntax.token_span(child).offset >= first_code_offset) {
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
            FormId form;
            std::size_t begin{};
            std::size_t end{};
        };
        std::map<std::string_view, SourceCode> source_codes;
        auto row_begin{std::size_t{codes_begin}};
        auto const region_end{trailing_form.has_value()
                                  ? storage_.syntax.token_span(*trailing_form).offset
                                  : storage_.syntax.closing_span(parent).offset};
        for (auto const& code : source_code_forms) {
            if (storage_.syntax.children(code).size() < 2) {
                return false;
            }
            auto const row_end{source_form_line_end(code, original, region_end)};
            if (!row_end.has_value() || row_begin > storage_.syntax.token_span(code).offset ||
                *row_end < storage_.syntax.closing_span(code).offset + 1) {
                return false;
            }
            if (!source_codes
                     .emplace(storage_.syntax.text(storage_.syntax.children(code)[1]),
                              SourceCode{.form = code, .begin = row_begin, .end = *row_end})
                     .second) {
                return false;
            }
            row_begin = *row_end;
        }

        auto const codes_end{row_begin};
        auto const renamed_code{infer_single_positional_rename(
            store_.packed_named_codes,
            source_code_forms,
            codes,
            [&](FormId const source, lispb::codegen::PackedNamedCodeId const& code) {
                return source_property_matches(
                           source,
                           1,
                           "value",
                           std::optional{format_packed_integer(
                               store_.packed_named_codes
                                   .value[store_.packed_named_codes.index(code)])}) &&
                       source_property_matches(
                           source,
                           1,
                           "sentinel",
                           store_.packed_named_codes.sentinel[store_.packed_named_codes.index(code)]
                               ? std::optional<std::string>{"true"}
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
            auto const renamed{
                renamed_code.has_value() &&
                renamed_code->second ==
                    text(store_.packed_named_codes.name[store_.packed_named_codes.index(code)])};
            auto const found{source_codes.find(
                renamed
                    ? renamed_code->first
                    : text(store_.packed_named_codes.name[store_.packed_named_codes.index(code)]))};
            if (found == source_codes.end()) {
                append_row(std::string{indentation} + render_named_code(code));
                continue;
            }

            auto const code_properties{std::array<SourceProperty, 2>{
                std::pair{
                    "value",
                    std::optional{format_packed_integer(
                        store_.packed_named_codes.value[store_.packed_named_codes.index(code)])}},
                std::pair{"sentinel",
                          store_.packed_named_codes.sentinel[store_.packed_named_codes.index(code)]
                              ? std::optional<std::string>{"true"}
                              : std::nullopt}}};
            std::vector<SourceReplacement> code_replacements;
            if (renamed &&
                !patch_source_form(
                    storage_.syntax.children(found->second.form)[1],
                    text(store_.packed_named_codes.name[store_.packed_named_codes.index(code)]),
                    original,
                    code_replacements)) {
                return false;
            }
            if (!patch_source_properties(found->second.form,
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
        if (!rendered_codes.empty() && rendered_codes.back() != '\n' &&
            codes_end < original.size() && original[codes_end] != ')') {
            rendered_codes += '\n';
        }
        replacements.push_back(
            {.begin = codes_begin, .end = codes_end, .text = std::move(rendered_codes)});
        return true;
    }

    auto patch_source_optional_relation(FormId const parent,
                                        std::optional<FormId> const source_relation,
                                        std::optional<codegen::SemanticRelationId> relationship_id,
                                        std::string_view const indentation,
                                        std::string_view const property_indentation,
                                        std::string_view const original,
                                        std::vector<SourceReplacement>& replacements) -> bool {
        auto const relationship{relationship_id.transform([&](auto id) { return id; })};
        if (source_relation.has_value() && relationship.has_value()) {
            if (storage_.syntax.children(*source_relation).size() < 3 ||
                !patch_source_form(storage_.syntax.children(*source_relation)[1],
                                   std::string{semantic_relation_kind_name(
                                       store_.semantic_relations
                                           .kind[store_.semantic_relations.index(*relationship)])},
                                   original,
                                   replacements) ||
                !patch_source_form(
                    storage_.syntax.children(*source_relation)[2],
                    render_type_ref(store_.semantic_relations
                                        .target[store_.semantic_relations.index(*relationship)]),
                    original,
                    replacements)) {
                return false;
            }
            auto const relation_properties{std::array<SourceProperty, 1>{std::pair{
                "unit",
                store_.semantic_relations.unit[store_.semantic_relations.index(*relationship)]
                    .transform([&](codegen::SemanticRelationUnit const unit) {
                        return std::string{codegen::semantic_relation_unit_name(unit)};
                    })}}};
            return patch_source_properties(*source_relation,
                                           2,
                                           relation_properties,
                                           property_indentation,
                                           original,
                                           replacements);
        }

        if (!source_relation.has_value() && relationship.has_value()) {
            replacements.push_back({.begin = storage_.syntax.closing_span(parent).offset,
                                    .end = storage_.syntax.closing_span(parent).offset,
                                    .text = "\n" + std::string{indentation} +
                                            render_semantic_relation(*relationship)});
            return true;
        }

        if (!source_relation.has_value()) {
            return true;
        }

        auto relation_begin{std::size_t{}};
        for (auto const& child : storage_.syntax.children(parent)) {
            if (storage_.syntax.token_span(child).offset >=
                storage_.syntax.token_span(*source_relation).offset) {
                continue;
            }
            auto const child_end{source_form_line_end(
                child, original, storage_.syntax.token_span(*source_relation).offset)};
            if (!child_end.has_value()) {
                return false;
            }
            relation_begin = (std::max)(relation_begin, *child_end);
        }
        auto const relation_end{source_form_line_end(
            *source_relation, original, storage_.syntax.closing_span(parent).offset)};
        if (!relation_end.has_value() ||
            relation_begin > storage_.syntax.token_span(*source_relation).offset ||
            *relation_end < storage_.syntax.closing_span(*source_relation).offset + 1) {
            return false;
        }
        replacements.push_back({.begin = relation_begin, .end = *relation_end, .text = {}});
        return true;
    }

    auto patch_source_packed_segment(codegen::PackedSegmentId const segment,
                                     FormId const source_segment,
                                     std::string_view const original,
                                     std::vector<SourceReplacement>& replacements) -> bool {
        if (auto const field{((segment).kind() == lispb::codegen::PackedSegmentKind::field
                                  ? std::optional{segment}
                                  : std::nullopt)}) {
            if (storage_.syntax.children(source_segment).size() < 3 ||
                !patch_source_form(
                    storage_.syntax.children(source_segment)[2],
                    render_type_ref(store_.packed_fields.type[store_.packed_fields.index(*field)]),
                    original,
                    replacements)) {
                return false;
            }
            auto const field_properties{std::array<SourceProperty, 6>{
                std::pair{"default",
                          store_.packed_fields.default_value[store_.packed_fields.index(*field)]
                              .transform(codegen::format_packed_integer)},
                std::pair{
                    "bits",
                    std::optional{
                        store_.packed_fields.bits[store_.packed_fields.index(*field)].has_value()
                            ? std::to_string(
                                  *store_.packed_fields.bits[store_.packed_fields.index(*field)])
                            : std::string{"auto"}}},
                std::pair{"kind",
                          store_.packed_fields.kind[store_.packed_fields.index(*field)] !=
                                  codegen::PackedFieldKind::unsigned_integer
                              ? std::optional{std::string{packed_field_kind_name(
                                    store_.packed_fields.kind[store_.packed_fields.index(*field)])}}
                              : std::nullopt},
                std::pair{"range-helper",
                          store_.packed_fields.range_helper[store_.packed_fields.index(*field)]
                              ? std::optional<std::string>{"true"}
                              : std::nullopt},
                std::pair{"minimum",
                          store_.packed_fields.minimum_value[store_.packed_fields.index(*field)]
                                  .has_value()
                              ? std::optional{format_packed_integer(
                                    *store_.packed_fields
                                         .minimum_value[store_.packed_fields.index(*field)])}
                              : std::nullopt},
                std::pair{"maximum",
                          store_.packed_fields.maximum_value[store_.packed_fields.index(*field)]
                                  .has_value()
                              ? std::optional{format_packed_integer(
                                    *store_.packed_fields
                                         .maximum_value[store_.packed_fields.index(*field)])}
                              : std::nullopt}}};
            if (!patch_source_properties(
                    source_segment, 2, field_properties, "      ", original, replacements)) {
                return false;
            }

            std::vector<FormId> codes;
            std::optional<FormId> relation{};
            for (auto const& child : storage_.syntax.children(source_segment)) {
                if (storage_.syntax.head(child) == "code") {
                    codes.push_back(child);
                } else if (storage_.syntax.head(child) == "relation") {
                    if (relation.has_value()) {
                        return false;
                    }
                    relation = child;
                }
            }
            if (codes.empty() &&
                !items(store_.packed_fields.named_codes[store_.packed_fields.index(*field)])
                     .empty() &&
                !relation.has_value() &&
                store_.packed_fields.relationship[store_.packed_fields.index(*field)].has_value()) {
                return false;
            }
            if (!patch_source_named_code_rows(
                    source_segment,
                    codes,
                    items(store_.packed_fields.named_codes[store_.packed_fields.index(*field)]),
                    relation,
                    "      ",
                    "        ",
                    original,
                    replacements) ||
                !patch_source_optional_relation(
                    source_segment,
                    relation,
                    store_.packed_fields.relationship[store_.packed_fields.index(*field)],
                    "      ",
                    "        ",
                    original,
                    replacements)) {
                return false;
            }
            return true;
        }

        auto const& reserved{segment};
        auto const reserved_properties{std::array<SourceProperty, 1>{std::pair{
            "bits",
            std::optional{std::to_string(
                store_.packed_reserved_bits.bits[store_.packed_reserved_bits.index(reserved)])}}}};
        return patch_source_properties(
            source_segment, 1, reserved_properties, "      ", original, replacements);
    }

    auto packed_segment_matches_except_name(FormId const source_segment,
                                            codegen::PackedSegmentId const segment) -> bool {
        if (auto const field{((segment).kind() == lispb::codegen::PackedSegmentKind::field
                                  ? std::optional{segment}
                                  : std::nullopt)}) {
            auto const structural_match{storage_.syntax.head(source_segment) == "field" &&
                                        storage_.syntax.children(source_segment).size() >= 3};
            auto const type_match{
                structural_match &&
                source_form_matches_rendered(
                    storage_.syntax.children(source_segment)[2],
                    render_type_ref(
                        store_.packed_fields.type[store_.packed_fields.index(*field)]))};
            auto const bits_match{source_property_matches(
                source_segment,
                2,
                "bits",
                std::optional{
                    store_.packed_fields.bits[store_.packed_fields.index(*field)].has_value()
                        ? std::to_string(
                              *store_.packed_fields.bits[store_.packed_fields.index(*field)])
                        : std::string{"auto"}})};
            auto const kind_match{source_property_matches(
                source_segment,
                2,
                "kind",
                store_.packed_fields.kind[store_.packed_fields.index(*field)] !=
                        codegen::PackedFieldKind::unsigned_integer
                    ? std::optional{std::string{packed_field_kind_name(
                          store_.packed_fields.kind[store_.packed_fields.index(*field)])}}
                    : std::nullopt)};
            auto const range_match{source_property_matches(
                source_segment,
                2,
                "range-helper",
                store_.packed_fields.range_helper[store_.packed_fields.index(*field)]
                    ? std::optional<std::string>{"true"}
                    : std::nullopt)};
            auto const minimum_match{source_property_matches(
                source_segment,
                2,
                "minimum",
                store_.packed_fields.minimum_value[store_.packed_fields.index(*field)].transform(
                    codegen::format_packed_integer))};
            auto const maximum_match{source_property_matches(
                source_segment,
                2,
                "maximum",
                store_.packed_fields.maximum_value[store_.packed_fields.index(*field)].transform(
                    codegen::format_packed_integer))};
            auto const default_match{source_property_matches(
                source_segment,
                2,
                "default",
                store_.packed_fields.default_value[store_.packed_fields.index(*field)].transform(
                    codegen::format_packed_integer))};
            if (!default_match || !structural_match || !type_match || !bits_match || !kind_match ||
                !range_match || !minimum_match || !maximum_match) {
                return false;
            }

            std::vector<FormId> codes;
            std::optional<FormId> relation{};
            for (auto const& child : storage_.syntax.children(source_segment)) {
                if (storage_.syntax.head(child) == "code") {
                    codes.push_back(child);
                } else if (storage_.syntax.head(child) == "relation") {
                    if (relation.has_value()) {
                        return false;
                    }
                    relation = child;
                }
            }
            if (codes.size() !=
                items(store_.packed_fields.named_codes[store_.packed_fields.index(*field)])
                    .size()) {
                return false;
            }
            for (std::size_t index{}; index < codes.size(); ++index) {
                auto const& code{items(
                    store_.packed_fields.named_codes[store_.packed_fields.index(*field)])[index]};
                if (storage_.syntax.children(codes[index]).size() < 2 ||
                    storage_.syntax.text(storage_.syntax.children(codes[index])[1]) !=
                        text(store_.packed_named_codes
                                 .name[store_.packed_named_codes.index(code)]) ||
                    !source_property_matches(
                        codes[index],
                        1,
                        "value",
                        std::optional{format_packed_integer(
                            store_.packed_named_codes
                                .value[store_.packed_named_codes.index(code)])}) ||
                    !source_property_matches(
                        codes[index],
                        1,
                        "sentinel",
                        store_.packed_named_codes.sentinel[store_.packed_named_codes.index(code)]
                            ? std::optional<std::string>{"true"}
                            : std::nullopt)) {
                    return false;
                }
            }

            if (!relation.has_value() ||
                !store_.packed_fields.relationship[store_.packed_fields.index(*field)]
                     .has_value()) {
                return !relation.has_value() &&
                       !store_.packed_fields.relationship[store_.packed_fields.index(*field)]
                            .has_value();
            }
            auto const relation_match{
                storage_.syntax.children(*relation).size() >= 3 &&
                source_form_matches_rendered(
                    storage_.syntax.children(*relation)[1],
                    std::string{semantic_relation_kind_name(
                        store_.semantic_relations.kind[store_.semantic_relations.index(
                            *store_.packed_fields
                                 .relationship[store_.packed_fields.index(*field)])])}) &&
                source_form_matches_rendered(
                    storage_.syntax.children(*relation)[2],
                    render_type_ref(
                        store_.semantic_relations.target[store_.semantic_relations.index(
                            *store_.packed_fields
                                 .relationship[store_.packed_fields.index(*field)])])) &&
                source_property_matches(
                    *relation,
                    2,
                    "unit",
                    store_.semantic_relations
                        .unit[store_.semantic_relations.index(
                            *store_.packed_fields.relationship[store_.packed_fields.index(*field)])]
                        .transform([&](auto const unit) {
                            return std::string{semantic_relation_unit_name(unit)};
                        }))};
            return relation_match;
        }

        auto const& reserved{segment};
        return storage_.syntax.head(source_segment) == "reserved" &&
               storage_.syntax.children(source_segment).size() >= 2 &&
               source_property_matches(
                   source_segment,
                   1,
                   "bits",
                   std::optional{
                       std::to_string(store_.packed_reserved_bits
                                          .bits[store_.packed_reserved_bits.index(reserved)])});
    }

    auto infer_single_packed_segment_rename(
        std::span<FormId const> const source_segments,
        std::span<codegen::PackedSegmentId const> const current_segments)
        -> std::optional<std::pair<std::string_view, std::string_view>> {
        if (source_segments.size() != current_segments.size()) {
            return std::nullopt;
        }

        std::optional<std::pair<std::string_view, std::string_view>> renamed;
        for (std::size_t index{}; index < source_segments.size(); ++index) {
            if (storage_.syntax.children(source_segments[index]).size() < 2) {
                return std::nullopt;
            }
            auto const& source_name{
                storage_.syntax.text(storage_.syntax.children(source_segments[index])[1])};
            auto const& current_name{segment_name(current_segments[index])};
            if (source_name == current_name) {
                continue;
            }
            if (renamed.has_value() || !packed_segment_matches_except_name(
                                           source_segments[index], current_segments[index])) {
                return std::nullopt;
            }
            renamed = std::pair{std::string_view{source_name}, std::string_view{current_name}};
        }

        if (!renamed.has_value()) {
            return std::nullopt;
        }
        auto const source_has_new_name{
            std::ranges::any_of(source_segments, [&](FormId const segment) {
                return storage_.syntax.text(storage_.syntax.children(segment)[1]) ==
                       renamed->second;
            })};
        auto const current_has_old_name{
            std::ranges::any_of(current_segments, [&](codegen::PackedSegmentId const segment) {
                return segment_name(segment) == renamed->first;
            })};
        if (source_has_new_name || current_has_old_name) {
            return std::nullopt;
        }
        return renamed;
    }

    auto try_render_source_preserved_packed_value(lispb::codegen::DeclarationHandle const& schema,
                                                  std::string_view const original)
        -> std::optional<std::string> {
        auto parsed{source_declaration("packed-value")};
        if (!parsed.has_value()) {
            return std::nullopt;
        }

        std::vector<FormId> segments;
        for (auto const& child : storage_.syntax.children(*parsed)) {
            if (storage_.syntax.head(child) == "field" ||
                storage_.syntax.head(child) == "reserved") {
                if (storage_.syntax.children(child).size() < 2) {
                    return std::nullopt;
                }
                segments.push_back(child);
            }
        }

        auto const properties{std::array<SourceProperty, 6>{
            std::pair{"storage",
                      std::optional{render_type_ref(
                          store_.packed_values.storage_type[store_.packed_values.index(schema)])}},
            std::pair{
                "byte-order",
                store_.packed_values.byte_order[store_.packed_values.index(schema)].has_value()
                    ? std::optional{std::string{packed_byte_order_name(
                          *store_.packed_values.byte_order[store_.packed_values.index(schema)])}}
                    : std::nullopt},
            std::pair{
                "bit-order",
                store_.packed_values.bit_order[store_.packed_values.index(schema)].has_value()
                    ? std::optional{std::string{packed_bit_order_name(
                          *store_.packed_values.bit_order[store_.packed_values.index(schema)])}}
                    : std::nullopt},
            std::pair{
                "invalid-value",
                store_.packed_values.invalid_value[store_.packed_values.index(schema)].has_value()
                    ? std::optional{std::to_string(
                          *store_.packed_values.invalid_value[store_.packed_values.index(schema)])}
                    : std::nullopt},
            std::pair{
                "export-specifier",
                store_.packed_values.export_specifier[store_.packed_values.index(schema)].value != 0
                    ? std::optional{std::string{
                          text(store_.packed_values
                                   .export_specifier[store_.packed_values.index(schema)])}}
                    : std::nullopt},
            std::pair{"mutable",
                      store_.packed_values.mutable_value[store_.packed_values.index(schema)]
                          ? std::optional{std::string{"true"}}
                          : std::nullopt}}};
        std::vector<SourceReplacement> replacements;
        if (!patch_source_properties(*parsed, 1, properties, "    ", original, replacements)) {
            return std::nullopt;
        }

        if (segments.empty()) {
            return items(store_.packed_values.segments[store_.packed_values.index(schema)]).empty()
                     ? apply_source_replacements(original, std::move(replacements))
                     : std::nullopt;
        }

        auto const first_segment_offset{storage_.syntax.token_span(segments.front()).offset};
        for (auto const& child : storage_.syntax.children(*parsed)) {
            if (storage_.syntax.token_span(child).offset > first_segment_offset &&
                storage_.syntax.head(child) != "field" &&
                storage_.syntax.head(child) != "reserved") {
                return std::nullopt;
            }
        }

        auto segments_begin{std::size_t{}};
        for (auto const& child : storage_.syntax.children(*parsed)) {
            if (storage_.syntax.token_span(child).offset >= first_segment_offset) {
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
            FormId form;
            std::size_t begin{};
            std::size_t end{};
        };
        using SegmentKey = std::pair<std::string_view, std::string_view>;
        std::map<SegmentKey, SourceSegment> source_segments;
        auto row_begin{std::size_t{segments_begin}};
        for (auto const& segment : segments) {
            auto const row_end{source_form_line_end(
                segment, original, storage_.syntax.closing_span(*parsed).offset)};
            if (!row_end.has_value() || row_begin > storage_.syntax.token_span(segment).offset ||
                *row_end < storage_.syntax.closing_span(segment).offset + 1) {
                return std::nullopt;
            }
            auto const key{SegmentKey{storage_.syntax.head(segment),
                                      storage_.syntax.text(storage_.syntax.children(segment)[1])}};
            if (!source_segments
                     .emplace(key,
                              SourceSegment{.form = segment, .begin = row_begin, .end = *row_end})
                     .second) {
                return std::nullopt;
            }
            row_begin = *row_end;
        }

        auto const segments_end{row_begin};
        auto const renamed_segment{infer_single_packed_segment_rename(
            segments, items(store_.packed_values.segments[store_.packed_values.index(schema)]))};
        auto rendered_segments{std::string{}};
        auto append_row = [&](std::string row) {
            auto const preceding_newline{rendered_segments.empty()
                                             ? segments_begin > 0 &&
                                                   original[segments_begin - 1] == '\n'
                                             : rendered_segments.back() == '\n'};
            if (!preceding_newline && (row.empty() || row.front() != '\n')) {
                rendered_segments += '\n';
            }
            rendered_segments += row;
        };

        for (auto const& segment :
             items(store_.packed_values.segments[store_.packed_values.index(schema)])) {
            auto const head{segment.kind() == codegen::PackedSegmentKind::field
                                ? std::string_view{"field"}
                                : std::string_view{"reserved"}};
            auto const& current_name{segment_name(segment)};
            auto const renamed{renamed_segment.has_value() &&
                               renamed_segment->second == current_name};
            auto const key{SegmentKey{head, renamed ? renamed_segment->first : current_name}};
            auto const found{source_segments.find(key)};
            if (found == source_segments.end()) {
                append_row("    " + render_packed_segment(segment));
                continue;
            }

            std::vector<SourceReplacement> segment_replacements;
            if (renamed && !patch_source_form(storage_.syntax.children(found->second.form)[1],
                                              std::string{current_name},
                                              original,
                                              segment_replacements)) {
                return std::nullopt;
            }
            if (!patch_source_packed_segment(
                    segment, found->second.form, original, segment_replacements)) {
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
                                                std::span<SourceProperty const> properties,
                                                std::string_view const original)
        -> std::optional<std::string> {
        auto parsed{source_declaration(head)};
        if (!parsed.has_value()) {
            return std::nullopt;
        }
        std::vector<SourceReplacement> replacements;
        if (!patch_source_properties(*parsed, 1, properties, "    ", original, replacements)) {
            return std::nullopt;
        }
        return apply_source_replacements(original, std::move(replacements));
    }

    auto try_render_source_preserved_integer_scalar(lispb::codegen::DeclarationHandle const& schema,
                                                    std::string_view const original)
        -> std::optional<std::string> {
        auto parsed{source_declaration("integer-scalar")};
        if (!parsed.has_value()) {
            return std::nullopt;
        }

        std::vector<FormId> codes;
        std::optional<FormId> relation{};
        for (auto const& child : storage_.syntax.children(*parsed)) {
            if (storage_.syntax.head(child) == "code") {
                codes.push_back(child);
            } else if (storage_.syntax.head(child) == "relation") {
                if (relation.has_value()) {
                    return std::nullopt;
                }
                relation = child;
            }
        }
        for (auto const& code : codes) {
            if (storage_.syntax.children(code).size() < 2) {
                return std::nullopt;
            }
        }

        auto const properties{std::array<SourceProperty, 6>{
            std::pair{"signed",
                      std::optional{
                          store_.integer_scalars.signedness[store_.integer_scalars.index(schema)]
                              ? "true"
                              : "false"}},
            std::pair{
                "minimum",
                std::optional{format_packed_integer(
                    store_.integer_scalars.minimum_value[store_.integer_scalars.index(schema)])}},
            std::pair{
                "maximum",
                std::optional{format_packed_integer(
                    store_.integer_scalars.maximum_value[store_.integer_scalars.index(schema)])}},
            std::pair{
                "bit-width",
                std::optional{
                    store_.integer_scalars.bit_width[store_.integer_scalars.index(schema)]
                            .has_value()
                        ? std::to_string(*store_.integer_scalars
                                              .bit_width[store_.integer_scalars.index(schema)])
                        : std::string{"auto"}}},
            std::pair{"cpp-emission",
                      store_.integer_scalars.cpp_emission[store_.integer_scalars.index(schema)] !=
                              codegen::IntegerScalarCppEmission::none
                          ? std::optional{std::string{integer_scalar_cpp_emission_name(
                                store_.integer_scalars
                                    .cpp_emission[store_.integer_scalars.index(schema)])}}
                          : std::nullopt},
            std::pair{
                "cpp-type",
                store_.integer_scalars.cpp_type[store_.integer_scalars.index(schema)].has_value()
                    ? std::optional{render_type_ref(
                          *store_.integer_scalars.cpp_type[store_.integer_scalars.index(schema)])}
                    : std::nullopt}}};
        std::vector<SourceReplacement> replacements;
        if (!patch_source_properties(*parsed, 1, properties, "    ", original, replacements)) {
            return std::nullopt;
        }
        if (codes.empty() &&
            !items(store_.integer_scalars.named_codes[store_.integer_scalars.index(schema)])
                 .empty() &&
            !relation.has_value() &&
            store_.integer_scalars.relationship[store_.integer_scalars.index(schema)].has_value()) {
            return std::nullopt;
        }
        if (!patch_source_named_code_rows(
                *parsed,
                codes,
                items(store_.integer_scalars.named_codes[store_.integer_scalars.index(schema)]),
                relation,
                "    ",
                "      ",
                original,
                replacements) ||
            !patch_source_optional_relation(
                *parsed,
                relation,
                store_.integer_scalars.relationship[store_.integer_scalars.index(schema)],
                "    ",
                "      ",
                original,
                replacements)) {
            return std::nullopt;
        }
        return apply_source_replacements(original, std::move(replacements));
    }

    auto try_render_source_preserved_linear_quantized(
        lispb::codegen::DeclarationHandle const& schema, std::string_view const original)
        -> std::optional<std::string> {
        auto const properties{std::array<SourceProperty, 4>{
            std::pair{
                "source",
                std::optional{render_type_ref(
                    store_.linear_quantizeds.source[store_.linear_quantizeds.index(schema)])}},
            std::pair{
                "bits",
                std::optional{std::to_string(
                    store_.linear_quantizeds.bit_width[store_.linear_quantizeds.index(schema)])}},
            std::pair{"reserved-codes",
                      std::optional{std::to_string(
                          store_.linear_quantizeds
                              .reserved_codes[store_.linear_quantizeds.index(schema)])}},
            std::pair{
                "clipping",
                std::optional{std::string{quantization_clipping_name(
                    store_.linear_quantizeds.clipping[store_.linear_quantizeds.index(schema)])}}}}};
        return try_render_source_preserved_properties("linear-quantized", properties, original);
    }

    auto try_render_source_preserved_integer_varint(lispb::codegen::DeclarationHandle const& schema,
                                                    std::string_view const original)
        -> std::optional<std::string> {
        auto const properties{std::array<SourceProperty, 2>{
            std::pair{"source",
                      std::optional{render_type_ref(
                          store_.integer_varints.source[store_.integer_varints.index(schema)])}},
            std::pair{
                "encoding",
                std::optional{std::string{integer_varint_encoding_name(
                    store_.integer_varints.encoding[store_.integer_varints.index(schema)])}}}}};
        return try_render_source_preserved_properties("integer-varint", properties, original);
    }

    auto try_render_source_preserved_fixed_point(lispb::codegen::DeclarationHandle const& schema,
                                                 std::string_view const original)
        -> std::optional<std::string> {
        auto const properties{std::array<SourceProperty, 6>{
            std::pair{
                "signed",
                std::optional{store_.fixed_points.signedness[store_.fixed_points.index(schema)]
                                  ? "true"
                                  : "false"}},
            std::pair{"total-bits",
                      std::optional{std::to_string(
                          store_.fixed_points.total_bits[store_.fixed_points.index(schema)])}},
            std::pair{"fractional-bits",
                      std::optional{std::to_string(
                          store_.fixed_points.fractional_bits[store_.fixed_points.index(schema)])}},
            std::pair{"rounding",
                      std::optional{std::string{fixed_point_rounding_name(
                          store_.fixed_points.rounding[store_.fixed_points.index(schema)])}}},
            std::pair{"minimum",
                      optional_text(
                          store_.fixed_points.minimum_value[store_.fixed_points.index(schema)])},
            std::pair{"maximum",
                      optional_text(
                          store_.fixed_points.maximum_value[store_.fixed_points.index(schema)])}}};
        return try_render_source_preserved_properties("fixed-point", properties, original);
    }

    auto try_render_source_preserved_mini_float(lispb::codegen::DeclarationHandle const& schema,
                                                std::string_view const original)
        -> std::optional<std::string> {
        auto const properties{std::array<SourceProperty, 4>{
            std::pair{"sign-bits",
                      std::optional{std::to_string(
                          store_.mini_floats.sign_bits[store_.mini_floats.index(schema)])}},
            std::pair{"exponent-bits",
                      std::optional{std::to_string(
                          store_.mini_floats.exponent_bits[store_.mini_floats.index(schema)])}},
            std::pair{"significand-bits",
                      std::optional{std::to_string(
                          store_.mini_floats.significand_bits[store_.mini_floats.index(schema)])}},
            std::pair{"bias",
                      std::optional{std::to_string(
                          store_.mini_floats.exponent_bias[store_.mini_floats.index(schema)])}}}};
        return try_render_source_preserved_properties("mini-float", properties, original);
    }

    auto try_render_source_preserved_optional_sentinel(
        lispb::codegen::DeclarationHandle const& schema, std::string_view const original)
        -> std::optional<std::string> {
        auto const properties{std::array<SourceProperty, 2>{
            std::pair{
                "source",
                std::optional{render_type_ref(
                    store_.optional_sentinels.source[store_.optional_sentinels.index(schema)])}},
            std::pair{"sentinel",
                      std::optional{std::string{
                          text(store_.optional_sentinels
                                   .sentinel[store_.optional_sentinels.index(schema)])}}}}};
        return try_render_source_preserved_properties("optional-sentinel", properties, original);
    }

    auto try_render_source_preserved_optional_presence_bit(
        lispb::codegen::DeclarationHandle const& schema, std::string_view const original)
        -> std::optional<std::string> {
        auto const properties{std::array<SourceProperty, 1>{
            std::pair{"source",
                      std::optional{render_type_ref(
                          store_.optional_presence_bits
                              .source[store_.optional_presence_bits.index(schema)])}}}};
        return try_render_source_preserved_properties(
            "optional-presence-bit", properties, original);
    }

    template <typename Child>
    auto render_aggregate_child(auto const& columns, std::string_view const head, Child const child)
        -> std::string {
        std::ostringstream output;
        output << '(' << head << ' ' << text(columns.name[columns.index(child)]) << ' '
               << render_type_ref(columns.type[columns.index(child)]);
        if (columns.count[columns.index(child)].has_value()) {
            output << " :count " << *columns.count[columns.index(child)];
        }
        if constexpr (std::is_same_v<Child, lispb::codegen::RecordMemberId>) {
            if (columns.initializer[columns.index(child)]) {
                output << " :initializer " << quote(*columns.initializer[columns.index(child)]);
            }
            if (columns.relationship[columns.index(child)].has_value()) {
                output << "\n      "
                       << render_semantic_relation(*columns.relationship[columns.index(child)]);
            }
        }
        output << ')';
        return output.str();
    }

    auto render_record(lispb::codegen::DeclarationHandle const& schema) -> std::string {
        std::ostringstream output;
        output << "(record " << text(store_.records.name[store_.records.index(schema)]);
        if (store_.records.export_specifier[store_.records.index(schema)].value != 0) {
            output << "\n    :export-specifier "
                   << text(store_.records.export_specifier[store_.records.index(schema)]);
        }
        if (store_.records.comparison[store_.records.index(schema)] !=
            codegen::RecordComparison::none) {
            output << "\n    :comparison "
                   << (store_.records.comparison[store_.records.index(schema)] ==
                               codegen::RecordComparison::equality
                           ? "equality"
                           : "three-way");
        }
        if (store_.records.comparison_noexcept[store_.records.index(schema)]) {
            output << "\n    :comparison-noexcept true";
        }
        for (auto const& member : items(store_.records.members[store_.records.index(schema)])) {
            output << "\n    " << render_aggregate_child(store_.record_members, "member", member);
        }
        for (auto const& function : items(store_.records.functions[store_.records.index(schema)])) {
            output << '\n';
            render_function(output, function, "    ");
        }
        output << ')';
        return output.str();
    }

    auto render_union(lispb::codegen::DeclarationHandle const& schema) -> std::string {
        std::ostringstream output;
        output << "(union " << text(store_.unions.name[store_.unions.index(schema)]);
        if (store_.unions.export_specifier[store_.unions.index(schema)].value != 0) {
            output << "\n    :export-specifier "
                   << text(store_.unions.export_specifier[store_.unions.index(schema)]);
        }
        for (auto const& alternative :
             items(store_.unions.alternatives[store_.unions.index(schema)])) {
            output << "\n    "
                   << render_aggregate_child(store_.union_alternatives, "alternative", alternative);
        }
        output << ')';
        return output.str();
    }

    auto
        render_tagged_union_alternative(lispb::codegen::TaggedUnionAlternativeId const& alternative)
            -> std::string {
        std::ostringstream output;
        output << "(alternative "
               << text(store_.tagged_union_alternatives
                           .name[store_.tagged_union_alternatives.index(alternative)])
               << ' '
               << render_type_ref(store_.tagged_union_alternatives
                                      .type[store_.tagged_union_alternatives.index(alternative)])
               << " :tag "
               << text(store_.tagged_union_alternatives
                           .tag[store_.tagged_union_alternatives.index(alternative)]);
        if (store_.tagged_union_alternatives
                .count[store_.tagged_union_alternatives.index(alternative)]
                .has_value()) {
            output << " :count "
                   << *store_.tagged_union_alternatives
                           .count[store_.tagged_union_alternatives.index(alternative)];
        }
        output << ')';
        return output.str();
    }

    auto render_tagged_union(lispb::codegen::DeclarationHandle const& schema) -> std::string {
        std::ostringstream output;
        output << "(tagged-union "
               << text(store_.tagged_unions.name[store_.tagged_unions.index(schema)])
               << "\n    :discriminant "
               << render_type_ref(
                      store_.tagged_unions.discriminant[store_.tagged_unions.index(schema)]);
        if (store_.tagged_unions.export_specifier[store_.tagged_unions.index(schema)].value != 0) {
            output
                << "\n    :export-specifier "
                << text(store_.tagged_unions.export_specifier[store_.tagged_unions.index(schema)]);
        }
        for (auto const& alternative :
             items(store_.tagged_unions.alternatives[store_.tagged_unions.index(schema)])) {
            output << "\n    " << render_tagged_union_alternative(alternative);
        }
        output << ')';
        return output.str();
    }

    auto
        try_render_source_preserved_aggregate(auto const& columns,
                                              std::string_view const declaration_head,
                                              std::string_view const child_head,
                                              std::string_view const,
                                              auto const& children,
                                              IdentifierId const export_specifier,
                                              std::string_view const original,
                                              std::span<SourceProperty const> extra_properties = {},
                                              bool const retain_functions = false,
                                              std::vector<SourceReplacement> replacements = {})
            -> std::optional<std::string> {
        using Child = std::ranges::range_value_t<decltype(children)>;
        auto parsed{source_declaration(declaration_head)};
        if (!parsed.has_value()) {
            return std::nullopt;
        }

        std::vector<FormId> source_children;
        for (auto const& child : storage_.syntax.children(*parsed)) {
            if (storage_.syntax.head(child) == child_head) {
                if (storage_.syntax.children(child).size() < 3) {
                    return std::nullopt;
                }
                source_children.push_back(child);
            }
        }

        auto const properties{std::array<SourceProperty, 1>{std::pair{
            "export-specifier",
            export_specifier.value != 0 ? std::optional{std::string{text(export_specifier)}}
                                        : std::nullopt}}};
        if (!patch_source_properties(*parsed, 1, properties, "    ", original, replacements)) {
            return std::nullopt;
        }
        if (!patch_source_properties(
                *parsed, 1, extra_properties, "    ", original, replacements)) {
            return std::nullopt;
        }

        if (source_children.empty()) {
            return children.empty() ? apply_source_replacements(original, std::move(replacements))
                                    : std::nullopt;
        }

        auto const first_child_offset{storage_.syntax.token_span(source_children.front()).offset};
        for (auto const& child : storage_.syntax.children(*parsed)) {
            if (storage_.syntax.token_span(child).offset > first_child_offset &&
                storage_.syntax.head(child) != child_head &&
                !(retain_functions && storage_.syntax.head(child) == "function" &&
                  storage_.syntax.token_span(child).offset >
                      storage_.syntax.token_span(source_children.back()).offset)) {
                return std::nullopt;
            }
        }

        auto children_begin{std::size_t{}};
        for (auto const& child : storage_.syntax.children(*parsed)) {
            if (storage_.syntax.token_span(child).offset >= first_child_offset) {
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
            FormId form;
            std::size_t begin{};
            std::size_t end{};
        };
        std::map<std::string_view, SourceChild> source_by_name;
        auto row_begin{std::size_t{children_begin}};
        for (auto const& child : source_children) {
            auto const row_end{source_form_line_end(
                child, original, storage_.syntax.closing_span(*parsed).offset)};
            if (!row_end.has_value() || row_begin > storage_.syntax.token_span(child).offset ||
                *row_end < storage_.syntax.closing_span(child).offset + 1) {
                return std::nullopt;
            }
            if (!source_by_name
                     .emplace(storage_.syntax.text(storage_.syntax.children(child)[1]),
                              SourceChild{.form = child, .begin = row_begin, .end = *row_end})
                     .second) {
                return std::nullopt;
            }
            row_begin = *row_end;
        }

        auto const children_end{row_begin};
        auto const renamed_child{infer_single_positional_rename(
            columns, source_children, children, [&](FormId const source, Child const& child) {
                if (storage_.syntax.children(source).size() < 3 ||
                    !source_form_matches_rendered(
                        storage_.syntax.children(source)[2],
                        render_type_ref(columns.type[columns.index(child)])) ||
                    !source_property_matches(
                        source,
                        2,
                        "count",
                        columns.count[columns.index(child)].has_value()
                            ? std::optional{std::to_string(*columns.count[columns.index(child)])}
                            : std::nullopt)) {
                    return false;
                }
                if constexpr (!std::is_same_v<Child, lispb::codegen::RecordMemberId>) {
                    return true;
                } else {
                    if (!source_property_matches(
                            source,
                            2,
                            "initializer",
                            columns.initializer[columns.index(child)]
                                ? std::optional{quote(*columns.initializer[columns.index(child)])}
                                : std::nullopt)) {
                        return false;
                    }
                    std::optional<FormId> source_relation{};
                    for (auto const& nested : storage_.syntax.children(source)) {
                        if (storage_.syntax.head(nested) != "relation") {
                            continue;
                        }
                        if (source_relation.has_value()) {
                            return false;
                        }
                        source_relation = nested;
                    }
                    return columns.relationship[columns.index(child)].has_value()
                             ? source_relation.has_value() &&
                                   source_form_matches_rendered(
                                       *source_relation,
                                       render_semantic_relation(
                                           *columns.relationship[columns.index(child)]))
                             : !source_relation.has_value();
                }
            })};
        auto rendered_children{std::string{}};
        auto append_row = [&](std::string row) {
            auto const preceding_newline{rendered_children.empty()
                                             ? children_begin > 0 &&
                                                   original[children_begin - 1] == '\n'
                                             : rendered_children.back() == '\n'};
            if (!preceding_newline && (row.empty() || row.front() != '\n')) {
                rendered_children += '\n';
            }
            rendered_children += row;
        };

        for (auto const& child : children) {
            auto const renamed{renamed_child.has_value() &&
                               renamed_child->second == text(columns.name[columns.index(child)])};
            auto const found{source_by_name.find(
                renamed ? renamed_child->first
                        : std::string_view{text(columns.name[columns.index(child)])})};
            if (found == source_by_name.end()) {
                append_row("    " + render_aggregate_child(columns, child_head, child));
                continue;
            }

            std::vector<SourceReplacement> child_replacements;
            if (renamed && !patch_source_form(storage_.syntax.children(found->second.form)[1],
                                              text(columns.name[columns.index(child)]),
                                              original,
                                              child_replacements)) {
                return std::nullopt;
            }
            if (!patch_source_form(storage_.syntax.children(found->second.form)[2],
                                   render_type_ref(columns.type[columns.index(child)]),
                                   original,
                                   child_replacements)) {
                return std::nullopt;
            }
            auto const child_properties{std::array<SourceProperty, 1>{
                std::pair{"count",
                          columns.count[columns.index(child)].has_value()
                              ? std::optional{std::to_string(*columns.count[columns.index(child)])}
                              : std::nullopt}}};
            if (!patch_source_properties(found->second.form,
                                         2,
                                         child_properties,
                                         "      ",
                                         original,
                                         child_replacements)) {
                return std::nullopt;
            }
            if constexpr (std::is_same_v<Child, lispb::codegen::RecordMemberId>) {
                auto const initializer_properties{std::array<SourceProperty, 1>{
                    std::pair{"initializer",
                              columns.initializer[columns.index(child)]
                                  ? std::optional{quote(*columns.initializer[columns.index(child)])}
                                  : std::nullopt}}};
                if (!patch_source_properties(found->second.form,
                                             2,
                                             initializer_properties,
                                             "      ",
                                             original,
                                             child_replacements)) {
                    return std::nullopt;
                }
                std::optional<FormId> source_relation{};
                for (auto const& nested : storage_.syntax.children(found->second.form)) {
                    if (storage_.syntax.head(nested) != "relation") {
                        continue;
                    }
                    if (source_relation.has_value()) {
                        return std::nullopt;
                    }
                    source_relation = nested;
                }
                if (!patch_source_optional_relation(found->second.form,
                                                    source_relation,
                                                    columns.relationship[columns.index(child)],
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

    auto try_render_source_preserved_record(lispb::codegen::DeclarationHandle const& schema,
                                            std::string_view const original)
        -> std::optional<std::string> {
        auto const parsed{source_declaration("record")};
        if (!parsed) {
            return std::nullopt;
        }
        std::size_t function_index{};
        std::vector<SourceReplacement> function_replacements;
        for (auto const& child : storage_.syntax.children(*parsed)) {
            if (storage_.syntax.head(child) != "function") {
                continue;
            }
            if (function_index >=
                items(store_.records.functions[store_.records.index(schema)]).size()) {
                return std::nullopt;
            }
            std::ostringstream rendered;
            render_function(
                rendered,
                items(store_.records.functions[store_.records.index(schema)])[function_index++],
                "");
            if (!source_form_matches_rendered(child, rendered.str())) {
                function_replacements.push_back({.begin = storage_.syntax.token_span(child).offset,
                                                 .end = storage_.syntax.span(child).end_offset,
                                                 .text = rendered.str()});
            }
        }
        if (function_index !=
            items(store_.records.functions[store_.records.index(schema)]).size()) {
            return std::nullopt;
        }
        auto const properties{std::array<SourceProperty, 2>{
            std::pair{
                "comparison",
                store_.records.comparison[store_.records.index(schema)] ==
                        codegen::RecordComparison::none
                    ? std::nullopt
                    : std::optional<std::string>{store_.records.comparison[store_.records.index(
                                                     schema)] == codegen::RecordComparison::equality
                                                     ? "equality"
                                                     : "three-way"}},
            std::pair{"comparison-noexcept",
                      store_.records.comparison_noexcept[store_.records.index(schema)]
                          ? std::optional<std::string>{"true"}
                          : std::nullopt}}};
        return try_render_source_preserved_aggregate(
            store_.record_members,
            "record",
            "member",
            text(store_.records.name[store_.records.index(schema)]),
            items(store_.records.members[store_.records.index(schema)]),
            store_.records.export_specifier[store_.records.index(schema)],
            original,
            properties,
            true,
            std::move(function_replacements));
    }

    auto try_render_source_preserved_union(lispb::codegen::DeclarationHandle const& schema,
                                           std::string_view const original)
        -> std::optional<std::string> {
        return try_render_source_preserved_aggregate(
            store_.union_alternatives,
            "union",
            "alternative",
            text(store_.unions.name[store_.unions.index(schema)]),
            items(store_.unions.alternatives[store_.unions.index(schema)]),
            store_.unions.export_specifier[store_.unions.index(schema)],
            original);
    }

    auto try_render_source_preserved_tagged_union(lispb::codegen::DeclarationHandle const& schema,
                                                  std::string_view const original)
        -> std::optional<std::string> {
        auto parsed{source_declaration("tagged-union")};
        if (!parsed.has_value()) {
            return std::nullopt;
        }

        std::vector<FormId> alternatives;
        for (auto const& child : storage_.syntax.children(*parsed)) {
            if (storage_.syntax.head(child) == "alternative") {
                if (storage_.syntax.children(child).size() < 3) {
                    return std::nullopt;
                }
                alternatives.push_back(child);
            }
        }

        auto const properties{std::array<SourceProperty, 2>{
            std::pair{"discriminant",
                      std::optional{render_type_ref(
                          store_.tagged_unions.discriminant[store_.tagged_unions.index(schema)])}},
            std::pair{
                "export-specifier",
                store_.tagged_unions.export_specifier[store_.tagged_unions.index(schema)].value != 0
                    ? std::optional{std::string{
                          text(store_.tagged_unions
                                   .export_specifier[store_.tagged_unions.index(schema)])}}
                    : std::nullopt}}};
        std::vector<SourceReplacement> replacements;
        if (!patch_source_properties(*parsed, 1, properties, "    ", original, replacements)) {
            return std::nullopt;
        }

        if (alternatives.empty()) {
            return items(store_.tagged_unions.alternatives[store_.tagged_unions.index(schema)])
                           .empty()
                     ? apply_source_replacements(original, std::move(replacements))
                     : std::nullopt;
        }

        auto const first_alternative_offset{
            storage_.syntax.token_span(alternatives.front()).offset};
        for (auto const& child : storage_.syntax.children(*parsed)) {
            if (storage_.syntax.token_span(child).offset > first_alternative_offset &&
                storage_.syntax.head(child) != "alternative") {
                return std::nullopt;
            }
        }

        auto alternatives_begin{std::size_t{}};
        for (auto const& child : storage_.syntax.children(*parsed)) {
            if (storage_.syntax.token_span(child).offset >= first_alternative_offset) {
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
            FormId form;
            std::size_t begin{};
            std::size_t end{};
        };
        std::map<std::string_view, SourceAlternative> source_by_name;
        auto row_begin{std::size_t{alternatives_begin}};
        for (auto const& alternative : alternatives) {
            auto const row_end{source_form_line_end(
                alternative, original, storage_.syntax.closing_span(*parsed).offset)};
            if (!row_end.has_value() ||
                row_begin > storage_.syntax.token_span(alternative).offset ||
                *row_end < storage_.syntax.closing_span(alternative).offset + 1) {
                return std::nullopt;
            }
            if (!source_by_name
                     .emplace(storage_.syntax.text(storage_.syntax.children(alternative)[1]),
                              SourceAlternative{
                                  .form = alternative, .begin = row_begin, .end = *row_end})
                     .second) {
                return std::nullopt;
            }
            row_begin = *row_end;
        }

        auto const alternatives_end{row_begin};
        auto const renamed_alternative{infer_single_positional_rename(
            store_.tagged_union_alternatives,
            alternatives,
            items(store_.tagged_unions.alternatives[store_.tagged_unions.index(schema)]),
            [&](FormId const source, lispb::codegen::TaggedUnionAlternativeId const& alternative) {
                return storage_.syntax.children(source).size() >= 3 &&
                       source_form_matches_rendered(
                           storage_.syntax.children(source)[2],
                           render_type_ref(
                               store_.tagged_union_alternatives
                                   .type[store_.tagged_union_alternatives.index(alternative)])) &&
                       source_property_matches(
                           source,
                           2,
                           "tag",
                           std::optional{std::string{text(
                               store_.tagged_union_alternatives
                                   .tag[store_.tagged_union_alternatives.index(alternative)])}}) &&
                       source_property_matches(
                           source,
                           2,
                           "count",
                           store_.tagged_union_alternatives
                                   .count[store_.tagged_union_alternatives.index(alternative)]
                                   .has_value()
                               ? std::optional{std::to_string(
                                     *store_.tagged_union_alternatives
                                          .count[store_.tagged_union_alternatives.index(
                                              alternative)])}
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

        for (auto const& alternative :
             items(store_.tagged_unions.alternatives[store_.tagged_unions.index(schema)])) {
            auto const renamed{
                renamed_alternative.has_value() &&
                renamed_alternative->second ==
                    text(store_.tagged_union_alternatives
                             .name[store_.tagged_union_alternatives.index(alternative)])};
            auto const found{source_by_name.find(
                renamed ? renamed_alternative->first
                        : std::string_view{text(
                              store_.tagged_union_alternatives
                                  .name[store_.tagged_union_alternatives.index(alternative)])})};
            if (found == source_by_name.end()) {
                append_row("    " + render_tagged_union_alternative(alternative));
                continue;
            }

            std::vector<SourceReplacement> alternative_replacements;
            if (renamed && !patch_source_form(
                               storage_.syntax.children(found->second.form)[1],
                               text(store_.tagged_union_alternatives
                                        .name[store_.tagged_union_alternatives.index(alternative)]),
                               original,
                               alternative_replacements)) {
                return std::nullopt;
            }
            if (!patch_source_form(
                    storage_.syntax.children(found->second.form)[2],
                    render_type_ref(store_.tagged_union_alternatives
                                        .type[store_.tagged_union_alternatives.index(alternative)]),
                    original,
                    alternative_replacements)) {
                return std::nullopt;
            }
            auto const alternative_properties{std::array<SourceProperty, 2>{
                std::pair{"tag",
                          std::optional{std::string{text(
                              store_.tagged_union_alternatives
                                  .tag[store_.tagged_union_alternatives.index(alternative)])}}},
                std::pair{
                    "count",
                    store_.tagged_union_alternatives
                            .count[store_.tagged_union_alternatives.index(alternative)]
                            .has_value()
                        ? std::optional{std::to_string(
                              *store_.tagged_union_alternatives
                                   .count[store_.tagged_union_alternatives.index(alternative)])}
                        : std::nullopt}}};
            if (!patch_source_properties(found->second.form,
                                         2,
                                         alternative_properties,
                                         "      ",
                                         original,
                                         alternative_replacements)) {
                return std::nullopt;
            }
            auto rendered{apply_source_replacements_to_range(original,
                                                             found->second.begin,
                                                             found->second.end,
                                                             std::move(alternative_replacements))};
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

    auto render_soa_member(lispb::codegen::SoaMemberId const& member) -> std::string {
        std::ostringstream output;
        output << "(member " << text(store_.soa_members.name[store_.soa_members.index(member)])
               << ' '
               << soa_member_kind_name(store_.soa_members.kind[store_.soa_members.index(member)])
               << ' ' << render_type_ref(store_.soa_members.type[store_.soa_members.index(member)]);
        if (store_.soa_members.fixed_schema[store_.soa_members.index(member)].has_value()) {
            output << "\n      :fixed-schema "
                   << text(*store_.soa_members.fixed_schema[store_.soa_members.index(member)]);
        }
        if (store_.soa_members.nested_schema[store_.soa_members.index(member)].has_value()) {
            output << "\n      :nested-schema "
                   << text(*store_.soa_members.nested_schema[store_.soa_members.index(member)]);
        }
        if (store_.soa_members.mask_field[store_.soa_members.index(member)]) {
            output << "\n      :mask-field true";
        }
        if (!items(store_.soa_members.mask_dimensions[store_.soa_members.index(member)]).empty()) {
            output << "\n      :mask-dimensions (";
            for (std::size_t index{};
                 index <
                 items(store_.soa_members.mask_dimensions[store_.soa_members.index(member)]).size();
                 ++index) {
                auto const& dimension{items(
                    store_.soa_members.mask_dimensions[store_.soa_members.index(member)])[index]};
                output << (index == 0 ? "" : " ") << '('
                       << text(store_.soa_mask_dimensions
                                   .index_name[store_.soa_mask_dimensions.index(dimension)])
                       << ' '
                       << quote(store_.soa_mask_dimensions
                                    .extent[store_.soa_mask_dimensions.index(dimension)])
                       << ')';
            }
            output << ')';
        }
        if (store_.soa_members.relationship[store_.soa_members.index(member)].has_value()) {
            output << "\n      "
                   << render_semantic_relation(
                          *store_.soa_members.relationship[store_.soa_members.index(member)]);
        }
        output << ')';
        return output.str();
    }

    auto render_soa(lispb::codegen::DeclarationHandle const& schema) -> std::string {
        std::ostringstream output;
        output << "(struct " << text(store_.soas.name[store_.soas.index(schema)]);
        if (store_.soas.view_name[store_.soas.index(schema)].has_value()) {
            output << "\n    :view-name "
                   << text(*store_.soas.view_name[store_.soas.index(schema)]);
        }
        if (store_.soas.const_view_name[store_.soas.index(schema)].has_value()) {
            output << "\n    :const-view-name "
                   << text(*store_.soas.const_view_name[store_.soas.index(schema)]);
        }
        if (!items(store_.soas.operations[store_.soas.index(schema)]).empty()) {
            output << "\n    :operations (";
            for (std::size_t index{};
                 index < items(store_.soas.operations[store_.soas.index(schema)]).size();
                 ++index) {
                output << (index == 0 ? "" : " ")
                       << storage_operation_name(
                              items(store_.soas.operations[store_.soas.index(schema)])[index]);
            }
            output << ')';
        }
        if (store_.soas.export_specifier[store_.soas.index(schema)].value != 0) {
            output << "\n    :export-specifier "
                   << text(store_.soas.export_specifier[store_.soas.index(schema)]);
        }
        if (!texts(store_.soas.using_declarations[store_.soas.index(schema)]).empty()) {
            output << "\n    :using-declarations ";
            render_quoted_list(output,
                               texts(store_.soas.using_declarations[store_.soas.index(schema)]));
        }
        if (store_.soas.equivalent_type[store_.soas.index(schema)].has_value()) {
            output << "\n    :equivalent-type "
                   << render_type_ref(*store_.soas.equivalent_type[store_.soas.index(schema)]);
        }
        if (!texts(store_.soas.vector_components[store_.soas.index(schema)]).empty()) {
            output << "\n    :vector-components (";
            for (std::size_t index{};
                 index < texts(store_.soas.vector_components[store_.soas.index(schema)]).size();
                 ++index) {
                if (index != 0) {
                    output << ' ';
                }
                output << texts(store_.soas.vector_components[store_.soas.index(schema)])[index];
            }
            output << ')';
        }
        if (store_.soas.copy_element_memberwise[store_.soas.index(schema)]) {
            output << "\n    :copy-element-memberwise true";
        }
        if (store_.soas.layout_only[store_.soas.index(schema)]) {
            output << "\n    :layout-only true";
        }
        if (store_.soas.storage[store_.soas.index(schema)]) {
            output << "\n    :storage "
                   << codegen::soa_storage_name(*store_.soas.storage[store_.soas.index(schema)]);
        }
        if (store_.soas.field_mask_name[store_.soas.index(schema)].has_value()) {
            output << "\n    :field-mask-name "
                   << text(*store_.soas.field_mask_name[store_.soas.index(schema)]);
        }
        if (store_.soas.field_enum_name[store_.soas.index(schema)].has_value()) {
            output << "\n    :field-enum-name "
                   << text(*store_.soas.field_enum_name[store_.soas.index(schema)]);
        }
        for (auto const& member : items(store_.soas.members[store_.soas.index(schema)])) {
            output << "\n    " << render_soa_member(member);
        }
        for (auto const& function : items(store_.soas.functions[store_.soas.index(schema)])) {
            output << '\n';
            render_function(output, function, "    ");
        }
        for (auto const& function :
             items(store_.soas.const_view_functions[store_.soas.index(schema)])) {
            output << '\n';
            render_function(output, function, "    ", "view-function");
        }
        for (auto const& function :
             items(store_.soas.mutable_view_functions[store_.soas.index(schema)])) {
            output << '\n';
            render_function(output, function, "    ", "mutable-view-function");
        }
        if (store_.soas.array_allocator[store_.soas.index(schema)]) {
            output << "\n    :array-allocator "
                   << render_type_ref(*store_.soas.array_allocator[store_.soas.index(schema)]);
        }
        if (store_.soas.fixed[store_.soas.index(schema)].has_value()) {
            output << "\n    (fixed "
                   << text(store_.fixed_soas.storage_name[store_.fixed_soas.index(
                          *store_.soas.fixed[store_.soas.index(schema)])]);
            if (!texts(store_.fixed_soas.containers[store_.fixed_soas.index(
                           *store_.soas.fixed[store_.soas.index(schema)])])
                     .empty()) {
                output << " :containers (";
                for (std::size_t index{};
                     index < texts(store_.fixed_soas.containers[store_.fixed_soas.index(
                                       *store_.soas.fixed[store_.soas.index(schema)])])
                                 .size();
                     ++index) {
                    output << (index == 0 ? "" : " ")
                           << texts(store_.fixed_soas.containers[store_.fixed_soas.index(
                                  *store_.soas.fixed[store_.soas.index(schema)])])[index];
                }
                output << ')';
            }
            output << ')';
        }
        if (store_.soas.single_allocation[store_.soas.index(schema)].has_value()) {
            output << "\n    (single-allocation "
                   << text(*store_.soas.single_allocation[store_.soas.index(schema)]);
            output << ')';
        }
        output << ')';
        return output.str();
    }

    auto render_vector_soa(lispb::codegen::DeclarationHandle const& schema) -> std::string {
        std::ostringstream output;
        output << "(vector-soa " << text(store_.vector_soas.name[store_.vector_soas.index(schema)])
               << "\n    :value-type "
               << render_type_ref(store_.vector_soas.value_type[store_.vector_soas.index(schema)])
               << "\n    :components ";
        render_quoted_list(output,
                           texts(store_.vector_soas.components[store_.vector_soas.index(schema)]));
        if (!texts(store_.vector_soas.equivalent_members[store_.vector_soas.index(schema)])
                 .empty()) {
            output << "\n    :equivalent-members ";
            render_quoted_list(
                output,
                texts(store_.vector_soas.equivalent_members[store_.vector_soas.index(schema)]));
        }
        if (store_.vector_soas.equivalent_accessors[store_.vector_soas.index(schema)]) {
            output << "\n    :equivalent-accessors true";
        }
        if (store_.vector_soas.equivalent_constructor[store_.vector_soas.index(schema)]
                .has_value()) {
            output << "\n    :equivalent-constructor "
                   << text(*store_.vector_soas
                                .equivalent_constructor[store_.vector_soas.index(schema)]);
        }
        output << "\n    :equivalent-type "
               << render_type_ref(
                      store_.vector_soas.equivalent_type[store_.vector_soas.index(schema)]);
        if (store_.vector_soas.export_specifier[store_.vector_soas.index(schema)].value != 0) {
            output << "\n    :export-specifier "
                   << text(store_.vector_soas.export_specifier[store_.vector_soas.index(schema)]);
        }
        if (store_.vector_soas.fixed[store_.vector_soas.index(schema)].has_value()) {
            output << "\n    (fixed "
                   << text(store_.fixed_soas.storage_name[store_.fixed_soas.index(
                          *store_.vector_soas.fixed[store_.vector_soas.index(schema)])]);
            if (!texts(store_.fixed_soas.containers[store_.fixed_soas.index(
                           *store_.vector_soas.fixed[store_.vector_soas.index(schema)])])
                     .empty()) {
                output << " :containers ";
                render_quoted_list(
                    output,
                    texts(store_.fixed_soas.containers[store_.fixed_soas.index(
                        *store_.vector_soas.fixed[store_.vector_soas.index(schema)])]));
            }
            output << ')';
        }
        output << ')';
        return output.str();
    }

    auto render_homogeneous_layout(lispb::codegen::DeclarationHandle const& schema) -> std::string {
        std::ostringstream output;
        output << "(layout "
               << text(store_.homogeneous_layouts.name[store_.homogeneous_layouts.index(schema)])
               << "\n    :components ";
        render_quoted_list(
            output,
            texts(store_.homogeneous_layouts.components[store_.homogeneous_layouts.index(schema)]));
        if (!texts(
                 store_.homogeneous_layouts.input_members[store_.homogeneous_layouts.index(schema)])
                 .empty()) {
            output << "\n    :input-members ";
            render_quoted_list(output,
                               texts(store_.homogeneous_layouts
                                         .input_members[store_.homogeneous_layouts.index(schema)]));
        }
        if (store_.homogeneous_layouts.export_specifier[store_.homogeneous_layouts.index(schema)]
                .value != 0) {
            output << "\n    :export-specifier "
                   << text(store_.homogeneous_layouts
                               .export_specifier[store_.homogeneous_layouts.index(schema)]);
        }
        for (auto const& value :
             items(store_.homogeneous_layouts
                       .value_types[store_.homogeneous_layouts.index(schema)])) {
            output << "\n    (value-type "
                   << render_type_ref(
                          store_.homogeneous_values.type[store_.homogeneous_values.index(value)])
                   << ' '
                   << text(
                          store_.homogeneous_values.suffix[store_.homogeneous_values.index(value)]);
            if (store_.homogeneous_values.equivalent_type[store_.homogeneous_values.index(value)]
                    .has_value()) {
                output << "\n      :equivalent-type "
                       << render_type_ref(
                              *store_.homogeneous_values
                                   .equivalent_type[store_.homogeneous_values.index(value)]);
            }
            if (!store_.homogeneous_values.input_types[store_.homogeneous_values.index(value)]
                     .empty()) {
                output << "\n      :input-types (";
                for (std::size_t index{};
                     index <
                     store_.homogeneous_values.input_types[store_.homogeneous_values.index(value)]
                         .size();
                     ++index) {
                    output << (index == 0 ? "" : " ")
                           << render_type_ref(items(
                                  store_.homogeneous_values
                                      .input_types[store_.homogeneous_values.index(value)])[index]);
                }
                output << ')';
            }
            output << ')';
        }
        output << ')';
        return output.str();
    }

    auto render_static_table(lispb::codegen::DeclarationHandle const& schema) -> std::string {
        std::ostringstream output;
        output << "(table " << text(store_.static_tables.name[store_.static_tables.index(schema)]);
        if (store_.static_tables.export_specifier[store_.static_tables.index(schema)].value != 0) {
            output
                << "\n    :export-specifier "
                << text(store_.static_tables.export_specifier[store_.static_tables.index(schema)]);
        }
        for (auto const& row :
             items(store_.static_tables.rows[store_.static_tables.index(schema)])) {
            output << "\n    (row "
                   << text(store_.static_table_rows.name[store_.static_table_rows.index(row)])
                   << ')';
        }
        for (auto const& column :
             items(store_.static_tables.columns[store_.static_tables.index(schema)])) {
            output
                << "\n    (column "
                << text(store_.static_table_columns.name[store_.static_table_columns.index(column)])
                << ' '
                << render_type_ref(
                       store_.static_table_columns.type[store_.static_table_columns.index(column)])
                << ')';
        }
        for (auto const& group :
             items(store_.static_tables.groups[store_.static_tables.index(schema)])) {
            output << "\n    (group "
                   << text(store_.static_table_groups.name[store_.static_table_groups.index(group)])
                   << ' '
                   << render_type_ref(
                          store_.static_table_groups.type[store_.static_table_groups.index(group)])
                   << " :columns ";
            render_quoted_list(
                output,
                texts(store_.static_table_groups.columns[store_.static_table_groups.index(group)]));
            output << ')';
        }
        output << ')';
        return output.str();
    }

    auto render_facade(lispb::codegen::DeclarationHandle const& schema) -> std::string {
        std::ostringstream output;
        output << "(facade " << text(store_.facades.name[store_.facades.index(schema)]) << ' '
               << render_type_ref(store_.facades.target_type[store_.facades.index(schema)]) << ' '
               << text(store_.facades.target_member_name[store_.facades.index(schema)]);
        if (!texts(store_.facades.validation_lines[store_.facades.index(schema)]).empty()) {
            output << "\n    :validation ";
            render_quoted_list(
                output, texts(store_.facades.validation_lines[store_.facades.index(schema)]));
        }
        if (!texts(store_.facades.validation_dependencies[store_.facades.index(schema)]).empty()) {
            output << "\n    :validation-dependencies ";
            render_quoted_list(
                output,
                texts(store_.facades.validation_dependencies[store_.facades.index(schema)]));
        }
        if (store_.facades.export_specifier[store_.facades.index(schema)].value != 0) {
            output << "\n    :export-specifier "
                   << text(store_.facades.export_specifier[store_.facades.index(schema)]);
        }
        if (access_keyword(store_.facades.bind_access[store_.facades.index(schema)]) != "public") {
            output << "\n    :bind-access "
                   << access_keyword(store_.facades.bind_access[store_.facades.index(schema)]);
        }
        if (access_keyword(store_.facades.method_access[store_.facades.index(schema)]) !=
            "public") {
            output << "\n    :method-access "
                   << access_keyword(store_.facades.method_access[store_.facades.index(schema)]);
        }
        if (!texts(store_.facades.friends[store_.facades.index(schema)]).empty()) {
            output << "\n    :friends ";
            render_quoted_list(output, texts(store_.facades.friends[store_.facades.index(schema)]));
        }
        if (codegen::record_keyword(store_.facades.friend_kind[store_.facades.index(schema)]) !=
            "class") {
            output << "\n    :friend-kind "
                   << codegen::record_keyword(
                          store_.facades.friend_kind[store_.facades.index(schema)]);
        }
        if (store_.facades.definitions_in_source[store_.facades.index(schema)]) {
            output << "\n    :definitions-in-source true";
        }
        if (store_.facades.reference_target[store_.facades.index(schema)]) {
            output << "\n    :target-storage reference";
        }
        for (auto const& method : items(store_.facades.methods[store_.facades.index(schema)])) {
            output << "\n    (method "
                   << text(store_.facade_methods.name[store_.facade_methods.index(method)]) << ' '
                   << render_type_ref(
                          store_.facade_methods.return_type[store_.facade_methods.index(method)]);
            if (store_.facade_methods.is_const[store_.facade_methods.index(method)]) {
                output << "\n      :const true";
            }
            if (store_.facade_methods.is_noexcept[store_.facade_methods.index(method)]) {
                output << "\n      :noexcept true";
            }
            if (store_.facade_methods.target_name[store_.facade_methods.index(method)]
                    .has_value()) {
                output << "\n      :target-name "
                       << text(*store_.facade_methods
                                    .target_name[store_.facade_methods.index(method)]);
            }
            for (auto const& parameter :
                 items(store_.facade_methods.parameters[store_.facade_methods.index(method)])) {
                output << "\n      (parameter "
                       << text(store_.parameters.name[store_.parameters.index(parameter)]) << ' '
                       << render_type_ref(
                              store_.parameters.type[store_.parameters.index(parameter)]);
                if (store_.parameters.default_value[store_.parameters.index(parameter)]
                        .has_value()) {
                    output << " :default "
                           << quote(*store_.parameters
                                         .default_value[store_.parameters.index(parameter)]);
                }
                output << ')';
            }
            output << ')';
        }
        output << ')';
        return output.str();
    }

    template <typename>
    inline static constexpr bool unrendered_declaration_schema{false};

    auto render_declaration_schema(codegen::DeclarationHandle const declaration) -> std::string {
        return [&]() -> std::string {
            auto const value{declaration};
            switch (value.kind()) {
                case lispb::codegen::DeclarationKind::enumeration: {

                    return render_enum(value);
                }
                case lispb::codegen::DeclarationKind::integer_scalar: {

                    return render_integer_scalar(value);
                }
                case lispb::codegen::DeclarationKind::linear_quantized: {

                    return render_linear_quantized(value);
                }
                case lispb::codegen::DeclarationKind::integer_varint: {

                    return render_integer_varint(value);
                }
                case lispb::codegen::DeclarationKind::fixed_point: {

                    return render_fixed_point(value);
                }
                case lispb::codegen::DeclarationKind::optional_sentinel: {

                    return render_optional_sentinel(value);
                }
                case lispb::codegen::DeclarationKind::optional_presence_bit: {

                    return render_optional_presence_bit(value);
                }
                case lispb::codegen::DeclarationKind::mini_float: {

                    return render_mini_float(value);
                }
                case lispb::codegen::DeclarationKind::packed_value: {

                    return render_packed_value(value);
                }
                case lispb::codegen::DeclarationKind::record: {

                    return render_record(value);
                }
                case lispb::codegen::DeclarationKind::union_type: {

                    return render_union(value);
                }
                case lispb::codegen::DeclarationKind::tagged_union: {

                    return render_tagged_union(value);
                }
                case lispb::codegen::DeclarationKind::soa: {

                    return render_soa(value);
                }
                case lispb::codegen::DeclarationKind::vector_soa: {

                    return render_vector_soa(value);
                }
                case lispb::codegen::DeclarationKind::homogeneous_layout: {

                    return render_homogeneous_layout(value);
                }
                case lispb::codegen::DeclarationKind::static_table: {

                    return render_static_table(value);
                }
                case lispb::codegen::DeclarationKind::facade: {

                    return render_facade(value);
                }
                default:
                    std::unreachable();
            }
        }();
    }

    auto render_module(codegen::ModuleHandle const schema) -> std::optional<std::string> {
        auto const normal{((schema).kind() == lispb::codegen::ModuleKind::normal
                               ? std::optional{schema}
                               : std::nullopt)};
        if (!normal.has_value()) {
            return std::nullopt;
        }
        auto const& module{*normal};
        auto const settings{store_.normal_modules.settings[store_.normal_modules.index(module)]};
        std::ostringstream output;
        output
            << '(' << "module" << ' '
            << text(store_.module_settings_store.name[store_.module_settings_store.index(settings)])
            << "\n  :header "
            << quote(text(store_.module_settings_store
                              .header[store_.module_settings_store.index(settings)]));
        if (store_.module_settings_store.source[store_.module_settings_store.index(settings)]
                .has_value()) {
            output << "\n  :source "
                   << quote(text(*store_.module_settings_store
                                      .source[store_.module_settings_store.index(settings)]));
        }
        if (store_.module_settings_store
                .header_include[store_.module_settings_store.index(settings)]
                .has_value()) {
            output << "\n  :header-include "
                   << quote(*store_.module_settings_store
                                 .header_include[store_.module_settings_store.index(settings)]);
        }
        if (store_.module_settings_store
                .namespace_name[store_.module_settings_store.index(settings)]
                .has_value()) {
            output << "\n  :namespace "
                   << text(*store_.module_settings_store
                                .namespace_name[store_.module_settings_store.index(settings)]);
        }
        if (!store_.module_settings_store
                 .include_order[store_.module_settings_store.index(settings)]
                 .empty()) {
            output << "\n  :include-order ";
            render_quoted_list(
                output,
                texts(store_.module_settings_store
                          .include_order[store_.module_settings_store.index(settings)]));
        }
        if (!store_.module_settings_store
                 .prelude_lines[store_.module_settings_store.index(settings)]
                 .empty()) {
            output << "\n  :prelude ";
            render_quoted_list(
                output,
                texts(store_.module_settings_store
                          .prelude_lines[store_.module_settings_store.index(settings)]));
        }

        if (store_.normal_modules.enum_helper_namespace[store_.normal_modules.index(module)]
                .has_value()) {
            output << "\n  :helper-namespace "
                   << text(*store_.normal_modules
                                .enum_helper_namespace[store_.normal_modules.index(module)]);
        }
        if (store_.normal_modules.soa_backend[store_.normal_modules.index(module)] ==
            codegen::SoaBackend::standard_library) {
            output << "\n  :backend standard-library";
        }
        for (auto const& allocator : items(
                 store_.normal_modules.soa_array_allocators[store_.normal_modules.index(module)])) {
            output << "\n  (array-allocator "
                   << text(store_.soa_allocator_variants
                               .prefix[store_.soa_allocator_variants.index(allocator)])
                   << ' '
                   << render_type_ref(
                          store_.soa_allocator_variants
                              .allocator[store_.soa_allocator_variants.index(allocator)])
                   << ')';
        }
        for (auto const& declaration :
             items(store_.normal_modules.declarations[store_.normal_modules.index(module)])) {
            output << "\n  " << render_declaration_schema(declaration);
        }
        output << ')';
        return output.str();
    }

    auto render_fixed_containers(auto const& containers) -> std::string {
        auto rendered{std::string{"("}};
        for (std::size_t index{}; index < containers.size(); ++index) {
            rendered += index == 0 ? "" : " ";
            rendered += containers[index];
        }
        rendered += ')';
        return rendered;
    }

    auto render_fixed_soa(lispb::codegen::FixedSoaId const& schema) -> std::string {
        auto rendered{
            std::string{"(fixed "} +
            std::string{text(store_.fixed_soas.storage_name[store_.fixed_soas.index(schema)])}};
        if (!texts(store_.fixed_soas.containers[store_.fixed_soas.index(schema)]).empty()) {
            rendered += " :containers " +
                        render_fixed_containers(
                            texts(store_.fixed_soas.containers[store_.fixed_soas.index(schema)]));
        }
        rendered += ')';
        return rendered;
    }

    auto render_mask_dimension(lispb::codegen::SoaMaskDimensionId const& dimension) -> std::string {
        return std::string{"("} +
               std::string{text(store_.soa_mask_dimensions
                                    .index_name[store_.soa_mask_dimensions.index(dimension)])} +
               " " +
               quote(text(store_.soa_mask_dimensions
                              .extent[store_.soa_mask_dimensions.index(dimension)])) +
               ")";
    }

    auto render_mask_dimensions(auto const& dimensions) -> std::string {
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
        std::span<FormId const> const source_dimensions,
        std::span<codegen::SoaMaskDimensionId const> const current_dimensions)
        -> std::optional<std::pair<std::string_view, std::string_view>> {
        if (source_dimensions.size() != current_dimensions.size()) {
            return std::nullopt;
        }

        std::optional<std::pair<std::string_view, std::string_view>> renamed;
        for (std::size_t index{}; index < source_dimensions.size(); ++index) {
            if (!storage_.syntax.is_list(source_dimensions[index]) ||
                storage_.syntax.children(source_dimensions[index]).size() != 2) {
                return std::nullopt;
            }
            auto const& source_name{
                storage_.syntax.text(storage_.syntax.children(source_dimensions[index])[0])};
            auto const current{current_dimensions[index]};
            if (source_name == text(store_.soa_mask_dimensions
                                        .index_name[store_.soa_mask_dimensions.index(current)])) {
                continue;
            }
            if (renamed.has_value() ||
                !source_form_matches_rendered(
                    storage_.syntax.children(source_dimensions[index])[1],
                    quote(store_.soa_mask_dimensions
                              .extent[store_.soa_mask_dimensions.index(current)]))) {
                return std::nullopt;
            }
            renamed = std::pair{std::string_view{source_name},
                                text(store_.soa_mask_dimensions
                                         .index_name[store_.soa_mask_dimensions.index(current)])};
        }

        if (!renamed.has_value()) {
            return std::nullopt;
        }
        auto const source_has_new_name{
            std::ranges::any_of(source_dimensions, [&](FormId const dimension) {
                return storage_.syntax.text(storage_.syntax.children(dimension)[0]) ==
                       renamed->second;
            })};
        auto const current_has_old_name{
            std::ranges::find(
                current_dimensions, renamed->first, [&](codegen::SoaMaskDimensionId const value) {
                    return text(store_.soa_mask_dimensions
                                    .index_name[store_.soa_mask_dimensions.index(value)]);
                }) != current_dimensions.end()};
        return source_has_new_name || current_has_old_name ? std::nullopt : renamed;
    }

    auto patch_source_mask_dimensions(FormId const source_member,
                                      auto const& dimensions,
                                      std::string_view const original,
                                      std::vector<SourceReplacement>& replacements) -> bool {
        std::optional<FormId> source_dimensions{};
        for (std::size_t index{4}; index + 1 < storage_.syntax.children(source_member).size();
             ++index) {
            if (storage_.syntax.kind(storage_.syntax.children(source_member)[index]) ==
                    codegen::sexpr::TokenKind::keyword &&
                storage_.syntax.text(storage_.syntax.children(source_member)[index]) ==
                    "mask-dimensions") {
                source_dimensions = storage_.syntax.children(source_member)[index + 1];
                break;
            }
        }

        auto const rendered_dimensions{render_mask_dimensions(dimensions)};
        if (!source_dimensions.has_value() || dimensions.empty()) {
            auto const properties{std::array<SourceProperty, 1>{std::pair{
                "mask-dimensions",
                rendered_dimensions.empty() ? std::nullopt : std::optional{rendered_dimensions}}}};
            return patch_source_properties(
                source_member, 3, properties, "      ", original, replacements);
        }
        if (!storage_.syntax.is_list(*source_dimensions)) {
            return false;
        }
        for (auto const& dimension : storage_.syntax.children(*source_dimensions)) {
            if (!storage_.syntax.is_list(dimension) ||
                storage_.syntax.children(dimension).size() != 2) {
                return false;
            }
        }

        auto const renamed_dimension{infer_single_mask_dimension_rename(
            storage_.syntax.children(*source_dimensions), dimensions)};
        auto const rows_begin{storage_.syntax.token_span(*source_dimensions).offset + 1};
        auto const multiline{
            original
                .substr(rows_begin,
                        storage_.syntax.closing_span(*source_dimensions).offset - rows_begin)
                .contains('\n')};
        if (!multiline) {
            if (storage_.syntax.children(*source_dimensions).size() != dimensions.size()) {
                return patch_source_form(
                    *source_dimensions, rendered_dimensions, original, replacements);
            }
            for (std::size_t index{}; index < dimensions.size(); ++index) {
                auto const& source_dimension{storage_.syntax.children(*source_dimensions)[index]};
                auto const& dimension{dimensions[index]};
                auto const same_name{
                    storage_.syntax.text(storage_.syntax.children(source_dimension)[0]) ==
                    text(store_.soa_mask_dimensions
                             .index_name[store_.soa_mask_dimensions.index(dimension)])};
                auto const renamed{
                    renamed_dimension.has_value() &&
                    renamed_dimension->first ==
                        storage_.syntax.text(storage_.syntax.children(source_dimension)[0]) &&
                    renamed_dimension->second ==
                        text(store_.soa_mask_dimensions
                                 .index_name[store_.soa_mask_dimensions.index(dimension)])};
                if (!same_name && !renamed) {
                    return patch_source_form(
                        *source_dimensions, rendered_dimensions, original, replacements);
                }
                if ((renamed &&
                     !patch_source_form(
                         storage_.syntax.children(source_dimension)[0],
                         text(store_.soa_mask_dimensions
                                  .index_name[store_.soa_mask_dimensions.index(dimension)]),
                         original,
                         replacements)) ||
                    !patch_source_form(
                        storage_.syntax.children(source_dimension)[1],
                        quote(store_.soa_mask_dimensions
                                  .extent[store_.soa_mask_dimensions.index(dimension)]),
                        original,
                        replacements)) {
                    return false;
                }
            }
            return true;
        }

        struct SourceDimension {
            FormId form;
            std::size_t begin{};
            std::size_t end{};
        };
        std::map<std::string_view, SourceDimension> source_by_name;
        auto row_begin{std::size_t{rows_begin}};
        for (auto const& dimension : storage_.syntax.children(*source_dimensions)) {
            auto const row_end{source_form_line_end(
                dimension, original, storage_.syntax.closing_span(*source_dimensions).offset)};
            if (!row_end.has_value() || row_begin > storage_.syntax.token_span(dimension).offset ||
                !source_by_name
                     .emplace(
                         storage_.syntax.text(storage_.syntax.children(dimension)[0]),
                         SourceDimension{.form = dimension, .begin = row_begin, .end = *row_end})
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
            auto const renamed{
                renamed_dimension.has_value() &&
                renamed_dimension->second ==
                    text(store_.soa_mask_dimensions
                             .index_name[store_.soa_mask_dimensions.index(dimension)])};
            auto const found{source_by_name.find(
                renamed ? renamed_dimension->first
                        : text(store_.soa_mask_dimensions
                                   .index_name[store_.soa_mask_dimensions.index(dimension)]))};
            if (found == source_by_name.end()) {
                append_row("        " + render_mask_dimension(dimension));
                continue;
            }

            std::vector<SourceReplacement> dimension_replacements;
            if ((renamed && !patch_source_form(
                                storage_.syntax.children(found->second.form)[0],
                                text(store_.soa_mask_dimensions
                                         .index_name[store_.soa_mask_dimensions.index(dimension)]),
                                original,
                                dimension_replacements)) ||
                !patch_source_form(storage_.syntax.children(found->second.form)[1],
                                   quote(store_.soa_mask_dimensions
                                             .extent[store_.soa_mask_dimensions.index(dimension)]),
                                   original,
                                   dimension_replacements)) {
                return false;
            }
            auto rendered{apply_source_replacements_to_range(original,
                                                             found->second.begin,
                                                             found->second.end,
                                                             std::move(dimension_replacements))};
            if (!rendered.has_value()) {
                return false;
            }
            append_row(std::move(*rendered));
        }
        replacements.push_back(
            {.begin = rows_begin, .end = row_begin, .text = std::move(rendered_rows)});
        return true;
    }

    enum class SourceListValueKind : std::uint8_t { atom, quoted_string };

    auto render_source_list_value(std::string_view const value, SourceListValueKind const kind)
        -> std::string {
        return kind == SourceListValueKind::quoted_string ? quote(value) : std::string{value};
    }

    auto render_source_list_values(auto const& values, SourceListValueKind const kind)
        -> std::string {
        auto rendered{std::string{"("}};
        for (std::size_t index{}; index < values.size(); ++index) {
            rendered += index == 0 ? "" : " ";
            rendered += render_source_list_value(values[index], kind);
        }
        rendered += ')';
        return rendered;
    }

    auto render_quoted_values(auto const& values) -> std::string {
        return render_source_list_values(values, SourceListValueKind::quoted_string);
    }

    auto infer_single_list_value_edit(std::span<FormId const> const source_values,
                                      auto const& current_values)
        -> std::optional<std::pair<std::string_view, std::string_view>> {
        if (source_values.size() != current_values.size()) {
            return std::nullopt;
        }

        std::optional<std::pair<std::string_view, std::string_view>> edited;
        for (std::size_t index{}; index < source_values.size(); ++index) {
            auto const& source_value{storage_.syntax.text(source_values[index])};
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
        auto const source_has_new_value{std::ranges::any_of(source_values, [&](FormId const value) {
            return storage_.syntax.text(value) == edited->second;
        })};
        auto const current_has_old_value{std::ranges::find(current_values, edited->first) !=
                                         current_values.end()};
        return source_has_new_value || current_has_old_value ? std::nullopt : edited;
    }

    auto patch_source_scalar_list_property(FormId const source,
                                           std::size_t const positional_count,
                                           std::string_view const property_name,
                                           auto const& values,
                                           SourceListValueKind const value_kind,
                                           std::string_view const property_indentation,
                                           std::string_view const row_indentation,
                                           std::string_view const original,
                                           std::vector<SourceReplacement>& replacements) -> bool {
        std::optional<FormId> source_values{};
        for (auto index{positional_count + 1}; index + 1 < storage_.syntax.children(source).size();
             ++index) {
            if (storage_.syntax.kind(storage_.syntax.children(source)[index]) ==
                    codegen::sexpr::TokenKind::keyword &&
                storage_.syntax.text(storage_.syntax.children(source)[index]) == property_name) {
                source_values = storage_.syntax.children(source)[index + 1];
                break;
            }
        }

        auto const rendered_values{
            values.empty() ? std::optional<std::string>{}
                           : std::optional{render_source_list_values(values, value_kind)}};
        if (!source_values.has_value() || values.empty()) {
            auto const properties{
                std::array<SourceProperty, 1>{std::pair{property_name, rendered_values}}};
            return patch_source_properties(
                source, positional_count, properties, property_indentation, original, replacements);
        }
        auto const expected_token_kind{value_kind == SourceListValueKind::quoted_string
                                           ? codegen::sexpr::TokenKind::string
                                           : codegen::sexpr::TokenKind::atom};
        if (!storage_.syntax.is_list(*source_values) ||
            std::ranges::any_of(storage_.syntax.children(*source_values), [&](FormId const value) {
                return storage_.syntax.is_list(value) ||
                       storage_.syntax.kind(value) != expected_token_kind;
            })) {
            return false;
        }

        std::set<std::string_view> unique_source_values;
        for (auto const& value : storage_.syntax.children(*source_values)) {
            if (!unique_source_values.insert(storage_.syntax.text(value)).second) {
                return patch_source_form(*source_values, *rendered_values, original, replacements);
            }
        }
        std::set<std::string_view> unique_current_values;
        for (auto const& value : values) {
            if (!unique_current_values.insert(value).second) {
                return patch_source_form(*source_values, *rendered_values, original, replacements);
            }
        }

        auto const edited_value{
            infer_single_list_value_edit(storage_.syntax.children(*source_values), values)};
        auto const rows_begin{storage_.syntax.token_span(*source_values).offset + 1};
        auto const multiline{
            original
                .substr(rows_begin,
                        storage_.syntax.closing_span(*source_values).offset - rows_begin)
                .contains('\n')};
        if (!multiline) {
            if (storage_.syntax.children(*source_values).size() != values.size()) {
                return patch_source_form(*source_values, *rendered_values, original, replacements);
            }
            for (std::size_t index{}; index < values.size(); ++index) {
                auto const& source_value{storage_.syntax.children(*source_values)[index]};
                auto const& current_value{values[index]};
                auto const same_value{storage_.syntax.text(source_value) == current_value};
                auto const edited{edited_value.has_value() &&
                                  edited_value->first == storage_.syntax.text(source_value) &&
                                  edited_value->second == current_value};
                if (!same_value && !edited) {
                    return patch_source_form(
                        *source_values, *rendered_values, original, replacements);
                }
                if (edited &&
                    !patch_source_form(source_value,
                                       render_source_list_value(current_value, value_kind),
                                       original,
                                       replacements)) {
                    return false;
                }
            }
            return true;
        }

        struct SourceValue {
            FormId form;
            std::size_t begin{};
            std::size_t end{};
        };
        std::map<std::string_view, SourceValue> source_by_value;
        auto row_begin{std::size_t{rows_begin}};
        for (auto const& value : storage_.syntax.children(*source_values)) {
            auto const row_end{source_form_line_end(
                value, original, storage_.syntax.closing_span(*source_values).offset)};
            if (!row_end.has_value() || row_begin > storage_.syntax.token_span(value).offset ||
                !source_by_value
                     .emplace(storage_.syntax.text(value),
                              SourceValue{.form = value, .begin = row_begin, .end = *row_end})
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
                append_row(std::string{row_indentation} +
                           render_source_list_value(value, value_kind));
                continue;
            }

            std::vector<SourceReplacement> value_replacements;
            if (edited && !patch_source_form(found->second.form,
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

    auto patch_source_quoted_list_property(FormId const source,
                                           std::size_t const positional_count,
                                           std::string_view const property_name,
                                           auto const& values,
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

    auto patch_source_atom_list_property(FormId const source,
                                         std::size_t const positional_count,
                                         std::string_view const property_name,
                                         auto const& values,
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

    auto infer_single_fixed_container_rename(std::span<FormId const> const source_containers,
                                             auto const& current_containers)
        -> std::optional<std::pair<std::string_view, std::string_view>> {
        if (source_containers.size() != current_containers.size()) {
            return std::nullopt;
        }

        std::optional<std::pair<std::string_view, std::string_view>> renamed;
        for (std::size_t index{}; index < source_containers.size(); ++index) {
            auto const& source_name{storage_.syntax.text(source_containers[index])};
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
            std::ranges::any_of(source_containers, [&](FormId const container) {
                return storage_.syntax.text(container) == renamed->second;
            })};
        auto const current_has_old_name{std::ranges::find(current_containers, renamed->first) !=
                                        current_containers.end()};
        return source_has_new_name || current_has_old_name ? std::nullopt : renamed;
    }

    auto patch_source_fixed_soa(lispb::codegen::FixedSoaId const& schema,
                                FormId const source,
                                std::string_view const original,
                                std::vector<SourceReplacement>& replacements) -> bool {
        if (storage_.syntax.children(source).size() < 2 ||
            !patch_source_form(
                storage_.syntax.children(source)[1],
                text(store_.fixed_soas.storage_name[store_.fixed_soas.index(schema)]),
                original,
                replacements)) {
            return false;
        }

        std::optional<FormId> source_containers{};
        for (std::size_t index{2}; index + 1 < storage_.syntax.children(source).size(); ++index) {
            if (storage_.syntax.kind(storage_.syntax.children(source)[index]) ==
                    codegen::sexpr::TokenKind::keyword &&
                storage_.syntax.text(storage_.syntax.children(source)[index]) == "containers") {
                source_containers = storage_.syntax.children(source)[index + 1];
                break;
            }
        }

        auto const rendered_containers{
            texts(store_.fixed_soas.containers[store_.fixed_soas.index(schema)]).empty()
                ? std::optional<std::string>{}
                : std::optional{render_fixed_containers(
                      texts(store_.fixed_soas.containers[store_.fixed_soas.index(schema)]))}};
        if (!source_containers.has_value() ||
            texts(store_.fixed_soas.containers[store_.fixed_soas.index(schema)]).empty()) {
            auto const properties{
                std::array<SourceProperty, 1>{std::pair{"containers", rendered_containers}}};
            return patch_source_properties(source, 1, properties, "      ", original, replacements);
        }
        if (!storage_.syntax.is_list(*source_containers)) {
            return false;
        }

        auto const renamed_container{infer_single_fixed_container_rename(
            storage_.syntax.children(*source_containers),
            texts(store_.fixed_soas.containers[store_.fixed_soas.index(schema)]))};
        auto const rows_begin{storage_.syntax.token_span(*source_containers).offset + 1};
        auto const multiline{
            original
                .substr(rows_begin,
                        storage_.syntax.closing_span(*source_containers).offset - rows_begin)
                .contains('\n')};
        if (!multiline) {
            if (renamed_container.has_value()) {
                for (auto const& container : storage_.syntax.children(*source_containers)) {
                    if (storage_.syntax.text(container) == renamed_container->first) {
                        return patch_source_form(container,
                                                 std::string{renamed_container->second},
                                                 original,
                                                 replacements);
                    }
                }
            }
            return patch_source_form(
                *source_containers, *rendered_containers, original, replacements);
        }

        struct SourceContainer {
            FormId form;
            std::size_t begin{};
            std::size_t end{};
        };
        std::map<std::string_view, SourceContainer> source_by_name;
        auto row_begin{std::size_t{rows_begin}};
        for (auto const& container : storage_.syntax.children(*source_containers)) {
            auto const row_end{source_form_line_end(
                container, original, storage_.syntax.closing_span(*source_containers).offset)};
            if (!row_end.has_value() || row_begin > storage_.syntax.token_span(container).offset ||
                !source_by_name
                     .emplace(
                         storage_.syntax.text(container),
                         SourceContainer{.form = container, .begin = row_begin, .end = *row_end})
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
        for (auto const& container :
             texts(store_.fixed_soas.containers[store_.fixed_soas.index(schema)])) {
            auto const renamed{renamed_container.has_value() &&
                               renamed_container->second == container};
            auto const found{source_by_name.find(renamed ? renamed_container->first
                                                         : std::string_view{container})};
            if (found == source_by_name.end()) {
                append_row(std::string{"        "} + std::string{container});
                continue;
            }

            std::vector<SourceReplacement> container_replacements;
            if (renamed && !patch_source_form(
                               found->second.form, container, original, container_replacements)) {
                return false;
            }
            auto rendered{apply_source_replacements_to_range(original,
                                                             found->second.begin,
                                                             found->second.end,
                                                             std::move(container_replacements))};
            if (!rendered.has_value()) {
                return false;
            }
            append_row(std::move(*rendered));
        }
        replacements.push_back(
            {.begin = rows_begin, .end = row_begin, .text = std::move(rendered_rows)});
        return true;
    }

    auto render_single_allocation(lispb::codegen::DeclarationHandle const& schema) -> std::string {
        std::ostringstream rendered;
        rendered << "(single-allocation "
                 << text(*store_.soas.single_allocation[store_.soas.index(schema)]);
        rendered << ')';
        return std::move(rendered).str();
    }

    auto patch_source_single_allocation(lispb::codegen::DeclarationHandle const& schema,
                                        FormId const source,
                                        std::string_view const original,
                                        std::vector<SourceReplacement>& replacements) -> bool {
        if (!store_.soas.single_allocation[store_.soas.index(schema)].has_value() ||
            storage_.syntax.children(source).size() < 2 ||
            !patch_source_form(storage_.syntax.children(source)[1],
                               text(*store_.soas.single_allocation[store_.soas.index(schema)]),
                               original,
                               replacements)) {
            return false;
        }

        return true;
    }

    auto try_render_source_preserved_soa(lispb::codegen::DeclarationHandle const& schema,
                                         std::string_view const original)
        -> std::optional<std::string> {
        if (!items(store_.soas.const_view_functions[store_.soas.index(schema)]).empty() ||
            !items(store_.soas.mutable_view_functions[store_.soas.index(schema)]).empty() ||
            store_.soas.array_allocator[store_.soas.index(schema)].has_value()) {
            return std::nullopt;
        }
        auto parsed{source_declaration("struct")};
        if (!parsed.has_value()) {
            return std::nullopt;
        }

        for (auto const& child : storage_.syntax.children(*parsed)) {
            if (storage_.syntax.head(child) == "view-function" ||
                storage_.syntax.head(child) == "mutable-view-function" ||
                storage_.syntax.text(child) == ":array-allocator") {
                return std::nullopt;
            }
        }

        std::vector<SourceReplacement> replacements;

        std::vector<FormId> members;
        std::vector<FormId> functions;
        std::optional<FormId> fixed{};
        std::optional<FormId> single_allocation{};
        auto members_limit{storage_.syntax.closing_span(*parsed).offset};
        auto functions_limit{storage_.syntax.closing_span(*parsed).offset};
        auto found_member{false};
        auto member_region_ended{false};
        auto found_function{false};
        auto function_region_ended{false};
        for (auto const& child : storage_.syntax.children(*parsed)) {
            if (storage_.syntax.head(child) == "member") {
                if (member_region_ended || storage_.syntax.children(child).size() < 4) {
                    return std::nullopt;
                }
                found_member = true;
                members.push_back(child);
            } else {
                if (found_member && !member_region_ended) {
                    member_region_ended = true;
                    members_limit = storage_.syntax.token_span(child).offset;
                }
                if (storage_.syntax.head(child) == "function") {
                    if (function_region_ended || storage_.syntax.children(child).size() < 3) {
                        return std::nullopt;
                    }
                    found_function = true;
                    functions.push_back(child);
                } else {
                    if (found_function && !function_region_ended) {
                        function_region_ended = true;
                        functions_limit = storage_.syntax.token_span(child).offset;
                    }
                    if (storage_.syntax.head(child) == "fixed") {
                        fixed = child;
                    } else if (storage_.syntax.head(child) == "single-allocation") {
                        single_allocation = child;
                    }
                }
            }
        }
        std::string trailing_forms;
        if (fixed.has_value() && store_.soas.fixed[store_.soas.index(schema)].has_value()) {
            if (!patch_source_fixed_soa(*store_.soas.fixed[store_.soas.index(schema)],
                                        *fixed,
                                        original,
                                        replacements)) {
                return std::nullopt;
            }
        } else if (fixed.has_value()) {
            replacements.push_back({.begin = storage_.syntax.token_span(*fixed).offset,
                                    .end = storage_.syntax.closing_span(*fixed).offset + 1,
                                    .text = {}});
        } else if (store_.soas.fixed[store_.soas.index(schema)].has_value()) {
            auto rendered{render_fixed_soa(*store_.soas.fixed[store_.soas.index(schema)])};
            if (single_allocation.has_value()) {
                replacements.push_back(
                    {.begin = storage_.syntax.token_span(*single_allocation).offset,
                     .end = storage_.syntax.token_span(*single_allocation).offset,
                     .text = std::move(rendered) + "\n    "});
            } else {
                trailing_forms += "\n    " + std::move(rendered);
            }
        }
        if (single_allocation.has_value() &&
            store_.soas.single_allocation[store_.soas.index(schema)].has_value()) {
            if (!patch_source_single_allocation(
                    schema, *single_allocation, original, replacements)) {
                return std::nullopt;
            }
        } else if (single_allocation.has_value()) {
            replacements.push_back(
                {.begin = storage_.syntax.token_span(*single_allocation).offset,
                 .end = storage_.syntax.closing_span(*single_allocation).offset + 1,
                 .text = {}});
        } else if (store_.soas.single_allocation[store_.soas.index(schema)].has_value()) {
            auto rendered{render_single_allocation(schema)};
            trailing_forms += "\n    " + std::move(rendered);
        }
        if (!trailing_forms.empty()) {
            replacements.push_back({.begin = storage_.syntax.closing_span(*parsed).offset,
                                    .end = storage_.syntax.closing_span(*parsed).offset,
                                    .text = std::move(trailing_forms)});
        }

        std::vector<std::string> operation_names;
        operation_names.reserve(items(store_.soas.operations[store_.soas.index(schema)]).size());
        for (auto const operation : items(store_.soas.operations[store_.soas.index(schema)])) {
            operation_names.emplace_back(storage_operation_name(operation));
        }
        auto source_operations_use_all{false};
        for (std::size_t index{2}; index + 1 < storage_.syntax.children(*parsed).size(); ++index) {
            if (storage_.syntax.kind(storage_.syntax.children(*parsed)[index]) !=
                    codegen::sexpr::TokenKind::keyword ||
                storage_.syntax.text(storage_.syntax.children(*parsed)[index]) != "operations") {
                continue;
            }
            auto const& value{storage_.syntax.children(*parsed)[index + 1]};
            source_operations_use_all =
                storage_.syntax.is_list(value) && storage_.syntax.children(value).size() == 1 &&
                storage_.syntax.kind(storage_.syntax.children(value).front()) ==
                    codegen::sexpr::TokenKind::atom &&
                storage_.syntax.text(storage_.syntax.children(value).front()) == "all";
            break;
        }
        auto const preserve_all_operations{
            source_operations_use_all &&
            std::ranges::equal(items(store_.soas.operations[store_.soas.index(schema)]),
                               codegen::all_storage_operations())};
        auto const properties{std::array<SourceProperty, 9>{
            std::pair{"storage",
                      store_.soas.storage[store_.soas.index(schema)]
                          ? std::optional<std::string>{codegen::soa_storage_name(
                                *store_.soas.storage[store_.soas.index(schema)])}
                          : std::nullopt},
            std::pair{"view-name", optional_text(store_.soas.view_name[store_.soas.index(schema)])},
            std::pair{"const-view-name",
                      optional_text(store_.soas.const_view_name[store_.soas.index(schema)])},
            std::pair{"export-specifier",
                      store_.soas.export_specifier[store_.soas.index(schema)].value != 0
                          ? std::optional{std::string{
                                text(store_.soas.export_specifier[store_.soas.index(schema)])}}
                          : std::nullopt},
            std::pair{"equivalent-type",
                      store_.soas.equivalent_type[store_.soas.index(schema)].has_value()
                          ? std::optional{render_type_ref(
                                *store_.soas.equivalent_type[store_.soas.index(schema)])}
                          : std::nullopt},
            std::pair{"copy-element-memberwise",
                      store_.soas.copy_element_memberwise[store_.soas.index(schema)]
                          ? std::optional<std::string>{"true"}
                          : std::nullopt},
            std::pair{"layout-only",
                      store_.soas.layout_only[store_.soas.index(schema)]
                          ? std::optional<std::string>{"true"}
                          : std::nullopt},
            std::pair{"field-mask-name",
                      optional_text(store_.soas.field_mask_name[store_.soas.index(schema)])},
            std::pair{"field-enum-name",
                      optional_text(store_.soas.field_enum_name[store_.soas.index(schema)])}}};
        if (!patch_source_properties(*parsed, 1, properties, "    ", original, replacements) ||
            !patch_source_atom_list_property(
                *parsed,
                1,
                "vector-components",
                texts(store_.soas.vector_components[store_.soas.index(schema)]),
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
            !patch_source_quoted_list_property(
                *parsed,
                1,
                "using-declarations",
                texts(store_.soas.using_declarations[store_.soas.index(schema)]),
                "    ",
                "      ",
                original,
                replacements)) {
            return std::nullopt;
        }

        if (members.empty()) {
            return items(store_.soas.members[store_.soas.index(schema)]).empty()
                     ? apply_source_replacements(original, std::move(replacements))
                     : std::nullopt;
        }

        auto const first_member_offset{storage_.syntax.token_span(members.front()).offset};
        auto members_begin{std::size_t{}};
        for (auto const& child : storage_.syntax.children(*parsed)) {
            if (storage_.syntax.token_span(child).offset >= first_member_offset) {
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
            FormId form;
            std::size_t begin{};
            std::size_t end{};
        };
        std::map<std::string_view, SourceMember> source_by_name;
        auto row_begin{std::size_t{members_begin}};
        for (auto const& member : members) {
            auto const row_end{source_form_line_end(member, original, members_limit)};
            if (!row_end.has_value() || row_begin > storage_.syntax.token_span(member).offset ||
                *row_end < storage_.syntax.closing_span(member).offset + 1) {
                return std::nullopt;
            }
            if (!source_by_name
                     .emplace(storage_.syntax.text(storage_.syntax.children(member)[1]),
                              SourceMember{.form = member, .begin = row_begin, .end = *row_end})
                     .second) {
                return std::nullopt;
            }
            row_begin = *row_end;
        }

        auto const members_end{row_begin};
        auto const renamed_member{infer_single_positional_rename(
            store_.soa_members,
            members,
            items(store_.soas.members[store_.soas.index(schema)]),
            [&](FormId const source, lispb::codegen::SoaMemberId const& member) {
                auto const dimensions{render_mask_dimensions(
                    items(store_.soa_members.mask_dimensions[store_.soa_members.index(member)]))};
                if (storage_.syntax.children(source).size() < 4 ||
                    !source_form_matches_rendered(
                        storage_.syntax.children(source)[2],
                        soa_member_kind_name(
                            store_.soa_members.kind[store_.soa_members.index(member)])) ||
                    !source_form_matches_rendered(
                        storage_.syntax.children(source)[3],
                        render_type_ref(
                            store_.soa_members.type[store_.soa_members.index(member)])) ||
                    !source_property_matches(
                        source,
                        3,
                        "fixed-schema",
                        optional_text(
                            store_.soa_members.fixed_schema[store_.soa_members.index(member)])) ||
                    !source_property_matches(
                        source,
                        3,
                        "nested-schema",
                        optional_text(
                            store_.soa_members.nested_schema[store_.soa_members.index(member)])) ||
                    !source_property_matches(
                        source,
                        3,
                        "mask-field",
                        store_.soa_members.mask_field[store_.soa_members.index(member)]
                            ? std::optional<std::string>{"true"}
                            : std::nullopt) ||
                    !source_property_matches(source,
                                             3,
                                             "mask-dimensions",
                                             dimensions.empty() ? std::nullopt
                                                                : std::optional{dimensions})) {
                    return false;
                }

                std::optional<FormId> source_relation{};
                for (auto const& nested : storage_.syntax.children(source)) {
                    if (storage_.syntax.head(nested) != "relation") {
                        continue;
                    }
                    if (source_relation.has_value()) {
                        return false;
                    }
                    source_relation = nested;
                }
                return store_.soa_members.relationship[store_.soa_members.index(member)].has_value()
                         ? source_relation.has_value() &&
                               source_form_matches_rendered(
                                   *source_relation,
                                   render_semantic_relation(
                                       *store_.soa_members
                                            .relationship[store_.soa_members.index(member)]))
                         : !source_relation.has_value();
            })};
        auto rendered_members{std::string{}};
        auto append_row = [&](std::string row) {
            auto const preceding_newline{
                rendered_members.empty() ? members_begin > 0 && original[members_begin - 1] == '\n'
                                         : rendered_members.back() == '\n'};
            if (!preceding_newline && (row.empty() || row.front() != '\n')) {
                rendered_members += '\n';
            }
            rendered_members += row;
        };

        for (auto const& member : items(store_.soas.members[store_.soas.index(schema)])) {
            auto const renamed{renamed_member.has_value() &&
                               renamed_member->second ==
                                   text(store_.soa_members.name[store_.soa_members.index(member)])};
            auto const found{source_by_name.find(
                renamed ? renamed_member->first
                        : std::string_view{
                              text(store_.soa_members.name[store_.soa_members.index(member)])})};
            if (found == source_by_name.end()) {
                append_row("    " + render_soa_member(member));
                continue;
            }

            std::vector<SourceReplacement> member_replacements;
            if (renamed &&
                !patch_source_form(storage_.syntax.children(found->second.form)[1],
                                   text(store_.soa_members.name[store_.soa_members.index(member)]),
                                   original,
                                   member_replacements)) {
                return std::nullopt;
            }
            if (!patch_source_form(storage_.syntax.children(found->second.form)[2],
                                   std::string{soa_member_kind_name(
                                       store_.soa_members.kind[store_.soa_members.index(member)])},
                                   original,
                                   member_replacements) ||
                !patch_source_form(
                    storage_.syntax.children(found->second.form)[3],
                    render_type_ref(store_.soa_members.type[store_.soa_members.index(member)]),
                    original,
                    member_replacements)) {
                return std::nullopt;
            }
            auto const member_properties{std::array<SourceProperty, 3>{
                std::pair{"fixed-schema",
                          optional_text(
                              store_.soa_members.fixed_schema[store_.soa_members.index(member)])},
                std::pair{"nested-schema",
                          optional_text(
                              store_.soa_members.nested_schema[store_.soa_members.index(member)])},
                std::pair{"mask-field",
                          store_.soa_members.mask_field[store_.soa_members.index(member)]
                              ? std::optional<std::string>{"true"}
                              : std::nullopt}}};
            if (!patch_source_properties(found->second.form,
                                         3,
                                         member_properties,
                                         "      ",
                                         original,
                                         member_replacements) ||
                !patch_source_mask_dimensions(
                    found->second.form,
                    items(store_.soa_members.mask_dimensions[store_.soa_members.index(member)]),
                    original,
                    member_replacements)) {
                return std::nullopt;
            }
            std::optional<FormId> source_relation{};
            for (auto const& nested : storage_.syntax.children(found->second.form)) {
                if (storage_.syntax.head(nested) != "relation") {
                    continue;
                }
                if (source_relation.has_value()) {
                    return std::nullopt;
                }
                source_relation = nested;
            }
            if (!patch_source_optional_relation(
                    found->second.form,
                    source_relation,
                    store_.soa_members.relationship[store_.soa_members.index(member)],
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

        auto source_property = [&](FormId const form,
                                   std::string_view const name) -> std::optional<FormId> {
            for (std::size_t index{3}; index + 1 < storage_.syntax.children(form).size(); ++index) {
                if (storage_.syntax.kind(storage_.syntax.children(form)[index]) ==
                        codegen::sexpr::TokenKind::keyword &&
                    storage_.syntax.text(storage_.syntax.children(form)[index]) == name) {
                    return storage_.syntax.children(form)[index + 1];
                }
            }
            return std::nullopt;
        };
        auto property_matches = [&](FormId const form,
                                    std::string_view const name,
                                    std::optional<std::string> const& expected) {
            auto const value{source_property(form, name)};
            return expected.has_value()
                     ? value.has_value() && source_form_matches_rendered(*value, *expected)
                     : !value.has_value();
        };
        auto rendered_function_properties = [&](lispb::codegen::FunctionSchemaId const& function) {
            auto const body{
                store_.functions.body_lines[store_.functions.index(function)].empty()
                    ? std::nullopt
                    : std::optional{render_quoted_values(
                          texts(store_.functions.body_lines[store_.functions.index(function)]))}};
            auto const dependencies{
                store_.functions.dependencies[store_.functions.index(function)].empty()
                    ? std::nullopt
                    : std::optional{render_quoted_values(
                          texts(store_.functions.dependencies[store_.functions.index(function)]))}};
            return std::array<SourceProperty, 12>{
                std::pair{"body", body},
                std::pair{"dependencies", dependencies},
                std::pair{"constexpr",
                          store_.functions.is_constexpr[store_.functions.index(function)]
                              ? std::optional<std::string>{"true"}
                              : std::nullopt},
                std::pair{"nodiscard",
                          store_.functions.is_nodiscard[store_.functions.index(function)]
                              ? std::optional<std::string>{"true"}
                              : std::nullopt},
                std::pair{"trailing-return-type",
                          store_.functions.trailing_return_type[store_.functions.index(function)]
                                  .has_value()
                              ? std::optional{render_type_ref(
                                    *store_.functions
                                         .trailing_return_type[store_.functions.index(function)])}
                              : std::nullopt},
                std::pair{"const",
                          store_.functions.is_const[store_.functions.index(function)]
                              ? std::optional<std::string>{"true"}
                              : std::nullopt},
                std::pair{"noexcept",
                          store_.functions.is_noexcept[store_.functions.index(function)]
                              ? std::optional<std::string>{"true"}
                              : std::nullopt},
                std::pair{"static",
                          store_.functions.is_static[store_.functions.index(function)]
                              ? std::optional<std::string>{"true"}
                              : std::nullopt},
                std::pair{"inline",
                          store_.functions.is_inline[store_.functions.index(function)]
                              ? std::optional<std::string>{"true"}
                              : std::nullopt},
                std::pair{"definition-in-source",
                          store_.functions.definition_in_source[store_.functions.index(function)]
                              ? std::optional<std::string>{"true"}
                              : std::nullopt},
                std::pair{"template-parameters",
                          store_.functions.template_parameters[store_.functions.index(function)]
                                  .has_value()
                              ? std::optional{quote(
                                    *store_.functions
                                         .template_parameters[store_.functions.index(function)])}
                              : std::nullopt},
                std::pair{
                    "requires",
                    store_.functions.requires_clause[store_.functions.index(function)].has_value()
                        ? std::optional{quote(
                              *store_.functions.requires_clause[store_.functions.index(function)])}
                        : std::nullopt}};
        };
        auto body_property_matches = [&](FormId const source,
                                         lispb::codegen::FunctionSchemaId const& function) {
            auto const source_body{source_property(source, "body")};
            if (!source_body.has_value() ||
                storage_.syntax.kind(*source_body) != codegen::sexpr::TokenKind::raw_literal) {
                auto const body{
                    store_.functions.body_lines[store_.functions.index(function)].empty()
                        ? std::nullopt
                        : std::optional{render_quoted_values(texts(
                              store_.functions.body_lines[store_.functions.index(function)]))}};
                return property_matches(source, "body", body);
            }
            return storage_.syntax.tag(*source_body) == "cpp" &&
                   (store_.functions.body_lines[store_.functions.index(function)].empty()
                        ? storage_.syntax.text(*source_body).empty()
                        : store_.functions.body_lines[store_.functions.index(function)].size() ==
                                  1 &&
                              texts(store_.functions.body_lines[store_.functions.index(function)])
                                      .front() == storage_.syntax.text(*source_body));
        };
        auto parameter_fields_match_except_name =
            [&](FormId const source, lispb::codegen::ParameterId const& parameter) {
                auto const expected_default{
                    store_.parameters.default_value[store_.parameters.index(parameter)].has_value()
                        ? std::optional{quote(
                              *store_.parameters.default_value[store_.parameters.index(parameter)])}
                        : std::nullopt};
                return storage_.syntax.children(source).size() >= 3 &&
                       source_form_matches_rendered(
                           storage_.syntax.children(source)[2],
                           render_type_ref(
                               store_.parameters.type[store_.parameters.index(parameter)])) &&
                       property_matches(source, "default", expected_default);
            };
        auto function_fields_match_except_name = [&](FormId const source,
                                                     lispb::codegen::FunctionSchemaId const&
                                                         function) {
            if (storage_.syntax.children(source).size() < 3 ||
                !source_form_matches_rendered(
                    storage_.syntax.children(source)[2],
                    render_type_ref(
                        store_.functions.return_type[store_.functions.index(function)]))) {
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

            std::vector<FormId> source_parameters;
            for (auto const& child : storage_.syntax.children(source)) {
                if (storage_.syntax.head(child) == "parameter") {
                    source_parameters.push_back(child);
                }
            }
            if (source_parameters.size() !=
                store_.functions.parameters[store_.functions.index(function)].size()) {
                return false;
            }
            auto renamed_parameters{std::size_t{}};
            for (std::size_t index{}; index < source_parameters.size(); ++index) {
                if (storage_.syntax.children(source_parameters[index]).size() < 2 ||
                    !parameter_fields_match_except_name(
                        source_parameters[index],
                        items(store_.functions
                                  .parameters[store_.functions.index(function)])[index])) {
                    return false;
                }
                if (storage_.syntax.text(storage_.syntax.children(source_parameters[index])[1]) !=
                    text(store_.parameters.name[store_.parameters.index(store_.items(
                        store_.functions.parameters[store_.functions.index(function)])[index])])) {
                    ++renamed_parameters;
                }
            }
            return renamed_parameters <= 1;
        };

        auto render_parameter_row = [&](lispb::codegen::ParameterId const& parameter) {
            std::ostringstream rendered;
            rendered << "      (parameter "
                     << text(store_.parameters.name[store_.parameters.index(parameter)]) << ' '
                     << render_type_ref(store_.parameters.type[store_.parameters.index(parameter)]);
            if (store_.parameters.default_value[store_.parameters.index(parameter)].has_value()) {
                rendered
                    << " :default "
                    << quote(*store_.parameters.default_value[store_.parameters.index(parameter)]);
            }
            rendered << ')';
            return std::move(rendered).str();
        };
        auto patch_function_parameters = [&](FormId const source,
                                             lispb::codegen::FunctionSchemaId const& function,
                                             std::vector<SourceReplacement>&
                                                 function_replacements) {
            std::vector<FormId> source_parameters;
            for (auto const& child : storage_.syntax.children(source)) {
                if (storage_.syntax.head(child) == "parameter") {
                    if (storage_.syntax.children(child).size() < 3) {
                        return false;
                    }
                    source_parameters.push_back(child);
                }
            }

            if (source_parameters.empty()) {
                if (store_.functions.parameters[store_.functions.index(function)].empty()) {
                    return true;
                }
                std::string additions;
                for (auto const& parameter :
                     items(store_.functions.parameters[store_.functions.index(function)])) {
                    additions += "\n" + render_parameter_row(parameter);
                }
                auto const existing_insertion{
                    std::ranges::find_if(function_replacements, [&](auto const& replacement) {
                        return replacement.begin == storage_.syntax.closing_span(source).offset &&
                               replacement.end == storage_.syntax.closing_span(source).offset;
                    })};
                if (existing_insertion == function_replacements.end()) {
                    function_replacements.push_back(
                        {.begin = storage_.syntax.closing_span(source).offset,
                         .end = storage_.syntax.closing_span(source).offset,
                         .text = std::move(additions)});
                } else {
                    existing_insertion->text += additions;
                }
                return true;
            }

            auto const first_parameter_offset{
                storage_.syntax.token_span(source_parameters.front()).offset};
            auto parameters_begin{std::size_t{storage_.syntax.token_span(source).offset}};
            for (auto const& child : storage_.syntax.children(source)) {
                if (storage_.syntax.token_span(child).offset >= first_parameter_offset) {
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
                FormId form;
                std::size_t begin{};
                std::size_t end{};
            };
            std::map<std::string_view, SourceParameter> source_by_name;
            auto row_begin{std::size_t{parameters_begin}};
            for (auto const& parameter : source_parameters) {
                auto const row_end{source_form_line_end(
                    parameter, original, storage_.syntax.closing_span(source).offset)};
                if (!row_end.has_value() ||
                    row_begin > storage_.syntax.token_span(parameter).offset ||
                    *row_end < storage_.syntax.closing_span(parameter).offset + 1 ||
                    !source_by_name
                         .emplace(storage_.syntax.text(storage_.syntax.children(parameter)[1]),
                                  SourceParameter{
                                      .form = parameter, .begin = row_begin, .end = *row_end})
                         .second) {
                    return false;
                }
                row_begin = *row_end;
            }

            auto const parameters_end{row_begin};
            std::set<IdentifierId> schema_names;
            std::vector<SourceParameter const*> source_for_parameter(
                store_.functions.parameters[store_.functions.index(function)].size());
            std::set<std::string_view> matched_source_names;
            for (std::size_t index{};
                 index < store_.functions.parameters[store_.functions.index(function)].size();
                 ++index) {
                auto const& parameter{
                    items(store_.functions.parameters[store_.functions.index(function)])[index]};
                if (!schema_names.insert(store_.parameters.name[store_.parameters.index(parameter)])
                         .second) {
                    return false;
                }
                auto const found{source_by_name.find(
                    text(store_.parameters.name[store_.parameters.index(parameter)]))};
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
            if (store_.functions.parameters[store_.functions.index(function)].size() ==
                    source_parameters.size() &&
                unmatched_schema_indices.size() == 1 && unmatched_source_parameters.size() == 1) {
                auto const index{unmatched_schema_indices.front()};
                if (parameter_fields_match_except_name(
                        unmatched_source_parameters.front()->form,
                        items(store_.functions
                                  .parameters[store_.functions.index(function)])[index])) {
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
            for (std::size_t index{};
                 index < store_.functions.parameters[store_.functions.index(function)].size();
                 ++index) {
                auto const& parameter{
                    items(store_.functions.parameters[store_.functions.index(function)])[index]};
                auto const* found{source_for_parameter[index]};
                if (found == nullptr) {
                    append_parameter(render_parameter_row(parameter));
                    continue;
                }

                std::vector<SourceReplacement> parameter_replacements;
                if (!patch_source_form(
                        storage_.syntax.children(found->form)[1],
                        text(store_.parameters.name[store_.parameters.index(parameter)]),
                        original,
                        parameter_replacements) ||
                    !patch_source_form(
                        storage_.syntax.children(found->form)[2],
                        render_type_ref(store_.parameters.type[store_.parameters.index(parameter)]),
                        original,
                        parameter_replacements)) {
                    return false;
                }
                auto const parameter_properties{std::array<SourceProperty, 1>{std::pair{
                    "default",
                    store_.parameters.default_value[store_.parameters.index(parameter)].has_value()
                        ? std::optional{quote(
                              *store_.parameters.default_value[store_.parameters.index(parameter)])}
                        : std::nullopt}}};
                if (!patch_source_properties(found->form,
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

        auto render_function_row = [&](lispb::codegen::FunctionSchemaId const& function) {
            std::ostringstream rendered;
            render_function(rendered, function, "    ");
            return std::move(rendered).str();
        };
        if (functions.empty()) {
            if (!items(store_.soas.functions[store_.soas.index(schema)]).empty()) {
                std::string rendered_functions;
                for (auto const& function :
                     items(store_.soas.functions[store_.soas.index(schema)])) {
                    rendered_functions += "\n" + render_function_row(function);
                }
                replacements.push_back({.begin = members_end,
                                        .end = members_end,
                                        .text = std::move(rendered_functions)});
            }
        } else {
            auto const functions_begin{members_end};
            if (functions_begin > storage_.syntax.token_span(functions.front()).offset ||
                functions_limit < storage_.syntax.token_span(functions.front()).offset) {
                return std::nullopt;
            }

            struct SourceFunction {
                FormId form;
                std::size_t begin{};
                std::size_t end{};
            };
            std::map<std::string_view, SourceFunction> source_by_name;
            auto row_begin{std::size_t{functions_begin}};
            for (auto const& function : functions) {
                auto const row_end{source_form_line_end(function, original, functions_limit)};
                if (!row_end.has_value() ||
                    row_begin > storage_.syntax.token_span(function).offset ||
                    *row_end < storage_.syntax.closing_span(function).offset + 1 ||
                    !source_by_name
                         .emplace(
                             storage_.syntax.text(storage_.syntax.children(function)[1]),
                             SourceFunction{.form = function, .begin = row_begin, .end = *row_end})
                         .second) {
                    return std::nullopt;
                }
                row_begin = *row_end;
            }

            auto const functions_end{row_begin};
            std::set<IdentifierId> schema_names;
            std::vector<SourceFunction const*> source_for_function(
                items(store_.soas.functions[store_.soas.index(schema)]).size());
            std::set<std::string_view> matched_source_names;
            for (std::size_t index{};
                 index < items(store_.soas.functions[store_.soas.index(schema)]).size();
                 ++index) {
                auto const& function{
                    items(store_.soas.functions[store_.soas.index(schema)])[index]};
                if (!schema_names.insert(store_.functions.name[store_.functions.index(function)])
                         .second) {
                    return std::nullopt;
                }
                auto const found{source_by_name.find(
                    text(store_.functions.name[store_.functions.index(function)]))};
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
            if (items(store_.soas.functions[store_.soas.index(schema)]).size() ==
                    functions.size() &&
                unmatched_schema_indices.size() == 1 && unmatched_source_functions.size() == 1) {
                auto const index{unmatched_schema_indices.front()};
                if (function_fields_match_except_name(
                        unmatched_source_functions.front()->form,
                        items(store_.soas.functions[store_.soas.index(schema)])[index])) {
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
            for (std::size_t index{};
                 index < items(store_.soas.functions[store_.soas.index(schema)]).size();
                 ++index) {
                auto const& function{
                    items(store_.soas.functions[store_.soas.index(schema)])[index]};
                auto const* found{source_for_function[index]};
                if (found == nullptr) {
                    append_function(render_function_row(function));
                    continue;
                }
                std::vector<SourceReplacement> function_replacements;
                if (!patch_source_form(
                        storage_.syntax.children(found->form)[1],
                        text(store_.functions.name[store_.functions.index(function)]),
                        original,
                        function_replacements) ||
                    !patch_source_form(
                        storage_.syntax.children(found->form)[2],
                        render_type_ref(
                            store_.functions.return_type[store_.functions.index(function)]),
                        original,
                        function_replacements)) {
                    return std::nullopt;
                }
                auto function_properties{rendered_function_properties(function)};
                auto const source_body{source_property(found->form, "body")};
                auto const source_dependencies{source_property(found->form, "dependencies")};
                auto const patch_body_rows{
                    source_body.has_value() && storage_.syntax.is_list(*source_body) &&
                    !store_.functions.body_lines[store_.functions.index(function)].empty()};
                auto const patch_dependency_rows{
                    source_dependencies.has_value() &&
                    storage_.syntax.is_list(*source_dependencies) &&
                    !store_.functions.dependencies[store_.functions.index(function)].empty()};
                auto retain_source_property = [&](FormId const property,
                                                  std::optional<std::string>& rendered) {
                    auto const property_end{source_form_end(property, original)};
                    if (!property_end.has_value()) {
                        return false;
                    }
                    rendered = std::string{original.substr(
                        storage_.syntax.token_span(property).offset,
                        *property_end - storage_.syntax.token_span(property).offset)};
                    return true;
                };
                if (source_body.has_value() &&
                    storage_.syntax.kind(*source_body) == codegen::sexpr::TokenKind::raw_literal &&
                    body_property_matches(found->form, function)) {
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
                if (!patch_source_properties(found->form,
                                             2,
                                             function_properties,
                                             "      ",
                                             original,
                                             function_replacements)) {
                    return std::nullopt;
                }
                if ((patch_body_rows &&
                     !patch_source_quoted_list_property(
                         found->form,
                         2,
                         "body",
                         texts(store_.functions.body_lines[store_.functions.index(function)]),
                         "      ",
                         "        ",
                         original,
                         function_replacements)) ||
                    (patch_dependency_rows &&
                     !patch_source_quoted_list_property(
                         found->form,
                         2,
                         "dependencies",
                         texts(store_.functions.dependencies[store_.functions.index(function)]),
                         "      ",
                         "        ",
                         original,
                         function_replacements))) {
                    return std::nullopt;
                }
                if (!patch_function_parameters(found->form, function, function_replacements)) {
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

    auto render_declaration_source(codegen::DeclarationHandle const declaration,
                                   std::optional<SourceRange> const source) -> std::string {
        original_ = source;
        current_name_ = codegen::declaration_name(store_, declaration);
        if (source) {
            auto const form{source->form};
            auto const original{storage_.sources.text(storage_.syntax.span(form).file)};
            if (source->original == declaration) {
                return std::string{original.substr(storage_.syntax.span(form).offset,
                                                   storage_.syntax.span(form).end_offset -
                                                       storage_.syntax.span(form).offset)};
            }
            auto const preserved{[&]() -> std::optional<std::string> {
                auto const schema{declaration};
                switch (schema.kind()) {
                    case lispb::codegen::DeclarationKind::enumeration: {

                        auto preserve = [&](auto render) -> std::optional<std::string> {
                            return std::invoke(render, this, schema, original);
                        };
                        return preserve(&SourceRenderer::try_render_source_preserved_enum);
                    }
                    case lispb::codegen::DeclarationKind::packed_value: {

                        auto preserve = [&](auto render) -> std::optional<std::string> {
                            return std::invoke(render, this, schema, original);
                        };
                        return preserve(&SourceRenderer::try_render_source_preserved_packed_value);
                    }
                    case lispb::codegen::DeclarationKind::integer_scalar: {

                        auto preserve = [&](auto render) -> std::optional<std::string> {
                            return std::invoke(render, this, schema, original);
                        };
                        return preserve(
                            &SourceRenderer::try_render_source_preserved_integer_scalar);
                    }
                    case lispb::codegen::DeclarationKind::linear_quantized: {

                        auto preserve = [&](auto render) -> std::optional<std::string> {
                            return std::invoke(render, this, schema, original);
                        };
                        return preserve(
                            &SourceRenderer::try_render_source_preserved_linear_quantized);
                    }
                    case lispb::codegen::DeclarationKind::integer_varint: {

                        auto preserve = [&](auto render) -> std::optional<std::string> {
                            return std::invoke(render, this, schema, original);
                        };
                        return preserve(
                            &SourceRenderer::try_render_source_preserved_integer_varint);
                    }
                    case lispb::codegen::DeclarationKind::fixed_point: {

                        auto preserve = [&](auto render) -> std::optional<std::string> {
                            return std::invoke(render, this, schema, original);
                        };
                        return preserve(&SourceRenderer::try_render_source_preserved_fixed_point);
                    }
                    case lispb::codegen::DeclarationKind::optional_sentinel: {

                        auto preserve = [&](auto render) -> std::optional<std::string> {
                            return std::invoke(render, this, schema, original);
                        };
                        return preserve(
                            &SourceRenderer::try_render_source_preserved_optional_sentinel);
                    }
                    case lispb::codegen::DeclarationKind::optional_presence_bit: {

                        auto preserve = [&](auto render) -> std::optional<std::string> {
                            return std::invoke(render, this, schema, original);
                        };
                        return preserve(
                            &SourceRenderer::try_render_source_preserved_optional_presence_bit);
                    }
                    case lispb::codegen::DeclarationKind::mini_float: {

                        auto preserve = [&](auto render) -> std::optional<std::string> {
                            return std::invoke(render, this, schema, original);
                        };
                        return preserve(&SourceRenderer::try_render_source_preserved_mini_float);
                    }
                    case lispb::codegen::DeclarationKind::record: {

                        auto preserve = [&](auto render) -> std::optional<std::string> {
                            return std::invoke(render, this, schema, original);
                        };
                        return preserve(&SourceRenderer::try_render_source_preserved_record);
                    }
                    case lispb::codegen::DeclarationKind::union_type: {

                        auto preserve = [&](auto render) -> std::optional<std::string> {
                            return std::invoke(render, this, schema, original);
                        };
                        return preserve(&SourceRenderer::try_render_source_preserved_union);
                    }
                    case lispb::codegen::DeclarationKind::tagged_union: {

                        auto preserve = [&](auto render) -> std::optional<std::string> {
                            return std::invoke(render, this, schema, original);
                        };
                        return preserve(&SourceRenderer::try_render_source_preserved_tagged_union);
                    }
                    case lispb::codegen::DeclarationKind::soa: {

                        auto preserve = [&](auto render) -> std::optional<std::string> {
                            return std::invoke(render, this, schema, original);
                        };
                        return preserve(&SourceRenderer::try_render_source_preserved_soa);
                    }
                    case lispb::codegen::DeclarationKind::facade: {
                        return std::nullopt;
                    }
                    case lispb::codegen::DeclarationKind::homogeneous_layout: {
                        return std::nullopt;
                    }
                    case lispb::codegen::DeclarationKind::static_table: {
                        return std::nullopt;
                    }
                    case lispb::codegen::DeclarationKind::vector_soa: {
                        return std::nullopt;
                    }
                    default:
                        std::unreachable();
                }
            }()};
            if (preserved) {
                return *preserved;
            }
        }
        return render_declaration_schema(declaration);
    }
  private:
    std::optional<SourceRange> original_;
    IdentifierId current_name_;
    CompilerStorage& storage_;
    codegen::Declarations& store_;
};

} // namespace

auto render_declaration_source(CompilerStorage& storage,
                               codegen::DeclarationHandle declaration,
                               std::optional<SourceRange> original) -> std::string {
    return SourceRenderer{storage}.render_declaration_source(declaration, original);
}

auto render_editable_module(CompilerStorage& storage, codegen::ModuleHandle module)
    -> std::optional<std::string> {
    return SourceRenderer{storage}.render_module(module);
}

} // namespace lispb::schema::detail
