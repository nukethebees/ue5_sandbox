#pragma once
#include "planner_ui.hpp"
#include "planner_ui_support.hpp"

#include <codegen/schema.h>
#include <codegen/schema/declarations.h>
#include <codegen/schema/enum_conversion.h>
#include <codegen/schema/enum_reflection.h>
#include <codegen/schema/integer_scalar_schemas.h>
#include <codegen/schema/packed_field_kind.h>
#include <codegen/schema/packed_field_schemas.h>
#include <codegen/schema/packed_integer_value.h>
#include <codegen/schema/packed_named_code_schemas.h>
#include <codegen/schema/packed_reserved_bits_schemas.h>
#include <codegen/schema/packed_segment_id.h>
#include <codegen/schema/schema_range.h>
#include <codegen/schema/semantic_relation_kind.h>
#include <codegen/schema/semantic_relation_unit.h>
#include <codegen/schema/storage_operation.h>
#include <lispb/indices.h>
#include <lispb/schema/packed_segment_id.h>

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <limits>
#include <optional>
#include <ranges>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ioj::layout_planner {
namespace {

using namespace layout;
using namespace lispb::schema;

inline constexpr std::array semantic_relationship_kinds{
    lispb::codegen::SemanticRelationKind::index_into,
    lispb::codegen::SemanticRelationKind::count_of,
    lispb::codegen::SemanticRelationKind::offset_into,
    lispb::codegen::SemanticRelationKind::discriminates,
    lispb::codegen::SemanticRelationKind::contains,
    lispb::codegen::SemanticRelationKind::member_of,
    lispb::codegen::SemanticRelationKind::quantises,
    lispb::codegen::SemanticRelationKind::encoded_as,
    lispb::codegen::SemanticRelationKind::references,
};
inline constexpr std::array semantic_relationship_units{
    lispb::codegen::SemanticRelationUnit::elements,
    lispb::codegen::SemanticRelationUnit::bytes,
};
inline constexpr auto enum_conversions{lispb::codegen::all_enum_conversions()};
inline constexpr auto enum_reflections{lispb::codegen::all_enum_reflections()};
inline constexpr std::array enum_projection_reflections{lispb::codegen::EnumReflection::uenum,
                                                        lispb::codegen::EnumReflection::blueprint};

struct EnumConversionDescriptor {
    std::string_view source_name;
    char const* description;
};

inline auto enum_conversion_descriptor(lispb::codegen::EnumConversion const conversion)
    -> EnumConversionDescriptor {
    switch (conversion) {
        case lispb::codegen::EnumConversion::lex_to_string:
            return {lispb::codegen::enum_conversion_name(conversion),
                    "Generate the basic enum-to-string conversion."};
        case lispb::codegen::EnumConversion::string_view:
            return {lispb::codegen::enum_conversion_name(conversion),
                    "Generate a string-view conversion."};
        case lispb::codegen::EnumConversion::string:
            return {lispb::codegen::enum_conversion_name(conversion),
                    "Generate an owning string conversion."};
        case lispb::codegen::EnumConversion::lex_to_display_string:
            return {lispb::codegen::enum_conversion_name(conversion),
                    "Generate the display-name conversion."};
        case lispb::codegen::EnumConversion::display_string_view:
            return {lispb::codegen::enum_conversion_name(conversion),
                    "Generate a display-name string-view conversion."};
        case lispb::codegen::EnumConversion::display_string:
            return {lispb::codegen::enum_conversion_name(conversion),
                    "Generate an owning display-name string conversion."};
        case lispb::codegen::EnumConversion::lex_to_serialized_string:
            return {lispb::codegen::enum_conversion_name(conversion),
                    "Generate the serialized-name conversion."};
        case lispb::codegen::EnumConversion::try_parse_serialized:
            return {lispb::codegen::enum_conversion_name(conversion),
                    "Generate parsing from serialized names."};
    }
    return {"unknown", "Unknown enum conversion."};
}

inline auto has_enum_conversion(std::span<lispb::codegen::EnumConversion const> const conversions,
                                lispb::codegen::EnumConversion const conversion) -> bool {
    return std::ranges::find(conversions, conversion) != conversions.end();
}

inline auto with_enum_conversion(std::span<lispb::codegen::EnumConversion const> const conversions,
                                 lispb::codegen::EnumConversion const changed_conversion,
                                 bool const enabled)
    -> std::vector<lispb::codegen::EnumConversion> {
    std::vector<lispb::codegen::EnumConversion> result;
    result.reserve(enum_conversions.size());
    for (auto const conversion : enum_conversions) {
        auto const include{conversion == changed_conversion
                               ? enabled
                               : has_enum_conversion(conversions, conversion)};
        if (include) {
            result.push_back(conversion);
        }
    }
    return result;
}

struct StorageOperationDescriptor {
    char const* source_name;
    char const* description;
};

inline auto storage_operation_descriptor(lispb::codegen::StorageOperation const operation)
    -> StorageOperationDescriptor {
    switch (operation) {
        case lispb::codegen::StorageOperation::reset:
            return {"reset", "Reset every column to an empty state."};
        case lispb::codegen::StorageOperation::reserve:
            return {"reserve", "Reserve capacity in every column."};
        case lispb::codegen::StorageOperation::add_uninitialised:
            return {"add-uninitialised", "Append rows without value-initialising their elements."};
        case lispb::codegen::StorageOperation::add_defaulted:
            return {"add-defaulted", "Append value-initialised rows."};
        case lispb::codegen::StorageOperation::remove_at_swap:
            return {"remove-at-swap", "Remove rows by replacing them with rows from the end."};
        case lispb::codegen::StorageOperation::set_num:
            return {"set-num", "Resize every column to the requested row count."};
        case lispb::codegen::StorageOperation::copy_element:
            return {"copy-element", "Copy one logical row between indices."};
        case lispb::codegen::StorageOperation::append_from:
            return {"append-from", "Append logical rows from another compatible view."};
    }
    return {"unknown", "Unknown storage operation."};
}

inline auto
    has_storage_operation(std::span<lispb::codegen::StorageOperation const> const operations,
                          lispb::codegen::StorageOperation const operation) -> bool {
    return std::ranges::find(operations, operation) != operations.end();
}

inline auto
    with_storage_operation(std::span<lispb::codegen::StorageOperation const> const operations,
                           lispb::codegen::StorageOperation const changed_operation,
                           bool const enabled) -> std::vector<lispb::codegen::StorageOperation> {
    auto const canonical_operations{lispb::codegen::all_storage_operations()};
    std::vector<lispb::codegen::StorageOperation> result;
    result.reserve(canonical_operations.size());
    for (auto const operation : canonical_operations) {
        auto const include{operation == changed_operation
                               ? enabled
                               : has_storage_operation(operations, operation)};
        if (include) {
            result.push_back(operation);
        }
    }
    return result;
}

inline auto unique_soa_using_declaration(lispb::codegen::Declarations const& store,
                                         lispb::codegen::SchemaRange<lispb::StringId> declarations)
    -> std::string {
    auto alias{std::string{"alias"}};
    for (auto suffix{2U};; ++suffix) {
        auto const alias_taken{std::ranges::any_of(store.items(declarations), [&](auto const id) {
            auto const value{store.strings().view(id)};
            auto const equals{value.find('=')};
            if (equals == std::string::npos) {
                return false;
            }
            auto name{std::string_view{value}.substr(0, equals)};
            while (!name.empty() && std::isspace(static_cast<unsigned char>(name.front()))) {
                name.remove_prefix(1);
            }
            while (!name.empty() && std::isspace(static_cast<unsigned char>(name.back()))) {
                name.remove_suffix(1);
            }
            return name == alias;
        })};
        if (!alias_taken) {
            return alias + " = std::uint32_t";
        }
        alias = "alias" + std::to_string(suffix);
    }
}

inline auto unique_child_name(lispb::codegen::Declarations const& store,
                              auto const& columns,
                              std::span<lispb::IdentifierId const> names,
                              auto const& children,
                              std::string const& stem) -> std::string {
    auto suffix{std::size_t{1}};
    auto candidate{stem};
    while (std::ranges::any_of(children, [&](auto const id) {
        return store.strings().view(names[columns.index(id)]) == candidate;
    })) {
        candidate = stem + std::to_string(suffix++);
    }
    return candidate;
}

inline auto resize_text_input(ImGuiInputTextCallbackData* data) -> int {
    auto* value{static_cast<std::string*>(data->UserData)};
    value->resize(static_cast<std::size_t>(data->BufTextLen));
    data->Buf = value->data();
    return 0;
}

inline auto input_text(std::string const& label,
                       std::string& value,
                       ImGuiInputTextFlags const flags = ImGuiInputTextFlags_None) -> bool {
    return ImGui::InputText(label.c_str(),
                            value.data(),
                            value.capacity() + 1,
                            flags | ImGuiInputTextFlags_CallbackResize,
                            resize_text_input,
                            &value);
}

inline auto input_text_multiline(std::string const& label, std::string& value, ImVec2 const& size)
    -> bool {
    return ImGui::InputTextMultiline(label.c_str(),
                                     value.data(),
                                     value.capacity() + 1,
                                     size,
                                     ImGuiInputTextFlags_CallbackResize,
                                     resize_text_input,
                                     &value);
}

inline void draw_override_note(bool const overridden) {
    if (overridden) {
        ImGui::SameLine();
        ImGui::TextColored({0.4F, 0.75F, 0.95F, 1.0F}, "Override");
    }
}

[[nodiscard]] inline auto section_with_tooltip(char const* title, char const* description) -> bool {
    auto const open{detail::section(title)};
    ImGui::SetItemTooltip("%s", description);
    return open;
}

inline void text_disabled_wrapped(char const* text) {
    ImGui::PushTextWrapPos(0.0F);
    ImGui::TextDisabled("%s", text);
    ImGui::PopTextWrapPos();
}

template <std::size_t Size>
inline void set_buffer(std::array<char, Size>& buffer, std::optional<std::string> const& value) {
    std::snprintf(buffer.data(), buffer.size(), "%s", value.value_or("").c_str());
}

template <std::size_t Size>
inline auto optional_text(std::array<char, Size> const& buffer) -> std::optional<std::string> {
    return buffer.front() == '\0' ? std::nullopt : std::optional<std::string>{buffer.data()};
}

inline auto matching_signed_type(std::string_view const type) -> std::optional<std::string> {
    constexpr std::array pairs{
        std::pair{"uint8", "int8"},
        std::pair{"uint16", "int16"},
        std::pair{"uint32", "int32"},
        std::pair{"uint64", "int64"},
        std::pair{"std::uint8_t", "std::int8_t"},
        std::pair{"std::uint16_t", "std::int16_t"},
        std::pair{"std::uint32_t", "std::int32_t"},
        std::pair{"std::uint64_t", "std::int64_t"},
    };
    auto const found{
        std::ranges::find_if(pairs, [&](auto const& pair) { return pair.first == type; })};
    return found == pairs.end() ? std::nullopt : std::optional<std::string>{found->second};
}

inline auto matching_unsigned_type(std::string_view const type) -> std::optional<std::string> {
    constexpr std::array pairs{
        std::pair{"int8", "uint8"},
        std::pair{"int16", "uint16"},
        std::pair{"int32", "uint32"},
        std::pair{"int64", "uint64"},
        std::pair{"std::int8_t", "std::uint8_t"},
        std::pair{"std::int16_t", "std::uint16_t"},
        std::pair{"std::int32_t", "std::uint32_t"},
        std::pair{"std::int64_t", "std::uint64_t"},
    };
    auto const found{
        std::ranges::find_if(pairs, [&](auto const& pair) { return pair.first == type; })};
    return found == pairs.end() ? std::nullopt : std::optional<std::string>{found->second};
}

inline auto
    unique_segment_name(lispb::codegen::Declarations const& store,
                        lispb::codegen::SchemaRange<lispb::codegen::PackedSegmentId> const segments,
                        std::string const& stem) -> std::string {
    auto suffix{std::size_t{1}};
    auto candidate{stem};
    while (std::ranges::any_of(store.items(segments), [&](auto const segment) {
        auto const field{((segment).kind() == lispb::codegen::PackedSegmentKind::field
                              ? std::optional{segment}
                              : std::nullopt)};
        auto const name{
            field ? store.packed_fields.name[store.packed_fields.index(*field)]
                  : store.packed_reserved_bits.name[store.packed_reserved_bits.index(segment)]};
        return store.strings().view(name) == candidate;
    })) {
        candidate = stem + std::to_string(suffix++);
    }
    return candidate;
}

inline auto next_packed_integer(lispb::codegen::PackedIntegerValue const value)
    -> std::optional<lispb::codegen::PackedIntegerValue> {
    if (value.negative) {
        return value.magnitude == 1
                 ? lispb::codegen::PackedIntegerValue{0}
                 : lispb::codegen::PackedIntegerValue::from_parts(true, value.magnitude - 1);
    }
    return value.magnitude == std::numeric_limits<std::uint64_t>::max()
             ? std::nullopt
             : std::optional{lispb::codegen::PackedIntegerValue{value.magnitude + 1}};
}

inline auto previous_packed_integer(lispb::codegen::PackedIntegerValue const value)
    -> std::optional<lispb::codegen::PackedIntegerValue> {
    if (!value.negative) {
        return value.magnitude == 0
                 ? std::optional{lispb::codegen::PackedIntegerValue::from_parts(true, 1)}
                 : std::optional{lispb::codegen::PackedIntegerValue{value.magnitude - 1}};
    }
    return value.magnitude == std::numeric_limits<std::uint64_t>::max()
             ? std::nullopt
             : std::optional{
                   lispb::codegen::PackedIntegerValue::from_parts(true, value.magnitude + 1)};
}

inline auto first_available_code(lispb::codegen::Declarations const& store,
                                 lispb::codegen::PackedSegmentId const& field,
                                 std::uint32_t const effective_width,
                                 bool const sentinel)
    -> std::optional<lispb::codegen::PackedIntegerValue> {
    auto const signed_field{store.packed_fields.kind[store.packed_fields.index(field)] ==
                            lispb::codegen::PackedFieldKind::signed_integer};
    auto const magnitude_limit{effective_width >= 64 ? (std::uint64_t{1} << 63)
                                                     : (std::uint64_t{1} << (effective_width - 1))};
    auto const field_minimum{
        signed_field ? lispb::codegen::PackedIntegerValue::from_parts(true, magnitude_limit)
                     : lispb::codegen::PackedIntegerValue{0}};
    auto const field_maximum{signed_field ? lispb::codegen::PackedIntegerValue{magnitude_limit - 1}
                                          : lispb::codegen::PackedIntegerValue{
                                                effective_width >= 64
                                                    ? std::numeric_limits<std::uint64_t>::max()
                                                    : (std::uint64_t{1} << effective_width) - 1}};
    auto const unused{[&](lispb::codegen::PackedIntegerValue const candidate) {
        return std::ranges::none_of(
            store.items(store.packed_fields.named_codes[store.packed_fields.index(field)]),
            [&](auto const id) {
                return store.packed_named_codes.value[store.packed_named_codes.index(id)] ==
                       candidate;
            });
    }};
    auto const search{[&](lispb::codegen::PackedIntegerValue const first,
                          lispb::codegen::PackedIntegerValue const last)
                          -> std::optional<lispb::codegen::PackedIntegerValue> {
        auto candidate{first};
        for (std::size_t attempt{};
             attempt <= store.packed_fields.named_codes[store.packed_fields.index(field)].count &&
             lispb::codegen::packed_integer_less_equal(candidate, last);
             ++attempt) {
            if (unused(candidate)) {
                return candidate;
            }
            auto const next{next_packed_integer(candidate)};
            if (!next.has_value()) {
                break;
            }
            candidate = *next;
        }
        return std::nullopt;
    }};

    if (!sentinel) {
        return search(store.packed_fields.minimum_value[store.packed_fields.index(field)].value_or(
                          lispb::codegen::PackedIntegerValue{0}),
                      store.packed_fields.maximum_value[store.packed_fields.index(field)].value_or(
                          field_maximum));
    }
    if (!store.packed_fields.minimum_value[store.packed_fields.index(field)].has_value()) {
        return std::nullopt;
    }
    if (lispb::codegen::packed_integer_less(
            *store.packed_fields.maximum_value[store.packed_fields.index(field)], field_maximum)) {
        if (auto const next{next_packed_integer(
                *store.packed_fields.maximum_value[store.packed_fields.index(field)])};
            next.has_value()) {
            if (auto value{search(*next, field_maximum)}) {
                return value;
            }
        }
    }
    if (lispb::codegen::packed_integer_less(
            field_minimum, *store.packed_fields.minimum_value[store.packed_fields.index(field)])) {
        if (auto const previous{previous_packed_integer(
                *store.packed_fields.minimum_value[store.packed_fields.index(field)])};
            previous.has_value()) {
            auto candidate{*previous};
            for (std::size_t attempt{};
                 attempt <= store.packed_fields.named_codes[store.packed_fields.index(field)].count;
                 ++attempt) {
                if (unused(candidate)) {
                    return candidate;
                }
                auto const prior{previous_packed_integer(candidate)};
                if (!prior.has_value() ||
                    lispb::codegen::packed_integer_less(*prior, field_minimum)) {
                    break;
                }
                candidate = *prior;
            }
        }
    }
    return std::nullopt;
}

inline auto first_available_scalar_code(lispb::codegen::Declarations const& store,
                                        lispb::codegen::DeclarationHandle const& scalar,
                                        std::uint32_t const effective_width,
                                        bool const sentinel)
    -> std::optional<lispb::codegen::PackedIntegerValue> {
    auto const signed_limit{effective_width >= 64 ? (std::uint64_t{1} << 63)
                                                  : (std::uint64_t{1} << (effective_width - 1))};
    auto const representable_minimum{
        store.integer_scalars.signedness[store.integer_scalars.index(scalar)]
            ? lispb::codegen::PackedIntegerValue::from_parts(true, signed_limit)
            : lispb::codegen::PackedIntegerValue{0}};
    auto const representable_maximum{
        store.integer_scalars.signedness[store.integer_scalars.index(scalar)]
            ? lispb::codegen::PackedIntegerValue{signed_limit - 1}
            : lispb::codegen::PackedIntegerValue{effective_width >= 64
                                                     ? std::numeric_limits<std::uint64_t>::max()
                                                     : (std::uint64_t{1} << effective_width) - 1}};
    auto const unused{[&](lispb::codegen::PackedIntegerValue const candidate) {
        return std::ranges::none_of(
            store.items(store.integer_scalars.named_codes[store.integer_scalars.index(scalar)]),
            [&](auto const id) {
                return store.packed_named_codes.value[store.packed_named_codes.index(id)] ==
                       candidate;
            });
    }};
    auto const search{[&](lispb::codegen::PackedIntegerValue const first,
                          lispb::codegen::PackedIntegerValue const last)
                          -> std::optional<lispb::codegen::PackedIntegerValue> {
        auto candidate{first};
        for (std::size_t attempt{};
             attempt <=
                 store.integer_scalars.named_codes[store.integer_scalars.index(scalar)].count &&
             lispb::codegen::packed_integer_less_equal(candidate, last);
             ++attempt) {
            if (unused(candidate)) {
                return candidate;
            }
            auto const next{next_packed_integer(candidate)};
            if (!next.has_value()) {
                break;
            }
            candidate = *next;
        }
        return std::nullopt;
    }};

    if (!sentinel) {
        return search(store.integer_scalars.minimum_value[store.integer_scalars.index(scalar)],
                      store.integer_scalars.maximum_value[store.integer_scalars.index(scalar)]);
    }
    if (lispb::codegen::packed_integer_less(
            store.integer_scalars.maximum_value[store.integer_scalars.index(scalar)],
            representable_maximum)) {
        if (auto const next{next_packed_integer(
                store.integer_scalars.maximum_value[store.integer_scalars.index(scalar)])};
            next.has_value()) {
            if (auto value{search(*next, representable_maximum)}) {
                return value;
            }
        }
    }
    if (lispb::codegen::packed_integer_less(
            representable_minimum,
            store.integer_scalars.minimum_value[store.integer_scalars.index(scalar)])) {
        if (auto const previous{previous_packed_integer(
                store.integer_scalars.minimum_value[store.integer_scalars.index(scalar)])};
            previous.has_value()) {
            auto candidate{*previous};
            for (std::size_t attempt{};
                 attempt <=
                 store.integer_scalars.named_codes[store.integer_scalars.index(scalar)].count;
                 ++attempt) {
                if (unused(candidate)) {
                    return candidate;
                }
                auto const prior{previous_packed_integer(candidate)};
                if (!prior.has_value() ||
                    lispb::codegen::packed_integer_less(*prior, representable_minimum)) {
                    break;
                }
                candidate = *prior;
            }
        }
    }
    return std::nullopt;
}

template <typename Value>
inline void move_element(std::vector<Value>& values,
                         std::size_t const source_index,
                         std::size_t const target_index) {
    if (source_index < target_index) {
        std::rotate(values.begin() + static_cast<std::ptrdiff_t>(source_index),
                    values.begin() + static_cast<std::ptrdiff_t>(source_index + 1),
                    values.begin() + static_cast<std::ptrdiff_t>(target_index + 1));
    } else if (source_index > target_index) {
        std::rotate(values.begin() + static_cast<std::ptrdiff_t>(target_index),
                    values.begin() + static_cast<std::ptrdiff_t>(source_index),
                    values.begin() + static_cast<std::ptrdiff_t>(source_index + 1));
    }
}

inline auto suggested_type_name(std::string_view const identifier, std::string_view const suffix)
    -> std::string {
    std::string result;
    result.reserve(identifier.size() + suffix.size());
    auto uppercase_next{true};
    for (auto const character : identifier) {
        if (character == '_') {
            uppercase_next = true;
            continue;
        }
        result.push_back(
            uppercase_next ? static_cast<char>(std::toupper(static_cast<unsigned char>(character)))
                           : character);
        uppercase_next = false;
    }
    if (result.empty() || std::isdigit(static_cast<unsigned char>(result.front()))) {
        result = "Type";
    }
    result += suffix;
    return result;
}

} // namespace

} // namespace ioj::layout_planner
