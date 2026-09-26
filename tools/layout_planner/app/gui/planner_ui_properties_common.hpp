#pragma once

#include "planner_ui.hpp"
#include "planner_ui_support.hpp"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <limits>
#include <set>
#include <string>
#include <string_view>

namespace ioj::layout_planner {
namespace {

using namespace layout;
using namespace lispb::schema;

inline constexpr std::array semantic_relationship_kinds{
    codegen::SemanticRelationKind::index_into,
    codegen::SemanticRelationKind::count_of,
    codegen::SemanticRelationKind::offset_into,
    codegen::SemanticRelationKind::discriminates,
    codegen::SemanticRelationKind::contains,
    codegen::SemanticRelationKind::member_of,
    codegen::SemanticRelationKind::quantises,
    codegen::SemanticRelationKind::encoded_as,
    codegen::SemanticRelationKind::references,
};
inline constexpr std::array semantic_relationship_units{
    codegen::SemanticRelationUnit::elements,
    codegen::SemanticRelationUnit::bytes,
};
inline constexpr auto enum_conversions{codegen::all_enum_conversions()};
inline constexpr auto enum_reflections{codegen::all_enum_reflections()};
inline constexpr std::array enum_projection_reflections{codegen::EnumReflection::uenum,
                                                        codegen::EnumReflection::blueprint};

struct EnumConversionDescriptor {
    std::string_view source_name;
    char const* description;
};

inline auto enum_conversion_descriptor(codegen::EnumConversion const conversion)
    -> EnumConversionDescriptor {
    switch (conversion) {
        case codegen::EnumConversion::lex_to_string:
            return {codegen::enum_conversion_name(conversion),
                    "Generate the basic enum-to-string conversion."};
        case codegen::EnumConversion::string_view:
            return {codegen::enum_conversion_name(conversion),
                    "Generate a string-view conversion."};
        case codegen::EnumConversion::string:
            return {codegen::enum_conversion_name(conversion),
                    "Generate an owning string conversion."};
        case codegen::EnumConversion::lex_to_display_string:
            return {codegen::enum_conversion_name(conversion),
                    "Generate the display-name conversion."};
        case codegen::EnumConversion::display_string_view:
            return {codegen::enum_conversion_name(conversion),
                    "Generate a display-name string-view conversion."};
        case codegen::EnumConversion::display_string:
            return {codegen::enum_conversion_name(conversion),
                    "Generate an owning display-name string conversion."};
        case codegen::EnumConversion::lex_to_serialized_string:
            return {codegen::enum_conversion_name(conversion),
                    "Generate the serialized-name conversion."};
        case codegen::EnumConversion::try_parse_serialized:
            return {codegen::enum_conversion_name(conversion),
                    "Generate parsing from serialized names."};
    }
    return {"unknown", "Unknown enum conversion."};
}

inline auto has_enum_conversion(std::vector<codegen::EnumConversion> const& conversions,
                                codegen::EnumConversion const conversion) -> bool {
    return std::ranges::find(conversions, conversion) != conversions.end();
}

inline auto with_enum_conversion(std::vector<codegen::EnumConversion> const& conversions,
                                 codegen::EnumConversion const changed_conversion,
                                 bool const enabled) -> std::vector<codegen::EnumConversion> {
    std::vector<codegen::EnumConversion> result;
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

inline auto storage_operation_descriptor(codegen::StorageOperation const operation)
    -> StorageOperationDescriptor {
    switch (operation) {
        case codegen::StorageOperation::reset:
            return {"reset", "Reset every column to an empty state."};
        case codegen::StorageOperation::reserve:
            return {"reserve", "Reserve capacity in every column."};
        case codegen::StorageOperation::add_uninitialised:
            return {"add-uninitialised", "Append rows without value-initialising their elements."};
        case codegen::StorageOperation::add_defaulted:
            return {"add-defaulted", "Append value-initialised rows."};
        case codegen::StorageOperation::remove_at_swap:
            return {"remove-at-swap", "Remove rows by replacing them with rows from the end."};
        case codegen::StorageOperation::set_num:
            return {"set-num", "Resize every column to the requested row count."};
        case codegen::StorageOperation::copy_element:
            return {"copy-element", "Copy one logical row between indices."};
        case codegen::StorageOperation::append_from:
            return {"append-from", "Append logical rows from another compatible view."};
    }
    return {"unknown", "Unknown storage operation."};
}

inline auto has_storage_operation(std::vector<codegen::StorageOperation> const& operations,
                                  codegen::StorageOperation const operation) -> bool {
    return std::ranges::find(operations, operation) != operations.end();
}

inline auto with_storage_operation(std::vector<codegen::StorageOperation> const& operations,
                                   codegen::StorageOperation const changed_operation,
                                   bool const enabled) -> std::vector<codegen::StorageOperation> {
    auto const canonical_operations{codegen::all_storage_operations()};
    std::vector<codegen::StorageOperation> result;
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

inline auto unique_soa_using_declaration(std::vector<std::string> const& declarations)
    -> std::string {
    auto alias{std::string{"alias"}};
    for (auto suffix{2U};; ++suffix) {
        auto const alias_taken{std::ranges::any_of(declarations, [&](std::string const& value) {
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

inline auto unique_soa_function_name(std::vector<codegen::FunctionSchema> const& functions,
                                     std::string const& stem) -> std::string {
    auto suffix{std::size_t{1}};
    auto candidate{stem};
    while (std::ranges::find(functions, candidate, &codegen::FunctionSchema::name) !=
           functions.end()) {
        candidate = stem + std::to_string(suffix++);
    }
    return candidate;
}

inline auto unique_function_parameter_name(std::vector<codegen::ParameterSchema> const& parameters,
                                           std::string const& stem) -> std::string {
    auto suffix{std::size_t{1}};
    auto candidate{stem};
    while (std::ranges::find(parameters, candidate, &codegen::ParameterSchema::name) !=
           parameters.end()) {
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

inline auto unique_segment_name(std::vector<codegen::PackedSegmentSchema> const& segments,
                                std::string const& stem) -> std::string {
    auto suffix{std::size_t{1}};
    auto candidate{stem};
    while (std::ranges::any_of(segments, [&](auto const& segment) {
        return codegen::packed_segment_name(segment) == candidate;
    })) {
        candidate = stem + std::to_string(suffix++);
    }
    return candidate;
}

inline auto unique_code_name(std::vector<codegen::PackedNamedCodeSchema> const& codes,
                             std::string const& stem) -> std::string {
    auto suffix{std::size_t{1}};
    auto candidate{stem};
    while (std::ranges::find(codes, candidate, &codegen::PackedNamedCodeSchema::name) !=
           codes.end()) {
        candidate = stem + std::to_string(suffix++);
    }
    return candidate;
}

inline auto next_packed_integer(codegen::PackedIntegerValue const value)
    -> std::optional<codegen::PackedIntegerValue> {
    if (value.negative) {
        return value.magnitude == 1
                 ? codegen::PackedIntegerValue{0}
                 : codegen::PackedIntegerValue::from_parts(true, value.magnitude - 1);
    }
    return value.magnitude == std::numeric_limits<std::uint64_t>::max()
             ? std::nullopt
             : std::optional{codegen::PackedIntegerValue{value.magnitude + 1}};
}

inline auto previous_packed_integer(codegen::PackedIntegerValue const value)
    -> std::optional<codegen::PackedIntegerValue> {
    if (!value.negative) {
        return value.magnitude == 0
                 ? std::optional{codegen::PackedIntegerValue::from_parts(true, 1)}
                 : std::optional{codegen::PackedIntegerValue{value.magnitude - 1}};
    }
    return value.magnitude == std::numeric_limits<std::uint64_t>::max()
             ? std::nullopt
             : std::optional{codegen::PackedIntegerValue::from_parts(true, value.magnitude + 1)};
}

inline auto first_available_code(codegen::PackedFieldSchema const& field,
                                 std::uint32_t const effective_width,
                                 bool const sentinel)
    -> std::optional<codegen::PackedIntegerValue> {
    auto const signed_field{field.kind == codegen::PackedFieldKind::signed_integer};
    auto const magnitude_limit{effective_width >= 64 ? (std::uint64_t{1} << 63)
                                                     : (std::uint64_t{1} << (effective_width - 1))};
    auto const field_minimum{signed_field
                                 ? codegen::PackedIntegerValue::from_parts(true, magnitude_limit)
                                 : codegen::PackedIntegerValue{0}};
    auto const field_maximum{signed_field ? codegen::PackedIntegerValue{magnitude_limit - 1}
                                          : codegen::PackedIntegerValue{
                                                effective_width >= 64
                                                    ? std::numeric_limits<std::uint64_t>::max()
                                                    : (std::uint64_t{1} << effective_width) - 1}};
    auto const unused{[&](codegen::PackedIntegerValue const candidate) {
        return std::ranges::find(field.named_codes,
                                 candidate,
                                 &codegen::PackedNamedCodeSchema::value) == field.named_codes.end();
    }};
    auto const search{
        [&](codegen::PackedIntegerValue const first,
            codegen::PackedIntegerValue const last) -> std::optional<codegen::PackedIntegerValue> {
            auto candidate{first};
            for (std::size_t attempt{}; attempt <= field.named_codes.size() &&
                                        codegen::packed_integer_less_equal(candidate, last);
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
        return search(field.minimum_value.value_or(codegen::PackedIntegerValue{0}),
                      field.maximum_value.value_or(field_maximum));
    }
    if (!field.minimum_value.has_value()) {
        return std::nullopt;
    }
    if (codegen::packed_integer_less(*field.maximum_value, field_maximum)) {
        if (auto const next{next_packed_integer(*field.maximum_value)}; next.has_value()) {
            if (auto value{search(*next, field_maximum)}) {
                return value;
            }
        }
    }
    if (codegen::packed_integer_less(field_minimum, *field.minimum_value)) {
        if (auto const previous{previous_packed_integer(*field.minimum_value)};
            previous.has_value()) {
            auto candidate{*previous};
            for (std::size_t attempt{}; attempt <= field.named_codes.size(); ++attempt) {
                if (unused(candidate)) {
                    return candidate;
                }
                auto const prior{previous_packed_integer(candidate)};
                if (!prior.has_value() || codegen::packed_integer_less(*prior, field_minimum)) {
                    break;
                }
                candidate = *prior;
            }
        }
    }
    return std::nullopt;
}

inline auto first_available_scalar_code(codegen::IntegerScalarSchema const& scalar,
                                        std::uint32_t const effective_width,
                                        bool const sentinel)
    -> std::optional<codegen::PackedIntegerValue> {
    auto const signed_limit{effective_width >= 64 ? (std::uint64_t{1} << 63)
                                                  : (std::uint64_t{1} << (effective_width - 1))};
    auto const representable_minimum{
        scalar.signedness ? codegen::PackedIntegerValue::from_parts(true, signed_limit)
                          : codegen::PackedIntegerValue{0}};
    auto const representable_maximum{
        scalar.signedness ? codegen::PackedIntegerValue{signed_limit - 1}
                          : codegen::PackedIntegerValue{
                                effective_width >= 64 ? std::numeric_limits<std::uint64_t>::max()
                                                      : (std::uint64_t{1} << effective_width) - 1}};
    auto const unused{[&](codegen::PackedIntegerValue const candidate) {
        return std::ranges::find(
                   scalar.named_codes, candidate, &codegen::PackedNamedCodeSchema::value) ==
               scalar.named_codes.end();
    }};
    auto const search{
        [&](codegen::PackedIntegerValue const first,
            codegen::PackedIntegerValue const last) -> std::optional<codegen::PackedIntegerValue> {
            auto candidate{first};
            for (std::size_t attempt{}; attempt <= scalar.named_codes.size() &&
                                        codegen::packed_integer_less_equal(candidate, last);
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
        return search(scalar.minimum_value, scalar.maximum_value);
    }
    if (codegen::packed_integer_less(scalar.maximum_value, representable_maximum)) {
        if (auto const next{next_packed_integer(scalar.maximum_value)}; next.has_value()) {
            if (auto value{search(*next, representable_maximum)}) {
                return value;
            }
        }
    }
    if (codegen::packed_integer_less(representable_minimum, scalar.minimum_value)) {
        if (auto const previous{previous_packed_integer(scalar.minimum_value)};
            previous.has_value()) {
            auto candidate{*previous};
            for (std::size_t attempt{}; attempt <= scalar.named_codes.size(); ++attempt) {
                if (unused(candidate)) {
                    return candidate;
                }
                auto const prior{previous_packed_integer(candidate)};
                if (!prior.has_value() ||
                    codegen::packed_integer_less(*prior, representable_minimum)) {
                    break;
                }
                candidate = *prior;
            }
        }
    }
    return std::nullopt;
}

inline auto unique_member_name(std::vector<codegen::SoaMemberSchema> const& members,
                               std::string const& stem) -> std::string {
    auto suffix{std::size_t{1}};
    auto candidate{stem};
    while (std::ranges::find(members, candidate, &codegen::SoaMemberSchema::name) !=
           members.end()) {
        candidate = stem + std::to_string(suffix++);
    }
    return candidate;
}

inline auto
    unique_mask_dimension_name(std::vector<codegen::SoaMaskDimensionSchema> const& dimensions,
                               std::string const& stem) -> std::string {
    auto suffix{std::size_t{1}};
    auto candidate{stem};
    while (std::ranges::find(dimensions, candidate, &codegen::SoaMaskDimensionSchema::index_name) !=
           dimensions.end()) {
        candidate = stem + std::to_string(suffix++);
    }
    return candidate;
}

inline auto unique_record_member_name(std::vector<codegen::RecordMemberSchema> const& members,
                                      std::string const& stem) -> std::string {
    auto suffix{std::size_t{1}};
    auto candidate{stem};
    while (std::ranges::find(members, candidate, &codegen::RecordMemberSchema::name) !=
           members.end()) {
        candidate = stem + std::to_string(suffix++);
    }
    return candidate;
}

template <typename Alternative>
inline auto unique_union_alternative_name(std::vector<Alternative> const& alternatives,
                                          std::string const& stem) -> std::string {
    auto suffix{std::size_t{1}};
    auto candidate{stem};
    while (std::ranges::find(alternatives, candidate, &Alternative::name) != alternatives.end()) {
        candidate = stem + std::to_string(suffix++);
    }
    return candidate;
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
