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

auto enum_conversion_descriptor(codegen::EnumConversion const conversion)
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

auto has_enum_conversion(std::vector<codegen::EnumConversion> const& conversions,
                         codegen::EnumConversion const conversion) -> bool {
    return std::ranges::find(conversions, conversion) != conversions.end();
}

auto with_enum_conversion(std::vector<codegen::EnumConversion> const& conversions,
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

auto storage_operation_descriptor(codegen::StorageOperation const operation)
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

auto has_storage_operation(std::vector<codegen::StorageOperation> const& operations,
                           codegen::StorageOperation const operation) -> bool {
    return std::ranges::find(operations, operation) != operations.end();
}

auto with_storage_operation(std::vector<codegen::StorageOperation> const& operations,
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

auto unique_soa_using_declaration(std::vector<std::string> const& declarations) -> std::string {
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

auto unique_soa_function_name(std::vector<codegen::FunctionSchema> const& functions,
                              std::string const& stem) -> std::string {
    auto suffix{std::size_t{1}};
    auto candidate{stem};
    while (std::ranges::find(functions, candidate, &codegen::FunctionSchema::name) !=
           functions.end()) {
        candidate = stem + std::to_string(suffix++);
    }
    return candidate;
}

auto unique_function_parameter_name(std::vector<codegen::ParameterSchema> const& parameters,
                                    std::string const& stem) -> std::string {
    auto suffix{std::size_t{1}};
    auto candidate{stem};
    while (std::ranges::find(parameters, candidate, &codegen::ParameterSchema::name) !=
           parameters.end()) {
        candidate = stem + std::to_string(suffix++);
    }
    return candidate;
}

auto resize_text_input(ImGuiInputTextCallbackData* data) -> int {
    auto* value{static_cast<std::string*>(data->UserData)};
    value->resize(static_cast<std::size_t>(data->BufTextLen));
    data->Buf = value->data();
    return 0;
}

auto input_text(std::string const& label,
                std::string& value,
                ImGuiInputTextFlags const flags = ImGuiInputTextFlags_None) -> bool {
    return ImGui::InputText(label.c_str(),
                            value.data(),
                            value.capacity() + 1,
                            flags | ImGuiInputTextFlags_CallbackResize,
                            resize_text_input,
                            &value);
}

auto input_text_multiline(std::string const& label, std::string& value, ImVec2 const& size)
    -> bool {
    return ImGui::InputTextMultiline(label.c_str(),
                                     value.data(),
                                     value.capacity() + 1,
                                     size,
                                     ImGuiInputTextFlags_CallbackResize,
                                     resize_text_input,
                                     &value);
}

void draw_override_note(bool const overridden) {
    if (overridden) {
        ImGui::SameLine();
        ImGui::TextColored({0.4F, 0.75F, 0.95F, 1.0F}, "Override");
    }
}

void draw_optional_text(std::optional<std::string> const& value) {
    ImGui::TextUnformatted(value.has_value() ? value->c_str() : "-");
}

template <std::size_t Size>
void set_buffer(std::array<char, Size>& buffer, std::optional<std::string> const& value) {
    std::snprintf(buffer.data(), buffer.size(), "%s", value.value_or("").c_str());
}

template <std::size_t Size>
auto optional_text(std::array<char, Size> const& buffer) -> std::optional<std::string> {
    return buffer.front() == '\0' ? std::nullopt : std::optional<std::string>{buffer.data()};
}

auto matching_signed_type(std::string_view const type) -> std::optional<std::string> {
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

auto matching_unsigned_type(std::string_view const type) -> std::optional<std::string> {
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

auto unique_segment_name(std::vector<codegen::PackedSegmentSchema> const& segments,
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

auto unique_code_name(std::vector<codegen::PackedNamedCodeSchema> const& codes,
                      std::string const& stem) -> std::string {
    auto suffix{std::size_t{1}};
    auto candidate{stem};
    while (std::ranges::find(codes, candidate, &codegen::PackedNamedCodeSchema::name) !=
           codes.end()) {
        candidate = stem + std::to_string(suffix++);
    }
    return candidate;
}

auto next_packed_integer(codegen::PackedIntegerValue const value)
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

auto previous_packed_integer(codegen::PackedIntegerValue const value)
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

auto first_available_code(codegen::PackedFieldSchema const& field,
                          std::uint32_t const effective_width,
                          bool const sentinel) -> std::optional<codegen::PackedIntegerValue> {
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

auto first_available_scalar_code(codegen::IntegerScalarSchema const& scalar,
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

auto unique_member_name(std::vector<codegen::SoaMemberSchema> const& members,
                        std::string const& stem) -> std::string {
    auto suffix{std::size_t{1}};
    auto candidate{stem};
    while (std::ranges::find(members, candidate, &codegen::SoaMemberSchema::name) !=
           members.end()) {
        candidate = stem + std::to_string(suffix++);
    }
    return candidate;
}

auto unique_mask_dimension_name(std::vector<codegen::SoaMaskDimensionSchema> const& dimensions,
                                std::string const& stem) -> std::string {
    auto suffix{std::size_t{1}};
    auto candidate{stem};
    while (std::ranges::find(dimensions, candidate, &codegen::SoaMaskDimensionSchema::index_name) !=
           dimensions.end()) {
        candidate = stem + std::to_string(suffix++);
    }
    return candidate;
}

auto unique_record_member_name(std::vector<codegen::RecordMemberSchema> const& members,
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
auto unique_union_alternative_name(std::vector<Alternative> const& alternatives,
                                   std::string const& stem) -> std::string {
    auto suffix{std::size_t{1}};
    auto candidate{stem};
    while (std::ranges::find(alternatives, candidate, &Alternative::name) != alternatives.end()) {
        candidate = stem + std::to_string(suffix++);
    }
    return candidate;
}

template <typename Value>
void move_element(std::vector<Value>& values,
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

auto suggested_type_name(std::string_view const identifier, std::string_view const suffix)
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

auto PlannerUi::draw_type_picker(std::string_view const module_name, TypeIdentity const& owner)
    -> std::optional<std::string> {
    if (ImGui::SmallButton("...")) {
        type_picker_filter_.fill('\0');
        ImGui::OpenPopup("semantic-type-picker");
    }
    if (!ImGui::BeginPopup("semantic-type-picker")) {
        return std::nullopt;
    }

    ImGui::SetNextItemWidth(420.0F);
    ImGui::InputTextWithHint("##type-filter",
                             "Filter semantic or physical types",
                             type_picker_filter_.data(),
                             type_picker_filter_.size());
    auto lowercase{[](std::string text) {
        std::ranges::transform(text, text.begin(), [](unsigned char const character) {
            return static_cast<char>(std::tolower(character));
        });
        return text;
    }};
    auto const filter{lowercase(type_picker_filter_.data())};
    std::set<std::string, std::less<>> seen;
    std::optional<std::string> selected;
    auto draw_candidate = [&](std::string reference, std::string const& description) {
        if (!seen.insert(reference).second) {
            return;
        }
        auto const label{reference + "  [" + description + "]"};
        if (!filter.empty() && lowercase(label).find(filter) == std::string::npos) {
            return;
        }
        if (ImGui::Selectable(label.c_str())) {
            selected = std::move(reference);
            ImGui::CloseCurrentPopup();
        }
    };

    ImGui::BeginChild("type-candidates", {420.0F, 260.0F}, true);
    std::map<std::string, std::size_t, std::less<>> declaration_spelling_counts;
    for (auto const& declaration : document_->declarations()) {
        if (auto const type{workspace_.types().find(declaration.identity)}; type.has_value()) {
            ++declaration_spelling_counts[workspace_.types().type(*type).cpp_spelling];
        }
    }

    ImGui::SeparatorText("Declared semantic types");
    for (auto const& declaration : document_->declarations()) {
        if (declaration.identity == owner) {
            continue;
        }
        auto const type{workspace_.types().find(declaration.identity)};
        if (!type.has_value()) {
            continue;
        }
        auto const& node{workspace_.types().type(*type)};
        auto const same_module{declaration.identity.module_name == module_name};
        if (!same_module && declaration_spelling_counts[node.cpp_spelling] != 1) {
            auto const label{node.cpp_spelling + "  [ambiguous across declaration modules]"};
            if (filter.empty() || lowercase(label).find(filter) != std::string::npos) {
                ImGui::TextDisabled("%s", label.c_str());
            }
            continue;
        }
        auto reference{same_module ? declaration.identity.name : node.cpp_spelling};
        draw_candidate(std::move(reference), "declared in " + declaration.identity.module_name);
    }
    ImGui::SeparatorText("Registered semantic types");
    auto const owner_type{workspace_.types().find(owner)};
    for (auto const& [name, cpp_type] : document_->manifest().types) {
        auto const type{workspace_.types().find_registered(name)};
        if (type.has_value() && type != owner_type) {
            draw_candidate("@" + name, cpp_type.spelling);
        }
    }
    ImGui::SeparatorText("Target physical types");
    for (auto const& [spelling, facts] : abi_.types()) {
        static_cast<void>(facts);
        draw_candidate(spelling, "target ABI type");
    }
    ImGui::EndChild();
    ImGui::EndPopup();
    return selected;
}

void PlannerUi::draw_properties_panel() {
    if (!properties_view_open_) {
        return;
    }
    auto const was_open{properties_view_open_};
    ImGui::Begin("Properties", &properties_view_open_);
    persist_view_visibility(was_open, properties_view_open_);
    if (!selected_type_.has_value()) {
        ImGui::TextDisabled("No selection.");
        ImGui::End();
        return;
    }

    auto const selected{*selected_type_};
    auto const& node{workspace_.types().type(selected)};
    ImGui::Text("%s", node.identity.name.c_str());
    ImGui::TextDisabled("%s", node.identity.module_name.c_str());
    ImGui::TextDisabled("%s", node.cpp_spelling.c_str());
    auto const selected_declaration{document_.has_value()
                                        ? document_->find_declaration(node.identity)
                                        : std::optional<DeclarationId>{}};
    if (selected_declaration.has_value() && ImGui::Button("Duplicate declaration")) {
        if (duplicate_selected_declaration(node)) {
            ImGui::End();
            return;
        }
    }
    if (selected_declaration.has_value()) {
        auto const* declaration_info{document_->declaration(*selected_declaration)};
        auto const editable{document_->enum_schema(*selected_declaration) != nullptr ||
                            document_->packed_value_schema(*selected_declaration) != nullptr ||
                            document_->integer_scalar_schema(*selected_declaration) != nullptr ||
                            document_->linear_quantized_schema(*selected_declaration) != nullptr ||
                            document_->integer_varint_schema(*selected_declaration) != nullptr ||
                            document_->fixed_point_schema(*selected_declaration) != nullptr ||
                            document_->optional_sentinel_schema(*selected_declaration) != nullptr ||
                            document_->optional_presence_bit_schema(*selected_declaration) !=
                                nullptr ||
                            document_->mini_float_schema(*selected_declaration) != nullptr ||
                            document_->record_schema(*selected_declaration) != nullptr ||
                            document_->union_schema(*selected_declaration) != nullptr ||
                            document_->tagged_union_schema(*selected_declaration) != nullptr ||
                            document_->soa_schema(*selected_declaration) != nullptr};
        if (declaration_info != nullptr && editable) {
            auto const& modules{document_->manifest().modules};
            auto const& source_module{modules[declaration_info->module_index]};
            auto const& source_settings{std::visit(
                [](auto const& module) -> codegen::ModuleSettings const& {
                    return module.settings;
                },
                source_module)};
            std::vector<std::size_t> destinations;
            for (std::size_t index{}; index < modules.size(); ++index) {
                if (index == declaration_info->module_index ||
                    modules[index].index() != source_module.index()) {
                    continue;
                }
                destinations.push_back(index);
            }
            if (!destinations.empty()) {
                std::optional<std::size_t> requested_destination;
                ImGui::SetNextItemWidth(std::max(120.0F, ImGui::GetContentRegionAvail().x));
                if (ImGui::BeginCombo("Module", source_settings.name.c_str())) {
                    for (auto const index : destinations) {
                        auto const& settings{std::visit(
                            [](auto const& module) -> codegen::ModuleSettings const& {
                                return module.settings;
                            },
                            modules[index])};
                        if (ImGui::Selectable(settings.name.c_str())) {
                            requested_destination = index;
                        }
                    }
                    ImGui::EndCombo();
                }
                if (requested_destination.has_value()) {
                    auto const& destination_settings{std::visit(
                        [](auto const& module) -> codegen::ModuleSettings const& {
                            return module.settings;
                        },
                        modules[*requested_destination])};
                    auto selection{node.identity};
                    selection.module_name = destination_settings.name;
                    if (apply_document_edit(MoveDeclaration{.declaration = *selected_declaration,
                                                            .module_index = *requested_destination,
                                                            .insertion_index = std::nullopt},
                                            selection)) {
                        ImGui::End();
                        return;
                    }
                }
                ImGui::TextDisabled(
                    "Moving repairs semantic references when the namespace changes.");
            }
        }
    }
    auto const rename_supported{selected_declaration.has_value() &&
                                (!std::holds_alternative<SoaType>(node.definition) ||
                                 document_->soa_schema(*selected_declaration) != nullptr)};
    if (rename_supported) {
        if (rename_editor_declaration_ != selected_declaration) {
            rename_editor_declaration_ = selected_declaration;
            std::snprintf(declaration_name_.data(),
                          declaration_name_.size(),
                          "%s",
                          node.identity.name.c_str());
        }
        ImGui::SetNextItemWidth(std::max(120.0F, ImGui::GetContentRegionAvail().x - 90.0F));
        auto const submitted{ImGui::InputText("##declaration-name",
                                              declaration_name_.data(),
                                              declaration_name_.size(),
                                              ImGuiInputTextFlags_EnterReturnsTrue)};
        ImGui::SameLine();
        auto const rename_clicked{ImGui::Button("Rename")};
        auto const user_count{workspace_.types().users_of(selected).size()};
        ImGui::TextDisabled("Renaming repairs %llu direct semantic user%s.",
                            static_cast<unsigned long long>(user_count),
                            user_count == 1 ? "" : "s");
        if (submitted || rename_clicked) {
            if (declaration_name_.front() == '\0') {
                schema_edit_message_ = "Declaration name cannot be empty.";
            } else if (node.identity.name != declaration_name_.data()) {
                auto selection{node.identity};
                selection.name = declaration_name_.data();
                if (apply_document_edit(RenameDeclaration{.declaration = *selected_declaration,
                                                          .new_name = declaration_name_.data()},
                                        selection)) {
                    rename_editor_declaration_.reset();
                    ImGui::End();
                    return;
                }
            }
        }
    }
    if (selected_declaration.has_value()) {
        auto const* declaration_info{document_->declaration(*selected_declaration)};
        if (declaration_info != nullptr) {
            auto const user_count{workspace_.types().users_of(selected).size()};
            ImGui::BeginDisabled(user_count != 0);
            if (ImGui::Button("Delete declaration")) {
                delete_declaration_ = *selected_declaration;
                delete_declaration_name_ = node.identity.name;
                ImGui::OpenPopup("Delete declaration?");
            }
            ImGui::EndDisabled();
            if (user_count != 0) {
                ImGui::SameLine();
                ImGui::TextDisabled("Remove %llu direct user%s first.",
                                    static_cast<unsigned long long>(user_count),
                                    user_count == 1 ? "" : "s");
            }
        }
    }
    if (ImGui::BeginPopupModal("Delete declaration?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Delete '%s' from this schema?", delete_declaration_name_.c_str());
        ImGui::TextDisabled("This is a semantic edit; File > Undo restores the declaration.");
        if (ImGui::Button("Delete", {120.0F, 0.0F})) {
            if (delete_declaration_.has_value() && delete_declaration(*delete_declaration_)) {
                delete_declaration_.reset();
                delete_declaration_name_.clear();
                ImGui::CloseCurrentPopup();
                ImGui::EndPopup();
                ImGui::End();
                return;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", {120.0F, 0.0F})) {
            delete_declaration_.reset();
            delete_declaration_name_.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (auto const* enumeration{std::get_if<EnumType>(&node.definition)}) {
        ImGui::SeparatorText("Enum");
        if (enumeration->underlying_type.has_value()) {
            auto const& underlying{workspace_.types().type(enumeration->underlying_type->type)};
            ImGui::TextUnformatted("Explicit C++ backing");
            ImGui::SameLine();
            if (ImGui::SmallButton(underlying.cpp_spelling.c_str())) {
                selected_type_ = enumeration->underlying_type->type;
                selected_field_.clear();
                record_access_members_.clear();
                record_access_set_explicit_ = false;
            }
        } else if (enum_domain_.has_value()) {
            ImGui::Text("Derived C++ backing: %s", enum_domain_->backing_type.c_str());
        } else {
            ImGui::TextDisabled("Derived C++ backing: Unknown");
        }
        if (enumeration->count.has_value()) {
            ImGui::Text("Count sentinel: %s", enumeration->count->c_str());
        }
        if (enum_domain_.has_value()) {
            auto const& domain{*enum_domain_};
            ImGui::SeparatorText("Value domain");
            ImGui::Text("Live symbols: %llu",
                        static_cast<unsigned long long>(domain.live_value_count));
            ImGui::Text("Reserved/sentinel symbols: %llu",
                        static_cast<unsigned long long>(domain.reserved_value_count));
            if (domain.minimum_value.has_value() && domain.maximum_value.has_value()) {
                auto const minimum{lispb::schema::format_enum_code(*domain.minimum_value)};
                auto const maximum{lispb::schema::format_enum_code(*domain.maximum_value)};
                ImGui::Text("Known range: %s .. %s", minimum.c_str(), maximum.c_str());
            } else {
                ImGui::TextDisabled("Known range: Unknown");
            }
            if (domain.signed_domain.has_value()) {
                ImGui::Text("Effective domain: %s", *domain.signed_domain ? "signed" : "unsigned");
            } else {
                ImGui::TextDisabled("Effective domain: Unknown");
            }
            if (domain.declared_signedness.has_value()) {
                ImGui::Text("Declared signedness: %s",
                            *domain.declared_signedness ? "signed" : "unsigned");
            } else {
                ImGui::TextUnformatted("Declared signedness: auto / inferred");
            }
            if (domain.minimum_required_bits.has_value()) {
                ImGui::Text("Minimum semantic width: %u bits", *domain.minimum_required_bits);
            } else {
                ImGui::TextDisabled("Minimum semantic width: Unknown");
            }
            if (domain.declared_bit_width.has_value()) {
                ImGui::Text("Declared semantic width: %u bits", *domain.declared_bit_width);
            } else {
                ImGui::TextUnformatted("Declared semantic width: auto");
            }
            if (domain.effective_bit_width.has_value()) {
                ImGui::Text("Effective semantic width: %u bits", *domain.effective_bit_width);
            } else {
                ImGui::TextDisabled("Effective semantic width: Unknown");
            }
            if (domain.semantic_width_can_represent_domain.has_value()) {
                ImGui::Text("Semantic width fit: %s",
                            *domain.semantic_width_can_represent_domain ? "yes" : "NO");
            } else {
                ImGui::TextDisabled("Semantic width fit: Unknown");
            }
            if (domain.unused_semantic_codes.has_value()) {
                ImGui::Text("Unused semantic codes: %llu",
                            static_cast<unsigned long long>(*domain.unused_semantic_codes));
            } else {
                ImGui::TextDisabled("Unused semantic codes: Unknown");
            }
            if (domain.backing_bits.has_value()) {
                ImGui::Text("Current physical backing: %s (%llu bits)",
                            domain.backing_type.c_str(),
                            static_cast<unsigned long long>(*domain.backing_bits));
            } else {
                ImGui::Text("Current physical backing: %s (Unknown size)",
                            domain.backing_type.c_str());
            }
            if (domain.backing_can_represent_domain.has_value()) {
                ImGui::Text("Backing fit: %s", *domain.backing_can_represent_domain ? "yes" : "NO");
            } else {
                ImGui::TextDisabled("Backing fit: Unknown");
            }
            if (domain.unused_backing_codes.has_value()) {
                ImGui::Text("Unused backing codes: %llu",
                            static_cast<unsigned long long>(*domain.unused_backing_codes));
                ImGui::TextDisabled("Code-space inefficiency; not allocated byte waste.");
            } else {
                ImGui::TextDisabled("Unused backing codes: Unknown");
            }
            draw_diagnostics(domain.diagnostics);
        }
        auto const revision_before_edit{workspace_.revision()};
        draw_enum_editor(node, *enumeration);
        if (workspace_.revision() != revision_before_edit) {
            ImGui::End();
            return;
        }
    } else if (auto const* scalar{std::get_if<IntegerScalarType>(&node.definition)}) {
        ImGui::SeparatorText("Semantic integer domain");
        if (integer_scalar_analysis_.has_value()) {
            auto const& analysis{*integer_scalar_analysis_};
            auto const minimum{codegen::format_packed_integer(analysis.minimum_value)};
            auto const maximum{codegen::format_packed_integer(analysis.maximum_value)};
            ImGui::Text("Signedness: %s", analysis.signedness ? "signed" : "unsigned");
            ImGui::Text("Live range: %s .. %s", minimum.c_str(), maximum.c_str());
            ImGui::Text("Live values: %s",
                        detail::format_number(analysis.live_value_count).c_str());
            ImGui::Text("Sentinel codes: %llu",
                        static_cast<unsigned long long>(analysis.sentinel_code_count));
            ImGui::Text("Required codes: %s",
                        detail::format_number(analysis.required_code_count).c_str());
            ImGui::Text("Minimum width: %u bits", analysis.minimum_required_bits);
            if (analysis.declared_bit_width.has_value()) {
                ImGui::Text("Declared width: %u bits", *analysis.declared_bit_width);
            } else {
                ImGui::TextUnformatted("Declared width: auto");
            }
            ImGui::Text("Effective width: %u bits", analysis.effective_bit_width);
            ImGui::Text("Unused codes: %s", detail::format_number(analysis.unused_codes).c_str());
            ImGui::TextDisabled(
                "Code-space inefficiency; this semantic declaration allocates no bytes itself.");
        }
        if (draw_integer_scalar_editor(node, *scalar)) {
            ImGui::End();
            return;
        }
    } else if (auto const* quantized{std::get_if<LinearQuantizedType>(&node.definition)}) {
        ImGui::SeparatorText("Linear quantized representation");
        auto const& source{workspace_.types().type(quantized->source.type)};
        ImGui::TextUnformatted("Semantic source");
        ImGui::SameLine();
        if (ImGui::SmallButton(source.cpp_spelling.c_str())) {
            selected_type_ = quantized->source.type;
            selected_field_.clear();
            record_access_members_.clear();
            record_access_set_explicit_ = false;
        }
        if (linear_quantized_analysis_.has_value()) {
            auto const& analysis{*linear_quantized_analysis_};
            auto const minimum{codegen::format_packed_integer(analysis.source_minimum)};
            auto const maximum{codegen::format_packed_integer(analysis.source_maximum)};
            ImGui::Text("Source range: %s .. %s", minimum.c_str(), maximum.c_str());
            ImGui::Text("Source span: %s", detail::format_number(analysis.source_span).c_str());
            ImGui::Text("Encoded width: %u bits", analysis.encoded_storage_bits);
            ImGui::Text("Total codes: %s",
                        detail::format_code_count(analysis.total_code_count).c_str());
            ImGui::Text("Reserved codes: %llu",
                        static_cast<unsigned long long>(analysis.reserved_code_count));
            ImGui::Text("Usable codes: %s",
                        detail::format_code_count(analysis.usable_code_count).c_str());
            ImGui::Text("Resolution: %.12g", static_cast<double>(analysis.resolution));
            ImGui::Text("Maximum rounding error: %.12g",
                        static_cast<double>(analysis.maximum_rounding_error));
            ImGui::Text("Endpoint mapping: minimum %s, maximum %s",
                        analysis.minimum_endpoint_exact ? "exact" : "inexact",
                        analysis.maximum_endpoint_exact ? "exact" : "inexact");
            ImGui::Text("Out-of-range policy: %s",
                        codegen::quantization_clipping_name(analysis.clipping).data());
            ImGui::TextDisabled(
                "Resolution/error are numeric encoding facts; no performance estimate is made.");
        }
        if (draw_linear_quantized_editor(node, *quantized)) {
            ImGui::End();
            return;
        }
    } else if (auto const* optional{std::get_if<OptionalSentinelType>(&node.definition)}) {
        ImGui::SeparatorText("Sentinel-encoded optional representation");
        auto const& source{workspace_.types().type(optional->source.type)};
        ImGui::TextUnformatted("Semantic source");
        ImGui::SameLine();
        if (ImGui::SmallButton(source.cpp_spelling.c_str())) {
            selected_type_ = optional->source.type;
            selected_field_.clear();
            record_access_members_.clear();
            record_access_set_explicit_ = false;
        }
        if (optional_sentinel_analysis_.has_value()) {
            auto const& analysis{*optional_sentinel_analysis_};
            auto const minimum{codegen::format_packed_integer(analysis.source_minimum)};
            auto const maximum{codegen::format_packed_integer(analysis.source_maximum)};
            auto const sentinel{codegen::format_packed_integer(analysis.sentinel_value)};
            ImGui::Text("Present range: %s .. %s", minimum.c_str(), maximum.c_str());
            ImGui::Text("Present values: %s",
                        detail::format_number(analysis.present_value_count).c_str());
            ImGui::Text(
                "Absence sentinel: %s = %s", analysis.sentinel_name.c_str(), sentinel.c_str());
            ImGui::Text("Other named sentinel codes: %llu",
                        static_cast<unsigned long long>(analysis.other_sentinel_code_count));
            ImGui::Text("Unused codes: %s",
                        detail::format_number(analysis.unused_code_count).c_str());
            ImGui::Text("Encoded width: %u bits", analysis.encoded_storage_bits);
            ImGui::Text("Total code space: %s",
                        detail::format_code_count(analysis.total_code_count).c_str());
            ImGui::Text("Encoded payload at %llu values: %s bits",
                        static_cast<unsigned long long>(analysis.element_count),
                        detail::format_number(analysis.total_encoded_bits).c_str());
            ImGui::TextDisabled(
                "Sentinel and unused codes are code-space roles, not allocated byte waste.");
            draw_diagnostics(analysis.diagnostics);
        }
        if (draw_optional_sentinel_editor(node, *optional)) {
            ImGui::End();
            return;
        }
    } else if (auto const* optional{std::get_if<OptionalPresenceBitType>(&node.definition)}) {
        ImGui::SeparatorText("Presence-bit optional representation");
        auto const& source{workspace_.types().type(optional->source.type)};
        ImGui::TextUnformatted("Semantic source");
        ImGui::SameLine();
        if (ImGui::SmallButton(source.cpp_spelling.c_str())) {
            selected_type_ = optional->source.type;
            selected_field_.clear();
            record_access_members_.clear();
            record_access_set_explicit_ = false;
        }
        if (optional_presence_bit_analysis_.has_value()) {
            auto const& analysis{*optional_presence_bit_analysis_};
            auto const minimum{codegen::format_packed_integer(analysis.source_minimum)};
            auto const maximum{codegen::format_packed_integer(analysis.source_maximum)};
            ImGui::Text("Present range: %s .. %s", minimum.c_str(), maximum.c_str());
            ImGui::Text("Present values: %s",
                        detail::format_number(analysis.present_value_count).c_str());
            ImGui::Text("Bit allocation: %u presence + %u payload = %u bits/value",
                        analysis.presence_bits,
                        analysis.payload_bits,
                        analysis.encoded_storage_bits);
            ImGui::Text("Canonical absence states: %llu",
                        static_cast<unsigned long long>(analysis.canonical_absence_state_count));
            ImGui::Text("Source named sentinel codes: %llu",
                        static_cast<unsigned long long>(analysis.source_sentinel_code_count));
            ImGui::Text("Source unused payload codes: %s",
                        detail::format_number(analysis.source_unused_payload_codes).c_str());
            ImGui::Text("Ignored-payload absence patterns: %s noncanonical",
                        detail::format_number(analysis.noncanonical_absence_patterns).c_str());
            ImGui::Text("Encoded payload at %llu values: %s bits",
                        static_cast<unsigned long long>(analysis.element_count),
                        detail::format_number(analysis.total_encoded_bits).c_str());
            ImGui::TextDisabled(
                "The one canonical absence state is semantic; other absent payload patterns are "
                "redundant physical encodings, not extra states or byte padding.");
            draw_diagnostics(analysis.diagnostics);
        }
        if (draw_optional_presence_bit_editor(node, *optional)) {
            ImGui::End();
            return;
        }
    } else if (auto const* fixed{std::get_if<FixedPointType>(&node.definition)}) {
        ImGui::SeparatorText("Fixed-point representation");
        if (fixed_point_analysis_.has_value()) {
            auto const& analysis{*fixed_point_analysis_};
            auto const minimum_raw{codegen::format_packed_integer(analysis.minimum_raw_value)};
            auto const maximum_raw{codegen::format_packed_integer(analysis.maximum_raw_value)};
            ImGui::Text("Signedness: %s", analysis.signedness ? "signed" : "unsigned");
            ImGui::Text("Bit allocation: %u whole + %u fractional%s",
                        analysis.whole_bits,
                        analysis.fractional_bits,
                        analysis.signedness ? " + 1 sign" : "");
            ImGui::Text("Total encoded width: %u bits", analysis.total_bits);
            ImGui::Text("Raw range: %s .. %s", minimum_raw.c_str(), maximum_raw.c_str());
            ImGui::Text("Scale: %.12g raw codes / unit", static_cast<double>(analysis.scale));
            ImGui::Text("Resolution: %.12g", static_cast<double>(analysis.resolution));
            ImGui::Text("Representable range: %.12g .. %.12g",
                        static_cast<double>(analysis.minimum_value),
                        static_cast<double>(analysis.maximum_value));
            ImGui::Text("Rounding: %s",
                        codegen::fixed_point_rounding_name(analysis.rounding).data());
            ImGui::Text("Maximum rounding error: %.12g",
                        static_cast<double>(analysis.maximum_rounding_error));
            ImGui::Text("Encoded payload at %llu values: %s bits",
                        static_cast<unsigned long long>(analysis.element_count),
                        detail::format_number(analysis.total_encoded_bits).c_str());
            ImGui::TextDisabled(
                "Payload bits are not an allocated byte size until placement/lowering is chosen.");
            draw_diagnostics(analysis.diagnostics);
        }
        if (draw_fixed_point_editor(node, *fixed)) {
            ImGui::End();
            return;
        }
    } else if (auto const* mini_float{std::get_if<MiniFloatType>(&node.definition)}) {
        ImGui::SeparatorText("Mini-float representation");
        if (mini_float_analysis_.has_value()) {
            auto const& analysis{*mini_float_analysis_};
            ImGui::Text("Bit allocation: %u sign + %u exponent + %u significand = %u bits",
                        analysis.sign_bits,
                        analysis.exponent_bits,
                        analysis.significand_bits,
                        analysis.total_bits);
            ImGui::Text("Exponent bias: %d", analysis.exponent_bias);
            ImGui::Text("Normal exponent range: %d .. %d",
                        analysis.minimum_normal_exponent,
                        analysis.maximum_normal_exponent);
            ImGui::Text("Total code space: %s",
                        detail::format_code_count(analysis.total_code_count).c_str());
            ImGui::Text("Code roles: %llu zero, %llu infinity, %llu NaN, %llu subnormal",
                        static_cast<unsigned long long>(analysis.zero_code_count),
                        static_cast<unsigned long long>(analysis.infinity_code_count),
                        static_cast<unsigned long long>(analysis.nan_code_count),
                        static_cast<unsigned long long>(analysis.nonzero_subnormal_code_count));
            auto draw_numeric = [](char const* label, std::optional<long double> const value) {
                if (value.has_value()) {
                    ImGui::Text("%s: %.12g", label, static_cast<double>(*value));
                } else {
                    ImGui::TextDisabled("%s: Unknown", label);
                }
            };
            draw_numeric("Minimum positive subnormal", analysis.minimum_positive_subnormal);
            draw_numeric("Minimum positive normal", analysis.minimum_positive_normal);
            draw_numeric("Minimum finite", analysis.minimum_finite);
            draw_numeric("Maximum finite", analysis.maximum_finite);
            draw_numeric("Resolution around 1.0", analysis.unit_interval_resolution);
            ImGui::Text("Maximum relative rounding error: %.12g",
                        static_cast<double>(analysis.maximum_relative_rounding_error));
            ImGui::Text("Encoded payload at %llu values: %s bits",
                        static_cast<unsigned long long>(analysis.element_count),
                        detail::format_number(analysis.total_encoded_bits).c_str());
            ImGui::TextDisabled(
                "IEEE-style code roles are encoding facts; arithmetic conformance, byte order, "
                "ABI placement, and native compiler support are unspecified.");
            draw_diagnostics(analysis.diagnostics);
        }
        if (draw_mini_float_editor(node, *mini_float)) {
            ImGui::End();
            return;
        }
    } else if (auto const* varint{std::get_if<IntegerVarintType>(&node.definition)}) {
        ImGui::SeparatorText("Variable-length integer representation");
        auto const& source{workspace_.types().type(varint->source.type)};
        ImGui::TextUnformatted("Semantic source");
        ImGui::SameLine();
        if (ImGui::SmallButton(source.cpp_spelling.c_str())) {
            selected_type_ = varint->source.type;
            selected_field_.clear();
            record_access_members_.clear();
            record_access_set_explicit_ = false;
        }
        if (integer_varint_analysis_.has_value()) {
            auto const& analysis{*integer_varint_analysis_};
            ImGui::Text("Encoding: %s",
                        codegen::integer_varint_encoding_name(analysis.encoding).data());
            ImGui::Text("Encoded size: %u .. %u bytes/value",
                        analysis.minimum_encoded_bytes,
                        analysis.maximum_encoded_bytes);
            ImGui::Text("At %llu values: %s .. %s",
                        static_cast<unsigned long long>(analysis.element_count),
                        detail::format_bytes(analysis.minimum_total_bytes).c_str(),
                        detail::format_bytes(analysis.maximum_total_bytes).c_str());
            ImGui::TextDisabled(
                "Expected encoded size is Unknown until a value distribution is supplied.");
            draw_diagnostics(analysis.diagnostics);
        }

        ImGui::SeparatorText("Session value distribution");
        auto& rows{varint_distributions_[source.identity]};
        auto remove_index{std::optional<std::size_t>{}};
        if (ImGui::BeginTable("varint-distribution",
                              3,
                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                  ImGuiTableFlags_SizingStretchProp)) {
            ImGui::TableSetupColumn("Semantic value");
            ImGui::TableSetupColumn("Weight");
            ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableHeadersRow();
            for (std::size_t index{}; index < rows.size(); ++index) {
                ImGui::PushID(static_cast<int>(index));
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::SetNextItemWidth(-1.0F);
                ImGui::InputText("##value", rows[index].value.data(), rows[index].value.size());
                ImGui::TableNextColumn();
                ImGui::SetNextItemWidth(-1.0F);
                ImGui::InputScalar(
                    "##weight", ImGuiDataType_U64, &rows[index].weight, nullptr, nullptr, "%llu");
                ImGui::TableNextColumn();
                if (ImGui::SmallButton("Delete")) {
                    remove_index = index;
                }
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        if (remove_index.has_value()) {
            rows.erase(rows.begin() + static_cast<std::ptrdiff_t>(*remove_index));
        }

        ImGui::SetNextItemWidth(150.0F);
        ImGui::InputText("Value##new-varint-distribution",
                         new_varint_distribution_value_.data(),
                         new_varint_distribution_value_.size());
        ImGui::SameLine();
        ImGui::SetNextItemWidth(130.0F);
        ImGui::InputScalar("Weight##new-varint-distribution",
                           ImGuiDataType_U64,
                           &new_varint_distribution_weight_,
                           nullptr,
                           nullptr,
                           "%llu");
        ImGui::SameLine();
        auto const new_value{detail::parse_packed_integer(new_varint_distribution_value_.data())};
        ImGui::BeginDisabled(!new_value.has_value());
        if (ImGui::Button("Add##varint-distribution")) {
            VarintDistributionRow row{.value = {}, .weight = new_varint_distribution_weight_};
            std::snprintf(
                row.value.data(), row.value.size(), "%s", new_varint_distribution_value_.data());
            rows.push_back(row);
        }
        ImGui::EndDisabled();

        std::vector<IntegerVarintDistributionEntry> entries;
        entries.reserve(rows.size());
        auto invalid_literal_count{std::size_t{}};
        for (auto const& row : rows) {
            if (auto const value{detail::parse_packed_integer(row.value.data())}) {
                entries.push_back({.value = *value, .weight = row.weight});
            } else {
                ++invalid_literal_count;
            }
        }
        auto const distribution{
            Analyzer::analyze_integer_varint_distribution(workspace_.types(),
                                                          integer_varint_analysis_->type,
                                                          entries,
                                                          workspace_.element_count())};
        if (invalid_literal_count != 0) {
            ImGui::TextColored(detail::diagnostic_color(DiagnosticSeverity::error),
                               "%llu distribution row%s contain an invalid integer literal.",
                               static_cast<unsigned long long>(invalid_literal_count),
                               invalid_literal_count == 1 ? "" : "s");
        }
        ImGui::Text("Exact sample weight: %s",
                    detail::format_number(distribution.total_weight).c_str());
        ImGui::Text("Exact sample bytes: %s",
                    detail::format_bytes(distribution.total_encoded_bytes).c_str());
        if (!distribution.entries.empty() &&
            ImGui::BeginTable("varint-distribution-breakdown",
                              4,
                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                  ImGuiTableFlags_SizingStretchProp)) {
            ImGui::TableSetupColumn("Value");
            ImGui::TableSetupColumn("Weight");
            ImGui::TableSetupColumn("Bytes / value");
            ImGui::TableSetupColumn("Weighted bytes");
            ImGui::TableHeadersRow();
            for (auto const& entry : distribution.entries) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(codegen::format_packed_integer(entry.value).c_str());
                ImGui::TableNextColumn();
                ImGui::Text("%llu", static_cast<unsigned long long>(entry.weight));
                ImGui::TableNextColumn();
                ImGui::Text("%u", entry.encoded_bytes);
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(detail::format_bytes(entry.weighted_encoded_bytes).c_str());
            }
            ImGui::EndTable();
        }
        if (distribution.expected_bytes_per_value.has_value()) {
            ImGui::Text("Expected bytes / value: %.8g",
                        static_cast<double>(*distribution.expected_bytes_per_value));
            ImGui::Text("Expected bytes at %llu values: %.8g",
                        static_cast<unsigned long long>(distribution.selected_element_count),
                        static_cast<double>(*distribution.expected_selected_bytes));
        } else {
            ImGui::TextDisabled("Expected bytes / value: Unknown");
        }
        ImGui::TextDisabled(
            "Session workload only; weights are not written into the LispB semantic declaration.");
        draw_diagnostics(distribution.diagnostics);
        if (draw_integer_varint_editor(node, *varint)) {
            ImGui::End();
            return;
        }
    } else if (auto const* record{std::get_if<RecordType>(&node.definition)}) {
        ImGui::SeparatorText("LispB record declaration");
        if (draw_record_editor(node, *record)) {
            ImGui::End();
            return;
        }
    } else if (auto const* union_type{std::get_if<UnionType>(&node.definition)}) {
        ImGui::SeparatorText("Raw union layout");
        auto const& analysis{*union_analysis_};
        ImGui::Text("Alternatives: %llu",
                    static_cast<unsigned long long>(union_type->alternatives.size()));
        ImGui::Text("Size: %s", detail::format_bytes(analysis.size_bytes).c_str());
        ImGui::Text("Alignment: %s", detail::format_bytes(analysis.alignment_bytes).c_str());
        ImGui::Text("Largest alternative: %s",
                    detail::format_bytes(analysis.largest_alternative_bytes).c_str());
        ImGui::Text("Tail padding: %s", detail::format_bytes(analysis.tail_padding_bytes).c_str());
        if (selected_declaration.has_value()) {
            ImGui::SeparatorText("Session alternative workload");
            auto& weights{union_distributions_[*selected_declaration]};
            if (ImGui::BeginTable("raw-union-workload-weights",
                                  2,
                                  ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                      ImGuiTableFlags_SizingStretchProp)) {
                ImGui::TableSetupColumn("Alternative");
                ImGui::TableSetupColumn("Weight");
                ImGui::TableHeadersRow();
                for (auto const& alternative : union_type->alternatives) {
                    ImGui::PushID(alternative.name.c_str());
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(alternative.name.c_str());
                    ImGui::TableNextColumn();
                    auto& weight{weights[alternative.name]};
                    ImGui::SetNextItemWidth(-1.0F);
                    if (ImGui::InputScalar("##weight", ImGuiDataType_U64, &weight)) {
                        ++union_distribution_revision_;
                    }
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }
            if (ImGui::SmallButton("Clear alternative workload")) {
                weights.clear();
                ++union_distribution_revision_;
            }
            if (union_distribution_analysis_.has_value()) {
                auto const& distribution{*union_distribution_analysis_};
                ImGui::Text("Sample weight: %s",
                            detail::format_number(distribution.total_weight).c_str());
                ImGui::Text("Sample active extent: %s",
                            detail::format_bytes(distribution.total_extent_bytes).c_str());
                ImGui::Text("Sample conditional slack: %s",
                            detail::format_bytes(distribution.total_slack_bytes).c_str());
                if (distribution.expected_extent_bytes_per_value.has_value()) {
                    ImGui::Text("Expected active extent / value: %.8g bytes",
                                static_cast<double>(*distribution.expected_extent_bytes_per_value));
                    ImGui::Text("Expected conditional slack / value: %.8g bytes",
                                static_cast<double>(*distribution.expected_slack_bytes_per_value));
                    ImGui::Text(
                        "Expected active extent at %llu values: %.8g bytes",
                        static_cast<unsigned long long>(distribution.selected_element_count),
                        static_cast<double>(*distribution.expected_selected_extent_bytes));
                    ImGui::Text(
                        "Expected conditional slack at %llu values: %.8g bytes",
                        static_cast<unsigned long long>(distribution.selected_element_count),
                        static_cast<double>(*distribution.expected_selected_slack_bytes));
                } else {
                    ImGui::TextDisabled("Expected alternative usage: Unknown");
                }
                if (ImGui::BeginTable("raw-union-workload-analysis",
                                      6,
                                      ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                          ImGuiTableFlags_SizingStretchProp)) {
                    ImGui::TableSetupColumn("Alternative");
                    ImGui::TableSetupColumn("Weight");
                    ImGui::TableSetupColumn("Extent");
                    ImGui::TableSetupColumn("Slack");
                    ImGui::TableSetupColumn("Weighted extent");
                    ImGui::TableSetupColumn("Weighted slack");
                    ImGui::TableHeadersRow();
                    for (auto const& entry : distribution.entries) {
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(entry.alternative_name.c_str());
                        ImGui::TableNextColumn();
                        ImGui::Text("%llu", static_cast<unsigned long long>(entry.weight));
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(detail::format_bytes(entry.extent_bytes).c_str());
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(detail::format_bytes(entry.slack_bytes).c_str());
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(
                            detail::format_bytes(entry.weighted_extent_bytes).c_str());
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(
                            detail::format_bytes(entry.weighted_slack_bytes).c_str());
                    }
                    ImGui::EndTable();
                }
                draw_diagnostics(distribution.diagnostics);
            }
            ImGui::TextDisabled(
                "Session-only conditional workload; weights are not LispB semantics and do not "
                "imply a stored discriminant or a performance result.");
        }
        ImGui::TextDisabled("A raw union has no stored discriminant; tagged unions are separate.");
        draw_diagnostics(analysis.diagnostics);
        ImGui::SeparatorText("LispB union declaration");
        if (draw_union_editor(node, *union_type)) {
            ImGui::End();
            return;
        }
    } else if (auto const* tagged{std::get_if<TaggedUnionType>(&node.definition)}) {
        ImGui::SeparatorText("Tagged union semantics");
        auto const& analysis{*tagged_union_analysis_};
        auto const& discriminant{workspace_.types().type(tagged->discriminant.type)};
        ImGui::Text("Discriminant: %s", discriminant.identity.name.c_str());
        ImGui::Text("Size: %s", detail::format_bytes(analysis.size_bytes).c_str());
        ImGui::Text("Alignment: %s", detail::format_bytes(analysis.alignment_bytes).c_str());
        ImGui::Text("Payload offset: %s",
                    detail::format_bytes(analysis.payload_offset_bytes).c_str());
        ImGui::Text("Internal padding: %s",
                    detail::format_bytes(analysis.internal_padding_bytes).c_str());
        ImGui::Text("Tail padding: %s", detail::format_bytes(analysis.tail_padding_bytes).c_str());
        ImGui::Text("Tag coverage: %llu mapped, %llu unmapped, %llu named sentinel%s",
                    static_cast<unsigned long long>(analysis.mapped_live_tags.size()),
                    static_cast<unsigned long long>(analysis.unmapped_live_tags.size()),
                    static_cast<unsigned long long>(analysis.sentinel_tags.size()),
                    analysis.count_sentinel_tag.has_value() ? ", count sentinel present" : "");
        if (ImGui::SmallButton("Go to discriminant")) {
            selected_type_ = tagged->discriminant.type;
            selected_field_.clear();
            ImGui::End();
            return;
        }
        if (ImGui::BeginTable("tagged-union-properties",
                              4,
                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                  ImGuiTableFlags_SizingStretchProp)) {
            ImGui::TableSetupColumn("Tag");
            ImGui::TableSetupColumn("Alternative");
            ImGui::TableSetupColumn("Type");
            ImGui::TableSetupColumn("Count");
            ImGui::TableHeadersRow();
            for (auto const& alternative : tagged->alternatives) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(alternative.tag.c_str());
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(alternative.name.c_str());
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(
                    workspace_.types().type(alternative.semantic_type.type).cpp_spelling.c_str());
                ImGui::TableNextColumn();
                ImGui::Text("%llu", static_cast<unsigned long long>(alternative.count.value_or(1)));
            }
            ImGui::EndTable();
        }
        if (selected_declaration.has_value()) {
            ImGui::SeparatorText("Session tag workload");
            auto& weights{tagged_union_distributions_[*selected_declaration]};
            if (ImGui::BeginTable("tagged-union-workload-weights",
                                  3,
                                  ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                      ImGuiTableFlags_SizingStretchProp)) {
                ImGui::TableSetupColumn("Tag");
                ImGui::TableSetupColumn("Alternative");
                ImGui::TableSetupColumn("Weight");
                ImGui::TableHeadersRow();
                for (auto const& alternative : tagged->alternatives) {
                    ImGui::PushID(alternative.tag.c_str());
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(alternative.tag.c_str());
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(alternative.name.c_str());
                    ImGui::TableNextColumn();
                    auto& weight{weights[alternative.tag]};
                    ImGui::SetNextItemWidth(-1.0F);
                    if (ImGui::InputScalar("##weight", ImGuiDataType_U64, &weight)) {
                        ++tagged_distribution_revision_;
                    }
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }
            if (ImGui::SmallButton("Clear tag workload")) {
                weights.clear();
                ++tagged_distribution_revision_;
            }
            if (tagged_union_distribution_analysis_.has_value()) {
                auto const& distribution{*tagged_union_distribution_analysis_};
                ImGui::Text("Sample weight: %s",
                            detail::format_number(distribution.total_weight).c_str());
                ImGui::Text("Sample active payload extent: %s",
                            detail::format_bytes(distribution.total_payload_extent_bytes).c_str());
                ImGui::Text("Sample payload slack: %s",
                            detail::format_bytes(distribution.total_payload_slack_bytes).c_str());
                if (distribution.expected_payload_extent_bytes_per_value.has_value()) {
                    ImGui::Text(
                        "Expected active payload / value: %.8g bytes",
                        static_cast<double>(*distribution.expected_payload_extent_bytes_per_value));
                    ImGui::Text(
                        "Expected payload slack / value: %.8g bytes",
                        static_cast<double>(*distribution.expected_payload_slack_bytes_per_value));
                    ImGui::Text(
                        "Expected active payload at %llu values: %.8g bytes",
                        static_cast<unsigned long long>(distribution.selected_element_count),
                        static_cast<double>(*distribution.expected_selected_payload_extent_bytes));
                    ImGui::Text(
                        "Expected payload slack at %llu values: %.8g bytes",
                        static_cast<unsigned long long>(distribution.selected_element_count),
                        static_cast<double>(*distribution.expected_selected_payload_slack_bytes));
                }
                if (ImGui::BeginTable("tagged-union-workload-analysis",
                                      6,
                                      ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                          ImGuiTableFlags_SizingStretchProp)) {
                    ImGui::TableSetupColumn("Tag");
                    ImGui::TableSetupColumn("Weight");
                    ImGui::TableSetupColumn("Extent");
                    ImGui::TableSetupColumn("Slack");
                    ImGui::TableSetupColumn("Weighted extent");
                    ImGui::TableSetupColumn("Weighted slack");
                    ImGui::TableHeadersRow();
                    for (auto const& entry : distribution.entries) {
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(entry.tag.c_str());
                        ImGui::TableNextColumn();
                        ImGui::Text("%llu", static_cast<unsigned long long>(entry.weight));
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(
                            detail::format_bytes(entry.payload_extent_bytes).c_str());
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(
                            detail::format_bytes(entry.payload_slack_bytes).c_str());
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(
                            detail::format_bytes(entry.weighted_payload_extent_bytes).c_str());
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(
                            detail::format_bytes(entry.weighted_payload_slack_bytes).c_str());
                    }
                    ImGui::EndTable();
                }
                draw_diagnostics(distribution.diagnostics);
            } else {
                ImGui::TextDisabled("Assign a positive weight to calculate workload expectations.");
            }
            ImGui::TextDisabled("Session workload only; weights are not written into LispB.");
        }
        draw_diagnostics(analysis.diagnostics);
        ImGui::SeparatorText("LispB tagged-union declaration");
        if (draw_tagged_union_editor(node, *tagged)) {
            ImGui::End();
            return;
        }
    } else if (auto const* external{std::get_if<ExternalType>(&node.definition)}) {
        ImGui::SeparatorText("External type");
        ImGui::Text("C++ spelling: %s", external->cpp_type.spelling.c_str());
        if (!external->registered_names.empty()) {
            ImGui::TextUnformatted("Registered as");
            for (auto const& name : external->registered_names) {
                ImGui::BulletText("@%s", name.c_str());
            }
        }
        ImGui::TextDisabled("Internal structure is not declared in LispB.");
    } else {
        if (auto const* packed{std::get_if<PackedType>(&node.definition)}) {
            ImGui::SeparatorText("LispB packed declaration");
            if (draw_packed_editor(node, *packed)) {
                ImGui::End();
                return;
            }
        } else if (auto const* soa{std::get_if<SoaType>(&node.definition)}) {
            ImGui::SeparatorText("LispB SoA declaration");
            if (draw_soa_editor(node, *soa)) {
                ImGui::End();
                return;
            }
        }

        auto const editable{workspace_.active_variant_id() != LayoutWorkspace::baseline_variant_id};
        if (!editable) {
            ImGui::SeparatorText("Baseline");
            if (std::holds_alternative<PackedType>(node.definition) ||
                std::holds_alternative<SoaType>(node.definition)) {
                ImGui::TextDisabled("Planning overrides are read only on the baseline.");
                ImGui::TextWrapped("Edit the LispB declaration above, or create an experiment for "
                                   "session-only physical overrides.");
            } else {
                ImGui::TextDisabled("Loaded from LispB — read only.");
                ImGui::TextWrapped("Experiments are session-only variants. The production schema "
                                   "is never modified.");
            }
            ImGui::TextDisabled("Use + Add variant in the Layout panel to create an experiment.");
            if (std::holds_alternative<SoaType>(node.definition)) {
                ImGui::TextDisabled("Capacity uses the planner default of %llu.",
                                    static_cast<unsigned long long>(workspace_.default_capacity()));
            }
        } else {
            ImGui::SeparatorText("Experiment");
            ImGui::Text("Editing %s", workspace_.active_variant().name.c_str());
        }

        ImGui::BeginDisabled(!editable);
        if (auto const* packed{std::get_if<PackedType>(&node.definition)}) {
            ImGui::SeparatorText("Packed storage");
            ImGui::Text("Schema storage: %s", active_packed_->schema_storage_type.c_str());
            auto const& analysis{*active_packed_};
            if (ImGui::BeginCombo("Planning storage", analysis.storage_type.c_str())) {
                if (ImGui::Selectable("Schema storage", !analysis.storage_overridden)) {
                    workspace_.set_packed_storage_type(selected, std::nullopt);
                }
                for (auto const& [type, facts] : abi_.types()) {
                    if (facts.unsigned_value_bits.has_value() &&
                        ImGui::Selectable(type.c_str(), analysis.storage_type == type)) {
                        workspace_.set_packed_storage_type(selected, type);
                    }
                }
                ImGui::EndCombo();
            }
            draw_override_note(analysis.storage_overridden);
            if (analysis.storage_overridden) {
                ImGui::SameLine();
                if (ImGui::SmallButton("Reset storage")) {
                    workspace_.set_packed_storage_type(selected, std::nullopt);
                }
            }

            if (selected_field_.empty() && !packed->segments.empty()) {
                selected_field_ = std::visit(
                    [](auto const& segment) -> std::string const& { return segment.name; },
                    packed->segments.front());
            }
            ImGui::SeparatorText("Fields");
            if (ImGui::BeginTable("packed-fields",
                                  4,
                                  ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                      ImGuiTableFlags_Resizable |
                                      ImGuiTableFlags_SizingStretchProp)) {
                ImGui::TableSetupColumn("Field");
                ImGui::TableSetupColumn("Semantic type");
                ImGui::TableSetupColumn("Schema");
                ImGui::TableSetupColumn("Planning");
                ImGui::TableHeadersRow();
                for (auto const& field : analysis.fields) {
                    ImGui::PushID(field.name.c_str());
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    if (ImGui::Selectable(field.name.c_str(), selected_field_ == field.name)) {
                        selected_field_ = field.name;
                    }
                    ImGui::TableNextColumn();
                    if (field.semantic_type.has_value()) {
                        auto const& semantic_type{workspace_.types().type(*field.semantic_type)};
                        if (ImGui::SmallButton(semantic_type.cpp_spelling.c_str())) {
                            selected_type_ = *field.semantic_type;
                            selected_field_.clear();
                            record_access_members_.clear();
                            record_access_set_explicit_ = false;
                        }
                    } else {
                        ImGui::TextDisabled("Reserved");
                    }
                    ImGui::TableNextColumn();
                    if (field.schema_bit_width_auto) {
                        ImGui::Text("auto => %u", field.schema_bit_width);
                    } else {
                        ImGui::Text("%u bits", field.schema_bit_width);
                    }
                    ImGui::TableNextColumn();
                    if (field.reserved) {
                        ImGui::TextDisabled("Durable");
                    } else {
                        auto width{field.bit_width};
                        ImGui::SetNextItemWidth(-1.0F);
                        if (ImGui::InputScalar("##planning-width", ImGuiDataType_U32, &width)) {
                            width = std::clamp(width, std::uint32_t{1}, std::uint32_t{64});
                            workspace_.set_packed_field_width(selected, field.name, width);
                        }
                        if (field.overridden && ImGui::SmallButton("Reset")) {
                            workspace_.set_packed_field_width(selected, field.name, std::nullopt);
                        }
                    }
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }
        } else if (auto const* soa{std::get_if<SoaType>(&node.definition)}) {
            auto const& analysis{*active_soa_};
            ImGui::SeparatorText("Planner capacity");
            auto capacity{analysis.capacity};
            if (ImGui::InputScalar("Capacity", ImGuiDataType_U64, &capacity)) {
                workspace_.set_capacity(selected, capacity);
            }
            draw_override_note(analysis.capacity_overridden);
            if (analysis.capacity_overridden) {
                ImGui::SameLine();
                if (ImGui::SmallButton("Reset capacity")) {
                    workspace_.set_capacity(selected, std::nullopt);
                }
            }

            if (selected_field_.empty() && !soa->columns.empty()) {
                selected_field_ = soa->columns.front().name;
            }
            ImGui::SeparatorText("Columns");
            if (ImGui::BeginTable("soa-columns-properties",
                                  3,
                                  ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                      ImGuiTableFlags_Resizable |
                                      ImGuiTableFlags_SizingStretchProp)) {
                ImGui::TableSetupColumn("Column");
                ImGui::TableSetupColumn("Semantic type");
                ImGui::TableSetupColumn("Planning");
                ImGui::TableHeadersRow();
                for (auto const& column : analysis.columns) {
                    ImGui::PushID(column.name.c_str());
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    if (ImGui::Selectable(column.name.c_str(), selected_field_ == column.name)) {
                        selected_field_ = column.name;
                    }
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(column.schema_type.c_str());
                    ImGui::TableNextColumn();
                    ImGui::SetNextItemWidth(-1.0F);
                    if (ImGui::BeginCombo("##planning-type", column.physical_type.c_str())) {
                        if (ImGui::Selectable("Schema type", !column.overridden)) {
                            workspace_.set_soa_column_type(selected, column.name, std::nullopt);
                        }
                        for (auto const& [type, facts] : abi_.types()) {
                            static_cast<void>(facts);
                            if (ImGui::Selectable(type.c_str(), column.physical_type == type)) {
                                workspace_.set_soa_column_type(selected, column.name, type);
                            }
                        }
                        ImGui::EndCombo();
                    }
                    if (column.overridden && ImGui::SmallButton("Reset")) {
                        workspace_.set_soa_column_type(selected, column.name, std::nullopt);
                    }
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }
        }
        ImGui::EndDisabled();
    }

    auto draw_links = [&](char const* heading, std::span<TypeId const> const links) {
        ImGui::SeparatorText(heading);
        if (links.empty()) {
            ImGui::TextDisabled("None");
        }
        for (auto const linked : links) {
            auto const& linked_node{workspace_.types().type(linked)};
            ImGui::PushID(static_cast<int>(linked.value));
            if (ImGui::SmallButton(linked_node.cpp_spelling.c_str())) {
                selected_type_ = linked;
                selected_field_.clear();
                record_access_members_.clear();
                record_access_set_explicit_ = false;
            }
            ImGui::PopID();
        }
    };
    draw_links("Depends on", workspace_.types().dependencies_of(selected));
    draw_links("Used by", workspace_.types().users_of(selected));
    ImGui::End();
}

auto PlannerUi::duplicate_selected_declaration(TypeNode const& node) -> bool {
    if (!document_.has_value()) {
        return false;
    }
    auto const declaration{document_->find_declaration(node.identity)};
    if (!declaration.has_value()) {
        return false;
    }
    auto const* info{document_->declaration(*declaration)};
    if (info == nullptr) {
        return false;
    }

    auto name{node.identity.name + "_copy"};
    for (auto suffix{2U}; workspace_.types().find_declared(node.identity.module_name, name);
         ++suffix) {
        name = node.identity.name + "_copy" + std::to_string(suffix);
    }
    auto selection{node.identity};
    selection.name = name;
    auto const id{document_->allocate_declaration_id()};

    if (auto const* schema{document_->enum_schema(*declaration)}) {
        auto copy{*schema};
        copy.name = name;
        return apply_document_edit(CreateEnum{.declaration = id,
                                              .module_index = info->module_index,
                                              .schema = std::move(copy),
                                              .insertion_index = std::nullopt},
                                   selection);
    }
    if (auto const* schema{document_->integer_scalar_schema(*declaration)}) {
        auto copy{*schema};
        copy.name = name;
        return apply_document_edit(CreateIntegerScalar{.declaration = id,
                                                       .module_index = info->module_index,
                                                       .schema = std::move(copy),
                                                       .insertion_index = std::nullopt},
                                   selection);
    }
    if (auto const* schema{document_->linear_quantized_schema(*declaration)}) {
        auto copy{*schema};
        copy.name = name;
        return apply_document_edit(CreateLinearQuantized{.declaration = id,
                                                         .module_index = info->module_index,
                                                         .schema = std::move(copy),
                                                         .insertion_index = std::nullopt},
                                   selection);
    }
    if (auto const* schema{document_->integer_varint_schema(*declaration)}) {
        auto copy{*schema};
        copy.name = name;
        return apply_document_edit(CreateIntegerVarint{.declaration = id,
                                                       .module_index = info->module_index,
                                                       .schema = std::move(copy),
                                                       .insertion_index = std::nullopt},
                                   selection);
    }
    if (auto const* schema{document_->fixed_point_schema(*declaration)}) {
        auto copy{*schema};
        copy.name = name;
        return apply_document_edit(CreateFixedPoint{.declaration = id,
                                                    .module_index = info->module_index,
                                                    .schema = std::move(copy),
                                                    .insertion_index = std::nullopt},
                                   selection);
    }
    if (auto const* schema{document_->mini_float_schema(*declaration)}) {
        auto copy{*schema};
        copy.name = name;
        return apply_document_edit(CreateMiniFloat{.declaration = id,
                                                   .module_index = info->module_index,
                                                   .schema = std::move(copy),
                                                   .insertion_index = std::nullopt},
                                   selection);
    }
    if (auto const* schema{document_->optional_sentinel_schema(*declaration)}) {
        auto copy{*schema};
        copy.name = name;
        return apply_document_edit(CreateOptionalSentinel{.declaration = id,
                                                          .module_index = info->module_index,
                                                          .schema = std::move(copy),
                                                          .insertion_index = std::nullopt},
                                   selection);
    }
    if (auto const* schema{document_->optional_presence_bit_schema(*declaration)}) {
        auto copy{*schema};
        copy.name = name;
        return apply_document_edit(CreateOptionalPresenceBit{.declaration = id,
                                                             .module_index = info->module_index,
                                                             .schema = std::move(copy),
                                                             .insertion_index = std::nullopt},
                                   selection);
    }
    if (auto const* schema{document_->packed_value_schema(*declaration)}) {
        auto copy{*schema};
        copy.name = name;
        return apply_document_edit(CreatePackedValue{.declaration = id,
                                                     .module_index = info->module_index,
                                                     .schema = std::move(copy),
                                                     .insertion_index = std::nullopt},
                                   selection);
    }
    if (auto const* schema{document_->record_schema(*declaration)}) {
        auto copy{*schema};
        copy.name = name;
        return apply_document_edit(CreateRecord{.declaration = id,
                                                .module_index = info->module_index,
                                                .schema = std::move(copy),
                                                .insertion_index = std::nullopt},
                                   selection);
    }
    if (auto const* schema{document_->union_schema(*declaration)}) {
        auto copy{*schema};
        copy.name = name;
        return apply_document_edit(CreateUnion{.declaration = id,
                                               .module_index = info->module_index,
                                               .schema = std::move(copy),
                                               .insertion_index = std::nullopt},
                                   selection);
    }
    if (auto const* schema{document_->tagged_union_schema(*declaration)}) {
        auto copy{*schema};
        copy.name = name;
        return apply_document_edit(CreateTaggedUnion{.declaration = id,
                                                     .module_index = info->module_index,
                                                     .schema = std::move(copy),
                                                     .insertion_index = std::nullopt},
                                   selection);
    }
    if (document_->soa_schema(*declaration) != nullptr) {
        auto copy{document_->prepare_soa_duplicate(*declaration)};
        if (!copy.has_value()) {
            schema_edit_message_ = copy.error().message;
            return false;
        }
        selection.name = copy->name;
        return apply_document_edit(CreateSoa{.declaration = id,
                                             .module_index = info->module_index,
                                             .schema = std::move(*copy),
                                             .insertion_index = std::nullopt},
                                   selection);
    }

    schema_edit_message_ = "This declaration kind is not editable and cannot be duplicated.";
    return false;
}

auto PlannerUi::delete_declaration(DeclarationId const declaration) -> bool {
    if (!document_.has_value()) {
        return false;
    }
    auto const* info{document_->declaration(declaration)};
    if (info == nullptr) {
        schema_edit_message_ = "The declaration no longer exists.";
        return false;
    }

    auto command{std::optional<SchemaEditCommand>{}};
    if (document_->enum_schema(declaration) != nullptr) {
        command = DeleteEnum{.declaration = declaration};
    } else if (document_->integer_scalar_schema(declaration) != nullptr) {
        command = DeleteIntegerScalar{.declaration = declaration};
    } else if (document_->linear_quantized_schema(declaration) != nullptr) {
        command = DeleteLinearQuantized{.declaration = declaration};
    } else if (document_->integer_varint_schema(declaration) != nullptr) {
        command = DeleteIntegerVarint{.declaration = declaration};
    } else if (document_->fixed_point_schema(declaration) != nullptr) {
        command = DeleteFixedPoint{.declaration = declaration};
    } else if (document_->mini_float_schema(declaration) != nullptr) {
        command = DeleteMiniFloat{.declaration = declaration};
    } else if (document_->optional_sentinel_schema(declaration) != nullptr) {
        command = DeleteOptionalSentinel{.declaration = declaration};
    } else if (document_->optional_presence_bit_schema(declaration) != nullptr) {
        command = DeleteOptionalPresenceBit{.declaration = declaration};
    } else if (document_->packed_value_schema(declaration) != nullptr) {
        command = DeletePackedValue{.declaration = declaration};
    } else if (document_->record_schema(declaration) != nullptr) {
        command = DeleteRecord{.declaration = declaration};
    } else if (document_->union_schema(declaration) != nullptr) {
        command = DeleteUnion{.declaration = declaration};
    } else if (document_->tagged_union_schema(declaration) != nullptr) {
        command = DeleteTaggedUnion{.declaration = declaration};
    } else if (document_->soa_schema(declaration) != nullptr) {
        command = DeleteSoa{.declaration = declaration};
    }
    if (!command.has_value()) {
        schema_edit_message_ = "This draft declaration kind cannot be deleted.";
        return false;
    }

    if (!apply_document_edit(std::move(*command))) {
        return false;
    }
    record_access_members_.clear();
    record_access_set_explicit_ = false;
    return true;
}

void PlannerUi::draw_enum_editor(TypeNode const& node, EnumType const&) {
    if (!document_.has_value()) {
        return;
    }
    auto const declaration{document_->find_declaration(node.identity)};
    if (!declaration.has_value()) {
        return;
    }
    auto const* schema{document_->enum_schema(*declaration)};
    if (schema == nullptr) {
        return;
    }

    if (selected_enumerator_.empty() && !schema->values.empty()) {
        selected_enumerator_ = schema->values.front().name;
    }
    auto selected{
        std::ranges::find(schema->values, selected_enumerator_, &codegen::EnumeratorSchema::name)};
    if (selected == schema->values.end() && !schema->values.empty()) {
        selected = schema->values.begin();
        selected_enumerator_ = selected->name;
    }
    auto const selected_index{selected == schema->values.end()
                                  ? std::optional<std::size_t>{}
                                  : std::optional<std::size_t>{static_cast<std::size_t>(
                                        selected - schema->values.begin())}};
    auto const enum_declaration_changed{enum_editor_declaration_ != declaration};
    if (enum_declaration_changed || enum_editor_value_ != selected_enumerator_) {
        enum_editor_declaration_ = declaration;
        enum_editor_value_ = selected_enumerator_;
        std::snprintf(enum_underlying_type_.data(),
                      enum_underlying_type_.size(),
                      "%s",
                      schema->underlying_type.has_value() ? schema->underlying_type->name.c_str()
                                                          : "std::uint8_t");
        std::snprintf(enum_export_specifier_.data(),
                      enum_export_specifier_.size(),
                      "%s",
                      schema->export_specifier.value_or("").c_str());
        if (enum_declaration_changed) {
            auto const* projection{
                schema->unreal_projection.has_value() ? &*schema->unreal_projection : nullptr};
            std::snprintf(enum_projection_name_.data(),
                          enum_projection_name_.size(),
                          "%s",
                          projection != nullptr ? projection->name.c_str() : "");
            std::snprintf(enum_projection_header_.data(),
                          enum_projection_header_.size(),
                          "%s",
                          projection != nullptr ? projection->header.string().c_str() : "");
            std::snprintf(enum_projection_header_include_.data(),
                          enum_projection_header_include_.size(),
                          "%s",
                          projection != nullptr ? projection->header_include.c_str() : "");
            std::snprintf(enum_projection_conversion_header_.data(),
                          enum_projection_conversion_header_.size(),
                          "%s",
                          projection != nullptr ? projection->conversion_header.string().c_str()
                                                : "");
            std::snprintf(enum_projection_native_header_include_.data(),
                          enum_projection_native_header_include_.size(),
                          "%s",
                          projection != nullptr ? projection->native_header_include.c_str() : "");
            auto const reflection{
                projection != nullptr
                    ? std::ranges::find(enum_projection_reflections, projection->reflection)
                    : enum_projection_reflections.begin()};
            enum_projection_reflection_ =
                reflection == enum_projection_reflections.end()
                    ? 0
                    : static_cast<int>(reflection - enum_projection_reflections.begin());
        }
        if (selected != schema->values.end()) {
            std::snprintf(
                enum_value_name_.data(), enum_value_name_.size(), "%s", selected->name.c_str());
            set_buffer(enum_value_initializer_, selected->initializer);
            set_buffer(enum_value_display_name_, selected->display_name);
            set_buffer(enum_value_serialized_name_, selected->serialized_name);
        }
    }

    auto automatic_backing{!schema->underlying_type.has_value()};
    if (ImGui::Checkbox("Auto C++ backing", &automatic_backing)) {
        auto replacement{*schema};
        replacement.underlying_type =
            automatic_backing
                ? std::optional<codegen::TypeRef>{}
                : std::optional{codegen::TypeRef{
                      .name = enum_underlying_type_.data(), .suffix = {}, .nested = std::nullopt}};
        static_cast<void>(apply_document_edit(
            ReplaceEnum{.declaration = *declaration, .schema = std::move(replacement)}));
        return;
    }
    if (schema->underlying_type.has_value()) {
        auto const submitted{ImGui::InputText("C++ backing type",
                                              enum_underlying_type_.data(),
                                              enum_underlying_type_.size(),
                                              ImGuiInputTextFlags_EnterReturnsTrue)};
        if (submitted || ImGui::IsItemDeactivatedAfterEdit()) {
            if (enum_underlying_type_.front() == '\0') {
                schema_edit_message_ = "Explicit enum C++ backing type cannot be empty.";
            } else if (schema->underlying_type->name != enum_underlying_type_.data()) {
                auto replacement{*schema};
                replacement.underlying_type = codegen::TypeRef{
                    .name = enum_underlying_type_.data(), .suffix = {}, .nested = std::nullopt};
                static_cast<void>(apply_document_edit(
                    ReplaceEnum{.declaration = *declaration, .schema = std::move(replacement)}));
            }
            return;
        }
    } else if (enum_domain_.has_value()) {
        ImGui::TextDisabled("Derived for C++: %s", enum_domain_->backing_type.c_str());
    }

    auto automatic_width{!schema->bit_width.has_value()};
    if (ImGui::Checkbox("Auto semantic width", &automatic_width)) {
        auto replacement{*schema};
        if (automatic_width) {
            replacement.bit_width.reset();
        } else {
            replacement.bit_width = enum_domain_.has_value()
                                      ? enum_domain_->minimum_required_bits.value_or(1)
                                      : std::uint32_t{1};
        }
        static_cast<void>(apply_document_edit(
            ReplaceEnum{.declaration = *declaration, .schema = std::move(replacement)}));
        return;
    }
    if (schema->bit_width.has_value()) {
        auto bit_width{*schema->bit_width};
        ImGui::SetNextItemWidth(140.0F);
        auto const submitted{ImGui::InputScalar("Semantic width",
                                                ImGuiDataType_U32,
                                                &bit_width,
                                                nullptr,
                                                nullptr,
                                                "%u",
                                                ImGuiInputTextFlags_EnterReturnsTrue)};
        if (submitted || ImGui::IsItemDeactivatedAfterEdit()) {
            if (bit_width == 0 || bit_width > 64) {
                schema_edit_message_ = "Enum semantic width must be between 1 and 64 bits.";
            } else {
                auto replacement{*schema};
                replacement.bit_width = bit_width;
                static_cast<void>(apply_document_edit(
                    ReplaceEnum{.declaration = *declaration, .schema = std::move(replacement)}));
            }
            return;
        }
    }
    auto signedness_mode{schema->signedness.has_value() ? (*schema->signedness ? 2 : 1) : 0};
    constexpr std::array signedness_labels{"Auto / inferred", "Unsigned", "Signed"};
    ImGui::SetNextItemWidth(180.0F);
    if (ImGui::Combo("Semantic signedness",
                     &signedness_mode,
                     signedness_labels.data(),
                     static_cast<int>(signedness_labels.size()))) {
        auto replacement{*schema};
        replacement.signedness = signedness_mode == 0 ? std::optional<bool>{}
                               : signedness_mode == 2 ? std::optional<bool>{true}
                                                      : std::optional<bool>{false};
        static_cast<void>(apply_document_edit(
            ReplaceEnum{.declaration = *declaration, .schema = std::move(replacement)}));
        return;
    }
    ImGui::TextDisabled("Semantic width describes the value domain; C++ backing storage is a "
                        "separate lowering choice.");

    ImGui::SeparatorText("Generation policy");
    auto reflection_index{static_cast<int>(std::ranges::find(enum_reflections, schema->reflection) -
                                           enum_reflections.begin())};
    if (reflection_index < 0 || reflection_index >= static_cast<int>(enum_reflections.size())) {
        reflection_index = 0;
    }
    ImGui::SetNextItemWidth(180.0F);
    if (ImGui::BeginCombo("Reflection",
                          codegen::enum_reflection_name(
                              enum_reflections[static_cast<std::size_t>(reflection_index)])
                              .data())) {
        for (std::size_t index{}; index < enum_reflections.size(); ++index) {
            auto const reflection{enum_reflections[index]};
            auto const selected_reflection{index == static_cast<std::size_t>(reflection_index)};
            if (ImGui::Selectable(codegen::enum_reflection_name(reflection).data(),
                                  selected_reflection)) {
                auto replacement{*schema};
                replacement.reflection = reflection;
                ImGui::EndCombo();
                static_cast<void>(apply_document_edit(
                    ReplaceEnum{.declaration = *declaration, .schema = std::move(replacement)}));
                return;
            }
            if (selected_reflection) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    auto enum_array{schema->enum_array};
    if (ImGui::Checkbox("Generate enum-array helpers", &enum_array)) {
        auto replacement{*schema};
        replacement.enum_array = enum_array;
        static_cast<void>(apply_document_edit(
            ReplaceEnum{.declaration = *declaration, .schema = std::move(replacement)}));
        return;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Requires a valid final count-sentinel enumerator and implicit values.");
    }

    auto native_api{schema->native_api};
    if (ImGui::Checkbox("Use native enum API", &native_api)) {
        auto replacement{*schema};
        replacement.native_api = native_api;
        static_cast<void>(apply_document_edit(
            ReplaceEnum{.declaration = *declaration, .schema = std::move(replacement)}));
        return;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(
            "Native API mode is incompatible with reflection, enum arrays, conversions, and an "
            "export specifier.");
    }

    ImGui::SetNextItemWidth(-1.0F);
    auto const export_submitted{ImGui::InputText("Export specifier (optional)##enum",
                                                 enum_export_specifier_.data(),
                                                 enum_export_specifier_.size(),
                                                 ImGuiInputTextFlags_EnterReturnsTrue)};
    if (export_submitted || ImGui::IsItemDeactivatedAfterEdit()) {
        auto const export_specifier{optional_text(enum_export_specifier_)};
        if (schema->export_specifier != export_specifier) {
            auto replacement{*schema};
            replacement.export_specifier = export_specifier;
            static_cast<void>(apply_document_edit(
                ReplaceEnum{.declaration = *declaration, .schema = std::move(replacement)}));
        }
        return;
    }

    ImGui::SeparatorText("Unreal projection");
    std::optional<codegen::EnumSchema> projection_edit;
    auto const projection_exists{schema->unreal_projection.has_value()};
    auto edit_projection_text = [&](char const* label, auto& buffer, auto&& assign) {
        ImGui::SetNextItemWidth(-1.0F);
        auto const submitted{ImGui::InputText(
            label, buffer.data(), buffer.size(), ImGuiInputTextFlags_EnterReturnsTrue)};
        if (projection_exists && (submitted || ImGui::IsItemDeactivatedAfterEdit())) {
            if (!projection_edit.has_value()) {
                projection_edit = *schema;
            }
            assign(*projection_edit->unreal_projection, buffer.data());
        }
    };
    edit_projection_text("Projected enum name",
                         enum_projection_name_,
                         [](auto& projection, char const* value) { projection.name = value; });
    edit_projection_text(
        "Generated header path", enum_projection_header_, [](auto& projection, char const* value) {
            projection.header = std::filesystem::path{value};
        });
    edit_projection_text(
        "Generated header include",
        enum_projection_header_include_,
        [](auto& projection, char const* value) { projection.header_include = value; });
    edit_projection_text("Conversion header path",
                         enum_projection_conversion_header_,
                         [](auto& projection, char const* value) {
                             projection.conversion_header = std::filesystem::path{value};
                         });
    edit_projection_text(
        "Native header include",
        enum_projection_native_header_include_,
        [](auto& projection, char const* value) { projection.native_header_include = value; });

    enum_projection_reflection_ = std::clamp(
        enum_projection_reflection_, 0, static_cast<int>(enum_projection_reflections.size() - 1));
    auto const selected_projection_reflection{
        enum_projection_reflections[static_cast<std::size_t>(enum_projection_reflection_)]};
    ImGui::SetNextItemWidth(180.0F);
    if (ImGui::BeginCombo("Projection reflection",
                          codegen::enum_reflection_name(selected_projection_reflection).data())) {
        for (std::size_t index{}; index < enum_projection_reflections.size(); ++index) {
            auto const reflection{enum_projection_reflections[index]};
            auto const selected_reflection{enum_projection_reflection_ == static_cast<int>(index)};
            if (ImGui::Selectable(codegen::enum_reflection_name(reflection).data(),
                                  selected_reflection)) {
                enum_projection_reflection_ = static_cast<int>(index);
                if (projection_exists) {
                    projection_edit = *schema;
                    projection_edit->unreal_projection->reflection = reflection;
                }
            }
            if (selected_reflection) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    if (projection_edit.has_value()) {
        static_cast<void>(apply_document_edit(
            ReplaceEnum{.declaration = *declaration, .schema = std::move(*projection_edit)}));
        return;
    }

    if (projection_exists) {
        if (ImGui::Button("Remove Unreal projection")) {
            auto replacement{*schema};
            replacement.unreal_projection.reset();
            static_cast<void>(apply_document_edit(
                ReplaceEnum{.declaration = *declaration, .schema = std::move(replacement)}));
            return;
        }
    } else {
        auto const fields_complete{enum_projection_name_.front() != '\0' &&
                                   enum_projection_header_.front() != '\0' &&
                                   enum_projection_header_include_.front() != '\0' &&
                                   enum_projection_conversion_header_.front() != '\0' &&
                                   enum_projection_native_header_include_.front() != '\0'};
        ImGui::BeginDisabled(!schema->native_api || !fields_complete);
        if (ImGui::Button("Add Unreal projection")) {
            auto replacement{*schema};
            replacement.unreal_projection = codegen::EnumUnrealProjection{
                .name = enum_projection_name_.data(),
                .header = std::filesystem::path{enum_projection_header_.data()},
                .header_include = enum_projection_header_include_.data(),
                .conversion_header =
                    std::filesystem::path{enum_projection_conversion_header_.data()},
                .native_header_include = enum_projection_native_header_include_.data(),
                .reflection = selected_projection_reflection};
            static_cast<void>(apply_document_edit(
                ReplaceEnum{.declaration = *declaration, .schema = std::move(replacement)}));
            return;
        }
        ImGui::EndDisabled();
        if (!schema->native_api) {
            ImGui::TextDisabled("A projection requires native enum API mode.");
        } else if (!fields_complete) {
            ImGui::TextDisabled("Fill every projection field before adding it.");
        }
    }
    ImGui::TextDisabled("Projection files are generated consumer outputs; semantic enum width and "
                        "target layout remain unchanged.");

    ImGui::SeparatorText("Generated conversions");
    if (ImGui::BeginTable("enum-conversions", 2, ImGuiTableFlags_SizingStretchSame)) {
        for (auto const conversion : enum_conversions) {
            auto const descriptor{enum_conversion_descriptor(conversion)};
            ImGui::TableNextColumn();
            auto enabled{has_enum_conversion(schema->conversions, conversion)};
            if (ImGui::Checkbox(descriptor.source_name.data(), &enabled)) {
                auto replacement{*schema};
                replacement.conversions =
                    with_enum_conversion(schema->conversions, conversion, enabled);
                ImGui::EndTable();
                static_cast<void>(apply_document_edit(
                    ReplaceEnum{.declaration = *declaration, .schema = std::move(replacement)}));
                return;
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", descriptor.description);
            }
        }
        ImGui::EndTable();
    }

    if (ImGui::Button("+ Enumerator")) {
        auto replacement{*schema};
        auto suffix{replacement.values.size()};
        std::string name;
        do {
            name = "Value" + std::to_string(suffix++);
        } while (std::ranges::find(replacement.values, name, &codegen::EnumeratorSchema::name) !=
                 replacement.values.end());
        auto insertion_index{replacement.values.size()};
        if (replacement.count.has_value()) {
            auto const count{std::ranges::find(
                replacement.values, *replacement.count, &codegen::EnumeratorSchema::name)};
            if (count != replacement.values.end()) {
                insertion_index = static_cast<std::size_t>(count - replacement.values.begin());
            }
        }
        replacement.values.insert(replacement.values.begin() +
                                      static_cast<std::ptrdiff_t>(insertion_index),
                                  {.name = name,
                                   .initializer = std::nullopt,
                                   .display_name = std::nullopt,
                                   .hidden = false,
                                   .serialized_name = std::nullopt});
        if (apply_document_edit(
                ReplaceEnum{.declaration = *declaration, .schema = std::move(replacement)})) {
            selected_enumerator_ = std::move(name);
            return;
        }
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!selected_index.has_value());
    if (ImGui::Button("Duplicate")) {
        auto replacement{*schema};
        auto copy{replacement.values[*selected_index]};
        auto suffix{std::size_t{1}};
        auto const stem{copy.name + "_copy"};
        copy.name = stem;
        while (std::ranges::find(replacement.values, copy.name, &codegen::EnumeratorSchema::name) !=
               replacement.values.end()) {
            copy.name = stem + std::to_string(suffix++);
        }
        auto const insertion_index{schema->count == replacement.values[*selected_index].name
                                       ? *selected_index
                                       : *selected_index + 1};
        replacement.values.insert(
            replacement.values.begin() + static_cast<std::ptrdiff_t>(insertion_index), copy);
        if (apply_document_edit(
                ReplaceEnum{.declaration = *declaration, .schema = std::move(replacement)})) {
            selected_enumerator_ = std::move(copy.name);
            return;
        }
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!selected_index.has_value() || *selected_index == 0);
    if (ImGui::Button("Move up")) {
        auto replacement{*schema};
        std::swap(replacement.values[*selected_index], replacement.values[*selected_index - 1]);
        if (apply_document_edit(
                ReplaceEnum{.declaration = *declaration, .schema = std::move(replacement)})) {
            selected_enumerator_ = enum_editor_value_;
            return;
        }
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!selected_index.has_value() ||
                         *selected_index + 1 >= schema->values.size());
    if (ImGui::Button("Move down")) {
        auto replacement{*schema};
        std::swap(replacement.values[*selected_index], replacement.values[*selected_index + 1]);
        if (apply_document_edit(
                ReplaceEnum{.declaration = *declaration, .schema = std::move(replacement)})) {
            selected_enumerator_ = enum_editor_value_;
            return;
        }
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!selected_index.has_value() || schema->values.size() == 1);
    if (ImGui::Button("Delete")) {
        auto replacement{*schema};
        auto const deleted_name{replacement.values[*selected_index].name};
        replacement.values.erase(replacement.values.begin() +
                                 static_cast<std::ptrdiff_t>(*selected_index));
        if (replacement.count == deleted_name) {
            replacement.count.reset();
        }
        auto const next_index{std::min(*selected_index, replacement.values.size() - 1)};
        auto const next_name{replacement.values[next_index].name};
        if (apply_document_edit(
                ReplaceEnum{.declaration = *declaration, .schema = std::move(replacement)})) {
            selected_enumerator_ = next_name;
            return;
        }
    }
    ImGui::EndDisabled();
    ImGui::EndDisabled();

    std::optional<codegen::EnumSchema> pending;
    auto selected_after_edit{selected_enumerator_};
    if (ImGui::BeginTable("enumerators",
                          9,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                              ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Edit", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Name");
        ImGui::TableSetupColumn("Initializer");
        ImGui::TableSetupColumn("Derived code");
        ImGui::TableSetupColumn("Display");
        ImGui::TableSetupColumn("Serialized");
        ImGui::TableSetupColumn("Hidden", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Sentinel", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Count", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableHeadersRow();
        for (std::size_t index{}; index < schema->values.size(); ++index) {
            auto const& value{schema->values[index]};
            auto const row_selected{selected_enumerator_ == value.name};
            ImGui::PushID(static_cast<int>(index));
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            if (ImGui::Selectable("::", row_selected, ImGuiSelectableFlags_SpanAllColumns)) {
                selected_enumerator_ = value.name;
                enum_editor_declaration_.reset();
            }
            if (ImGui::BeginDragDropSource()) {
                ImGui::SetDragDropPayload("ENUMERATOR_ROW", &index, sizeof(index));
                ImGui::Text("Move %s", value.name.c_str());
                ImGui::EndDragDropSource();
            }
            if (ImGui::BeginDragDropTarget()) {
                if (auto const* payload{ImGui::AcceptDragDropPayload("ENUMERATOR_ROW")}) {
                    auto const source_index{*static_cast<std::size_t const*>(payload->Data)};
                    if (source_index < schema->values.size() && source_index != index) {
                        pending = *schema;
                        selected_after_edit = pending->values[source_index].name;
                        move_element(pending->values, source_index, index);
                    }
                }
                ImGui::EndDragDropTarget();
            }

            ImGui::TableNextColumn();
            if (row_selected) {
                ImGui::SetNextItemWidth(-1.0F);
                auto const submitted{ImGui::InputText("##name",
                                                      enum_value_name_.data(),
                                                      enum_value_name_.size(),
                                                      ImGuiInputTextFlags_EnterReturnsTrue)};
                if (submitted || ImGui::IsItemDeactivatedAfterEdit()) {
                    pending = *schema;
                    auto& edited{pending->values[index]};
                    auto const previous_name{edited.name};
                    edited.name = enum_value_name_.data();
                    if (pending->count == previous_name) {
                        pending->count = edited.name;
                    }
                    selected_after_edit = edited.name;
                }
            } else {
                ImGui::TextUnformatted(value.name.c_str());
            }

            auto draw_optional_editor = [&](char const* label,
                                            auto& buffer,
                                            std::optional<std::string> codegen::EnumeratorSchema::*
                                                member) {
                ImGui::TableNextColumn();
                if (row_selected) {
                    ImGui::SetNextItemWidth(-1.0F);
                    auto const submitted{ImGui::InputText(
                        label, buffer.data(), buffer.size(), ImGuiInputTextFlags_EnterReturnsTrue)};
                    if (!pending.has_value() &&
                        (submitted || ImGui::IsItemDeactivatedAfterEdit())) {
                        pending = *schema;
                        pending->values[index].*member = optional_text(buffer);
                    }
                } else {
                    draw_optional_text(value.*member);
                }
            };
            draw_optional_editor(
                "##value", enum_value_initializer_, &codegen::EnumeratorSchema::initializer);
            ImGui::TableNextColumn();
            if (enum_domain_.has_value() && index < enum_domain_->enumerators.size() &&
                enum_domain_->enumerators[index].code.has_value()) {
                auto const code{
                    lispb::schema::format_enum_code(*enum_domain_->enumerators[index].code)};
                ImGui::TextUnformatted(code.c_str());
            } else {
                ImGui::TextDisabled("Unknown");
            }
            draw_optional_editor(
                "##display", enum_value_display_name_, &codegen::EnumeratorSchema::display_name);
            draw_optional_editor("##serialized",
                                 enum_value_serialized_name_,
                                 &codegen::EnumeratorSchema::serialized_name);

            ImGui::TableNextColumn();
            auto hidden{value.hidden};
            if (row_selected) {
                if (ImGui::Checkbox("##hidden", &hidden)) {
                    pending = *schema;
                    pending->values[index].hidden = hidden;
                }
            } else {
                ImGui::TextUnformatted(hidden ? "yes" : "-");
            }

            ImGui::TableNextColumn();
            auto sentinel{value.sentinel};
            if (row_selected) {
                if (ImGui::Checkbox("##sentinel", &sentinel)) {
                    pending = *schema;
                    pending->values[index].sentinel = sentinel;
                }
            } else {
                ImGui::TextUnformatted(sentinel ? "yes" : "-");
            }

            ImGui::TableNextColumn();
            auto count_sentinel{schema->count == value.name};
            if (row_selected) {
                if (ImGui::Checkbox("##count", &count_sentinel)) {
                    pending = *schema;
                    if (count_sentinel) {
                        pending->count = value.name;
                    } else if (pending->count == value.name) {
                        pending->count.reset();
                    }
                }
            } else {
                ImGui::TextUnformatted(count_sentinel ? "yes" : "-");
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    if (pending.has_value()) {
        if (pending->values[*selected_index].name.empty()) {
            schema_edit_message_ = "Enumerator name cannot be empty.";
            return;
        }
        if (apply_document_edit(
                ReplaceEnum{.declaration = *declaration, .schema = std::move(*pending)})) {
            selected_enumerator_ = std::move(selected_after_edit);
        }
    }
}

auto PlannerUi::draw_integer_scalar_editor(TypeNode const& node, IntegerScalarType const& scalar)
    -> bool {
    if (!document_.has_value()) {
        return false;
    }
    auto const declaration{document_->find_declaration(node.identity)};
    if (!declaration.has_value()) {
        return false;
    }
    auto const* schema{document_->integer_scalar_schema(*declaration)};
    if (schema == nullptr) {
        return false;
    }

    if (selected_integer_scalar_code_.empty() && !schema->named_codes.empty()) {
        selected_integer_scalar_code_ = schema->named_codes.front().name;
    }
    auto selected{std::ranges::find(
        schema->named_codes, selected_integer_scalar_code_, &codegen::PackedNamedCodeSchema::name)};
    if (selected == schema->named_codes.end() && !schema->named_codes.empty()) {
        selected = schema->named_codes.begin();
        selected_integer_scalar_code_ = selected->name;
    }
    auto const selected_index{selected == schema->named_codes.end()
                                  ? std::optional<std::size_t>{}
                                  : std::optional<std::size_t>{static_cast<std::size_t>(
                                        selected - schema->named_codes.begin())}};
    if (integer_scalar_editor_declaration_ != declaration) {
        integer_scalar_editor_declaration_ = declaration;
        integer_scalar_editor_code_.clear();
        std::snprintf(integer_scalar_minimum_.data(),
                      integer_scalar_minimum_.size(),
                      "%s",
                      codegen::format_packed_integer(schema->minimum_value).c_str());
        std::snprintf(integer_scalar_maximum_.data(),
                      integer_scalar_maximum_.size(),
                      "%s",
                      codegen::format_packed_integer(schema->maximum_value).c_str());
        std::snprintf(integer_scalar_cpp_type_.data(),
                      integer_scalar_cpp_type_.size(),
                      "%s",
                      schema->cpp_type.has_value() ? schema->cpp_type->name.c_str() : "");
        if (schema->relationship.has_value()) {
            std::snprintf(integer_scalar_relationship_target_.data(),
                          integer_scalar_relationship_target_.size(),
                          "%s",
                          schema->relationship->target.name.c_str());
            auto const kind{
                std::ranges::find(semantic_relationship_kinds, schema->relationship->kind)};
            integer_scalar_relationship_kind_ =
                kind == semantic_relationship_kinds.end()
                    ? 0
                    : static_cast<int>(kind - semantic_relationship_kinds.begin());
            auto const unit{
                schema->relationship->unit.has_value()
                    ? std::ranges::find(semantic_relationship_units, *schema->relationship->unit)
                    : semantic_relationship_units.end()};
            integer_scalar_relationship_unit_ =
                unit == semantic_relationship_units.end()
                    ? 0
                    : static_cast<int>(unit - semantic_relationship_units.begin());
        } else {
            integer_scalar_relationship_target_.front() = '\0';
            integer_scalar_relationship_kind_ = 0;
            integer_scalar_relationship_unit_ = 0;
        }
    }
    if (integer_scalar_editor_code_ != selected_integer_scalar_code_) {
        integer_scalar_editor_code_ = selected_integer_scalar_code_;
        if (selected != schema->named_codes.end()) {
            std::snprintf(integer_scalar_code_name_.data(),
                          integer_scalar_code_name_.size(),
                          "%s",
                          selected->name.c_str());
            std::snprintf(integer_scalar_code_value_.data(),
                          integer_scalar_code_value_.size(),
                          "%s",
                          codegen::format_packed_integer(selected->value).c_str());
            integer_scalar_code_sentinel_ = selected->sentinel;
        }
    }

    auto signedness{schema->signedness};
    if (ImGui::Checkbox("Signed domain", &signedness)) {
        auto replacement{*schema};
        replacement.signedness = signedness;
        if (apply_document_edit(ReplaceIntegerScalar{.declaration = *declaration,
                                                     .schema = std::move(replacement)})) {
            integer_scalar_editor_declaration_.reset();
            return true;
        }
    }
    auto automatic_width{!schema->bit_width.has_value()};
    if (ImGui::Checkbox("Auto bit width", &automatic_width)) {
        auto replacement{*schema};
        replacement.bit_width =
            automatic_width ? std::nullopt
                            : std::optional{integer_scalar_analysis_.has_value()
                                                ? integer_scalar_analysis_->minimum_required_bits
                                                : std::uint32_t{1}};
        if (apply_document_edit(ReplaceIntegerScalar{.declaration = *declaration,
                                                     .schema = std::move(replacement)})) {
            integer_scalar_editor_declaration_.reset();
            return true;
        }
    }
    if (schema->bit_width.has_value()) {
        auto bit_width{*schema->bit_width};
        ImGui::SetNextItemWidth(140.0F);
        auto const submitted{ImGui::InputScalar("Bit width",
                                                ImGuiDataType_U32,
                                                &bit_width,
                                                nullptr,
                                                nullptr,
                                                "%u",
                                                ImGuiInputTextFlags_EnterReturnsTrue)};
        if (submitted || ImGui::IsItemDeactivatedAfterEdit()) {
            if (bit_width == 0 || bit_width > 64) {
                schema_edit_message_ = "Integer scalar width must be between 1 and 64 bits.";
            } else {
                auto replacement{*schema};
                replacement.bit_width = bit_width;
                if (apply_document_edit(ReplaceIntegerScalar{.declaration = *declaration,
                                                             .schema = std::move(replacement)})) {
                    integer_scalar_editor_declaration_.reset();
                    return true;
                }
            }
        }
    }

    ImGui::SetNextItemWidth(130.0F);
    auto range_submitted{ImGui::InputText("Minimum",
                                          integer_scalar_minimum_.data(),
                                          integer_scalar_minimum_.size(),
                                          ImGuiInputTextFlags_EnterReturnsTrue)};
    range_submitted = range_submitted || ImGui::IsItemDeactivatedAfterEdit();
    ImGui::SetNextItemWidth(130.0F);
    auto const maximum_submitted{ImGui::InputText("Maximum",
                                                  integer_scalar_maximum_.data(),
                                                  integer_scalar_maximum_.size(),
                                                  ImGuiInputTextFlags_EnterReturnsTrue)};
    range_submitted = range_submitted || maximum_submitted || ImGui::IsItemDeactivatedAfterEdit();
    if (range_submitted) {
        auto const minimum{detail::parse_packed_integer(integer_scalar_minimum_.data())};
        auto const maximum{detail::parse_packed_integer(integer_scalar_maximum_.data())};
        if (!minimum.has_value() || !maximum.has_value()) {
            schema_edit_message_ =
                "Scalar bounds must be signed decimal or hexadecimal integer values.";
        } else {
            auto replacement{*schema};
            replacement.minimum_value = *minimum;
            replacement.maximum_value = *maximum;
            if (apply_document_edit(ReplaceIntegerScalar{.declaration = *declaration,
                                                         .schema = std::move(replacement)})) {
                integer_scalar_editor_declaration_.reset();
                return true;
            }
        }
    }
    ImGui::TextDisabled(
        "This is a semantic domain. A packed field or future representation chooses storage.");

    ImGui::SeparatorText("C++ output policy");
    auto emit_cpp_constants{schema->cpp_emission != codegen::IntegerScalarCppEmission::none};
    if (ImGui::Checkbox("Emit named constants", &emit_cpp_constants)) {
        auto replacement{*schema};
        replacement.cpp_emission = emit_cpp_constants ? codegen::IntegerScalarCppEmission::constants
                                                      : codegen::IntegerScalarCppEmission::none;
        replacement.cpp_type =
            emit_cpp_constants ? std::optional{codegen::TypeRef{
                                     .name = schema->signedness ? "std::int64_t" : "std::uint64_t",
                                     .suffix = {},
                                     .nested = std::nullopt}}
                               : std::nullopt;
        if (apply_document_edit(ReplaceIntegerScalar{.declaration = *declaration,
                                                     .schema = std::move(replacement)})) {
            integer_scalar_editor_declaration_.reset();
            return true;
        }
    }
    if (emit_cpp_constants) {
        ImGui::SetNextItemWidth(220.0F);
        auto const submitted{ImGui::InputText("Constants type",
                                              integer_scalar_cpp_type_.data(),
                                              integer_scalar_cpp_type_.size(),
                                              ImGuiInputTextFlags_EnterReturnsTrue)};
        if (submitted || ImGui::IsItemDeactivatedAfterEdit()) {
            if (integer_scalar_cpp_type_.front() == '\0') {
                schema_edit_message_ = "C++ constants type cannot be empty.";
            } else if (!schema->cpp_type.has_value() ||
                       schema->cpp_type->name != integer_scalar_cpp_type_.data()) {
                auto replacement{*schema};
                replacement.cpp_type = codegen::TypeRef{
                    .name = integer_scalar_cpp_type_.data(), .suffix = {}, .nested = std::nullopt};
                if (apply_document_edit(ReplaceIntegerScalar{.declaration = *declaration,
                                                             .schema = std::move(replacement)})) {
                    integer_scalar_editor_declaration_.reset();
                    return true;
                }
            }
        }
        auto emit_name_lookup{schema->cpp_emission ==
                              codegen::IntegerScalarCppEmission::constants_with_names};
        if (ImGui::Checkbox("Emit value-to-name lookup", &emit_name_lookup)) {
            auto replacement{*schema};
            replacement.cpp_emission = emit_name_lookup
                                         ? codegen::IntegerScalarCppEmission::constants_with_names
                                         : codegen::IntegerScalarCppEmission::constants;
            if (apply_document_edit(ReplaceIntegerScalar{.declaration = *declaration,
                                                         .schema = std::move(replacement)})) {
                integer_scalar_editor_declaration_.reset();
                return true;
            }
        }
        ImGui::TextDisabled("Named codes emit as <Scalar>_<Code>; lookup returns an empty view for "
                            "unnamed values.");
    } else {
        ImGui::TextDisabled("No C++ scalar type or constants are emitted for this domain.");
    }

    ImGui::SeparatorText("Semantic relationship");
    auto const current_kind{semantic_relationship_kinds[static_cast<std::size_t>(
        std::clamp(integer_scalar_relationship_kind_,
                   0,
                   static_cast<int>(semantic_relationship_kinds.size() - 1)))]};
    auto const current_unit{semantic_relationship_units[static_cast<std::size_t>(
        std::clamp(integer_scalar_relationship_unit_,
                   0,
                   static_cast<int>(semantic_relationship_units.size() - 1)))]};
    ImGui::SetNextItemWidth(180.0F);
    if (ImGui::BeginCombo("Kind", codegen::semantic_relation_kind_name(current_kind).data())) {
        for (std::size_t kind_index{}; kind_index < semantic_relationship_kinds.size();
             ++kind_index) {
            auto const kind{semantic_relationship_kinds[kind_index]};
            auto const chosen{integer_scalar_relationship_kind_ == static_cast<int>(kind_index)};
            if (ImGui::Selectable(codegen::semantic_relation_kind_name(kind).data(), chosen)) {
                integer_scalar_relationship_kind_ = static_cast<int>(kind_index);
                if (schema->relationship.has_value()) {
                    auto replacement{*schema};
                    replacement.relationship->kind = kind;
                    replacement.relationship->unit =
                        kind == codegen::SemanticRelationKind::offset_into
                            ? std::optional{current_unit}
                            : std::nullopt;
                    if (apply_document_edit(ReplaceIntegerScalar{
                            .declaration = *declaration, .schema = std::move(replacement)})) {
                        integer_scalar_editor_declaration_.reset();
                        ImGui::EndCombo();
                        return true;
                    }
                }
            }
        }
        ImGui::EndCombo();
    }
    if (current_kind == codegen::SemanticRelationKind::offset_into) {
        ImGui::SetNextItemWidth(180.0F);
        if (ImGui::BeginCombo("Unit", codegen::semantic_relation_unit_name(current_unit).data())) {
            for (std::size_t unit_index{}; unit_index < semantic_relationship_units.size();
                 ++unit_index) {
                auto const unit{semantic_relationship_units[unit_index]};
                auto const chosen{integer_scalar_relationship_unit_ ==
                                  static_cast<int>(unit_index)};
                if (ImGui::Selectable(codegen::semantic_relation_unit_name(unit).data(), chosen)) {
                    integer_scalar_relationship_unit_ = static_cast<int>(unit_index);
                    if (schema->relationship.has_value()) {
                        auto replacement{*schema};
                        replacement.relationship->unit = unit;
                        if (apply_document_edit(ReplaceIntegerScalar{
                                .declaration = *declaration, .schema = std::move(replacement)})) {
                            integer_scalar_editor_declaration_.reset();
                            ImGui::EndCombo();
                            return true;
                        }
                    }
                }
            }
            ImGui::EndCombo();
        }
    }

    ImGui::SetNextItemWidth(std::max(80.0F, ImGui::GetContentRegionAvail().x - 132.0F));
    auto const target_submitted{ImGui::InputText("Target",
                                                 integer_scalar_relationship_target_.data(),
                                                 integer_scalar_relationship_target_.size(),
                                                 ImGuiInputTextFlags_EnterReturnsTrue)};
    if (schema->relationship.has_value() &&
        (target_submitted || ImGui::IsItemDeactivatedAfterEdit())) {
        if (integer_scalar_relationship_target_.front() == '\0') {
            schema_edit_message_ = "Relationship target cannot be empty.";
        } else {
            auto replacement{*schema};
            replacement.relationship->target.name = integer_scalar_relationship_target_.data();
            if (apply_document_edit(ReplaceIntegerScalar{.declaration = *declaration,
                                                         .schema = std::move(replacement)})) {
                integer_scalar_editor_declaration_.reset();
                return true;
            }
        }
    }
    ImGui::SameLine();
    ImGui::PushID("integer-scalar-relationship-target");
    auto picked_relationship_target{draw_type_picker(node.identity.module_name, node.identity)};
    ImGui::PopID();
    if (picked_relationship_target.has_value()) {
        std::snprintf(integer_scalar_relationship_target_.data(),
                      integer_scalar_relationship_target_.size(),
                      "%s",
                      picked_relationship_target->c_str());
        auto replacement{*schema};
        replacement.relationship = codegen::SemanticRelationSchema{
            .kind = current_kind,
            .target = codegen::TypeRef{.name = *picked_relationship_target,
                                       .suffix = {},
                                       .nested = std::nullopt},
            .unit = current_kind == codegen::SemanticRelationKind::offset_into
                      ? std::optional{current_unit}
                      : std::nullopt};
        if (apply_document_edit(ReplaceIntegerScalar{.declaration = *declaration,
                                                     .schema = std::move(replacement)})) {
            integer_scalar_editor_declaration_.reset();
            return true;
        }
    }
    ImGui::SameLine();
    if (schema->relationship.has_value()) {
        if (ImGui::SmallButton("Clear")) {
            auto replacement{*schema};
            replacement.relationship.reset();
            if (apply_document_edit(ReplaceIntegerScalar{.declaration = *declaration,
                                                         .schema = std::move(replacement)})) {
                integer_scalar_editor_declaration_.reset();
                return true;
            }
        }
        ImGui::SameLine();
        if (scalar.relationship.has_value() && ImGui::SmallButton(">")) {
            selected_type_ = scalar.relationship->target.type;
            selected_field_.clear();
            return true;
        }
    } else {
        ImGui::BeginDisabled(integer_scalar_relationship_target_.front() == '\0');
        if (ImGui::SmallButton("Add")) {
            auto replacement{*schema};
            replacement.relationship = codegen::SemanticRelationSchema{
                .kind = current_kind,
                .target = codegen::TypeRef{.name = integer_scalar_relationship_target_.data(),
                                           .suffix = {},
                                           .nested = std::nullopt},
                .unit = current_kind == codegen::SemanticRelationKind::offset_into
                          ? std::optional{current_unit}
                          : std::nullopt};
            if (apply_document_edit(ReplaceIntegerScalar{.declaration = *declaration,
                                                         .schema = std::move(replacement)})) {
                integer_scalar_editor_declaration_.reset();
                return true;
            }
        }
        ImGui::EndDisabled();
    }
    ImGui::TextDisabled(
        "The relationship is durable semantic metadata; session capacity does not rewrite this "
        "domain or bit width.");
    if (integer_scalar_analysis_.has_value() &&
        integer_scalar_analysis_->relationship_target_extent.has_value()) {
        auto const& analysis{*integer_scalar_analysis_};
        auto const kind{*analysis.relationship_kind};
        auto const term{detail::relationship_extent_term(kind)};
        auto const unit{detail::relationship_extent_unit(kind, analysis.relationship_unit)};
        auto const heading{"Session " + std::string{term} + " requirement"};
        ImGui::SeparatorText(heading.c_str());
        ImGui::Text("Target %s: %llu %s",
                    term.data(),
                    static_cast<unsigned long long>(*analysis.relationship_target_extent),
                    unit.data());
        ImGui::Text("Live values: %s",
                    analysis.relationship_live_value_count.has_value()
                        ? detail::format_code_count(*analysis.relationship_live_value_count).c_str()
                        : "Unknown");
        auto const required_codes{
            analysis.relationship_required_code_count.has_value()
                ? detail::format_code_count(*analysis.relationship_required_code_count)
            : analysis.relationship_minimum_required_bits.value_or(0) > 64
                ? std::string{"> 2^64"}
                : std::string{"Unknown"}};
        ImGui::Text("Required codes: %s", required_codes.c_str());
        ImGui::Text("Minimum width: %u bits", *analysis.relationship_minimum_required_bits);
        ImGui::Text("Current semantic width fits: %s",
                    *analysis.relationship_width_sufficient ? "Yes" : "No");
        ImGui::Text("Code-space %s limit: %s",
                    term.data(),
                    detail::format_number(analysis.relationship_code_space_capacity_limit).c_str());
        ImGui::Text("Code-space %s headroom: %s",
                    term.data(),
                    detail::format_number(analysis.relationship_capacity_headroom).c_str());
        ImGui::Text("Semantic-range %s limit: %s",
                    term.data(),
                    detail::format_number(analysis.relationship_semantic_capacity_limit).c_str());
        ImGui::Text("Sentinel-placement %s limit: %s",
                    term.data(),
                    detail::format_number(analysis.relationship_sentinel_capacity_limit).c_str());
        ImGui::Text("Effective valid %s limit: %s",
                    term.data(),
                    detail::format_number(analysis.relationship_effective_capacity_limit).c_str());
        ImGui::Text(
            "Effective valid %s headroom: %s",
            term.data(),
            detail::format_number(analysis.relationship_effective_capacity_headroom).c_str());
        if (kind == codegen::SemanticRelationKind::count_of) {
            ImGui::TextDisabled(
                "Live counts include 0 through capacity; named sentinels add code states.");
        } else if (kind == codegen::SemanticRelationKind::offset_into) {
            ImGui::TextDisabled(
                "Live offsets span 0 through extent-1 in the declared unit; named sentinels add "
                "code states.");
        } else {
            ImGui::TextDisabled(
                "Live indices span 0 through capacity-1; named sentinels add code states.");
        }
    }

    auto const code_width{schema->bit_width.value_or(64)};
    auto const named_value{first_available_scalar_code(*schema, code_width, false)};
    auto const sentinel_value{first_available_scalar_code(*schema, code_width, true)};
    ImGui::SeparatorText("Named codes");
    ImGui::BeginDisabled(!named_value.has_value());
    if (ImGui::Button("+ Named code")) {
        auto replacement{*schema};
        auto name{unique_code_name(replacement.named_codes, "Code")};
        replacement.named_codes.push_back({.name = name, .value = *named_value, .sentinel = false});
        if (apply_document_edit(ReplaceIntegerScalar{.declaration = *declaration,
                                                     .schema = std::move(replacement)})) {
            selected_integer_scalar_code_ = std::move(name);
            integer_scalar_editor_declaration_.reset();
            return true;
        }
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!sentinel_value.has_value());
    if (ImGui::Button("+ Sentinel")) {
        auto replacement{*schema};
        auto name{unique_code_name(replacement.named_codes, "Invalid")};
        replacement.named_codes.push_back(
            {.name = name, .value = *sentinel_value, .sentinel = true});
        if (apply_document_edit(ReplaceIntegerScalar{.declaration = *declaration,
                                                     .schema = std::move(replacement)})) {
            selected_integer_scalar_code_ = std::move(name);
            integer_scalar_editor_declaration_.reset();
            return true;
        }
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    auto const duplicate_value{
        selected_index.has_value()
            ? first_available_scalar_code(
                  *schema, code_width, schema->named_codes[*selected_index].sentinel)
            : std::nullopt};
    ImGui::BeginDisabled(!selected_index.has_value() || !duplicate_value.has_value());
    if (ImGui::Button("Duplicate")) {
        auto replacement{*schema};
        auto copy{replacement.named_codes[*selected_index]};
        copy.name = unique_code_name(replacement.named_codes, copy.name + "_copy");
        copy.value = *duplicate_value;
        replacement.named_codes.insert(replacement.named_codes.begin() +
                                           static_cast<std::ptrdiff_t>(*selected_index + 1),
                                       copy);
        if (apply_document_edit(ReplaceIntegerScalar{.declaration = *declaration,
                                                     .schema = std::move(replacement)})) {
            selected_integer_scalar_code_ = std::move(copy.name);
            integer_scalar_editor_declaration_.reset();
            return true;
        }
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!selected_index.has_value() || *selected_index == 0);
    if (ImGui::Button("Move up")) {
        auto replacement{*schema};
        std::swap(replacement.named_codes[*selected_index],
                  replacement.named_codes[*selected_index - 1]);
        if (apply_document_edit(ReplaceIntegerScalar{.declaration = *declaration,
                                                     .schema = std::move(replacement)})) {
            integer_scalar_editor_declaration_.reset();
            return true;
        }
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!selected_index.has_value() ||
                         *selected_index + 1 >= schema->named_codes.size());
    if (ImGui::Button("Move down")) {
        auto replacement{*schema};
        std::swap(replacement.named_codes[*selected_index],
                  replacement.named_codes[*selected_index + 1]);
        if (apply_document_edit(ReplaceIntegerScalar{.declaration = *declaration,
                                                     .schema = std::move(replacement)})) {
            integer_scalar_editor_declaration_.reset();
            return true;
        }
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!selected_index.has_value());
    if (ImGui::Button("Delete")) {
        auto replacement{*schema};
        replacement.named_codes.erase(replacement.named_codes.begin() +
                                      static_cast<std::ptrdiff_t>(*selected_index));
        auto const next_code{
            replacement.named_codes.empty()
                ? std::string{}
                : replacement
                      .named_codes[std::min(*selected_index, replacement.named_codes.size() - 1)]
                      .name};
        if (apply_document_edit(ReplaceIntegerScalar{.declaration = *declaration,
                                                     .schema = std::move(replacement)})) {
            selected_integer_scalar_code_ = next_code;
            integer_scalar_editor_declaration_.reset();
            return true;
        }
    }
    ImGui::EndDisabled();

    std::optional<codegen::IntegerScalarSchema> pending;
    auto selected_after_edit{selected_integer_scalar_code_};
    if (ImGui::BeginTable("integer-scalar-codes",
                          4,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                              ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Edit", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Name");
        ImGui::TableSetupColumn("Value");
        ImGui::TableSetupColumn("Sentinel", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableHeadersRow();
        for (std::size_t index{}; index < schema->named_codes.size(); ++index) {
            auto const& code{schema->named_codes[index]};
            auto const row_selected{selected_integer_scalar_code_ == code.name};
            ImGui::PushID(static_cast<int>(index));
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            if (ImGui::Selectable("::", row_selected, ImGuiSelectableFlags_SpanAllColumns)) {
                selected_integer_scalar_code_ = code.name;
                integer_scalar_editor_code_.clear();
            }
            if (ImGui::BeginDragDropSource()) {
                ImGui::SetDragDropPayload("INTEGER_SCALAR_CODE_ROW", &index, sizeof(index));
                ImGui::Text("Move %s", code.name.c_str());
                ImGui::EndDragDropSource();
            }
            if (ImGui::BeginDragDropTarget()) {
                if (auto const* payload{ImGui::AcceptDragDropPayload("INTEGER_SCALAR_CODE_ROW")}) {
                    auto const source_index{*static_cast<std::size_t const*>(payload->Data)};
                    if (source_index < schema->named_codes.size() && source_index != index) {
                        pending = *schema;
                        selected_after_edit = pending->named_codes[source_index].name;
                        move_element(pending->named_codes, source_index, index);
                    }
                }
                ImGui::EndDragDropTarget();
            }

            ImGui::TableNextColumn();
            if (row_selected) {
                ImGui::SetNextItemWidth(-1.0F);
                auto const submitted{ImGui::InputText("##name",
                                                      integer_scalar_code_name_.data(),
                                                      integer_scalar_code_name_.size(),
                                                      ImGuiInputTextFlags_EnterReturnsTrue)};
                if (!pending.has_value() && (submitted || ImGui::IsItemDeactivatedAfterEdit())) {
                    pending = *schema;
                    pending->named_codes[index].name = integer_scalar_code_name_.data();
                    selected_after_edit = pending->named_codes[index].name;
                }
            } else {
                ImGui::TextUnformatted(code.name.c_str());
            }

            ImGui::TableNextColumn();
            if (row_selected) {
                ImGui::SetNextItemWidth(-1.0F);
                auto const submitted{ImGui::InputText("##value",
                                                      integer_scalar_code_value_.data(),
                                                      integer_scalar_code_value_.size(),
                                                      ImGuiInputTextFlags_EnterReturnsTrue)};
                if (!pending.has_value() && (submitted || ImGui::IsItemDeactivatedAfterEdit())) {
                    if (auto const value{
                            detail::parse_packed_integer(integer_scalar_code_value_.data())}) {
                        pending = *schema;
                        pending->named_codes[index].value = *value;
                    } else {
                        schema_edit_message_ =
                            "Named code value must be a signed decimal or hexadecimal integer.";
                    }
                }
            } else {
                auto const value{codegen::format_packed_integer(code.value)};
                ImGui::TextUnformatted(value.c_str());
            }

            ImGui::TableNextColumn();
            if (row_selected) {
                auto sentinel{integer_scalar_code_sentinel_};
                auto const alternate_value{
                    first_available_scalar_code(*schema, code_width, !code.sentinel)};
                ImGui::BeginDisabled(!alternate_value.has_value());
                if (ImGui::Checkbox("##sentinel", &sentinel)) {
                    pending = *schema;
                    pending->named_codes[index].sentinel = sentinel;
                    pending->named_codes[index].value = *alternate_value;
                }
                ImGui::EndDisabled();
            } else {
                ImGui::TextUnformatted(code.sentinel ? "yes" : "-");
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    if (pending.has_value()) {
        if (std::ranges::any_of(pending->named_codes,
                                [](auto const& code) { return code.name.empty(); })) {
            schema_edit_message_ = "Named code name cannot be empty.";
            return false;
        }
        if (apply_document_edit(
                ReplaceIntegerScalar{.declaration = *declaration, .schema = std::move(*pending)})) {
            selected_integer_scalar_code_ = std::move(selected_after_edit);
            integer_scalar_editor_declaration_.reset();
            return true;
        }
    }
    return false;
}

auto PlannerUi::draw_linear_quantized_editor(TypeNode const& node, LinearQuantizedType const&)
    -> bool {
    if (!document_.has_value()) {
        return false;
    }
    auto const declaration{document_->find_declaration(node.identity)};
    if (!declaration.has_value()) {
        return false;
    }
    auto const* schema{document_->linear_quantized_schema(*declaration)};
    if (schema == nullptr) {
        return false;
    }
    linear_quantized_editor_declaration_ = declaration;

    if (ImGui::BeginCombo("Semantic source", schema->source.name.c_str())) {
        for (auto const& candidate : document_->types().types()) {
            if (!std::holds_alternative<IntegerScalarType>(candidate.definition)) {
                continue;
            }
            auto const selected{candidate.cpp_spelling == schema->source.name};
            if (ImGui::Selectable(candidate.cpp_spelling.c_str(), selected)) {
                auto replacement{*schema};
                replacement.source = codegen::TypeRef{
                    .name = candidate.cpp_spelling, .suffix = {}, .nested = std::nullopt};
                ImGui::EndCombo();
                if (apply_document_edit(ReplaceLinearQuantized{.declaration = *declaration,
                                                               .schema = std::move(replacement)})) {
                    linear_quantized_editor_declaration_.reset();
                    return true;
                }
                return false;
            }
        }
        ImGui::EndCombo();
    }

    auto bit_width{schema->bit_width};
    ImGui::SetNextItemWidth(150.0F);
    auto const width_submitted{ImGui::InputScalar("Encoded bit width",
                                                  ImGuiDataType_U32,
                                                  &bit_width,
                                                  nullptr,
                                                  nullptr,
                                                  "%u",
                                                  ImGuiInputTextFlags_EnterReturnsTrue)};
    if (width_submitted || ImGui::IsItemDeactivatedAfterEdit()) {
        if (bit_width == 0 || bit_width > 64) {
            schema_edit_message_ = "Encoded width must be between 1 and 64 bits.";
        } else {
            auto replacement{*schema};
            replacement.bit_width = bit_width;
            if (apply_document_edit(ReplaceLinearQuantized{.declaration = *declaration,
                                                           .schema = std::move(replacement)})) {
                linear_quantized_editor_declaration_.reset();
                return true;
            }
        }
    }

    auto reserved_codes{schema->reserved_codes};
    ImGui::SetNextItemWidth(180.0F);
    auto const reserved_submitted{ImGui::InputScalar("Reserved codes",
                                                     ImGuiDataType_U64,
                                                     &reserved_codes,
                                                     nullptr,
                                                     nullptr,
                                                     "%llu",
                                                     ImGuiInputTextFlags_EnterReturnsTrue)};
    if (reserved_submitted || ImGui::IsItemDeactivatedAfterEdit()) {
        auto const maximum_reserved{schema->bit_width == 64
                                        ? (std::numeric_limits<std::uint64_t>::max)() - 1
                                        : (std::uint64_t{1} << schema->bit_width) - 2};
        if (reserved_codes > maximum_reserved) {
            schema_edit_message_ = "At least two usable encoded codes are required.";
        } else {
            auto replacement{*schema};
            replacement.reserved_codes = reserved_codes;
            if (apply_document_edit(ReplaceLinearQuantized{.declaration = *declaration,
                                                           .schema = std::move(replacement)})) {
                linear_quantized_editor_declaration_.reset();
                return true;
            }
        }
    }

    auto clipping{schema->clipping == codegen::QuantizationClipping::reject ? 0 : 1};
    constexpr std::array clipping_labels{"Reject", "Clamp"};
    if (ImGui::Combo("Out-of-range values",
                     &clipping,
                     clipping_labels.data(),
                     static_cast<int>(clipping_labels.size()))) {
        auto replacement{*schema};
        replacement.clipping = clipping == 0 ? codegen::QuantizationClipping::reject
                                             : codegen::QuantizationClipping::clamp;
        if (apply_document_edit(ReplaceLinearQuantized{.declaration = *declaration,
                                                       .schema = std::move(replacement)})) {
            linear_quantized_editor_declaration_.reset();
            return true;
        }
    }
    return false;
}

auto PlannerUi::draw_integer_varint_editor(TypeNode const& node, IntegerVarintType const& varint)
    -> bool {
    if (!document_.has_value()) {
        return false;
    }
    auto const declaration{document_->find_declaration(node.identity)};
    if (!declaration.has_value()) {
        return false;
    }
    auto const* schema{document_->integer_varint_schema(*declaration)};
    if (schema == nullptr) {
        return false;
    }
    integer_varint_editor_declaration_ = declaration;

    if (ImGui::BeginCombo("Semantic source", schema->source.name.c_str())) {
        for (auto const& candidate : document_->types().types()) {
            auto const* scalar{std::get_if<IntegerScalarType>(&candidate.definition)};
            if (scalar == nullptr) {
                continue;
            }
            auto const selected{candidate.cpp_spelling == schema->source.name};
            if (ImGui::Selectable(candidate.cpp_spelling.c_str(), selected)) {
                auto replacement{*schema};
                replacement.source = codegen::TypeRef{
                    .name = candidate.cpp_spelling, .suffix = {}, .nested = std::nullopt};
                if (scalar->signedness &&
                    replacement.encoding == codegen::IntegerVarintEncoding::unsigned_varint) {
                    replacement.encoding = codegen::IntegerVarintEncoding::zigzag_varint;
                } else if (!scalar->signedness) {
                    replacement.encoding = codegen::IntegerVarintEncoding::unsigned_varint;
                }
                ImGui::EndCombo();
                if (apply_document_edit(ReplaceIntegerVarint{.declaration = *declaration,
                                                             .schema = std::move(replacement)})) {
                    integer_varint_editor_declaration_.reset();
                    return true;
                }
                return false;
            }
        }
        ImGui::EndCombo();
    }

    auto const& source_scalar{
        std::get<IntegerScalarType>(workspace_.types().type(varint.source.type).definition)};
    auto const current_label{codegen::integer_varint_encoding_name(schema->encoding)};
    if (ImGui::BeginCombo("Encoding", current_label.data())) {
        constexpr std::array encodings{codegen::IntegerVarintEncoding::unsigned_varint,
                                       codegen::IntegerVarintEncoding::signed_varint,
                                       codegen::IntegerVarintEncoding::zigzag_varint};
        for (auto const encoding : encodings) {
            auto const compatible{source_scalar.signedness ==
                                  (encoding != codegen::IntegerVarintEncoding::unsigned_varint)};
            if (!compatible) {
                continue;
            }
            auto const label{codegen::integer_varint_encoding_name(encoding)};
            auto const selected{encoding == schema->encoding};
            if (ImGui::Selectable(label.data(), selected)) {
                auto replacement{*schema};
                replacement.encoding = encoding;
                ImGui::EndCombo();
                if (apply_document_edit(ReplaceIntegerVarint{.declaration = *declaration,
                                                             .schema = std::move(replacement)})) {
                    integer_varint_editor_declaration_.reset();
                    return true;
                }
                return false;
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    return false;
}

auto PlannerUi::draw_optional_sentinel_editor(TypeNode const& node,
                                              OptionalSentinelType const& optional) -> bool {
    if (!document_.has_value()) {
        return false;
    }
    auto const declaration{document_->find_declaration(node.identity)};
    if (!declaration.has_value()) {
        return false;
    }
    auto const* schema{document_->optional_sentinel_schema(*declaration)};
    if (schema == nullptr) {
        return false;
    }
    optional_sentinel_editor_declaration_ = declaration;

    auto const& current_source{workspace_.types().type(optional.source.type)};
    if (ImGui::BeginCombo("Source scalar", current_source.cpp_spelling.c_str())) {
        auto const types{workspace_.types().types()};
        for (std::size_t index{}; index < types.size(); ++index) {
            auto const* candidate{std::get_if<IntegerScalarType>(&types[index].definition)};
            if (candidate == nullptr ||
                !std::ranges::any_of(candidate->named_codes, &PackedNamedCode::sentinel)) {
                continue;
            }
            auto const type{TypeId{static_cast<std::uint32_t>(index)}};
            auto const selected{type == optional.source.type};
            if (ImGui::Selectable(types[index].cpp_spelling.c_str(), selected)) {
                auto replacement{*schema};
                replacement.source = codegen::TypeRef{
                    .name = types[index].cpp_spelling, .suffix = {}, .nested = std::nullopt};
                auto const existing_code{std::ranges::find(
                    candidate->named_codes, replacement.sentinel, &PackedNamedCode::name)};
                if (existing_code == candidate->named_codes.end() || !existing_code->sentinel) {
                    replacement.sentinel =
                        std::ranges::find_if(candidate->named_codes, [](auto const& code) {
                            return code.sentinel;
                        })->name;
                }
                ImGui::EndCombo();
                if (apply_document_edit(ReplaceOptionalSentinel{
                        .declaration = *declaration, .schema = std::move(replacement)})) {
                    optional_sentinel_editor_declaration_.reset();
                    return true;
                }
                return false;
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    auto const& scalar{std::get<IntegerScalarType>(current_source.definition)};
    if (ImGui::BeginCombo("Absence sentinel", schema->sentinel.c_str())) {
        for (auto const& code : scalar.named_codes) {
            if (!code.sentinel) {
                continue;
            }
            auto const selected{code.name == schema->sentinel};
            auto const value{codegen::format_packed_integer(code.value)};
            auto const label{code.name + " = " + value};
            if (ImGui::Selectable(label.c_str(), selected)) {
                auto replacement{*schema};
                replacement.sentinel = code.name;
                ImGui::EndCombo();
                if (apply_document_edit(ReplaceOptionalSentinel{
                        .declaration = *declaration, .schema = std::move(replacement)})) {
                    optional_sentinel_editor_declaration_.reset();
                    return true;
                }
                return false;
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    return false;
}

auto PlannerUi::draw_optional_presence_bit_editor(TypeNode const& node,
                                                  OptionalPresenceBitType const& optional) -> bool {
    if (!document_.has_value()) {
        return false;
    }
    auto const declaration{document_->find_declaration(node.identity)};
    if (!declaration.has_value()) {
        return false;
    }
    auto const* schema{document_->optional_presence_bit_schema(*declaration)};
    if (schema == nullptr) {
        return false;
    }
    optional_presence_bit_editor_declaration_ = declaration;

    auto const& current_source{workspace_.types().type(optional.source.type)};
    if (ImGui::BeginCombo("Source scalar", current_source.cpp_spelling.c_str())) {
        auto const types{workspace_.types().types()};
        for (std::size_t index{}; index < types.size(); ++index) {
            if (!std::holds_alternative<IntegerScalarType>(types[index].definition)) {
                continue;
            }
            auto const type{TypeId{static_cast<std::uint32_t>(index)}};
            auto const selected{type == optional.source.type};
            if (ImGui::Selectable(types[index].cpp_spelling.c_str(), selected)) {
                auto replacement{*schema};
                replacement.source = codegen::TypeRef{
                    .name = types[index].cpp_spelling, .suffix = {}, .nested = std::nullopt};
                ImGui::EndCombo();
                if (apply_document_edit(ReplaceOptionalPresenceBit{
                        .declaration = *declaration, .schema = std::move(replacement)})) {
                    optional_presence_bit_editor_declaration_.reset();
                    return true;
                }
                return false;
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    return false;
}

auto PlannerUi::draw_fixed_point_editor(TypeNode const& node, FixedPointType const&) -> bool {
    if (!document_.has_value()) {
        return false;
    }
    auto const declaration{document_->find_declaration(node.identity)};
    if (!declaration.has_value()) {
        return false;
    }
    auto const* schema{document_->fixed_point_schema(*declaration)};
    if (schema == nullptr) {
        return false;
    }
    fixed_point_editor_declaration_ = declaration;

    auto signedness{schema->signedness};
    if (ImGui::Checkbox("Signed representation", &signedness)) {
        auto replacement{*schema};
        replacement.signedness = signedness;
        if (signedness && replacement.fractional_bits == replacement.total_bits) {
            --replacement.fractional_bits;
        }
        if (apply_document_edit(
                ReplaceFixedPoint{.declaration = *declaration, .schema = std::move(replacement)})) {
            fixed_point_editor_declaration_.reset();
            return true;
        }
    }

    auto total_bits{schema->total_bits};
    ImGui::SetNextItemWidth(150.0F);
    auto const total_submitted{ImGui::InputScalar("Total bit width",
                                                  ImGuiDataType_U32,
                                                  &total_bits,
                                                  nullptr,
                                                  nullptr,
                                                  "%u",
                                                  ImGuiInputTextFlags_EnterReturnsTrue)};
    if (total_submitted || ImGui::IsItemDeactivatedAfterEdit()) {
        auto const minimum_total{schema->fractional_bits + (schema->signedness ? 1U : 0U)};
        if (total_bits == 0 || total_bits > 64 || total_bits < minimum_total) {
            schema_edit_message_ = schema->signedness
                                     ? "Total width must be 1..64 and leave the sign plus all "
                                       "fractional bits."
                                     : "Total width must be 1..64 and contain all fractional "
                                       "bits.";
        } else {
            auto replacement{*schema};
            replacement.total_bits = total_bits;
            if (apply_document_edit(ReplaceFixedPoint{.declaration = *declaration,
                                                      .schema = std::move(replacement)})) {
                fixed_point_editor_declaration_.reset();
                return true;
            }
        }
    }

    auto fractional_bits{schema->fractional_bits};
    ImGui::SetNextItemWidth(150.0F);
    auto const fractional_submitted{ImGui::InputScalar("Fractional bit width",
                                                       ImGuiDataType_U32,
                                                       &fractional_bits,
                                                       nullptr,
                                                       nullptr,
                                                       "%u",
                                                       ImGuiInputTextFlags_EnterReturnsTrue)};
    if (fractional_submitted || ImGui::IsItemDeactivatedAfterEdit()) {
        auto const maximum_fractional{schema->total_bits - (schema->signedness ? 1U : 0U)};
        if (fractional_bits > maximum_fractional) {
            schema_edit_message_ = schema->signedness
                                     ? "Fractional width must leave one sign bit."
                                     : "Fractional width cannot exceed total width.";
        } else {
            auto replacement{*schema};
            replacement.fractional_bits = fractional_bits;
            if (apply_document_edit(ReplaceFixedPoint{.declaration = *declaration,
                                                      .schema = std::move(replacement)})) {
                fixed_point_editor_declaration_.reset();
                return true;
            }
        }
    }

    auto rounding{schema->rounding == codegen::FixedPointRounding::nearest_even ? 0 : 1};
    constexpr std::array rounding_labels{"Nearest even", "Toward zero"};
    if (ImGui::Combo("Rounding policy",
                     &rounding,
                     rounding_labels.data(),
                     static_cast<int>(rounding_labels.size()))) {
        auto replacement{*schema};
        replacement.rounding = rounding == 0 ? codegen::FixedPointRounding::nearest_even
                                             : codegen::FixedPointRounding::toward_zero;
        if (apply_document_edit(
                ReplaceFixedPoint{.declaration = *declaration, .schema = std::move(replacement)})) {
            fixed_point_editor_declaration_.reset();
            return true;
        }
    }
    return false;
}

auto PlannerUi::draw_mini_float_editor(TypeNode const& node, MiniFloatType const&) -> bool {
    if (!document_.has_value()) {
        return false;
    }
    auto const declaration{document_->find_declaration(node.identity)};
    if (!declaration.has_value()) {
        return false;
    }
    auto const* schema{document_->mini_float_schema(*declaration)};
    if (schema == nullptr) {
        return false;
    }

    auto has_sign{schema->sign_bits == 1};
    if (ImGui::Checkbox("Sign bit", &has_sign)) {
        auto replacement{*schema};
        replacement.sign_bits = has_sign ? 1U : 0U;
        auto const total{static_cast<std::uint64_t>(replacement.sign_bits) +
                         replacement.exponent_bits + replacement.significand_bits};
        if (total > 64) {
            schema_edit_message_ = "Mini-float total width must not exceed 64 bits.";
        } else if (apply_document_edit(ReplaceMiniFloat{.declaration = *declaration,
                                                        .schema = std::move(replacement)})) {
            return true;
        }
    }

    auto exponent_bits{schema->exponent_bits};
    ImGui::SetNextItemWidth(150.0F);
    auto const exponent_submitted{ImGui::InputScalar("Exponent bit width",
                                                     ImGuiDataType_U32,
                                                     &exponent_bits,
                                                     nullptr,
                                                     nullptr,
                                                     "%u",
                                                     ImGuiInputTextFlags_EnterReturnsTrue)};
    if (exponent_submitted || ImGui::IsItemDeactivatedAfterEdit()) {
        auto const total{static_cast<std::uint64_t>(schema->sign_bits) + exponent_bits +
                         schema->significand_bits};
        if (exponent_bits < 2 || exponent_bits > 15) {
            schema_edit_message_ = "Exponent width must be in the range 2..15.";
        } else if (total > 64) {
            schema_edit_message_ = "Mini-float total width must not exceed 64 bits.";
        } else {
            auto replacement{*schema};
            replacement.exponent_bits = exponent_bits;
            if (apply_document_edit(ReplaceMiniFloat{.declaration = *declaration,
                                                     .schema = std::move(replacement)})) {
                return true;
            }
        }
    }

    auto significand_bits{schema->significand_bits};
    ImGui::SetNextItemWidth(150.0F);
    auto const significand_submitted{ImGui::InputScalar("Significand bit width",
                                                        ImGuiDataType_U32,
                                                        &significand_bits,
                                                        nullptr,
                                                        nullptr,
                                                        "%u",
                                                        ImGuiInputTextFlags_EnterReturnsTrue)};
    if (significand_submitted || ImGui::IsItemDeactivatedAfterEdit()) {
        auto const total{static_cast<std::uint64_t>(schema->sign_bits) + schema->exponent_bits +
                         significand_bits};
        if (significand_bits > 62) {
            schema_edit_message_ = "Significand width must be in the range 0..62.";
        } else if (total > 64) {
            schema_edit_message_ = "Mini-float total width must not exceed 64 bits.";
        } else {
            auto replacement{*schema};
            replacement.significand_bits = significand_bits;
            if (apply_document_edit(ReplaceMiniFloat{.declaration = *declaration,
                                                     .schema = std::move(replacement)})) {
                return true;
            }
        }
    }

    auto exponent_bias{schema->exponent_bias};
    ImGui::SetNextItemWidth(150.0F);
    auto const bias_submitted{ImGui::InputScalar("Exponent bias",
                                                 ImGuiDataType_S32,
                                                 &exponent_bias,
                                                 nullptr,
                                                 nullptr,
                                                 "%d",
                                                 ImGuiInputTextFlags_EnterReturnsTrue)};
    if (bias_submitted || ImGui::IsItemDeactivatedAfterEdit()) {
        if (exponent_bias < -32'768 || exponent_bias > 32'767) {
            schema_edit_message_ = "Exponent bias must be in the range -32768..32767.";
        } else {
            auto replacement{*schema};
            replacement.exponent_bias = exponent_bias;
            if (apply_document_edit(ReplaceMiniFloat{.declaration = *declaration,
                                                     .schema = std::move(replacement)})) {
                return true;
            }
        }
    }
    return false;
}

auto PlannerUi::draw_packed_editor(TypeNode const& node, PackedType const& packed) -> bool {
    if (!document_.has_value()) {
        return false;
    }
    auto const declaration{document_->find_declaration(node.identity)};
    if (!declaration.has_value()) {
        return false;
    }
    auto const* schema{document_->packed_value_schema(*declaration)};
    if (schema == nullptr) {
        return false;
    }

    if (selected_field_.empty() && !schema->segments.empty()) {
        selected_field_ = codegen::packed_segment_name(schema->segments.front());
    }
    auto selected{std::ranges::find_if(schema->segments, [&](auto const& segment) {
        return codegen::packed_segment_name(segment) == selected_field_;
    })};
    if (selected == schema->segments.end() && !schema->segments.empty()) {
        selected = schema->segments.begin();
        selected_field_ = codegen::packed_segment_name(*selected);
    }
    auto const selected_index{selected == schema->segments.end()
                                  ? std::optional<std::size_t>{}
                                  : std::optional<std::size_t>{static_cast<std::size_t>(
                                        selected - schema->segments.begin())}};

    if (packed_editor_declaration_ != declaration || packed_editor_field_ != selected_field_) {
        packed_editor_declaration_ = declaration;
        packed_editor_field_ = selected_field_;
        std::snprintf(packed_storage_type_.data(),
                      packed_storage_type_.size(),
                      "%s",
                      schema->storage_type.name.c_str());
        std::snprintf(packed_invalid_value_.data(),
                      packed_invalid_value_.size(),
                      "%s",
                      schema->invalid_value.has_value()
                          ? std::to_string(*schema->invalid_value).c_str()
                          : "");
        std::snprintf(packed_export_specifier_.data(),
                      packed_export_specifier_.size(),
                      "%s",
                      schema->export_specifier.value_or("").c_str());
        if (selected != schema->segments.end()) {
            auto const& selected_name{codegen::packed_segment_name(*selected)};
            std::snprintf(
                packed_field_name_.data(), packed_field_name_.size(), "%s", selected_name.c_str());
            if (auto const* field{std::get_if<codegen::PackedFieldSchema>(&*selected)}) {
                std::snprintf(packed_field_type_.data(),
                              packed_field_type_.size(),
                              "%s",
                              field->type.name.c_str());
                std::snprintf(packed_field_minimum_.data(),
                              packed_field_minimum_.size(),
                              "%s",
                              field->minimum_value.has_value()
                                  ? codegen::format_packed_integer(*field->minimum_value).c_str()
                                  : "");
                std::snprintf(packed_field_maximum_.data(),
                              packed_field_maximum_.size(),
                              "%s",
                              field->maximum_value.has_value()
                                  ? codegen::format_packed_integer(*field->maximum_value).c_str()
                                  : "");
                std::snprintf(packed_relationship_target_.data(),
                              packed_relationship_target_.size(),
                              "%s",
                              field->relationship.has_value()
                                  ? field->relationship->target.name.c_str()
                                  : "");
                auto const relationship_kind{
                    field->relationship.has_value()
                        ? std::ranges::find(semantic_relationship_kinds, field->relationship->kind)
                        : semantic_relationship_kinds.end()};
                packed_relationship_kind_ =
                    relationship_kind == semantic_relationship_kinds.end()
                        ? 0
                        : static_cast<int>(relationship_kind - semantic_relationship_kinds.begin());
                auto const relationship_unit{
                    field->relationship.has_value() && field->relationship->unit.has_value()
                        ? std::ranges::find(semantic_relationship_units, *field->relationship->unit)
                        : semantic_relationship_units.end()};
                packed_relationship_unit_ =
                    relationship_unit == semantic_relationship_units.end()
                        ? 0
                        : static_cast<int>(relationship_unit - semantic_relationship_units.begin());
            } else {
                packed_field_type_.front() = '\0';
                packed_field_minimum_.front() = '\0';
                packed_field_maximum_.front() = '\0';
                packed_relationship_target_.front() = '\0';
                packed_relationship_kind_ = 0;
                packed_relationship_unit_ = 0;
            }
            auto const resolved_width{
                selected_index.has_value() && *selected_index < packed.segments.size()
                    ? std::visit([](auto const& value) { return value.bit_width; },
                                 packed.segments[*selected_index])
                    : std::uint32_t{1}};
            packed_field_bits_ = std::get_if<codegen::PackedFieldSchema>(&*selected) != nullptr
                                   ? std::get<codegen::PackedFieldSchema>(*selected).bits.value_or(
                                         static_cast<int>(resolved_width))
                                   : static_cast<int>(resolved_width);
        }
    }

    auto const* selected_schema_field{selected == schema->segments.end()
                                          ? nullptr
                                          : std::get_if<codegen::PackedFieldSchema>(&*selected)};
    auto const* selected_resolved_field{
        selected_index.has_value() && *selected_index < packed.segments.size()
            ? std::get_if<lispb::schema::PackedField>(&packed.segments[*selected_index])
            : nullptr};
    auto const* selected_integer_scalar{
        selected_resolved_field != nullptr
            ? std::get_if<IntegerScalarType>(
                  &workspace_.types().type(selected_resolved_field->semantic_type.type).definition)
            : nullptr};
    auto const* selected_linear_quantized{
        selected_resolved_field != nullptr
            ? std::get_if<LinearQuantizedType>(
                  &workspace_.types().type(selected_resolved_field->semantic_type.type).definition)
            : nullptr};
    auto const* selected_fixed_point{
        selected_resolved_field != nullptr
            ? std::get_if<FixedPointType>(
                  &workspace_.types().type(selected_resolved_field->semantic_type.type).definition)
            : nullptr};
    if (selected_schema_field == nullptr) {
        selected_packed_code_.clear();
    } else if (std::ranges::find(selected_schema_field->named_codes,
                                 selected_packed_code_,
                                 &codegen::PackedNamedCodeSchema::name) ==
               selected_schema_field->named_codes.end()) {
        selected_packed_code_ = selected_schema_field->named_codes.empty()
                                  ? std::string{}
                                  : selected_schema_field->named_codes.front().name;
    }
    auto const selected_code{selected_schema_field == nullptr
                                 ? std::vector<codegen::PackedNamedCodeSchema>::const_iterator{}
                                 : std::ranges::find(selected_schema_field->named_codes,
                                                     selected_packed_code_,
                                                     &codegen::PackedNamedCodeSchema::name)};
    if (packed_code_editor_declaration_ != declaration ||
        packed_code_editor_field_ != selected_field_ ||
        packed_code_editor_name_ != selected_packed_code_) {
        packed_code_editor_declaration_ = declaration;
        packed_code_editor_field_ = selected_field_;
        packed_code_editor_name_ = selected_packed_code_;
        if (selected_schema_field != nullptr &&
            selected_code != selected_schema_field->named_codes.end()) {
            std::snprintf(packed_code_name_.data(),
                          packed_code_name_.size(),
                          "%s",
                          selected_code->name.c_str());
            std::snprintf(packed_code_value_.data(),
                          packed_code_value_.size(),
                          "%s",
                          codegen::format_packed_integer(selected_code->value).c_str());
            packed_code_sentinel_ = selected_code->sentinel;
        } else {
            packed_code_name_.front() = '\0';
            packed_code_value_.front() = '\0';
            packed_code_sentinel_ = false;
        }
    }

    ImGui::SetNextItemWidth(-1.0F);
    auto const export_submitted{ImGui::InputText("Export specifier (optional)##packed",
                                                 packed_export_specifier_.data(),
                                                 packed_export_specifier_.size(),
                                                 ImGuiInputTextFlags_EnterReturnsTrue)};
    if (export_submitted || ImGui::IsItemDeactivatedAfterEdit()) {
        auto const export_specifier{optional_text(packed_export_specifier_)};
        if (schema->export_specifier != export_specifier) {
            auto replacement{*schema};
            replacement.export_specifier = export_specifier;
            if (apply_document_edit(ReplacePackedValue{.declaration = *declaration,
                                                       .schema = std::move(replacement)})) {
                selected_field_ = packed_editor_field_;
                return true;
            }
        }
    }

    auto mutable_value{schema->mutable_value};
    if (ImGui::Checkbox("Generate mutable field API", &mutable_value)) {
        auto replacement{*schema};
        replacement.mutable_value = mutable_value;
        if (apply_document_edit(ReplacePackedValue{.declaration = *declaration,
                                                   .schema = std::move(replacement)})) {
            selected_field_ = packed_editor_field_;
            return true;
        }
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(
            "Generates fallible try_make/try_set and checked setter APIs; it does not change the "
            "packed bit layout or semantic value domain.");
    }

    ImGui::SetNextItemWidth(-1.0F);
    auto const storage_submitted{ImGui::InputText("Storage type",
                                                  packed_storage_type_.data(),
                                                  packed_storage_type_.size(),
                                                  ImGuiInputTextFlags_EnterReturnsTrue)};
    if (storage_submitted || ImGui::IsItemDeactivatedAfterEdit()) {
        if (packed_storage_type_.front() == '\0') {
            schema_edit_message_ = "Packed storage type cannot be empty.";
        } else {
            auto replacement{*schema};
            replacement.storage_type.name = packed_storage_type_.data();
            if (apply_document_edit(ReplacePackedValue{.declaration = *declaration,
                                                       .schema = std::move(replacement)})) {
                selected_field_ = packed_editor_field_;
                return true;
            }
        }
    }

    constexpr std::array byte_order_labels{"Unspecified", "Little endian", "Big endian"};
    auto byte_order_index{
        schema->byte_order.has_value()
            ? (*schema->byte_order == codegen::PackedByteOrder::little_endian ? 1 : 2)
            : 0};
    if (ImGui::Combo("Serialized byte order",
                     &byte_order_index,
                     byte_order_labels.data(),
                     static_cast<int>(byte_order_labels.size()))) {
        auto replacement{*schema};
        replacement.byte_order =
            byte_order_index == 0
                ? std::nullopt
                : std::optional{byte_order_index == 1 ? codegen::PackedByteOrder::little_endian
                                                      : codegen::PackedByteOrder::big_endian};
        if (apply_document_edit(ReplacePackedValue{.declaration = *declaration,
                                                   .schema = std::move(replacement)})) {
            selected_field_ = packed_editor_field_;
            return true;
        }
    }

    constexpr std::array bit_order_labels{"Default (LSB-first)", "Explicit LSB-first", "MSB-first"};
    auto bit_order_index{
        schema->bit_order.has_value()
            ? (*schema->bit_order == codegen::PackedBitOrder::least_significant_first ? 1 : 2)
            : 0};
    if (ImGui::Combo("Segment bit order",
                     &bit_order_index,
                     bit_order_labels.data(),
                     static_cast<int>(bit_order_labels.size()))) {
        auto replacement{*schema};
        replacement.bit_order =
            bit_order_index == 0
                ? std::nullopt
                : std::optional{bit_order_index == 1
                                    ? codegen::PackedBitOrder::least_significant_first
                                    : codegen::PackedBitOrder::most_significant_first};
        if (apply_document_edit(ReplacePackedValue{.declaration = *declaration,
                                                   .schema = std::move(replacement)})) {
            selected_field_ = packed_editor_field_;
            return true;
        }
    }
    ImGui::TextDisabled(
        "Byte order describes serialized bytes; segment order controls numeric bit offsets.");

    ImGui::SetNextItemWidth(-1.0F);
    auto const invalid_submitted{ImGui::InputText("Invalid raw value",
                                                  packed_invalid_value_.data(),
                                                  packed_invalid_value_.size(),
                                                  ImGuiInputTextFlags_EnterReturnsTrue)};
    if (invalid_submitted || ImGui::IsItemDeactivatedAfterEdit()) {
        auto replacement{*schema};
        if (packed_invalid_value_.front() == '\0') {
            replacement.invalid_value.reset();
        } else if (auto const value{detail::parse_unsigned(packed_invalid_value_.data())}) {
            replacement.invalid_value = *value;
        } else {
            schema_edit_message_ = "Invalid raw value must be an unsigned decimal or 0x value.";
            return false;
        }
        if (apply_document_edit(ReplacePackedValue{.declaration = *declaration,
                                                   .schema = std::move(replacement)})) {
            selected_field_ = packed_editor_field_;
            return true;
        }
    }
    ImGui::TextDisabled("Leave invalid raw value empty when every storage code is valid.");

    if (ImGui::Button("+ Field")) {
        auto replacement{*schema};
        auto name{unique_segment_name(replacement.segments, "field")};
        replacement.segments.emplace_back(codegen::PackedFieldSchema{
            .name = name,
            .type = codegen::TypeRef{.name = "std::uint8_t", .suffix = {}, .nested = std::nullopt},
            .bits = 1,
            .kind = codegen::PackedFieldKind::unsigned_integer,
            .range_helper = false,
            .minimum_value = std::nullopt,
            .maximum_value = std::nullopt,
            .named_codes = {},
            .relationship = std::nullopt});
        if (apply_document_edit(ReplacePackedValue{.declaration = *declaration,
                                                   .schema = std::move(replacement)})) {
            selected_field_ = std::move(name);
            return true;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("+ Reserved")) {
        auto replacement{*schema};
        auto name{unique_segment_name(replacement.segments, "reserved")};
        replacement.segments.emplace_back(
            codegen::PackedReservedBitsSchema{.name = name, .bits = 1});
        if (apply_document_edit(ReplacePackedValue{.declaration = *declaration,
                                                   .schema = std::move(replacement)})) {
            selected_field_ = std::move(name);
            return true;
        }
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!selected_index.has_value());
    if (ImGui::Button("Duplicate")) {
        auto replacement{*schema};
        auto copy{replacement.segments[*selected_index]};
        auto const name{unique_segment_name(replacement.segments,
                                            codegen::packed_segment_name(copy) + "_copy")};
        std::visit([&](auto& value) { value.name = name; }, copy);
        replacement.segments.insert(replacement.segments.begin() +
                                        static_cast<std::ptrdiff_t>(*selected_index + 1),
                                    std::move(copy));
        if (apply_document_edit(ReplacePackedValue{.declaration = *declaration,
                                                   .schema = std::move(replacement)})) {
            selected_field_ = name;
            return true;
        }
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!selected_index.has_value() || *selected_index == 0);
    if (ImGui::Button("Move up")) {
        auto replacement{*schema};
        std::swap(replacement.segments[*selected_index], replacement.segments[*selected_index - 1]);
        if (apply_document_edit(ReplacePackedValue{.declaration = *declaration,
                                                   .schema = std::move(replacement)})) {
            selected_field_ = packed_editor_field_;
            return true;
        }
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!selected_index.has_value() ||
                         *selected_index + 1 >= schema->segments.size());
    if (ImGui::Button("Move down")) {
        auto replacement{*schema};
        std::swap(replacement.segments[*selected_index], replacement.segments[*selected_index + 1]);
        if (apply_document_edit(ReplacePackedValue{.declaration = *declaration,
                                                   .schema = std::move(replacement)})) {
            selected_field_ = packed_editor_field_;
            return true;
        }
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!selected_index.has_value() || schema->segments.size() == 1);
    if (ImGui::Button("Delete")) {
        auto replacement{*schema};
        auto const deleted_name{
            codegen::packed_segment_name(replacement.segments[*selected_index])};
        replacement.segments.erase(replacement.segments.begin() +
                                   static_cast<std::ptrdiff_t>(*selected_index));
        auto const next_index{std::min(*selected_index, replacement.segments.size() - 1)};
        auto const next_name{codegen::packed_segment_name(replacement.segments[next_index])};
        if (apply_document_edit(ReplacePackedValue{.declaration = *declaration,
                                                   .schema = std::move(replacement)})) {
            packed_access_fields_.erase(deleted_name);
            selected_field_ = next_name;
            return true;
        }
    }
    ImGui::EndDisabled();
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Selected only")) {
        packed_access_fields_.clear();
        packed_access_set_explicit_ = false;
    }
    ImGui::SameLine();
    if (ImGui::Button("Access all")) {
        packed_access_fields_.clear();
        for (auto const& segment : schema->segments) {
            if (auto const* field{std::get_if<codegen::PackedFieldSchema>(&segment)}) {
                packed_access_fields_.insert_or_assign(field->name, access_operation_);
            }
        }
        packed_access_set_explicit_ = true;
    }

    std::optional<codegen::PackedValueSchema> pending;
    std::optional<TypeId> navigate_to;
    std::optional<std::pair<std::string, std::string>> renamed_field;
    auto selected_after_edit{selected_field_};
    if (ImGui::BeginTable("packed-schema-fields",
                          11,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                              ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Edit", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Access", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Operation", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Name");
        ImGui::TableSetupColumn("Semantic type");
        ImGui::TableSetupColumn("Bits", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Auto", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Kind");
        ImGui::TableSetupColumn("Range", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Semantic range");
        ImGui::TableSetupColumn("Representable");
        ImGui::TableHeadersRow();
        for (std::size_t index{}; index < schema->segments.size(); ++index) {
            auto const& segment{schema->segments[index]};
            auto const* field{std::get_if<codegen::PackedFieldSchema>(&segment)};
            auto const* resolved_field{
                index < packed.segments.size()
                    ? std::get_if<lispb::schema::PackedField>(&packed.segments[index])
                    : nullptr};
            auto const field_uses_integer_scalar{
                resolved_field != nullptr &&
                std::holds_alternative<IntegerScalarType>(
                    workspace_.types().type(resolved_field->semantic_type.type).definition)};
            auto const* field_linear_quantized{
                resolved_field != nullptr
                    ? std::get_if<LinearQuantizedType>(
                          &workspace_.types().type(resolved_field->semantic_type.type).definition)
                    : nullptr};
            auto const* field_fixed_point{
                resolved_field != nullptr
                    ? std::get_if<FixedPointType>(
                          &workspace_.types().type(resolved_field->semantic_type.type).definition)
                    : nullptr};
            layout::LinearQuantizedAnalysis const* field_quantization_analysis{};
            layout::FixedPointAnalysis const* field_fixed_point_analysis{};
            if (field != nullptr && active_packed_.has_value()) {
                auto const analyzed_field{std::ranges::find(
                    active_packed_->fields, field->name, &layout::PackedFieldAnalysis::name)};
                if (analyzed_field != active_packed_->fields.end() &&
                    analyzed_field->linear_quantized.has_value()) {
                    field_quantization_analysis = &*analyzed_field->linear_quantized;
                }
                if (analyzed_field != active_packed_->fields.end() &&
                    analyzed_field->fixed_point.has_value()) {
                    field_fixed_point_analysis = &*analyzed_field->fixed_point;
                }
            }
            auto const& segment_name{codegen::packed_segment_name(segment)};
            auto const row_selected{selected_field_ == segment_name};
            ImGui::PushID(static_cast<int>(index));
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            if (ImGui::Selectable("::", row_selected, ImGuiSelectableFlags_SpanAllColumns)) {
                selected_field_ = segment_name;
                packed_editor_declaration_.reset();
            }
            if (ImGui::BeginDragDropSource()) {
                ImGui::SetDragDropPayload("PACKED_FIELD_ROW", &index, sizeof(index));
                ImGui::Text("Move %s", segment_name.c_str());
                ImGui::EndDragDropSource();
            }
            if (ImGui::BeginDragDropTarget()) {
                if (auto const* payload{ImGui::AcceptDragDropPayload("PACKED_FIELD_ROW")}) {
                    auto const source_index{*static_cast<std::size_t const*>(payload->Data)};
                    if (source_index < schema->segments.size() && source_index != index) {
                        pending = *schema;
                        selected_after_edit =
                            codegen::packed_segment_name(pending->segments[source_index]);
                        move_element(pending->segments, source_index, index);
                    }
                }
                ImGui::EndDragDropTarget();
            }

            ImGui::TableNextColumn();
            if (field == nullptr) {
                ImGui::TextDisabled("-");
            } else {
                auto accessed{packed_access_set_explicit_
                                  ? packed_access_fields_.contains(field->name)
                                  : selected_field_ == field->name};
                if (ImGui::Checkbox("##access", &accessed)) {
                    if (!packed_access_set_explicit_) {
                        packed_access_fields_.clear();
                        for (auto const& selected_segment : schema->segments) {
                            auto const* selected_schema_field{
                                std::get_if<codegen::PackedFieldSchema>(&selected_segment)};
                            if (selected_schema_field != nullptr &&
                                selected_schema_field->name == selected_field_) {
                                packed_access_fields_.insert_or_assign(selected_field_,
                                                                       access_operation_);
                                break;
                            }
                        }
                        packed_access_set_explicit_ = true;
                    }
                    if (accessed) {
                        packed_access_fields_.insert_or_assign(field->name, access_operation_);
                    } else {
                        packed_access_fields_.erase(field->name);
                    }
                }
            }

            ImGui::TableNextColumn();
            auto const accessed{field != nullptr &&
                                (packed_access_set_explicit_
                                     ? packed_access_fields_.contains(field->name)
                                     : selected_field_ == field->name)};
            if (accessed) {
                auto operation{access_operation_};
                if (packed_access_set_explicit_) {
                    if (auto const found{packed_access_fields_.find(field->name)};
                        found != packed_access_fields_.end()) {
                        operation = found->second;
                    }
                }
                auto operation_index{static_cast<int>(operation)};
                ImGui::SetNextItemWidth(105.0F);
                if (ImGui::Combo(
                        "##access-operation", &operation_index, "Read\0Write\0Read + write\0")) {
                    if (!packed_access_set_explicit_) {
                        packed_access_fields_.clear();
                        packed_access_set_explicit_ = true;
                    }
                    packed_access_fields_.insert_or_assign(
                        field->name, static_cast<AccessOperation>(operation_index));
                }
            } else {
                ImGui::TextDisabled("-");
            }

            ImGui::TableNextColumn();
            if (row_selected) {
                ImGui::SetNextItemWidth(-1.0F);
                auto const submitted{ImGui::InputText("##name",
                                                      packed_field_name_.data(),
                                                      packed_field_name_.size(),
                                                      ImGuiInputTextFlags_EnterReturnsTrue)};
                if (submitted || ImGui::IsItemDeactivatedAfterEdit()) {
                    pending = *schema;
                    std::visit([&](auto& value) { value.name = packed_field_name_.data(); },
                               pending->segments[index]);
                    selected_after_edit = packed_field_name_.data();
                    if (field != nullptr) {
                        renamed_field = std::pair{field->name, selected_after_edit};
                    }
                }
            } else {
                ImGui::TextUnformatted(segment_name.c_str());
            }

            ImGui::TableNextColumn();
            if (field == nullptr) {
                ImGui::TextDisabled("Reserved");
            } else if (row_selected) {
                ImGui::SetNextItemWidth(std::max(60.0F, ImGui::GetContentRegionAvail().x - 58.0F));
                auto const submitted{ImGui::InputText("##type",
                                                      packed_field_type_.data(),
                                                      packed_field_type_.size(),
                                                      ImGuiInputTextFlags_EnterReturnsTrue)};
                if (!pending.has_value() && (submitted || ImGui::IsItemDeactivatedAfterEdit())) {
                    pending = *schema;
                    std::get<codegen::PackedFieldSchema>(pending->segments[index]).type.name =
                        packed_field_type_.data();
                }
                ImGui::SameLine();
                if (auto picked{draw_type_picker(node.identity.module_name, node.identity)}) {
                    pending = *schema;
                    auto& pending_field{
                        std::get<codegen::PackedFieldSchema>(pending->segments[index])};
                    pending_field.type =
                        codegen::TypeRef{.name = *picked, .suffix = {}, .nested = std::nullopt};
                    auto const picked_type{std::ranges::find_if(
                        workspace_.types().types(), [&](TypeNode const& candidate) {
                            return candidate.identity.origin == TypeOrigin::declaration &&
                                   ((candidate.identity.module_name == node.identity.module_name &&
                                     candidate.identity.name == *picked) ||
                                    candidate.cpp_spelling == *picked);
                        })};
                    if (picked_type != workspace_.types().types().end() &&
                        std::holds_alternative<EnumType>(picked_type->definition)) {
                        pending_field.kind = codegen::PackedFieldKind::enumeration;
                        pending_field.range_helper = false;
                        pending_field.minimum_value.reset();
                        pending_field.maximum_value.reset();
                        pending_field.named_codes.clear();
                    } else if (picked_type != workspace_.types().types().end() &&
                               std::holds_alternative<LinearQuantizedType>(
                                   picked_type->definition)) {
                        pending_field.kind = codegen::PackedFieldKind::linear_quantized;
                        pending_field.bits.reset();
                        pending_field.range_helper = false;
                        pending_field.minimum_value.reset();
                        pending_field.maximum_value.reset();
                        pending_field.named_codes.clear();
                        pending_field.relationship.reset();
                    } else if (picked_type != workspace_.types().types().end() &&
                               std::holds_alternative<FixedPointType>(picked_type->definition)) {
                        pending_field.kind = codegen::PackedFieldKind::fixed_point;
                        pending_field.bits.reset();
                        pending_field.range_helper = false;
                        pending_field.minimum_value.reset();
                        pending_field.maximum_value.reset();
                        pending_field.named_codes.clear();
                        pending_field.relationship.reset();
                    } else if (picked_type != workspace_.types().types().end()) {
                        if (auto const* scalar{
                                std::get_if<IntegerScalarType>(&picked_type->definition)}) {
                            pending_field.kind = scalar->signedness
                                                   ? codegen::PackedFieldKind::signed_integer
                                                   : codegen::PackedFieldKind::unsigned_integer;
                            pending_field.range_helper = false;
                            pending_field.minimum_value.reset();
                            pending_field.maximum_value.reset();
                            pending_field.named_codes.clear();
                        }
                    }
                }
                ImGui::SameLine();
                if (index < packed.segments.size() && ImGui::SmallButton(">")) {
                    if (auto const* resolved{
                            std::get_if<lispb::schema::PackedField>(&packed.segments[index])}) {
                        navigate_to = resolved->semantic_type.type;
                    }
                }
            } else {
                ImGui::TextUnformatted(field->type.name.c_str());
            }

            ImGui::TableNextColumn();
            if (field != nullptr && !field->bits.has_value()) {
                auto const effective{
                    index < packed.segments.size()
                        ? std::get<lispb::schema::PackedField>(packed.segments[index]).bit_width
                        : 0U};
                ImGui::Text("=> %u", effective);
            } else if (row_selected) {
                ImGui::SetNextItemWidth(72.0F);
                auto const submitted{ImGui::InputInt(
                    "##bits", &packed_field_bits_, 0, 0, ImGuiInputTextFlags_EnterReturnsTrue)};
                if (!pending.has_value() && (submitted || ImGui::IsItemDeactivatedAfterEdit())) {
                    pending = *schema;
                    std::visit([&](auto& value) { value.bits = packed_field_bits_; },
                               pending->segments[index]);
                }
            } else {
                ImGui::Text("%d", *codegen::packed_segment_bits(segment));
            }

            ImGui::TableNextColumn();
            if (field == nullptr) {
                ImGui::TextDisabled("-");
            } else if (row_selected) {
                auto width_auto{!field->bits.has_value()};
                if (ImGui::Checkbox("##auto-bits", &width_auto)) {
                    pending = *schema;
                    auto& pending_field{
                        std::get<codegen::PackedFieldSchema>(pending->segments[index])};
                    pending_field.bits =
                        width_auto ? std::nullopt : std::optional<int>{packed_field_bits_};
                }
            } else {
                ImGui::TextUnformatted(field->bits.has_value() ? "-" : "yes");
            }

            ImGui::TableNextColumn();
            auto const kind_label{
                field == nullptr                                            ? "reserved"
                : field->kind == codegen::PackedFieldKind::enumeration      ? "enum"
                : field->kind == codegen::PackedFieldKind::linear_quantized ? "linear quantized"
                : field->kind == codegen::PackedFieldKind::fixed_point      ? "fixed point"
                : field->kind == codegen::PackedFieldKind::signed_integer   ? "signed"
                                                                            : "unsigned"};
            if (field == nullptr) {
                ImGui::TextDisabled("reserved");
            } else if (row_selected) {
                ImGui::BeginDisabled(field_uses_integer_scalar ||
                                     field_linear_quantized != nullptr ||
                                     field_fixed_point != nullptr);
                if (ImGui::BeginCombo("##kind", kind_label)) {
                    if (ImGui::Selectable("unsigned",
                                          field->kind ==
                                              codegen::PackedFieldKind::unsigned_integer)) {
                        pending = *schema;
                        auto& pending_field{
                            std::get<codegen::PackedFieldSchema>(pending->segments[index])};
                        pending_field.kind = codegen::PackedFieldKind::unsigned_integer;
                        if (auto const mapped{matching_unsigned_type(pending_field.type.name)}) {
                            pending_field.type.name = *mapped;
                        }
                        auto const has_negative_code{
                            std::ranges::any_of(pending_field.named_codes, [](auto const& code) {
                                return code.value.negative;
                            })};
                        if ((pending_field.minimum_value.has_value() &&
                             pending_field.minimum_value->negative) ||
                            has_negative_code) {
                            pending_field.minimum_value.reset();
                            pending_field.maximum_value.reset();
                            pending_field.named_codes.clear();
                            if (!pending_field.bits.has_value() && index < packed.segments.size()) {
                                pending_field.bits = static_cast<int>(
                                    std::get<lispb::schema::PackedField>(packed.segments[index])
                                        .bit_width);
                            }
                        }
                    }
                    if (ImGui::Selectable(
                            "signed", field->kind == codegen::PackedFieldKind::signed_integer)) {
                        pending = *schema;
                        auto& pending_field{
                            std::get<codegen::PackedFieldSchema>(pending->segments[index])};
                        pending_field.kind = codegen::PackedFieldKind::signed_integer;
                        if (auto const mapped{matching_signed_type(pending_field.type.name)}) {
                            pending_field.type.name = *mapped;
                        }
                        pending_field.range_helper = false;
                        auto const transition_width{
                            pending_field.bits.has_value()
                                ? static_cast<std::uint32_t>(*pending_field.bits)
                            : index < packed.segments.size()
                                ? std::get<lispb::schema::PackedField>(packed.segments[index])
                                      .bit_width
                                : std::uint32_t{1}};
                        if (pending_field.minimum_value.has_value() &&
                            (!codegen::packed_integer_fits_signed(*pending_field.minimum_value,
                                                                  transition_width) ||
                             !codegen::packed_integer_fits_signed(*pending_field.maximum_value,
                                                                  transition_width))) {
                            pending_field.minimum_value.reset();
                            pending_field.maximum_value.reset();
                            pending_field.named_codes.clear();
                            if (!pending_field.bits.has_value()) {
                                pending_field.bits = static_cast<int>(transition_width);
                            }
                        } else {
                            std::erase_if(pending_field.named_codes, [&](auto const& code) {
                                return !codegen::packed_integer_fits_signed(code.value,
                                                                            transition_width);
                            });
                        }
                    }
                    if (ImGui::Selectable("enum",
                                          field->kind == codegen::PackedFieldKind::enumeration)) {
                        pending = *schema;
                        auto& pending_field{
                            std::get<codegen::PackedFieldSchema>(pending->segments[index])};
                        pending_field.kind = codegen::PackedFieldKind::enumeration;
                        pending_field.range_helper = false;
                        pending_field.minimum_value.reset();
                        pending_field.maximum_value.reset();
                        pending_field.named_codes.clear();
                    }
                    if (ImGui::Selectable("linear quantized",
                                          field->kind ==
                                              codegen::PackedFieldKind::linear_quantized)) {
                        pending = *schema;
                        auto& pending_field{
                            std::get<codegen::PackedFieldSchema>(pending->segments[index])};
                        pending_field.kind = codegen::PackedFieldKind::linear_quantized;
                        pending_field.bits.reset();
                        pending_field.range_helper = false;
                        pending_field.minimum_value.reset();
                        pending_field.maximum_value.reset();
                        pending_field.named_codes.clear();
                        pending_field.relationship.reset();
                    }
                    if (ImGui::Selectable("fixed point",
                                          field->kind == codegen::PackedFieldKind::fixed_point)) {
                        pending = *schema;
                        auto& pending_field{
                            std::get<codegen::PackedFieldSchema>(pending->segments[index])};
                        pending_field.kind = codegen::PackedFieldKind::fixed_point;
                        pending_field.bits.reset();
                        pending_field.range_helper = false;
                        pending_field.minimum_value.reset();
                        pending_field.maximum_value.reset();
                        pending_field.named_codes.clear();
                        pending_field.relationship.reset();
                    }
                    ImGui::EndCombo();
                }
                ImGui::EndDisabled();
            } else if (!row_selected) {
                ImGui::TextUnformatted(kind_label);
            }

            ImGui::TableNextColumn();
            auto range_helper{field != nullptr && field->range_helper};
            if (field == nullptr) {
                ImGui::TextDisabled("-");
            } else if (row_selected) {
                ImGui::BeginDisabled(field->kind != codegen::PackedFieldKind::unsigned_integer ||
                                     field_uses_integer_scalar ||
                                     field_linear_quantized != nullptr ||
                                     field_fixed_point != nullptr);
                if (ImGui::Checkbox("##range-helper", &range_helper)) {
                    pending = *schema;
                    std::get<codegen::PackedFieldSchema>(pending->segments[index]).range_helper =
                        range_helper;
                }
                ImGui::EndDisabled();
            } else {
                ImGui::TextUnformatted(range_helper ? "yes" : "-");
            }

            ImGui::TableNextColumn();
            if (field == nullptr) {
                ImGui::TextDisabled("-");
            } else if (field_uses_integer_scalar && resolved_field->minimum_value.has_value()) {
                auto const minimum{codegen::format_packed_integer(*resolved_field->minimum_value)};
                auto const maximum{codegen::format_packed_integer(*resolved_field->maximum_value)};
                ImGui::TextDisabled("%s..%s (shared)", minimum.c_str(), maximum.c_str());
            } else if (field_quantization_analysis != nullptr) {
                auto const minimum{
                    codegen::format_packed_integer(field_quantization_analysis->source_minimum)};
                auto const maximum{
                    codegen::format_packed_integer(field_quantization_analysis->source_maximum)};
                ImGui::TextDisabled("%s..%s (quantized)", minimum.c_str(), maximum.c_str());
            } else if (field_fixed_point_analysis != nullptr) {
                ImGui::TextDisabled("%.9Lg..%.9Lg (fixed)",
                                    field_fixed_point_analysis->minimum_value,
                                    field_fixed_point_analysis->maximum_value);
            } else if (row_selected) {
                ImGui::SetNextItemWidth(70.0F);
                auto range_submitted{ImGui::InputText("##minimum",
                                                      packed_field_minimum_.data(),
                                                      packed_field_minimum_.size(),
                                                      ImGuiInputTextFlags_EnterReturnsTrue)};
                ImGui::SameLine();
                ImGui::TextUnformatted("..");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(70.0F);
                range_submitted = ImGui::InputText("##maximum",
                                                   packed_field_maximum_.data(),
                                                   packed_field_maximum_.size(),
                                                   ImGuiInputTextFlags_EnterReturnsTrue) ||
                                  range_submitted;
                range_submitted = range_submitted || ImGui::IsItemDeactivatedAfterEdit();
                if (!pending.has_value() && range_submitted) {
                    auto const minimum_text{std::string_view{packed_field_minimum_.data()}};
                    auto const maximum_text{std::string_view{packed_field_maximum_.data()}};
                    pending = *schema;
                    auto& pending_field{
                        std::get<codegen::PackedFieldSchema>(pending->segments[index])};
                    if (minimum_text.empty() && maximum_text.empty()) {
                        pending_field.minimum_value.reset();
                        pending_field.maximum_value.reset();
                    } else if (auto const minimum{detail::parse_packed_integer(minimum_text)};
                               minimum.has_value()) {
                        if (auto const maximum{detail::parse_packed_integer(maximum_text)};
                            maximum.has_value()) {
                            pending_field.minimum_value = *minimum;
                            pending_field.maximum_value = *maximum;
                        } else {
                            schema_edit_message_ =
                                "Semantic range requires valid integer minimum and maximum.";
                            pending.reset();
                        }
                    } else {
                        schema_edit_message_ =
                            "Semantic range requires valid integer minimum and maximum.";
                        pending.reset();
                    }
                }
            } else if (field->minimum_value.has_value()) {
                auto const minimum{codegen::format_packed_integer(*field->minimum_value)};
                auto const maximum{codegen::format_packed_integer(*field->maximum_value)};
                ImGui::Text("%s..%s", minimum.c_str(), maximum.c_str());
            } else {
                ImGui::TextDisabled("unconstrained");
            }

            ImGui::TableNextColumn();
            if (field == nullptr) {
                ImGui::TextDisabled("Reserved");
            } else if (field_quantization_analysis != nullptr) {
                auto const maximum{field_quantization_analysis->usable_code_count.two_to_64
                                       ? (std::numeric_limits<std::uint64_t>::max)()
                                       : field_quantization_analysis->usable_code_count.value - 1};
                ImGui::Text("encoded 0..%llu", static_cast<unsigned long long>(maximum));
            } else if (field_fixed_point_analysis != nullptr) {
                auto const minimum{
                    codegen::format_packed_integer(field_fixed_point_analysis->minimum_raw_value)};
                auto const maximum{
                    codegen::format_packed_integer(field_fixed_point_analysis->maximum_raw_value)};
                ImGui::Text("raw %s..%s", minimum.c_str(), maximum.c_str());
            } else if (index < packed.segments.size()) {
                auto const* resolved{
                    std::get_if<lispb::schema::PackedField>(&packed.segments[index])};
                if (resolved != nullptr &&
                    resolved->kind == codegen::PackedFieldKind::signed_integer) {
                    auto const minimum{resolved->bit_width == 64
                                           ? std::numeric_limits<std::int64_t>::min()
                                           : -static_cast<std::int64_t>(
                                                 std::uint64_t{1} << (resolved->bit_width - 1))};
                    auto const maximum{
                        resolved->bit_width == 64
                            ? std::numeric_limits<std::int64_t>::max()
                            : static_cast<std::int64_t>(
                                  (std::uint64_t{1} << (resolved->bit_width - 1)) - 1)};
                    ImGui::Text("%lld..%lld",
                                static_cast<long long>(minimum),
                                static_cast<long long>(maximum));
                } else {
                    auto const maximum{resolved == nullptr ? std::optional<std::uint64_t>{}
                                       : resolved->bit_width >= 64
                                           ? std::optional<std::uint64_t>{std::numeric_limits<
                                                 std::uint64_t>::max()}
                                           : std::optional<std::uint64_t>{
                                                 (std::uint64_t{1} << resolved->bit_width) - 1}};
                    ImGui::Text("0..%s", detail::format_number(maximum).c_str());
                }
            } else {
                ImGui::TextDisabled("Unknown");
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    if (!pending.has_value() && selected_schema_field != nullptr && selected_index.has_value()) {
        auto const selected_segment_name{selected_field_};
        auto enum_module_index{std::optional<std::size_t>{}};
        auto fallback_enum_module_index{std::optional<std::size_t>{}};
        auto const& modules{document_->manifest().modules};
        for (std::size_t module_index{}; module_index < modules.size(); ++module_index) {
            auto const* enum_module{std::get_if<codegen::EnumModuleSchema>(&modules[module_index])};
            if (enum_module == nullptr) {
                continue;
            }
            if (!fallback_enum_module_index.has_value()) {
                fallback_enum_module_index = module_index;
            }
            if (enum_module->settings.namespace_name.value_or("") == node.identity.namespace_name) {
                enum_module_index = module_index;
                break;
            }
        }
        if (!enum_module_index.has_value()) {
            enum_module_index = fallback_enum_module_index;
        }
        ImGui::BeginDisabled(!enum_module_index.has_value() || selected_resolved_field == nullptr);
        if (ImGui::Button("Create enum for selected field...")) {
            new_enum_module_index_ = *enum_module_index;
            auto const suggested_name{suggested_type_name(selected_schema_field->name, "Type")};
            std::snprintf(
                new_enum_name_.data(), new_enum_name_.size(), "%s", suggested_name.c_str());
            auto& enum_module{std::get<codegen::EnumModuleSchema>(modules[new_enum_module_index_])};
            auto unique_name{std::string{new_enum_name_.data()}};
            auto suffix_number{std::size_t{1}};
            while (std::ranges::find(enum_module.enums, unique_name, &codegen::EnumSchema::name) !=
                   enum_module.enums.end()) {
                unique_name = std::string{new_enum_name_.data()} + std::to_string(suffix_number++);
            }
            std::snprintf(new_enum_name_.data(), new_enum_name_.size(), "%s", unique_name.c_str());
            new_enum_backing_auto_ = true;
            new_enum_width_auto_ = false;
            new_enum_bit_width_ = selected_resolved_field->bit_width;
            new_enum_signedness_ = 1;
            pending_packed_enum_binding_ = PendingPackedEnumBinding{
                .packed_declaration = *declaration, .field_name = selected_segment_name};
            open_new_enum_dialog_ = true;
        }
        ImGui::EndDisabled();
        if (!enum_module_index.has_value()) {
            ImGui::SameLine();
            ImGui::TextDisabled("No enum module is available.");
        } else if (selected_resolved_field == nullptr) {
            ImGui::SameLine();
            ImGui::TextDisabled("The selected field width is unresolved.");
        } else {
            ImGui::SameLine();
            ImGui::TextDisabled("Creates a shared enum declaration, then binds this field.");
        }

        auto scalar_module_index{std::optional<std::size_t>{}};
        auto fallback_scalar_module_index{std::optional<std::size_t>{}};
        for (std::size_t module_index{}; module_index < modules.size(); ++module_index) {
            auto const* scalar_module{
                std::get_if<codegen::ScalarModuleSchema>(&modules[module_index])};
            if (scalar_module == nullptr) {
                continue;
            }
            if (!fallback_scalar_module_index.has_value()) {
                fallback_scalar_module_index = module_index;
            }
            if (scalar_module->settings.namespace_name.value_or("") ==
                node.identity.namespace_name) {
                scalar_module_index = module_index;
                break;
            }
        }
        if (!scalar_module_index.has_value()) {
            scalar_module_index = fallback_scalar_module_index;
        }
        auto const scalar_compatible{
            selected_resolved_field != nullptr && selected_integer_scalar == nullptr &&
            (selected_schema_field->kind == codegen::PackedFieldKind::signed_integer ||
             selected_schema_field->kind == codegen::PackedFieldKind::unsigned_integer)};
        ImGui::BeginDisabled(!scalar_module_index.has_value() || !scalar_compatible);
        if (ImGui::Button("Create integer scalar for selected field...")) {
            new_integer_scalar_module_index_ = *scalar_module_index;
            auto const& scalar_module{
                std::get<codegen::ScalarModuleSchema>(modules[new_integer_scalar_module_index_])};
            auto const scalar_namespace{scalar_module.settings.namespace_name.value_or("")};
            auto const suggested_name{suggested_type_name(selected_schema_field->name, "Value")};
            auto unique_name{suggested_name};
            auto suffix_number{std::size_t{1}};
            while (std::ranges::any_of(document_->types().types(), [&](auto const& candidate) {
                return candidate.identity.namespace_name == scalar_namespace &&
                       candidate.identity.name == unique_name;
            })) {
                unique_name = suggested_name + std::to_string(suffix_number++);
            }
            std::snprintf(new_integer_scalar_name_.data(),
                          new_integer_scalar_name_.size(),
                          "%s",
                          unique_name.c_str());

            new_integer_scalar_signed_ =
                selected_schema_field->kind == codegen::PackedFieldKind::signed_integer;
            new_integer_scalar_width_auto_ = !selected_schema_field->bits.has_value();
            new_integer_scalar_bit_width_ = selected_resolved_field->bit_width;

            auto minimum{codegen::PackedIntegerValue{0}};
            auto maximum{codegen::PackedIntegerValue{0}};
            if (selected_schema_field->minimum_value.has_value() &&
                selected_schema_field->maximum_value.has_value()) {
                minimum = *selected_schema_field->minimum_value;
                maximum = *selected_schema_field->maximum_value;
            } else if (new_integer_scalar_signed_) {
                auto const negative_limit{selected_resolved_field->bit_width == 64
                                              ? std::uint64_t{1} << 63
                                              : std::uint64_t{1}
                                                    << (selected_resolved_field->bit_width - 1)};
                minimum = codegen::PackedIntegerValue::from_parts(true, negative_limit);
                maximum =
                    codegen::PackedIntegerValue{static_cast<std::int64_t>(negative_limit - 1)};
            } else {
                maximum = codegen::PackedIntegerValue{
                    selected_resolved_field->bit_width == 64
                        ? (std::numeric_limits<std::uint64_t>::max)()
                        : (std::uint64_t{1} << selected_resolved_field->bit_width) - 1};
            }
            auto const minimum_text{codegen::format_packed_integer(minimum)};
            auto const maximum_text{codegen::format_packed_integer(maximum)};
            std::snprintf(new_integer_scalar_minimum_.data(),
                          new_integer_scalar_minimum_.size(),
                          "%s",
                          minimum_text.c_str());
            std::snprintf(new_integer_scalar_maximum_.data(),
                          new_integer_scalar_maximum_.size(),
                          "%s",
                          maximum_text.c_str());
            pending_packed_integer_scalar_binding_ = PendingPackedIntegerScalarBinding{
                .packed_declaration = *declaration, .field_name = selected_segment_name};
            open_new_integer_scalar_dialog_ = true;
        }
        ImGui::EndDisabled();
        if (!scalar_module_index.has_value()) {
            ImGui::SameLine();
            ImGui::TextDisabled("No scalar module is available.");
        } else if (selected_integer_scalar != nullptr) {
            ImGui::SameLine();
            ImGui::TextDisabled("This field already uses a shared integer scalar.");
        } else if (!scalar_compatible) {
            ImGui::SameLine();
            ImGui::TextDisabled("Requires a resolved signed or unsigned integer field.");
        } else {
            ImGui::SameLine();
            ImGui::TextDisabled("Moves this field's domain into a shared scalar declaration.");
        }

        auto representation_module_index{std::optional<std::size_t>{}};
        auto fallback_representation_module_index{std::optional<std::size_t>{}};
        for (std::size_t module_index{}; module_index < modules.size(); ++module_index) {
            auto const* representation_module{
                std::get_if<codegen::RepresentationModuleSchema>(&modules[module_index])};
            if (representation_module == nullptr) {
                continue;
            }
            if (!fallback_representation_module_index.has_value()) {
                fallback_representation_module_index = module_index;
            }
            if (representation_module->settings.namespace_name.value_or("") ==
                node.identity.namespace_name) {
                representation_module_index = module_index;
                break;
            }
        }
        if (!representation_module_index.has_value()) {
            representation_module_index = fallback_representation_module_index;
        }
        auto const plain_integer_field{
            selected_resolved_field != nullptr && selected_integer_scalar == nullptr &&
            (selected_schema_field->kind == codegen::PackedFieldKind::signed_integer ||
             selected_schema_field->kind == codegen::PackedFieldKind::unsigned_integer) &&
            !selected_schema_field->minimum_value.has_value() &&
            !selected_schema_field->maximum_value.has_value() &&
            selected_schema_field->named_codes.empty() &&
            !selected_schema_field->relationship.has_value() &&
            !selected_schema_field->range_helper};
        ImGui::BeginDisabled(!representation_module_index.has_value() || !plain_integer_field);
        if (ImGui::Button("Create fixed point for selected field...")) {
            new_fixed_point_module_index_ = *representation_module_index;
            auto const& module{std::get<codegen::RepresentationModuleSchema>(
                modules[new_fixed_point_module_index_])};
            auto const namespace_name{module.settings.namespace_name.value_or("")};
            auto const suggested_name{suggested_type_name(selected_schema_field->name, "Fixed")};
            auto unique_name{suggested_name};
            auto suffix_number{std::size_t{1}};
            while (std::ranges::any_of(document_->types().types(), [&](auto const& candidate) {
                return candidate.identity.namespace_name == namespace_name &&
                       candidate.identity.name == unique_name;
            })) {
                unique_name = suggested_name + std::to_string(suffix_number++);
            }
            std::snprintf(new_fixed_point_name_.data(),
                          new_fixed_point_name_.size(),
                          "%s",
                          unique_name.c_str());
            new_fixed_point_signed_ =
                selected_schema_field->kind == codegen::PackedFieldKind::signed_integer;
            new_fixed_point_total_bits_ = selected_resolved_field->bit_width;
            auto const available_magnitude_bits{new_fixed_point_total_bits_ -
                                                (new_fixed_point_signed_ ? 1U : 0U)};
            new_fixed_point_fractional_bits_ =
                std::min(new_fixed_point_total_bits_ / 2, available_magnitude_bits);
            new_fixed_point_rounding_ = 0;
            pending_packed_fixed_point_binding_ = PendingPackedFixedPointBinding{
                .packed_declaration = *declaration, .field_name = selected_segment_name};
            open_new_fixed_point_dialog_ = true;
        }
        ImGui::EndDisabled();
        if (!representation_module_index.has_value()) {
            ImGui::SameLine();
            ImGui::TextDisabled("No representation module is available.");
        } else if (!plain_integer_field) {
            ImGui::SameLine();
            ImGui::TextDisabled("Requires a plain integer field without local semantic metadata.");
        } else {
            ImGui::SameLine();
            ImGui::TextDisabled("Creates a shared scaled-integer representation for this width.");
        }

        ImGui::SeparatorText("Semantic relationship");
        if (selected_linear_quantized != nullptr) {
            ImGui::TextDisabled(
                "This placement inherits semantic meaning through the quantizer's source scalar.");
        } else if (selected_fixed_point != nullptr) {
            ImGui::TextDisabled(
                "This fixed-point placement owns its scaled numerical representation.");
        }
        ImGui::BeginDisabled(selected_linear_quantized != nullptr ||
                             selected_fixed_point != nullptr);
        ImGui::SetNextItemWidth(180.0F);
        auto const current_kind{semantic_relationship_kinds[static_cast<std::size_t>(
            std::clamp(packed_relationship_kind_,
                       0,
                       static_cast<int>(semantic_relationship_kinds.size() - 1)))]};
        auto const current_unit{semantic_relationship_units[static_cast<std::size_t>(
            std::clamp(packed_relationship_unit_,
                       0,
                       static_cast<int>(semantic_relationship_units.size() - 1)))]};
        if (ImGui::BeginCombo("Kind", codegen::semantic_relation_kind_name(current_kind).data())) {
            for (std::size_t kind_index{}; kind_index < semantic_relationship_kinds.size();
                 ++kind_index) {
                auto const kind{semantic_relationship_kinds[kind_index]};
                auto const chosen{packed_relationship_kind_ == static_cast<int>(kind_index)};
                if (ImGui::Selectable(codegen::semantic_relation_kind_name(kind).data(), chosen)) {
                    packed_relationship_kind_ = static_cast<int>(kind_index);
                    if (selected_schema_field->relationship.has_value()) {
                        auto replacement{*schema};
                        auto& relationship{*std::get<codegen::PackedFieldSchema>(
                                                replacement.segments[*selected_index])
                                                .relationship};
                        relationship.kind = kind;
                        relationship.unit = kind == codegen::SemanticRelationKind::offset_into
                                              ? std::optional{current_unit}
                                              : std::nullopt;
                        if (apply_document_edit(ReplacePackedValue{
                                .declaration = *declaration, .schema = std::move(replacement)})) {
                            selected_field_ = selected_segment_name;
                            return true;
                        }
                    }
                }
            }
            ImGui::EndCombo();
        }

        if (current_kind == codegen::SemanticRelationKind::offset_into) {
            ImGui::SetNextItemWidth(180.0F);
            if (ImGui::BeginCombo("Unit",
                                  codegen::semantic_relation_unit_name(current_unit).data())) {
                for (std::size_t unit_index{}; unit_index < semantic_relationship_units.size();
                     ++unit_index) {
                    auto const unit{semantic_relationship_units[unit_index]};
                    auto const chosen{packed_relationship_unit_ == static_cast<int>(unit_index)};
                    if (ImGui::Selectable(codegen::semantic_relation_unit_name(unit).data(),
                                          chosen)) {
                        packed_relationship_unit_ = static_cast<int>(unit_index);
                        if (selected_schema_field->relationship.has_value()) {
                            auto replacement{*schema};
                            std::get<codegen::PackedFieldSchema>(
                                replacement.segments[*selected_index])
                                .relationship->unit = unit;
                            if (apply_document_edit(
                                    ReplacePackedValue{.declaration = *declaration,
                                                       .schema = std::move(replacement)})) {
                                selected_field_ = selected_segment_name;
                                return true;
                            }
                        }
                    }
                }
                ImGui::EndCombo();
            }
        }

        ImGui::SetNextItemWidth(std::max(80.0F, ImGui::GetContentRegionAvail().x - 132.0F));
        auto const target_submitted{ImGui::InputText("Target",
                                                     packed_relationship_target_.data(),
                                                     packed_relationship_target_.size(),
                                                     ImGuiInputTextFlags_EnterReturnsTrue)};
        if (selected_schema_field->relationship.has_value() &&
            (target_submitted || ImGui::IsItemDeactivatedAfterEdit())) {
            if (packed_relationship_target_.front() == '\0') {
                schema_edit_message_ = "Relationship target cannot be empty.";
            } else {
                auto replacement{*schema};
                std::get<codegen::PackedFieldSchema>(replacement.segments[*selected_index])
                    .relationship->target.name = packed_relationship_target_.data();
                if (apply_document_edit(ReplacePackedValue{.declaration = *declaration,
                                                           .schema = std::move(replacement)})) {
                    selected_field_ = selected_segment_name;
                    return true;
                }
            }
        }
        ImGui::SameLine();
        ImGui::PushID("relationship-target");
        auto picked_relationship_target{draw_type_picker(node.identity.module_name, node.identity)};
        ImGui::PopID();
        if (picked_relationship_target.has_value()) {
            std::snprintf(packed_relationship_target_.data(),
                          packed_relationship_target_.size(),
                          "%s",
                          picked_relationship_target->c_str());
            auto replacement{*schema};
            auto& relationship{
                std::get<codegen::PackedFieldSchema>(replacement.segments[*selected_index])
                    .relationship};
            relationship = codegen::SemanticRelationSchema{
                .kind = current_kind,
                .target = codegen::TypeRef{.name = *picked_relationship_target,
                                           .suffix = {},
                                           .nested = std::nullopt},
                .unit = current_kind == codegen::SemanticRelationKind::offset_into
                          ? std::optional{current_unit}
                          : std::nullopt};
            if (apply_document_edit(ReplacePackedValue{.declaration = *declaration,
                                                       .schema = std::move(replacement)})) {
                selected_field_ = selected_segment_name;
                return true;
            }
        }
        ImGui::SameLine();
        if (selected_schema_field->relationship.has_value()) {
            if (ImGui::SmallButton("Clear")) {
                auto replacement{*schema};
                std::get<codegen::PackedFieldSchema>(replacement.segments[*selected_index])
                    .relationship.reset();
                if (apply_document_edit(ReplacePackedValue{.declaration = *declaration,
                                                           .schema = std::move(replacement)})) {
                    selected_field_ = selected_segment_name;
                    return true;
                }
            }
            ImGui::SameLine();
            if (selected_resolved_field != nullptr &&
                selected_resolved_field->relationship.has_value() && ImGui::SmallButton(">")) {
                selected_type_ = selected_resolved_field->relationship->target.type;
                selected_field_.clear();
                packed_access_fields_.clear();
                packed_access_set_explicit_ = false;
                record_access_members_.clear();
                record_access_set_explicit_ = false;
                return true;
            }
        } else {
            ImGui::BeginDisabled(packed_relationship_target_.front() == '\0');
            if (ImGui::SmallButton("Add")) {
                auto replacement{*schema};
                std::get<codegen::PackedFieldSchema>(replacement.segments[*selected_index])
                    .relationship = codegen::SemanticRelationSchema{
                    .kind = current_kind,
                    .target = codegen::TypeRef{.name = packed_relationship_target_.data(),
                                               .suffix = {},
                                               .nested = std::nullopt},
                    .unit = current_kind == codegen::SemanticRelationKind::offset_into
                              ? std::optional{current_unit}
                              : std::nullopt};
                if (apply_document_edit(ReplacePackedValue{.declaration = *declaration,
                                                           .schema = std::move(replacement)})) {
                    selected_field_ = selected_segment_name;
                    return true;
                }
            }
            ImGui::EndDisabled();
        }
        ImGui::EndDisabled();
        ImGui::TextDisabled(
            "Relationships are semantic graph edges; session capacity analysis does not change "
            "durable source widths.");
        layout::PackedFieldAnalysis const* analysis_field{};
        if (active_packed_.has_value()) {
            auto const found{std::ranges::find(
                active_packed_->fields, selected_segment_name, &layout::PackedFieldAnalysis::name)};
            if (found != active_packed_->fields.end()) {
                analysis_field = &*found;
            }
        }
        if (analysis_field != nullptr && analysis_field->relationship_target_extent.has_value()) {
            auto const kind{*analysis_field->relationship_kind};
            auto const term{detail::relationship_extent_term(kind)};
            auto const unit{
                detail::relationship_extent_unit(kind, analysis_field->relationship_unit)};
            auto const heading{"Session " + std::string{term} + " requirement"};
            ImGui::SeparatorText(heading.c_str());
            ImGui::Text(
                "Target %s: %llu %s",
                term.data(),
                static_cast<unsigned long long>(*analysis_field->relationship_target_extent),
                unit.data());
            ImGui::Text(
                "Live values: %s",
                analysis_field->relationship_live_value_count.has_value()
                    ? detail::format_code_count(*analysis_field->relationship_live_value_count)
                          .c_str()
                    : "Unknown");
            auto const required_codes{
                analysis_field->relationship_required_code_count.has_value()
                    ? detail::format_code_count(*analysis_field->relationship_required_code_count)
                : analysis_field->relationship_minimum_required_bits.value_or(0) > 64
                    ? std::string{"> 2^64"}
                    : std::string{"Unknown"}};
            ImGui::Text("Required codes: %s", required_codes.c_str());
            ImGui::Text("Minimum width: %s",
                        analysis_field->relationship_minimum_required_bits.has_value()
                            ? (std::to_string(*analysis_field->relationship_minimum_required_bits) +
                               " bits")
                                  .c_str()
                            : "Unknown");
            ImGui::Text("Current planning width fits: %s",
                        analysis_field->relationship_width_sufficient.has_value()
                            ? (*analysis_field->relationship_width_sufficient ? "Yes" : "No")
                            : "Unknown");
            ImGui::Text(
                "Code-space %s limit: %s",
                term.data(),
                detail::format_number(analysis_field->relationship_code_space_capacity_limit)
                    .c_str());
            ImGui::Text(
                "Code-space %s headroom: %s",
                term.data(),
                detail::format_number(analysis_field->relationship_capacity_headroom).c_str());
            ImGui::Text("Semantic-range %s limit: %s",
                        term.data(),
                        detail::format_number(analysis_field->relationship_semantic_capacity_limit)
                            .c_str());
            ImGui::Text("Sentinel-placement %s limit: %s",
                        term.data(),
                        detail::format_number(analysis_field->relationship_sentinel_capacity_limit)
                            .c_str());
            ImGui::Text("Effective valid %s limit: %s",
                        term.data(),
                        detail::format_number(analysis_field->relationship_effective_capacity_limit)
                            .c_str());
            ImGui::Text(
                "Effective valid %s headroom: %s",
                term.data(),
                detail::format_number(analysis_field->relationship_effective_capacity_headroom)
                    .c_str());
            if (kind == codegen::SemanticRelationKind::count_of) {
                ImGui::TextDisabled(
                    "Live counts include 0 through capacity; named sentinels add code states.");
            } else if (kind == codegen::SemanticRelationKind::offset_into) {
                ImGui::TextDisabled(
                    "Live offsets span 0 through extent-1 in the declared unit; named sentinels "
                    "add code states.");
            } else {
                ImGui::TextDisabled(
                    "Live indices span 0 through capacity-1; named sentinels add code states.");
            }
        }
    }

    auto selected_code_after_edit{selected_packed_code_};
    if (!pending.has_value() && selected_schema_field != nullptr &&
        selected_integer_scalar != nullptr && selected_resolved_field != nullptr) {
        ImGui::SeparatorText("Shared scalar domain");
        ImGui::TextDisabled(
            "Range, signedness, and named codes are authored on the referenced integer scalar.");
        for (auto const& code : selected_resolved_field->named_codes) {
            auto const value{codegen::format_packed_integer(code.value)};
            ImGui::BulletText(
                "%s = %s%s", code.name.c_str(), value.c_str(), code.sentinel ? " (sentinel)" : "");
        }
    } else if (!pending.has_value() && selected_schema_field != nullptr &&
               selected_linear_quantized != nullptr) {
        ImGui::SeparatorText("Linear quantization");
        layout::LinearQuantizedAnalysis const* quantization{};
        if (active_packed_.has_value()) {
            auto const analyzed_field{std::ranges::find(
                active_packed_->fields, selected_field_, &layout::PackedFieldAnalysis::name)};
            if (analyzed_field != active_packed_->fields.end() &&
                analyzed_field->linear_quantized.has_value()) {
                quantization = &*analyzed_field->linear_quantized;
            }
        }
        if (quantization != nullptr) {
            auto const source_minimum{codegen::format_packed_integer(quantization->source_minimum)};
            auto const source_maximum{codegen::format_packed_integer(quantization->source_maximum)};
            ImGui::Text("Source range: %s..%s", source_minimum.c_str(), source_maximum.c_str());
            ImGui::Text("Encoded width: %u bits", quantization->encoded_storage_bits);
            ImGui::Text("Usable codes: %s",
                        detail::format_code_count(quantization->usable_code_count).c_str());
            ImGui::Text("Reserved codes: %llu",
                        static_cast<unsigned long long>(quantization->reserved_code_count));
            ImGui::Text("Resolution: %.9Lg", quantization->resolution);
            ImGui::Text("Maximum rounding error: %.9Lg", quantization->maximum_rounding_error);
            ImGui::Text("Clipping: %s",
                        codegen::quantization_clipping_name(quantization->clipping).data());
        } else {
            ImGui::TextDisabled("Quantization analysis is unavailable.");
        }
        ImGui::TextDisabled(
            "Generated packed APIs expose encoded codes; decoding remains representation policy.");
    } else if (!pending.has_value() && selected_schema_field != nullptr &&
               selected_fixed_point != nullptr) {
        ImGui::SeparatorText("Fixed point");
        layout::FixedPointAnalysis const* fixed{};
        if (active_packed_.has_value()) {
            auto const analyzed_field{std::ranges::find(
                active_packed_->fields, selected_field_, &layout::PackedFieldAnalysis::name)};
            if (analyzed_field != active_packed_->fields.end() &&
                analyzed_field->fixed_point.has_value()) {
                fixed = &*analyzed_field->fixed_point;
            }
        }
        if (fixed != nullptr) {
            auto const minimum_raw{codegen::format_packed_integer(fixed->minimum_raw_value)};
            auto const maximum_raw{codegen::format_packed_integer(fixed->maximum_raw_value)};
            ImGui::Text("Raw range: %s..%s", minimum_raw.c_str(), maximum_raw.c_str());
            ImGui::Text(
                "Numerical range: %.9Lg..%.9Lg", fixed->minimum_value, fixed->maximum_value);
            ImGui::Text("Width: %u total / %u whole / %u fractional bits",
                        fixed->total_bits,
                        fixed->whole_bits,
                        fixed->fractional_bits);
            ImGui::Text("Scale: %.9Lg", fixed->scale);
            ImGui::Text("Resolution: %.9Lg", fixed->resolution);
            ImGui::Text("Maximum rounding error: %.9Lg", fixed->maximum_rounding_error);
            ImGui::Text("Rounding: %s", codegen::fixed_point_rounding_name(fixed->rounding).data());
        } else {
            ImGui::TextDisabled("Fixed-point analysis is unavailable.");
        }
        ImGui::TextDisabled(
            "Generated packed APIs expose scaled raw integers; conversion remains representation "
            "policy.");
    } else if (!pending.has_value() && selected_schema_field != nullptr &&
               selected_index.has_value() && selected_linear_quantized == nullptr &&
               selected_fixed_point == nullptr) {
        auto const selected_segment_name{selected_field_};
        auto const effective_width{
            selected_resolved_field != nullptr ? selected_resolved_field->bit_width : 1U};
        auto const named_value{
            first_available_code(*selected_schema_field, effective_width, false)};
        auto const sentinel_value{
            first_available_code(*selected_schema_field, effective_width, true)};
        auto const current_code_index{
            selected_code == selected_schema_field->named_codes.end()
                ? std::optional<std::size_t>{}
                : std::optional<std::size_t>{static_cast<std::size_t>(
                      selected_code - selected_schema_field->named_codes.begin())}};

        ImGui::SeparatorText("Named codes");
        ImGui::TextDisabled(
            "Named codes remain ordinary integer values; sentinels sit outside the live range.");
        ImGui::BeginDisabled(!named_value.has_value());
        if (ImGui::Button("+ Named code")) {
            auto replacement{*schema};
            auto& field{
                std::get<codegen::PackedFieldSchema>(replacement.segments[*selected_index])};
            auto name{unique_code_name(field.named_codes, "Code")};
            field.named_codes.push_back({.name = name, .value = *named_value, .sentinel = false});
            if (apply_document_edit(ReplacePackedValue{.declaration = *declaration,
                                                       .schema = std::move(replacement)})) {
                selected_field_ = selected_segment_name;
                selected_packed_code_ = std::move(name);
                return true;
            }
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(!sentinel_value.has_value());
        if (ImGui::Button("+ Sentinel")) {
            auto replacement{*schema};
            auto& field{
                std::get<codegen::PackedFieldSchema>(replacement.segments[*selected_index])};
            auto name{unique_code_name(field.named_codes, "Invalid")};
            field.named_codes.push_back({.name = name, .value = *sentinel_value, .sentinel = true});
            if (apply_document_edit(ReplacePackedValue{.declaration = *declaration,
                                                       .schema = std::move(replacement)})) {
                selected_field_ = selected_segment_name;
                selected_packed_code_ = std::move(name);
                return true;
            }
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        auto const duplicate_value{
            current_code_index.has_value()
                ? first_available_code(
                      *selected_schema_field,
                      effective_width,
                      selected_schema_field->named_codes[*current_code_index].sentinel)
                : std::nullopt};
        ImGui::BeginDisabled(!current_code_index.has_value() || !duplicate_value.has_value());
        if (ImGui::Button("Duplicate code")) {
            auto replacement{*schema};
            auto& field{
                std::get<codegen::PackedFieldSchema>(replacement.segments[*selected_index])};
            auto copy{field.named_codes[*current_code_index]};
            copy.name = unique_code_name(field.named_codes, copy.name + "_copy");
            copy.value = *duplicate_value;
            field.named_codes.insert(field.named_codes.begin() +
                                         static_cast<std::ptrdiff_t>(*current_code_index + 1),
                                     copy);
            if (apply_document_edit(ReplacePackedValue{.declaration = *declaration,
                                                       .schema = std::move(replacement)})) {
                selected_field_ = selected_segment_name;
                selected_packed_code_ = std::move(copy.name);
                return true;
            }
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(!current_code_index.has_value() || *current_code_index == 0);
        if (ImGui::Button("Code up")) {
            auto const code_name{selected_schema_field->named_codes[*current_code_index].name};
            auto replacement{*schema};
            auto& codes{std::get<codegen::PackedFieldSchema>(replacement.segments[*selected_index])
                            .named_codes};
            std::swap(codes[*current_code_index], codes[*current_code_index - 1]);
            if (apply_document_edit(ReplacePackedValue{.declaration = *declaration,
                                                       .schema = std::move(replacement)})) {
                selected_field_ = selected_segment_name;
                selected_packed_code_ = code_name;
                return true;
            }
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(!current_code_index.has_value() ||
                             *current_code_index + 1 >= selected_schema_field->named_codes.size());
        if (ImGui::Button("Code down")) {
            auto const code_name{selected_schema_field->named_codes[*current_code_index].name};
            auto replacement{*schema};
            auto& codes{std::get<codegen::PackedFieldSchema>(replacement.segments[*selected_index])
                            .named_codes};
            std::swap(codes[*current_code_index], codes[*current_code_index + 1]);
            if (apply_document_edit(ReplacePackedValue{.declaration = *declaration,
                                                       .schema = std::move(replacement)})) {
                selected_field_ = selected_segment_name;
                selected_packed_code_ = code_name;
                return true;
            }
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(!current_code_index.has_value());
        if (ImGui::Button("Delete code")) {
            auto replacement{*schema};
            auto& codes{std::get<codegen::PackedFieldSchema>(replacement.segments[*selected_index])
                            .named_codes};
            codes.erase(codes.begin() + static_cast<std::ptrdiff_t>(*current_code_index));
            auto const next_code{codes.empty()
                                     ? std::string{}
                                     : codes[std::min(*current_code_index, codes.size() - 1)].name};
            if (apply_document_edit(ReplacePackedValue{.declaration = *declaration,
                                                       .schema = std::move(replacement)})) {
                selected_field_ = selected_segment_name;
                selected_packed_code_ = next_code;
                return true;
            }
        }
        ImGui::EndDisabled();

        if (!selected_schema_field->minimum_value.has_value()) {
            ImGui::TextDisabled("Set a semantic range before adding sentinel codes.");
        } else if (!sentinel_value.has_value()) {
            ImGui::TextDisabled("No unused code outside the live range fits this field width.");
        }

        if (ImGui::BeginTable("packed-named-codes",
                              4,
                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                  ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp)) {
            ImGui::TableSetupColumn("Edit", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableSetupColumn("Name");
            ImGui::TableSetupColumn("Value");
            ImGui::TableSetupColumn("Sentinel", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableHeadersRow();
            for (std::size_t code_index{}; code_index < selected_schema_field->named_codes.size();
                 ++code_index) {
                auto const& code{selected_schema_field->named_codes[code_index]};
                auto const row_selected{selected_packed_code_ == code.name};
                ImGui::PushID(static_cast<int>(code_index));
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                if (ImGui::Selectable("::", row_selected, ImGuiSelectableFlags_SpanAllColumns)) {
                    selected_packed_code_ = code.name;
                    packed_code_editor_declaration_.reset();
                }
                if (ImGui::BeginDragDropSource()) {
                    ImGui::SetDragDropPayload("PACKED_CODE_ROW", &code_index, sizeof(code_index));
                    ImGui::Text("Move %s", code.name.c_str());
                    ImGui::EndDragDropSource();
                }
                if (ImGui::BeginDragDropTarget()) {
                    if (auto const* payload{ImGui::AcceptDragDropPayload("PACKED_CODE_ROW")}) {
                        auto const source_index{*static_cast<std::size_t const*>(payload->Data)};
                        if (source_index < selected_schema_field->named_codes.size() &&
                            source_index != code_index) {
                            pending = *schema;
                            auto& codes{std::get<codegen::PackedFieldSchema>(
                                            pending->segments[*selected_index])
                                            .named_codes};
                            selected_code_after_edit = codes[source_index].name;
                            move_element(codes, source_index, code_index);
                        }
                    }
                    ImGui::EndDragDropTarget();
                }

                ImGui::TableNextColumn();
                if (row_selected) {
                    ImGui::SetNextItemWidth(-1.0F);
                    auto const submitted{ImGui::InputText("##code-name",
                                                          packed_code_name_.data(),
                                                          packed_code_name_.size(),
                                                          ImGuiInputTextFlags_EnterReturnsTrue)};
                    if (!pending.has_value() &&
                        (submitted || ImGui::IsItemDeactivatedAfterEdit())) {
                        pending = *schema;
                        auto& edited{
                            std::get<codegen::PackedFieldSchema>(pending->segments[*selected_index])
                                .named_codes[code_index]};
                        edited.name = packed_code_name_.data();
                        selected_code_after_edit = edited.name;
                    }
                } else {
                    ImGui::TextUnformatted(code.name.c_str());
                }

                ImGui::TableNextColumn();
                if (row_selected) {
                    ImGui::SetNextItemWidth(-1.0F);
                    auto const submitted{ImGui::InputText("##code-value",
                                                          packed_code_value_.data(),
                                                          packed_code_value_.size(),
                                                          ImGuiInputTextFlags_EnterReturnsTrue)};
                    if (!pending.has_value() &&
                        (submitted || ImGui::IsItemDeactivatedAfterEdit())) {
                        if (auto const value{
                                detail::parse_packed_integer(packed_code_value_.data())}) {
                            pending = *schema;
                            std::get<codegen::PackedFieldSchema>(pending->segments[*selected_index])
                                .named_codes[code_index]
                                .value = *value;
                        } else {
                            schema_edit_message_ =
                                "Named code value must be a signed decimal or hexadecimal integer.";
                        }
                    }
                } else {
                    auto const value{codegen::format_packed_integer(code.value)};
                    ImGui::TextUnformatted(value.c_str());
                }

                ImGui::TableNextColumn();
                if (row_selected) {
                    auto sentinel{packed_code_sentinel_};
                    if (ImGui::Checkbox("##code-sentinel", &sentinel)) {
                        pending = *schema;
                        std::get<codegen::PackedFieldSchema>(pending->segments[*selected_index])
                            .named_codes[code_index]
                            .sentinel = sentinel;
                    }
                } else {
                    ImGui::TextUnformatted(code.sentinel ? "yes" : "-");
                }
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
    }

    if (navigate_to.has_value()) {
        selected_type_ = *navigate_to;
        selected_field_.clear();
        packed_access_fields_.clear();
        packed_access_set_explicit_ = false;
        record_access_members_.clear();
        record_access_set_explicit_ = false;
        return true;
    }

    if (pending.has_value()) {
        auto const& segment{pending->segments[*selected_index]};
        if (codegen::packed_segment_name(segment).empty()) {
            schema_edit_message_ = "Packed segment name cannot be empty.";
            return false;
        }
        if (auto const* field{std::get_if<codegen::PackedFieldSchema>(&segment)};
            field != nullptr && field->type.name.empty()) {
            schema_edit_message_ = "Packed field semantic type cannot be empty.";
            return false;
        }
        if (apply_document_edit(
                ReplacePackedValue{.declaration = *declaration, .schema = std::move(*pending)})) {
            if (packed_access_set_explicit_ && renamed_field.has_value()) {
                auto const existing{packed_access_fields_.find(renamed_field->first)};
                if (existing != packed_access_fields_.end()) {
                    auto const operation{existing->second};
                    packed_access_fields_.erase(existing);
                    packed_access_fields_.insert_or_assign(renamed_field->second, operation);
                }
            }
            selected_field_ = std::move(selected_after_edit);
            selected_packed_code_ = std::move(selected_code_after_edit);
            return true;
        }
    }
    return false;
}

auto PlannerUi::draw_union_editor(TypeNode const& node, UnionType const& union_type) -> bool {
    if (!document_.has_value()) {
        return false;
    }
    auto const declaration{document_->find_declaration(node.identity)};
    if (!declaration.has_value()) {
        return false;
    }
    auto const* schema{document_->union_schema(*declaration)};
    if (schema == nullptr) {
        return false;
    }

    if (selected_field_.empty() && !schema->alternatives.empty()) {
        selected_field_ = schema->alternatives.front().name;
    }
    auto selected{std::ranges::find(
        schema->alternatives, selected_field_, &codegen::UnionAlternativeSchema::name)};
    if (selected == schema->alternatives.end() && !schema->alternatives.empty()) {
        selected = schema->alternatives.begin();
        selected_field_ = selected->name;
    }
    auto const selected_index{selected == schema->alternatives.end()
                                  ? std::optional<std::size_t>{}
                                  : std::optional<std::size_t>{static_cast<std::size_t>(
                                        selected - schema->alternatives.begin())}};

    if (union_editor_declaration_ != declaration || union_editor_alternative_ != selected_field_) {
        union_editor_declaration_ = declaration;
        union_editor_alternative_ = selected_field_;
        std::snprintf(union_export_specifier_.data(),
                      union_export_specifier_.size(),
                      "%s",
                      schema->export_specifier.value_or("").c_str());
        if (selected != schema->alternatives.end()) {
            std::snprintf(union_alternative_name_.data(),
                          union_alternative_name_.size(),
                          "%s",
                          selected->name.c_str());
            std::snprintf(union_alternative_type_.data(),
                          union_alternative_type_.size(),
                          "%s",
                          selected->type.name.c_str());
            union_alternative_is_array_ = selected->count.has_value();
            union_alternative_count_ = selected->count.value_or(1);
        }
    }

    ImGui::SetNextItemWidth(-1.0F);
    auto const export_submitted{ImGui::InputText("Export specifier (optional)##union",
                                                 union_export_specifier_.data(),
                                                 union_export_specifier_.size(),
                                                 ImGuiInputTextFlags_EnterReturnsTrue)};
    if (export_submitted || ImGui::IsItemDeactivatedAfterEdit()) {
        auto const export_specifier{optional_text(union_export_specifier_)};
        if (schema->export_specifier != export_specifier) {
            auto replacement{*schema};
            replacement.export_specifier = export_specifier;
            if (apply_document_edit(
                    ReplaceUnion{.declaration = *declaration, .schema = std::move(replacement)})) {
                selected_field_ = union_editor_alternative_;
                return true;
            }
        }
    }

    if (ImGui::Button("+ Alternative")) {
        auto replacement{*schema};
        auto name{unique_union_alternative_name(replacement.alternatives, "alternative")};
        replacement.alternatives.push_back(
            {.name = name,
             .type =
                 codegen::TypeRef{.name = "std::uint32_t", .suffix = {}, .nested = std::nullopt},
             .count = std::nullopt});
        if (apply_document_edit(
                ReplaceUnion{.declaration = *declaration, .schema = std::move(replacement)})) {
            selected_field_ = std::move(name);
            return true;
        }
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!selected_index.has_value());
    if (ImGui::Button("Duplicate")) {
        auto replacement{*schema};
        auto copy{replacement.alternatives[*selected_index]};
        copy.name = unique_union_alternative_name(replacement.alternatives, copy.name + "_copy");
        replacement.alternatives.insert(replacement.alternatives.begin() +
                                            static_cast<std::ptrdiff_t>(*selected_index + 1),
                                        copy);
        if (apply_document_edit(
                ReplaceUnion{.declaration = *declaration, .schema = std::move(replacement)})) {
            selected_field_ = std::move(copy.name);
            return true;
        }
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!selected_index.has_value() || *selected_index == 0);
    if (ImGui::Button("Move up")) {
        auto replacement{*schema};
        std::swap(replacement.alternatives[*selected_index],
                  replacement.alternatives[*selected_index - 1]);
        if (apply_document_edit(
                ReplaceUnion{.declaration = *declaration, .schema = std::move(replacement)})) {
            selected_field_ = union_editor_alternative_;
            return true;
        }
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!selected_index.has_value() ||
                         *selected_index + 1 >= schema->alternatives.size());
    if (ImGui::Button("Move down")) {
        auto replacement{*schema};
        std::swap(replacement.alternatives[*selected_index],
                  replacement.alternatives[*selected_index + 1]);
        if (apply_document_edit(
                ReplaceUnion{.declaration = *declaration, .schema = std::move(replacement)})) {
            selected_field_ = union_editor_alternative_;
            return true;
        }
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!selected_index.has_value() || schema->alternatives.size() == 1);
    if (ImGui::Button("Delete")) {
        auto replacement{*schema};
        replacement.alternatives.erase(replacement.alternatives.begin() +
                                       static_cast<std::ptrdiff_t>(*selected_index));
        auto const next_index{std::min(*selected_index, replacement.alternatives.size() - 1)};
        auto const next_name{replacement.alternatives[next_index].name};
        if (apply_document_edit(
                ReplaceUnion{.declaration = *declaration, .schema = std::move(replacement)})) {
            selected_field_ = next_name;
            return true;
        }
    }
    ImGui::EndDisabled();
    ImGui::EndDisabled();

    std::optional<codegen::UnionSchema> pending;
    std::optional<TypeId> navigate_to;
    auto selected_after_edit{selected_field_};
    if (ImGui::BeginTable("union-schema-alternatives",
                          5,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                              ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Edit", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Name");
        ImGui::TableSetupColumn("Semantic type");
        ImGui::TableSetupColumn("Fixed array", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Count", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableHeadersRow();
        for (std::size_t index{}; index < schema->alternatives.size(); ++index) {
            auto const& alternative{schema->alternatives[index]};
            auto const row_selected{selected_field_ == alternative.name};
            ImGui::PushID(static_cast<int>(index));
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            if (ImGui::Selectable("::", row_selected, ImGuiSelectableFlags_SpanAllColumns)) {
                selected_field_ = alternative.name;
                union_editor_declaration_.reset();
            }
            if (ImGui::BeginDragDropSource()) {
                ImGui::SetDragDropPayload("UNION_ALTERNATIVE_ROW", &index, sizeof(index));
                ImGui::Text("Move %s", alternative.name.c_str());
                ImGui::EndDragDropSource();
            }
            if (ImGui::BeginDragDropTarget()) {
                if (auto const* payload{ImGui::AcceptDragDropPayload("UNION_ALTERNATIVE_ROW")}) {
                    auto const source_index{*static_cast<std::size_t const*>(payload->Data)};
                    if (source_index < schema->alternatives.size() && source_index != index) {
                        pending = *schema;
                        selected_after_edit = pending->alternatives[source_index].name;
                        move_element(pending->alternatives, source_index, index);
                    }
                }
                ImGui::EndDragDropTarget();
            }

            ImGui::TableNextColumn();
            if (row_selected) {
                ImGui::SetNextItemWidth(-1.0F);
                auto const submitted{ImGui::InputText("##name",
                                                      union_alternative_name_.data(),
                                                      union_alternative_name_.size(),
                                                      ImGuiInputTextFlags_EnterReturnsTrue)};
                if (submitted || ImGui::IsItemDeactivatedAfterEdit()) {
                    pending = *schema;
                    pending->alternatives[index].name = union_alternative_name_.data();
                    selected_after_edit = pending->alternatives[index].name;
                }
            } else {
                ImGui::TextUnformatted(alternative.name.c_str());
            }

            ImGui::TableNextColumn();
            if (row_selected) {
                ImGui::SetNextItemWidth(std::max(60.0F, ImGui::GetContentRegionAvail().x - 58.0F));
                auto const submitted{ImGui::InputText("##type",
                                                      union_alternative_type_.data(),
                                                      union_alternative_type_.size(),
                                                      ImGuiInputTextFlags_EnterReturnsTrue)};
                if (!pending.has_value() && (submitted || ImGui::IsItemDeactivatedAfterEdit())) {
                    pending = *schema;
                    pending->alternatives[index].type.name = union_alternative_type_.data();
                }
                ImGui::SameLine();
                if (auto picked{draw_type_picker(node.identity.module_name, node.identity)}) {
                    pending = *schema;
                    pending->alternatives[index].type.name = std::move(*picked);
                }
                ImGui::SameLine();
                if (index < union_type.alternatives.size() && ImGui::SmallButton(">")) {
                    navigate_to = union_type.alternatives[index].semantic_type.type;
                }
            } else {
                ImGui::TextUnformatted(alternative.type.name.c_str());
            }

            ImGui::TableNextColumn();
            if (row_selected) {
                if (ImGui::Checkbox("##fixed-array", &union_alternative_is_array_)) {
                    pending = *schema;
                    pending->alternatives[index].count = union_alternative_is_array_
                                                           ? std::optional{union_alternative_count_}
                                                           : std::nullopt;
                }
            } else {
                ImGui::TextUnformatted(alternative.count.has_value() ? "yes" : "-");
            }

            ImGui::TableNextColumn();
            if (row_selected && union_alternative_is_array_) {
                ImGui::SetNextItemWidth(96.0F);
                auto const submitted{ImGui::InputScalar("##count",
                                                        ImGuiDataType_U64,
                                                        &union_alternative_count_,
                                                        nullptr,
                                                        nullptr,
                                                        "%llu",
                                                        ImGuiInputTextFlags_EnterReturnsTrue)};
                if (!pending.has_value() && (submitted || ImGui::IsItemDeactivatedAfterEdit())) {
                    pending = *schema;
                    pending->alternatives[index].count = union_alternative_count_;
                }
            } else if (alternative.count.has_value()) {
                ImGui::Text("%llu", static_cast<unsigned long long>(*alternative.count));
            } else {
                ImGui::TextUnformatted("1");
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    if (navigate_to.has_value()) {
        selected_type_ = *navigate_to;
        selected_field_.clear();
        return true;
    }

    if (pending.has_value()) {
        auto const invalid_alternative{
            std::ranges::find_if(pending->alternatives, [](auto const& alternative) {
                return alternative.name.empty() || alternative.type.name.empty() ||
                       alternative.count == 0;
            })};
        if (invalid_alternative != pending->alternatives.end()) {
            if (invalid_alternative->name.empty()) {
                schema_edit_message_ = "Union alternative name cannot be empty.";
            } else if (invalid_alternative->type.name.empty()) {
                schema_edit_message_ = "Union alternative type cannot be empty.";
            } else {
                schema_edit_message_ = "Fixed array count must be greater than zero.";
            }
            return false;
        }
        if (apply_document_edit(
                ReplaceUnion{.declaration = *declaration, .schema = std::move(*pending)})) {
            selected_field_ = std::move(selected_after_edit);
            return true;
        }
    }
    return false;
}

auto PlannerUi::draw_tagged_union_editor(TypeNode const& node, TaggedUnionType const& tagged_union)
    -> bool {
    if (!document_.has_value()) {
        return false;
    }
    auto const declaration{document_->find_declaration(node.identity)};
    if (!declaration.has_value()) {
        return false;
    }
    auto const* schema{document_->tagged_union_schema(*declaration)};
    if (schema == nullptr) {
        return false;
    }

    if (selected_field_.empty() && !schema->alternatives.empty()) {
        selected_field_ = schema->alternatives.front().name;
    }
    auto selected{std::ranges::find(
        schema->alternatives, selected_field_, &codegen::TaggedUnionAlternativeSchema::name)};
    if (selected == schema->alternatives.end() && !schema->alternatives.empty()) {
        selected = schema->alternatives.begin();
        selected_field_ = selected->name;
    }
    auto const selected_index{selected == schema->alternatives.end()
                                  ? std::optional<std::size_t>{}
                                  : std::optional<std::size_t>{static_cast<std::size_t>(
                                        selected - schema->alternatives.begin())}};
    if (tagged_union_editor_declaration_ != declaration ||
        tagged_union_editor_alternative_ != selected_field_) {
        tagged_union_editor_declaration_ = declaration;
        tagged_union_editor_alternative_ = selected_field_;
        std::snprintf(tagged_union_export_specifier_.data(),
                      tagged_union_export_specifier_.size(),
                      "%s",
                      schema->export_specifier.value_or("").c_str());
        if (selected != schema->alternatives.end()) {
            std::snprintf(tagged_union_alternative_name_.data(),
                          tagged_union_alternative_name_.size(),
                          "%s",
                          selected->name.c_str());
            std::snprintf(tagged_union_alternative_type_.data(),
                          tagged_union_alternative_type_.size(),
                          "%s",
                          selected->type.name.c_str());
            tagged_union_alternative_is_array_ = selected->count.has_value();
            tagged_union_alternative_count_ = selected->count.value_or(1);
        }
    }

    ImGui::SetNextItemWidth(-1.0F);
    auto const export_submitted{ImGui::InputText("Export specifier (optional)##tagged-union",
                                                 tagged_union_export_specifier_.data(),
                                                 tagged_union_export_specifier_.size(),
                                                 ImGuiInputTextFlags_EnterReturnsTrue)};
    if (export_submitted || ImGui::IsItemDeactivatedAfterEdit()) {
        auto const export_specifier{optional_text(tagged_union_export_specifier_)};
        if (schema->export_specifier != export_specifier) {
            auto replacement{*schema};
            replacement.export_specifier = export_specifier;
            if (apply_document_edit(ReplaceTaggedUnion{.declaration = *declaration,
                                                       .schema = std::move(replacement)})) {
                selected_field_ = tagged_union_editor_alternative_;
                return true;
            }
        }
    }

    std::optional<codegen::TaggedUnionSchema> pending;
    auto selected_after_edit{selected_field_};
    std::optional<TypeId> navigate_to;
    auto const& discriminator_node{workspace_.types().type(tagged_union.discriminant.type)};
    auto const& discriminator{std::get<EnumType>(discriminator_node.definition)};
    auto tag_available = [&](std::string const& tag, std::optional<std::size_t> const except) {
        auto const value{std::ranges::find(discriminator.enumerators, tag, &Enumerator::name)};
        if (value == discriminator.enumerators.end() || value->sentinel || value->count_sentinel) {
            return false;
        }
        for (std::size_t index{}; index < schema->alternatives.size(); ++index) {
            if ((!except.has_value() || index != *except) &&
                schema->alternatives[index].tag == tag) {
                return false;
            }
        }
        return true;
    };
    auto first_available_tag = [&](std::optional<std::size_t> const except) -> std::string {
        auto const found{std::ranges::find_if(discriminator.enumerators, [&](auto const& value) {
            return tag_available(value.name, except);
        })};
        return found == discriminator.enumerators.end() ? std::string{} : found->name;
    };

    if (ImGui::BeginCombo("Discriminant", discriminator_node.cpp_spelling.c_str())) {
        for (auto const& candidate : workspace_.types().types()) {
            auto const* enumeration{std::get_if<EnumType>(&candidate.definition)};
            if (enumeration == nullptr) {
                continue;
            }
            auto const all_tags_exist{
                std::ranges::all_of(schema->alternatives, [&](auto const& alt) {
                    auto const value{
                        std::ranges::find(enumeration->enumerators, alt.tag, &Enumerator::name)};
                    return value != enumeration->enumerators.end() && !value->sentinel &&
                           !value->count_sentinel;
                })};
            if (!all_tags_exist) {
                continue;
            }
            auto const current{candidate.identity == discriminator_node.identity};
            if (ImGui::Selectable(candidate.cpp_spelling.c_str(), current) && !current) {
                pending = *schema;
                pending->discriminant.name = candidate.cpp_spelling;
            }
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Go to discriminant")) {
        selected_type_ = tagged_union.discriminant.type;
        selected_field_.clear();
        return true;
    }

    if (ImGui::Button("+ Alternative")) {
        auto const tag{first_available_tag(std::nullopt)};
        if (tag.empty()) {
            schema_edit_message_ = "The discriminant has no unused non-sentinel tag.";
        } else {
            auto replacement{*schema};
            auto name{unique_union_alternative_name(replacement.alternatives, "alternative")};
            codegen::TaggedUnionAlternativeSchema alternative{};
            alternative.name = name;
            alternative.type.name = "std::uint32_t";
            alternative.tag = tag;
            replacement.alternatives.push_back(std::move(alternative));
            if (apply_document_edit(ReplaceTaggedUnion{.declaration = *declaration,
                                                       .schema = std::move(replacement)})) {
                selected_field_ = std::move(name);
                return true;
            }
        }
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!selected_index.has_value());
    if (ImGui::Button("Duplicate")) {
        auto const tag{first_available_tag(std::nullopt)};
        if (tag.empty()) {
            schema_edit_message_ = "The discriminant has no unused non-sentinel tag.";
        } else {
            auto replacement{*schema};
            auto copy{replacement.alternatives[*selected_index]};
            copy.name =
                unique_union_alternative_name(replacement.alternatives, copy.name + "_copy");
            copy.tag = tag;
            replacement.alternatives.insert(replacement.alternatives.begin() +
                                                static_cast<std::ptrdiff_t>(*selected_index + 1),
                                            copy);
            if (apply_document_edit(ReplaceTaggedUnion{.declaration = *declaration,
                                                       .schema = std::move(replacement)})) {
                selected_field_ = std::move(copy.name);
                return true;
            }
        }
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!selected_index.has_value() || *selected_index == 0);
    if (ImGui::Button("Move up")) {
        auto replacement{*schema};
        std::swap(replacement.alternatives[*selected_index],
                  replacement.alternatives[*selected_index - 1]);
        if (apply_document_edit(ReplaceTaggedUnion{.declaration = *declaration,
                                                   .schema = std::move(replacement)})) {
            selected_field_ = tagged_union_editor_alternative_;
            return true;
        }
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!selected_index.has_value() ||
                         *selected_index + 1 >= schema->alternatives.size());
    if (ImGui::Button("Move down")) {
        auto replacement{*schema};
        std::swap(replacement.alternatives[*selected_index],
                  replacement.alternatives[*selected_index + 1]);
        if (apply_document_edit(ReplaceTaggedUnion{.declaration = *declaration,
                                                   .schema = std::move(replacement)})) {
            selected_field_ = tagged_union_editor_alternative_;
            return true;
        }
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!selected_index.has_value() || schema->alternatives.size() == 1);
    if (ImGui::Button("Delete")) {
        auto replacement{*schema};
        replacement.alternatives.erase(replacement.alternatives.begin() +
                                       static_cast<std::ptrdiff_t>(*selected_index));
        auto const next_index{std::min(*selected_index, replacement.alternatives.size() - 1)};
        auto const next_name{replacement.alternatives[next_index].name};
        if (apply_document_edit(ReplaceTaggedUnion{.declaration = *declaration,
                                                   .schema = std::move(replacement)})) {
            selected_field_ = next_name;
            return true;
        }
    }
    ImGui::EndDisabled();
    ImGui::EndDisabled();

    if (ImGui::BeginTable("tagged-union-schema-alternatives",
                          6,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                              ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Edit", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Tag");
        ImGui::TableSetupColumn("Name");
        ImGui::TableSetupColumn("Semantic type");
        ImGui::TableSetupColumn("Fixed array", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Count", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableHeadersRow();
        for (std::size_t index{}; index < schema->alternatives.size(); ++index) {
            auto const& alternative{schema->alternatives[index]};
            auto const row_selected{selected_field_ == alternative.name};
            ImGui::PushID(static_cast<int>(index));
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            if (ImGui::Selectable("::", row_selected, ImGuiSelectableFlags_SpanAllColumns)) {
                selected_field_ = alternative.name;
                tagged_union_editor_declaration_.reset();
            }
            if (ImGui::BeginDragDropSource()) {
                ImGui::SetDragDropPayload("TAGGED_UNION_ALTERNATIVE_ROW", &index, sizeof(index));
                ImGui::Text("Move %s", alternative.name.c_str());
                ImGui::EndDragDropSource();
            }
            if (ImGui::BeginDragDropTarget()) {
                if (auto const* payload{
                        ImGui::AcceptDragDropPayload("TAGGED_UNION_ALTERNATIVE_ROW")}) {
                    auto const source_index{*static_cast<std::size_t const*>(payload->Data)};
                    if (source_index < schema->alternatives.size() && source_index != index) {
                        pending = *schema;
                        selected_after_edit = pending->alternatives[source_index].name;
                        move_element(pending->alternatives, source_index, index);
                    }
                }
                ImGui::EndDragDropTarget();
            }

            ImGui::TableNextColumn();
            if (row_selected) {
                if (ImGui::BeginCombo("##tag", alternative.tag.c_str())) {
                    for (auto const& value : discriminator.enumerators) {
                        if (!tag_available(value.name, index)) {
                            continue;
                        }
                        if (ImGui::Selectable(value.name.c_str(), value.name == alternative.tag)) {
                            pending = *schema;
                            pending->alternatives[index].tag = value.name;
                        }
                    }
                    ImGui::EndCombo();
                }
            } else {
                ImGui::TextUnformatted(alternative.tag.c_str());
            }

            ImGui::TableNextColumn();
            if (row_selected) {
                ImGui::SetNextItemWidth(-1.0F);
                auto const submitted{ImGui::InputText("##name",
                                                      tagged_union_alternative_name_.data(),
                                                      tagged_union_alternative_name_.size(),
                                                      ImGuiInputTextFlags_EnterReturnsTrue)};
                if (!pending.has_value() && (submitted || ImGui::IsItemDeactivatedAfterEdit())) {
                    pending = *schema;
                    pending->alternatives[index].name = tagged_union_alternative_name_.data();
                    selected_after_edit = pending->alternatives[index].name;
                }
            } else {
                ImGui::TextUnformatted(alternative.name.c_str());
            }

            ImGui::TableNextColumn();
            if (row_selected) {
                ImGui::SetNextItemWidth(std::max(60.0F, ImGui::GetContentRegionAvail().x - 58.0F));
                auto const submitted{ImGui::InputText("##type",
                                                      tagged_union_alternative_type_.data(),
                                                      tagged_union_alternative_type_.size(),
                                                      ImGuiInputTextFlags_EnterReturnsTrue)};
                if (!pending.has_value() && (submitted || ImGui::IsItemDeactivatedAfterEdit())) {
                    pending = *schema;
                    pending->alternatives[index].type.name = tagged_union_alternative_type_.data();
                }
                ImGui::SameLine();
                if (auto picked{draw_type_picker(node.identity.module_name, node.identity)}) {
                    pending = *schema;
                    pending->alternatives[index].type.name = std::move(*picked);
                }
                ImGui::SameLine();
                if (index < tagged_union.alternatives.size() && ImGui::SmallButton(">")) {
                    navigate_to = tagged_union.alternatives[index].semantic_type.type;
                }
            } else {
                ImGui::TextUnformatted(alternative.type.name.c_str());
            }

            ImGui::TableNextColumn();
            if (row_selected) {
                if (ImGui::Checkbox("##fixed-array", &tagged_union_alternative_is_array_)) {
                    pending = *schema;
                    pending->alternatives[index].count =
                        tagged_union_alternative_is_array_
                            ? std::optional{tagged_union_alternative_count_}
                            : std::nullopt;
                }
            } else {
                ImGui::TextUnformatted(alternative.count.has_value() ? "yes" : "-");
            }

            ImGui::TableNextColumn();
            if (row_selected && tagged_union_alternative_is_array_) {
                ImGui::SetNextItemWidth(96.0F);
                auto const submitted{ImGui::InputScalar("##count",
                                                        ImGuiDataType_U64,
                                                        &tagged_union_alternative_count_,
                                                        nullptr,
                                                        nullptr,
                                                        "%llu",
                                                        ImGuiInputTextFlags_EnterReturnsTrue)};
                if (!pending.has_value() && (submitted || ImGui::IsItemDeactivatedAfterEdit())) {
                    pending = *schema;
                    pending->alternatives[index].count = tagged_union_alternative_count_;
                }
            } else if (alternative.count.has_value()) {
                ImGui::Text("%llu", static_cast<unsigned long long>(*alternative.count));
            } else {
                ImGui::TextUnformatted("1");
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    if (navigate_to.has_value()) {
        selected_type_ = *navigate_to;
        selected_field_.clear();
        return true;
    }
    if (pending.has_value()) {
        auto const invalid_alternative{
            std::ranges::find_if(pending->alternatives, [](auto const& alternative) {
                return alternative.name.empty() || alternative.type.name.empty() ||
                       alternative.tag.empty() || alternative.count == 0;
            })};
        if (invalid_alternative != pending->alternatives.end()) {
            schema_edit_message_ = "Tagged-union name, type, tag, and positive count are required.";
            return false;
        }
        if (apply_document_edit(
                ReplaceTaggedUnion{.declaration = *declaration, .schema = std::move(*pending)})) {
            selected_field_ = std::move(selected_after_edit);
            return true;
        }
    }
    return false;
}

auto PlannerUi::draw_record_editor(TypeNode const& node, RecordType const& record) -> bool {
    if (!document_.has_value()) {
        return false;
    }
    auto const declaration{document_->find_declaration(node.identity)};
    if (!declaration.has_value()) {
        return false;
    }
    auto const* schema{document_->record_schema(*declaration)};
    if (schema == nullptr) {
        return false;
    }

    if (selected_field_.empty() && !schema->members.empty()) {
        selected_field_ = schema->members.front().name;
    }
    auto selected{
        std::ranges::find(schema->members, selected_field_, &codegen::RecordMemberSchema::name)};
    if (selected == schema->members.end() && !schema->members.empty()) {
        selected = schema->members.begin();
        selected_field_ = selected->name;
    }
    auto const selected_index{selected == schema->members.end()
                                  ? std::optional<std::size_t>{}
                                  : std::optional<std::size_t>{static_cast<std::size_t>(
                                        selected - schema->members.begin())}};
    if (record_editor_declaration_ != declaration || record_editor_member_ != selected_field_) {
        record_editor_declaration_ = declaration;
        record_editor_member_ = selected_field_;
        std::snprintf(record_export_specifier_.data(),
                      record_export_specifier_.size(),
                      "%s",
                      schema->export_specifier.value_or("").c_str());
        if (selected != schema->members.end()) {
            std::snprintf(record_member_name_.data(),
                          record_member_name_.size(),
                          "%s",
                          selected->name.c_str());
            std::snprintf(record_member_type_.data(),
                          record_member_type_.size(),
                          "%s",
                          selected->type.name.c_str());
            record_member_is_array_ = selected->count.has_value();
            record_member_count_ = selected->count.value_or(1);
            std::snprintf(record_relationship_target_.data(),
                          record_relationship_target_.size(),
                          "%s",
                          selected->relationship.has_value()
                              ? selected->relationship->target.name.c_str()
                              : "");
            auto const relationship_kind{
                selected->relationship.has_value()
                    ? std::ranges::find(semantic_relationship_kinds, selected->relationship->kind)
                    : semantic_relationship_kinds.end()};
            record_relationship_kind_ =
                relationship_kind == semantic_relationship_kinds.end()
                    ? 0
                    : static_cast<int>(relationship_kind - semantic_relationship_kinds.begin());
            auto const relationship_unit{
                selected->relationship.has_value() && selected->relationship->unit.has_value()
                    ? std::ranges::find(semantic_relationship_units, *selected->relationship->unit)
                    : semantic_relationship_units.end()};
            record_relationship_unit_ =
                relationship_unit == semantic_relationship_units.end()
                    ? 0
                    : static_cast<int>(relationship_unit - semantic_relationship_units.begin());
        }
    }

    ImGui::SetNextItemWidth(-1.0F);
    auto const export_submitted{ImGui::InputText("Export specifier (optional)##record",
                                                 record_export_specifier_.data(),
                                                 record_export_specifier_.size(),
                                                 ImGuiInputTextFlags_EnterReturnsTrue)};
    if (export_submitted || ImGui::IsItemDeactivatedAfterEdit()) {
        auto const export_specifier{optional_text(record_export_specifier_)};
        if (schema->export_specifier != export_specifier) {
            auto replacement{*schema};
            replacement.export_specifier = export_specifier;
            if (apply_document_edit(
                    ReplaceRecord{.declaration = *declaration, .schema = std::move(replacement)})) {
                selected_field_ = record_editor_member_;
                return true;
            }
        }
    }

    if (ImGui::Button("+ Member")) {
        auto replacement{*schema};
        auto name{unique_record_member_name(replacement.members, "member")};
        replacement.members.push_back(codegen::RecordMemberSchema{
            .name = name,
            .type = codegen::TypeRef{.name = "std::uint32_t", .suffix = {}, .nested = std::nullopt},
            .count = std::nullopt,
            .relationship = std::nullopt});
        if (apply_document_edit(
                ReplaceRecord{.declaration = *declaration, .schema = std::move(replacement)})) {
            selected_field_ = std::move(name);
            return true;
        }
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!selected_index.has_value());
    if (ImGui::Button("Duplicate")) {
        auto replacement{*schema};
        auto copy{replacement.members[*selected_index]};
        copy.name = unique_record_member_name(replacement.members, copy.name + "_copy");
        replacement.members.insert(
            replacement.members.begin() + static_cast<std::ptrdiff_t>(*selected_index + 1), copy);
        if (apply_document_edit(
                ReplaceRecord{.declaration = *declaration, .schema = std::move(replacement)})) {
            selected_field_ = std::move(copy.name);
            return true;
        }
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!selected_index.has_value() || *selected_index == 0);
    if (ImGui::Button("Move up")) {
        auto replacement{*schema};
        std::swap(replacement.members[*selected_index], replacement.members[*selected_index - 1]);
        if (apply_document_edit(
                ReplaceRecord{.declaration = *declaration, .schema = std::move(replacement)})) {
            selected_field_ = record_editor_member_;
            return true;
        }
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!selected_index.has_value() ||
                         *selected_index + 1 >= schema->members.size());
    if (ImGui::Button("Move down")) {
        auto replacement{*schema};
        std::swap(replacement.members[*selected_index], replacement.members[*selected_index + 1]);
        if (apply_document_edit(
                ReplaceRecord{.declaration = *declaration, .schema = std::move(replacement)})) {
            selected_field_ = record_editor_member_;
            return true;
        }
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!selected_index.has_value() || schema->members.size() == 1);
    if (ImGui::Button("Delete")) {
        auto replacement{*schema};
        auto const deleted_name{replacement.members[*selected_index].name};
        replacement.members.erase(replacement.members.begin() +
                                  static_cast<std::ptrdiff_t>(*selected_index));
        auto const next_index{std::min(*selected_index, replacement.members.size() - 1)};
        auto const next_name{replacement.members[next_index].name};
        if (apply_document_edit(
                ReplaceRecord{.declaration = *declaration, .schema = std::move(replacement)})) {
            record_access_members_.erase(deleted_name);
            selected_field_ = next_name;
            return true;
        }
    }
    ImGui::EndDisabled();
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Selected only")) {
        record_access_members_.clear();
        record_access_set_explicit_ = false;
    }
    ImGui::SameLine();
    if (ImGui::Button("Access all")) {
        record_access_members_.clear();
        for (auto const& member : schema->members) {
            record_access_members_.insert_or_assign(member.name, access_operation_);
        }
        record_access_set_explicit_ = true;
    }

    std::optional<codegen::RecordSchema> pending;
    std::optional<TypeId> navigate_to;
    std::optional<std::pair<std::string, std::string>> renamed_member;
    auto selected_after_edit{selected_field_};
    if (ImGui::BeginTable("record-schema-members",
                          7,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                              ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Edit", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Access", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Operation", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Name");
        ImGui::TableSetupColumn("Semantic type");
        ImGui::TableSetupColumn("Fixed array", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Count", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableHeadersRow();
        for (std::size_t index{}; index < schema->members.size(); ++index) {
            auto const& member{schema->members[index]};
            auto const row_selected{selected_field_ == member.name};
            ImGui::PushID(static_cast<int>(index));
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            if (ImGui::Selectable("::", row_selected, ImGuiSelectableFlags_SpanAllColumns)) {
                selected_field_ = member.name;
                record_editor_declaration_.reset();
            }
            if (ImGui::BeginDragDropSource()) {
                ImGui::SetDragDropPayload("RECORD_MEMBER_ROW", &index, sizeof(index));
                ImGui::Text("Move %s", member.name.c_str());
                ImGui::EndDragDropSource();
            }
            if (ImGui::BeginDragDropTarget()) {
                if (auto const* payload{ImGui::AcceptDragDropPayload("RECORD_MEMBER_ROW")}) {
                    auto const source_index{*static_cast<std::size_t const*>(payload->Data)};
                    if (source_index < schema->members.size() && source_index != index) {
                        pending = *schema;
                        selected_after_edit = pending->members[source_index].name;
                        move_element(pending->members, source_index, index);
                    }
                }
                ImGui::EndDragDropTarget();
            }

            ImGui::TableNextColumn();
            auto accessed{record_access_set_explicit_ ? record_access_members_.contains(member.name)
                                                      : selected_field_ == member.name};
            if (ImGui::Checkbox("##access", &accessed)) {
                if (!record_access_set_explicit_) {
                    record_access_members_.clear();
                    if (!selected_field_.empty()) {
                        record_access_members_.insert_or_assign(selected_field_, access_operation_);
                    }
                    record_access_set_explicit_ = true;
                }
                if (accessed) {
                    record_access_members_.insert_or_assign(member.name, access_operation_);
                } else {
                    record_access_members_.erase(member.name);
                }
            }

            ImGui::TableNextColumn();
            if (accessed) {
                auto operation{access_operation_};
                if (record_access_set_explicit_) {
                    if (auto const found{record_access_members_.find(member.name)};
                        found != record_access_members_.end()) {
                        operation = found->second;
                    }
                }
                auto operation_index{static_cast<int>(operation)};
                ImGui::SetNextItemWidth(105.0F);
                if (ImGui::Combo(
                        "##access-operation", &operation_index, "Read\0Write\0Read + write\0")) {
                    if (!record_access_set_explicit_) {
                        record_access_members_.clear();
                        if (!selected_field_.empty()) {
                            record_access_members_.insert_or_assign(selected_field_,
                                                                    access_operation_);
                        }
                        record_access_set_explicit_ = true;
                    }
                    record_access_members_.insert_or_assign(
                        member.name, static_cast<AccessOperation>(operation_index));
                }
            } else {
                ImGui::TextDisabled("-");
            }

            ImGui::TableNextColumn();
            if (row_selected) {
                ImGui::SetNextItemWidth(-1.0F);
                auto const submitted{ImGui::InputText("##name",
                                                      record_member_name_.data(),
                                                      record_member_name_.size(),
                                                      ImGuiInputTextFlags_EnterReturnsTrue)};
                if (submitted || ImGui::IsItemDeactivatedAfterEdit()) {
                    pending = *schema;
                    pending->members[index].name = record_member_name_.data();
                    selected_after_edit = pending->members[index].name;
                    renamed_member = std::pair{member.name, selected_after_edit};
                }
            } else {
                ImGui::TextUnformatted(member.name.c_str());
            }

            ImGui::TableNextColumn();
            if (row_selected) {
                ImGui::SetNextItemWidth(std::max(60.0F, ImGui::GetContentRegionAvail().x - 58.0F));
                auto const submitted{ImGui::InputText("##type",
                                                      record_member_type_.data(),
                                                      record_member_type_.size(),
                                                      ImGuiInputTextFlags_EnterReturnsTrue)};
                if (!pending.has_value() && (submitted || ImGui::IsItemDeactivatedAfterEdit())) {
                    pending = *schema;
                    pending->members[index].type.name = record_member_type_.data();
                }
                ImGui::SameLine();
                if (auto picked{draw_type_picker(node.identity.module_name, node.identity)}) {
                    pending = *schema;
                    pending->members[index].type.name = std::move(*picked);
                }
                ImGui::SameLine();
                if (index < record.members.size() && ImGui::SmallButton(">")) {
                    navigate_to = record.members[index].semantic_type.type;
                }
            } else {
                ImGui::TextUnformatted(member.type.name.c_str());
            }

            ImGui::TableNextColumn();
            if (row_selected) {
                if (ImGui::Checkbox("##fixed-array", &record_member_is_array_)) {
                    pending = *schema;
                    pending->members[index].count = record_member_is_array_
                                                      ? std::optional{record_member_count_}
                                                      : std::nullopt;
                }
            } else {
                ImGui::TextUnformatted(member.count.has_value() ? "yes" : "-");
            }

            ImGui::TableNextColumn();
            if (row_selected && record_member_is_array_) {
                ImGui::SetNextItemWidth(96.0F);
                auto const submitted{ImGui::InputScalar("##count",
                                                        ImGuiDataType_U64,
                                                        &record_member_count_,
                                                        nullptr,
                                                        nullptr,
                                                        "%llu",
                                                        ImGuiInputTextFlags_EnterReturnsTrue)};
                if (!pending.has_value() && (submitted || ImGui::IsItemDeactivatedAfterEdit())) {
                    pending = *schema;
                    pending->members[index].count = record_member_count_;
                }
            } else if (member.count.has_value()) {
                ImGui::Text("%llu", static_cast<unsigned long long>(*member.count));
            } else {
                ImGui::TextUnformatted("1");
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    if (!pending.has_value() && selected_index.has_value()) {
        auto const member_name{schema->members[*selected_index].name};
        auto const& member{schema->members[*selected_index]};
        auto const* resolved_member{
            *selected_index < record.members.size() ? &record.members[*selected_index] : nullptr};
        auto const current_kind{semantic_relationship_kinds[static_cast<std::size_t>(
            std::clamp(record_relationship_kind_,
                       0,
                       static_cast<int>(semantic_relationship_kinds.size() - 1)))]};
        auto const current_unit{semantic_relationship_units[static_cast<std::size_t>(
            std::clamp(record_relationship_unit_,
                       0,
                       static_cast<int>(semantic_relationship_units.size() - 1)))]};
        ImGui::SeparatorText("Selected member relationship");
        ImGui::SetNextItemWidth(180.0F);
        if (ImGui::BeginCombo("Kind", codegen::semantic_relation_kind_name(current_kind).data())) {
            for (std::size_t kind_index{}; kind_index < semantic_relationship_kinds.size();
                 ++kind_index) {
                auto const kind{semantic_relationship_kinds[kind_index]};
                auto const chosen{record_relationship_kind_ == static_cast<int>(kind_index)};
                if (ImGui::Selectable(codegen::semantic_relation_kind_name(kind).data(), chosen)) {
                    record_relationship_kind_ = static_cast<int>(kind_index);
                    if (member.relationship.has_value()) {
                        auto replacement{*schema};
                        replacement.members[*selected_index].relationship->kind = kind;
                        replacement.members[*selected_index].relationship->unit =
                            kind == codegen::SemanticRelationKind::offset_into
                                ? std::optional{current_unit}
                                : std::nullopt;
                        if (apply_document_edit(ReplaceRecord{.declaration = *declaration,
                                                              .schema = std::move(replacement)})) {
                            record_editor_declaration_.reset();
                            selected_field_ = member_name;
                            ImGui::EndCombo();
                            return true;
                        }
                    }
                }
            }
            ImGui::EndCombo();
        }
        if (current_kind == codegen::SemanticRelationKind::offset_into) {
            ImGui::SetNextItemWidth(180.0F);
            if (ImGui::BeginCombo("Unit",
                                  codegen::semantic_relation_unit_name(current_unit).data())) {
                for (std::size_t unit_index{}; unit_index < semantic_relationship_units.size();
                     ++unit_index) {
                    auto const unit{semantic_relationship_units[unit_index]};
                    auto const chosen{record_relationship_unit_ == static_cast<int>(unit_index)};
                    if (ImGui::Selectable(codegen::semantic_relation_unit_name(unit).data(),
                                          chosen)) {
                        record_relationship_unit_ = static_cast<int>(unit_index);
                        if (member.relationship.has_value()) {
                            auto replacement{*schema};
                            replacement.members[*selected_index].relationship->unit = unit;
                            if (apply_document_edit(
                                    ReplaceRecord{.declaration = *declaration,
                                                  .schema = std::move(replacement)})) {
                                record_editor_declaration_.reset();
                                selected_field_ = member_name;
                                ImGui::EndCombo();
                                return true;
                            }
                        }
                    }
                }
                ImGui::EndCombo();
            }
        }

        ImGui::SetNextItemWidth(std::max(80.0F, ImGui::GetContentRegionAvail().x - 132.0F));
        auto const target_submitted{ImGui::InputText("Target",
                                                     record_relationship_target_.data(),
                                                     record_relationship_target_.size(),
                                                     ImGuiInputTextFlags_EnterReturnsTrue)};
        if (member.relationship.has_value() &&
            (target_submitted || ImGui::IsItemDeactivatedAfterEdit())) {
            if (record_relationship_target_.front() == '\0') {
                schema_edit_message_ = "Relationship target cannot be empty.";
            } else {
                auto replacement{*schema};
                replacement.members[*selected_index].relationship->target.name =
                    record_relationship_target_.data();
                if (apply_document_edit(ReplaceRecord{.declaration = *declaration,
                                                      .schema = std::move(replacement)})) {
                    record_editor_declaration_.reset();
                    selected_field_ = member_name;
                    return true;
                }
            }
        }
        ImGui::SameLine();
        ImGui::PushID("record-relationship-target");
        auto picked_relationship_target{draw_type_picker(node.identity.module_name, node.identity)};
        ImGui::PopID();
        if (picked_relationship_target.has_value()) {
            auto replacement{*schema};
            replacement.members[*selected_index].relationship = codegen::SemanticRelationSchema{
                .kind = current_kind,
                .target = codegen::TypeRef{.name = *picked_relationship_target,
                                           .suffix = {},
                                           .nested = std::nullopt},
                .unit = current_kind == codegen::SemanticRelationKind::offset_into
                          ? std::optional{current_unit}
                          : std::nullopt};
            if (apply_document_edit(
                    ReplaceRecord{.declaration = *declaration, .schema = std::move(replacement)})) {
                record_editor_declaration_.reset();
                selected_field_ = member_name;
                return true;
            }
        }
        ImGui::SameLine();
        if (member.relationship.has_value()) {
            if (ImGui::SmallButton("Clear")) {
                auto replacement{*schema};
                replacement.members[*selected_index].relationship.reset();
                if (apply_document_edit(ReplaceRecord{.declaration = *declaration,
                                                      .schema = std::move(replacement)})) {
                    record_editor_declaration_.reset();
                    selected_field_ = member_name;
                    return true;
                }
            }
            ImGui::SameLine();
            if (resolved_member != nullptr && resolved_member->relationship.has_value() &&
                ImGui::SmallButton(">")) {
                navigate_to = resolved_member->relationship->target.type;
            }
        } else {
            ImGui::BeginDisabled(record_relationship_target_.front() == '\0');
            if (ImGui::SmallButton("Add")) {
                auto replacement{*schema};
                replacement.members[*selected_index].relationship = codegen::SemanticRelationSchema{
                    .kind = current_kind,
                    .target = codegen::TypeRef{.name = record_relationship_target_.data(),
                                               .suffix = {},
                                               .nested = std::nullopt},
                    .unit = current_kind == codegen::SemanticRelationKind::offset_into
                              ? std::optional{current_unit}
                              : std::nullopt};
                if (apply_document_edit(ReplaceRecord{.declaration = *declaration,
                                                      .schema = std::move(replacement)})) {
                    record_editor_declaration_.reset();
                    selected_field_ = member_name;
                    return true;
                }
            }
            ImGui::EndDisabled();
        }
        ImGui::TextDisabled(
            "The relationship is durable semantic metadata; record offsets and ABI layout still "
            "come only from the member type, count, and target profile.");
    }

    if (navigate_to.has_value()) {
        selected_type_ = *navigate_to;
        selected_field_.clear();
        record_access_members_.clear();
        record_access_set_explicit_ = false;
        return true;
    }

    if (pending.has_value()) {
        auto const invalid_member{std::ranges::find_if(pending->members, [](auto const& member) {
            return member.name.empty() || member.type.name.empty() || member.count == 0;
        })};
        if (invalid_member != pending->members.end()) {
            if (invalid_member->name.empty()) {
                schema_edit_message_ = "Record member name cannot be empty.";
            } else if (invalid_member->type.name.empty()) {
                schema_edit_message_ = "Record member type cannot be empty.";
            } else {
                schema_edit_message_ = "Fixed array count must be greater than zero.";
            }
            return false;
        }
        if (apply_document_edit(
                ReplaceRecord{.declaration = *declaration, .schema = std::move(*pending)})) {
            if (record_access_set_explicit_ && renamed_member.has_value()) {
                auto const existing{record_access_members_.find(renamed_member->first)};
                if (existing != record_access_members_.end()) {
                    auto const operation{existing->second};
                    record_access_members_.erase(existing);
                    record_access_members_.insert_or_assign(renamed_member->second, operation);
                }
            }
            selected_field_ = std::move(selected_after_edit);
            return true;
        }
    }
    return false;
}

auto PlannerUi::draw_soa_editor(TypeNode const& node, SoaType const& soa) -> bool {
    if (!document_.has_value()) {
        return false;
    }
    auto const declaration{document_->find_declaration(node.identity)};
    if (!declaration.has_value()) {
        return false;
    }
    auto const* schema{document_->soa_schema(*declaration)};
    if (schema == nullptr) {
        return false;
    }

    if (selected_field_.empty() && !schema->members.empty()) {
        selected_field_ = schema->members.front().name;
    }
    auto selected{
        std::ranges::find(schema->members, selected_field_, &codegen::SoaMemberSchema::name)};
    if (selected == schema->members.end() && !schema->members.empty()) {
        selected = schema->members.begin();
        selected_field_ = selected->name;
    }
    auto const selected_index{selected == schema->members.end()
                                  ? std::optional<std::size_t>{}
                                  : std::optional<std::size_t>{static_cast<std::size_t>(
                                        selected - schema->members.begin())}};
    auto const selected_is_mask_storage{selected != schema->members.end() &&
                                        schema->field_mask_name.has_value() &&
                                        selected->type.name == *schema->field_mask_name};
    auto const selected_is_final_mask_field{
        selected != schema->members.end() && selected->mask_field &&
        std::ranges::count_if(schema->members,
                              [](auto const& member) { return member.mask_field; }) == 1};

    if (soa_editor_declaration_ != declaration || soa_editor_member_ != selected_field_) {
        soa_editor_declaration_ = declaration;
        soa_editor_member_ = selected_field_;
        std::snprintf(soa_view_name_.data(),
                      soa_view_name_.size(),
                      "%s",
                      schema->view_name.value_or("").c_str());
        std::snprintf(soa_const_view_name_.data(),
                      soa_const_view_name_.size(),
                      "%s",
                      schema->const_view_name.value_or("").c_str());
        std::snprintf(soa_equivalent_type_.data(),
                      soa_equivalent_type_.size(),
                      "%s",
                      schema->equivalent_type.has_value() ? schema->equivalent_type->name.c_str()
                                                          : "");
        std::snprintf(soa_export_specifier_.data(),
                      soa_export_specifier_.size(),
                      "%s",
                      schema->export_specifier.value_or("").c_str());
        soa_using_declarations_.clear();
        soa_using_declarations_.reserve(schema->using_declarations.size());
        for (auto const& using_declaration : schema->using_declarations) {
            std::array<char, 256> declaration_text{};
            std::snprintf(
                declaration_text.data(), declaration_text.size(), "%s", using_declaration.c_str());
            soa_using_declarations_.push_back(declaration_text);
        }
        if (schema->using_declarations.empty()) {
            soa_using_declaration_index_.reset();
        } else if (!soa_using_declaration_index_.has_value() ||
                   *soa_using_declaration_index_ >= schema->using_declarations.size()) {
            soa_using_declaration_index_ = 0;
        }
        soa_function_names_.clear();
        soa_function_return_types_.clear();
        soa_function_names_.reserve(schema->functions.size());
        soa_function_return_types_.reserve(schema->functions.size());
        for (auto const& function : schema->functions) {
            std::array<char, 128> name{};
            std::array<char, 128> return_type{};
            std::snprintf(name.data(), name.size(), "%s", function.name.c_str());
            std::snprintf(
                return_type.data(), return_type.size(), "%s", function.return_type.name.c_str());
            soa_function_names_.push_back(name);
            soa_function_return_types_.push_back(return_type);
        }
        if (schema->functions.empty()) {
            soa_function_index_.reset();
        } else if (!soa_function_index_.has_value() ||
                   *soa_function_index_ >= schema->functions.size()) {
            soa_function_index_ = 0;
        }
        soa_parameter_names_.clear();
        soa_parameter_types_.clear();
        soa_parameter_defaults_.clear();
        if (soa_function_index_.has_value()) {
            auto const& function{schema->functions[*soa_function_index_]};
            auto const& parameters{function.parameters};
            soa_parameter_names_.reserve(parameters.size());
            soa_parameter_types_.reserve(parameters.size());
            soa_parameter_defaults_.reserve(parameters.size());
            for (auto const& parameter : parameters) {
                std::array<char, 128> name{};
                std::array<char, 128> type{};
                std::array<char, 128> default_value{};
                std::snprintf(name.data(), name.size(), "%s", parameter.name.c_str());
                std::snprintf(type.data(), type.size(), "%s", parameter.type.name.c_str());
                std::snprintf(default_value.data(),
                              default_value.size(),
                              "%s",
                              parameter.default_value.value_or("").c_str());
                soa_parameter_names_.push_back(name);
                soa_parameter_types_.push_back(type);
                soa_parameter_defaults_.push_back(default_value);
            }
            if (parameters.empty()) {
                soa_parameter_index_.reset();
            } else if (!soa_parameter_index_.has_value() ||
                       *soa_parameter_index_ >= parameters.size()) {
                soa_parameter_index_ = 0;
            }

            soa_function_body_lines_.clear();
            soa_function_body_lines_.reserve(function.body_lines.size());
            for (auto const& body_line : function.body_lines) {
                soa_function_body_lines_.push_back(body_line);
            }
            if (function.body_lines.empty()) {
                soa_function_body_index_.reset();
            } else if (!soa_function_body_index_.has_value() ||
                       *soa_function_body_index_ >= function.body_lines.size()) {
                soa_function_body_index_ = 0;
            }

            soa_function_dependencies_.clear();
            soa_function_dependencies_.reserve(function.dependencies.size());
            for (auto const& dependency : function.dependencies) {
                soa_function_dependencies_.push_back(dependency);
            }
            if (function.dependencies.empty()) {
                soa_function_dependency_index_.reset();
            } else if (!soa_function_dependency_index_.has_value() ||
                       *soa_function_dependency_index_ >= function.dependencies.size()) {
                soa_function_dependency_index_ = 0;
            }
            soa_function_trailing_return_type_ = function.trailing_return_type.has_value()
                                                   ? function.trailing_return_type->name
                                                   : "";
            soa_function_template_parameters_ = function.template_parameters.value_or("");
            soa_function_requires_clause_ = function.requires_clause.value_or("");
        } else {
            soa_parameter_index_.reset();
            soa_function_body_lines_.clear();
            soa_function_body_index_.reset();
            soa_function_dependencies_.clear();
            soa_function_dependency_index_.reset();
            soa_function_trailing_return_type_.clear();
            soa_function_template_parameters_.clear();
            soa_function_requires_clause_.clear();
        }
        std::snprintf(soa_single_allocation_name_.data(),
                      soa_single_allocation_name_.size(),
                      "%s",
                      schema->single_allocation.value_or("").c_str());
        soa_single_allocation_variant_names_.clear();
        soa_single_allocation_variant_allocators_.clear();
        soa_single_allocation_variant_names_.reserve(schema->single_allocation_variants.size());
        soa_single_allocation_variant_allocators_.reserve(
            schema->single_allocation_variants.size());
        for (auto const& variant : schema->single_allocation_variants) {
            std::array<char, 128> name{};
            std::array<char, 128> allocator{};
            std::snprintf(name.data(), name.size(), "%s", variant.name.c_str());
            std::snprintf(allocator.data(), allocator.size(), "%s", variant.allocator.name.c_str());
            soa_single_allocation_variant_names_.push_back(name);
            soa_single_allocation_variant_allocators_.push_back(allocator);
        }
        if (schema->single_allocation_variants.empty()) {
            soa_single_allocation_variant_index_.reset();
        } else if (!soa_single_allocation_variant_index_.has_value() ||
                   *soa_single_allocation_variant_index_ >=
                       schema->single_allocation_variants.size()) {
            soa_single_allocation_variant_index_ = 0;
        }
        soa_fixed_container_names_.clear();
        if (schema->fixed.has_value()) {
            std::snprintf(soa_fixed_storage_name_.data(),
                          soa_fixed_storage_name_.size(),
                          "%s",
                          schema->fixed->storage_name.c_str());
            soa_fixed_container_names_.reserve(schema->fixed->containers.size());
            for (auto const& container : schema->fixed->containers) {
                std::array<char, 128> name{};
                std::snprintf(name.data(), name.size(), "%s", container.c_str());
                soa_fixed_container_names_.push_back(name);
            }
            if (schema->fixed->containers.empty()) {
                soa_fixed_container_index_.reset();
            } else if (!soa_fixed_container_index_.has_value() ||
                       *soa_fixed_container_index_ >= schema->fixed->containers.size()) {
                soa_fixed_container_index_ = 0;
            }
        } else {
            soa_fixed_storage_name_.front() = '\0';
            soa_fixed_container_index_.reset();
        }
        if (selected != schema->members.end()) {
            std::snprintf(
                soa_member_name_.data(), soa_member_name_.size(), "%s", selected->name.c_str());
            std::snprintf(soa_member_type_.data(),
                          soa_member_type_.size(),
                          "%s",
                          selected->type.name.c_str());
            std::snprintf(soa_member_fixed_schema_.data(),
                          soa_member_fixed_schema_.size(),
                          "%s",
                          selected->fixed_schema.value_or("").c_str());
            std::snprintf(soa_member_nested_schema_.data(),
                          soa_member_nested_schema_.size(),
                          "%s",
                          selected->nested_schema.value_or("").c_str());
            std::snprintf(soa_relationship_target_.data(),
                          soa_relationship_target_.size(),
                          "%s",
                          selected->relationship.has_value()
                              ? selected->relationship->target.name.c_str()
                              : "");
            auto const relationship_kind{
                selected->relationship.has_value()
                    ? std::ranges::find(semantic_relationship_kinds, selected->relationship->kind)
                    : semantic_relationship_kinds.begin()};
            soa_relationship_kind_ =
                relationship_kind == semantic_relationship_kinds.end()
                    ? 0
                    : static_cast<int>(relationship_kind - semantic_relationship_kinds.begin());
            auto const relationship_unit{
                selected->relationship.has_value() && selected->relationship->unit.has_value()
                    ? std::ranges::find(semantic_relationship_units, *selected->relationship->unit)
                    : semantic_relationship_units.begin()};
            soa_relationship_unit_ =
                relationship_unit == semantic_relationship_units.end()
                    ? 0
                    : static_cast<int>(relationship_unit - semantic_relationship_units.begin());
            soa_mask_dimension_names_.clear();
            soa_mask_dimension_extents_.clear();
            soa_mask_dimension_names_.reserve(selected->mask_dimensions.size());
            soa_mask_dimension_extents_.reserve(selected->mask_dimensions.size());
            for (auto const& dimension : selected->mask_dimensions) {
                std::array<char, 128> name{};
                std::array<char, 128> extent{};
                std::snprintf(name.data(), name.size(), "%s", dimension.index_name.c_str());
                std::snprintf(extent.data(), extent.size(), "%s", dimension.extent.c_str());
                soa_mask_dimension_names_.push_back(name);
                soa_mask_dimension_extents_.push_back(extent);
            }
            if (selected->mask_dimensions.empty()) {
                soa_mask_dimension_index_.reset();
            } else if (!soa_mask_dimension_index_.has_value() ||
                       *soa_mask_dimension_index_ >= selected->mask_dimensions.size()) {
                soa_mask_dimension_index_ = 0;
            }
        }
    }

    if (ImGui::Button("+ Column")) {
        auto replacement{*schema};
        auto name{unique_member_name(replacement.members, "column")};
        replacement.members.push_back(codegen::SoaMemberSchema{
            .name = name,
            .kind = codegen::SoaMemberKind::array,
            .type = codegen::TypeRef{.name = "std::uint32_t", .suffix = {}, .nested = std::nullopt},
            .fixed_schema = std::nullopt,
            .nested_schema = std::nullopt,
            .mask_field = false,
            .mask_dimensions = {},
            .relationship = std::nullopt});
        if (apply_document_edit(
                ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})) {
            selected_field_ = std::move(name);
            return true;
        }
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!selected_index.has_value() || selected_is_mask_storage);
    if (ImGui::Button("Duplicate")) {
        auto replacement{*schema};
        auto copy{replacement.members[*selected_index]};
        copy.name = unique_member_name(replacement.members, copy.name + "_copy");
        replacement.members.insert(
            replacement.members.begin() + static_cast<std::ptrdiff_t>(*selected_index + 1), copy);
        if (apply_document_edit(
                ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})) {
            selected_field_ = std::move(copy.name);
            return true;
        }
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!selected_index.has_value() || *selected_index == 0);
    if (ImGui::Button("Move up")) {
        auto replacement{*schema};
        std::swap(replacement.members[*selected_index], replacement.members[*selected_index - 1]);
        if (apply_document_edit(
                ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})) {
            selected_field_ = soa_editor_member_;
            return true;
        }
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!selected_index.has_value() ||
                         *selected_index + 1 >= schema->members.size());
    if (ImGui::Button("Move down")) {
        auto replacement{*schema};
        std::swap(replacement.members[*selected_index], replacement.members[*selected_index + 1]);
        if (apply_document_edit(
                ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})) {
            selected_field_ = soa_editor_member_;
            return true;
        }
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!selected_index.has_value() || schema->members.size() == 1 ||
                         selected_is_mask_storage || selected_is_final_mask_field);
    if (ImGui::Button("Delete")) {
        auto replacement{*schema};
        auto const deleted_name{replacement.members[*selected_index].name};
        replacement.members.erase(replacement.members.begin() +
                                  static_cast<std::ptrdiff_t>(*selected_index));
        auto const next_index{std::min(*selected_index, replacement.members.size() - 1)};
        auto const next_name{replacement.members[next_index].name};
        if (apply_document_edit(
                ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})) {
            soa_access_columns_.erase(deleted_name);
            selected_field_ = next_name;
            return true;
        }
    }
    ImGui::EndDisabled();
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Selected only")) {
        soa_access_columns_.clear();
        soa_access_set_explicit_ = false;
    }
    ImGui::SameLine();
    if (ImGui::Button("Access all")) {
        soa_access_columns_.clear();
        for (auto const& member : schema->members) {
            soa_access_columns_.insert_or_assign(member.name, access_operation_);
        }
        soa_access_set_explicit_ = true;
    }

    std::optional<codegen::SoaSchema> pending;
    std::optional<TypeId> navigate_to;
    std::optional<std::pair<std::string, std::string>> renamed_member;
    auto selected_after_edit{selected_field_};
    if (ImGui::BeginTable("soa-schema-members",
                          6,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                              ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Edit", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Access", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Operation", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Name");
        ImGui::TableSetupColumn("Semantic type");
        ImGui::TableSetupColumn("Kind");
        ImGui::TableHeadersRow();
        for (std::size_t index{}; index < schema->members.size(); ++index) {
            auto const& member{schema->members[index]};
            auto const row_selected{selected_field_ == member.name};
            ImGui::PushID(static_cast<int>(index));
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            if (ImGui::Selectable("::", row_selected, ImGuiSelectableFlags_SpanAllColumns)) {
                selected_field_ = member.name;
                soa_editor_declaration_.reset();
            }
            if (ImGui::BeginDragDropSource()) {
                ImGui::SetDragDropPayload("SOA_MEMBER_ROW", &index, sizeof(index));
                ImGui::Text("Move %s", member.name.c_str());
                ImGui::EndDragDropSource();
            }
            if (ImGui::BeginDragDropTarget()) {
                if (auto const* payload{ImGui::AcceptDragDropPayload("SOA_MEMBER_ROW")}) {
                    auto const source_index{*static_cast<std::size_t const*>(payload->Data)};
                    if (source_index < schema->members.size() && source_index != index) {
                        pending = *schema;
                        selected_after_edit = pending->members[source_index].name;
                        move_element(pending->members, source_index, index);
                    }
                }
                ImGui::EndDragDropTarget();
            }

            ImGui::TableNextColumn();
            auto accessed{soa_access_set_explicit_ ? soa_access_columns_.contains(member.name)
                                                   : selected_field_ == member.name};
            if (ImGui::Checkbox("##access", &accessed)) {
                if (!soa_access_set_explicit_) {
                    soa_access_columns_.clear();
                    if (!selected_field_.empty()) {
                        soa_access_columns_.insert_or_assign(selected_field_, access_operation_);
                    }
                    soa_access_set_explicit_ = true;
                }
                if (accessed) {
                    soa_access_columns_.insert_or_assign(member.name, access_operation_);
                } else {
                    soa_access_columns_.erase(member.name);
                }
            }

            ImGui::TableNextColumn();
            if (accessed) {
                auto operation{access_operation_};
                if (soa_access_set_explicit_) {
                    if (auto const found{soa_access_columns_.find(member.name)};
                        found != soa_access_columns_.end()) {
                        operation = found->second;
                    }
                }
                auto operation_index{static_cast<int>(operation)};
                ImGui::SetNextItemWidth(105.0F);
                if (ImGui::Combo(
                        "##access-operation", &operation_index, "Read\0Write\0Read + write\0")) {
                    if (!soa_access_set_explicit_) {
                        soa_access_columns_.clear();
                        if (!selected_field_.empty()) {
                            soa_access_columns_.insert_or_assign(selected_field_,
                                                                 access_operation_);
                        }
                        soa_access_set_explicit_ = true;
                    }
                    soa_access_columns_.insert_or_assign(
                        member.name, static_cast<AccessOperation>(operation_index));
                }
            } else {
                ImGui::TextDisabled("-");
            }

            ImGui::TableNextColumn();
            auto const row_mask_storage{schema->field_mask_name.has_value() &&
                                        member.type.name == *schema->field_mask_name};
            if (row_selected) {
                ImGui::SetNextItemWidth(-1.0F);
                auto const submitted{ImGui::InputText("##name",
                                                      soa_member_name_.data(),
                                                      soa_member_name_.size(),
                                                      ImGuiInputTextFlags_EnterReturnsTrue)};
                if (submitted || ImGui::IsItemDeactivatedAfterEdit()) {
                    pending = *schema;
                    pending->members[index].name = soa_member_name_.data();
                    selected_after_edit = pending->members[index].name;
                    renamed_member = std::pair{member.name, selected_after_edit};
                }
            } else {
                ImGui::TextUnformatted(member.name.c_str());
            }

            ImGui::TableNextColumn();
            if (row_selected && !row_mask_storage) {
                ImGui::SetNextItemWidth(std::max(60.0F, ImGui::GetContentRegionAvail().x - 58.0F));
                auto const submitted{ImGui::InputText("##type",
                                                      soa_member_type_.data(),
                                                      soa_member_type_.size(),
                                                      ImGuiInputTextFlags_EnterReturnsTrue)};
                if (!pending.has_value() && (submitted || ImGui::IsItemDeactivatedAfterEdit())) {
                    pending = *schema;
                    pending->members[index].type.name = soa_member_type_.data();
                }
                ImGui::SameLine();
                if (auto picked{draw_type_picker(node.identity.module_name, node.identity)}) {
                    pending = *schema;
                    pending->members[index].type.name = std::move(*picked);
                }
                ImGui::SameLine();
                if (index < soa.columns.size() && ImGui::SmallButton(">")) {
                    navigate_to = soa.columns[index].semantic_type.type;
                }
            } else {
                ImGui::TextUnformatted(member.type.name.c_str());
            }

            ImGui::TableNextColumn();
            auto const kind_label{member.kind == codegen::SoaMemberKind::nested ? "nested"
                                                                                : "array"};
            if (row_selected && ImGui::BeginCombo("##kind", kind_label)) {
                if (ImGui::Selectable("array", member.kind == codegen::SoaMemberKind::array)) {
                    pending = *schema;
                    pending->members[index].kind = codegen::SoaMemberKind::array;
                    pending->members[index].fixed_schema.reset();
                    pending->members[index].nested_schema.reset();
                    soa_member_fixed_schema_.front() = '\0';
                    soa_member_nested_schema_.front() = '\0';
                }
                auto const mask_storage{schema->field_mask_name.has_value() &&
                                        member.type.name == *schema->field_mask_name};
                ImGui::BeginDisabled(member.mask_field || mask_storage);
                if (ImGui::Selectable("nested", member.kind == codegen::SoaMemberKind::nested)) {
                    pending = *schema;
                    pending->members[index].kind = codegen::SoaMemberKind::nested;
                }
                ImGui::EndDisabled();
                ImGui::EndCombo();
            } else if (!row_selected) {
                ImGui::TextUnformatted(kind_label);
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    if (selected_index.has_value()) {
        auto const& member{schema->members[*selected_index]};
        auto const* resolved_column{
            *selected_index < soa.columns.size() ? &soa.columns[*selected_index] : nullptr};
        ImGui::SeparatorText("Selected column details");
        if (member.kind == codegen::SoaMemberKind::nested) {
            ImGui::SetNextItemWidth(-1.0F);
            auto const fixed_submitted{ImGui::InputText("Fixed schema (optional)",
                                                        soa_member_fixed_schema_.data(),
                                                        soa_member_fixed_schema_.size(),
                                                        ImGuiInputTextFlags_EnterReturnsTrue)};
            if (fixed_submitted || ImGui::IsItemDeactivatedAfterEdit()) {
                if (!pending.has_value()) {
                    pending = *schema;
                }
                auto value{std::string{soa_member_fixed_schema_.data()}};
                if (value.empty()) {
                    pending->members[*selected_index].fixed_schema.reset();
                } else {
                    pending->members[*selected_index].fixed_schema = std::move(value);
                }
            }

            ImGui::SetNextItemWidth(-1.0F);
            auto const nested_submitted{ImGui::InputText("Nested schema (optional)",
                                                         soa_member_nested_schema_.data(),
                                                         soa_member_nested_schema_.size(),
                                                         ImGuiInputTextFlags_EnterReturnsTrue)};
            if (nested_submitted || ImGui::IsItemDeactivatedAfterEdit()) {
                if (!pending.has_value()) {
                    pending = *schema;
                }
                auto value{std::string{soa_member_nested_schema_.data()}};
                if (value.empty()) {
                    pending->members[*selected_index].nested_schema.reset();
                } else {
                    pending->members[*selected_index].nested_schema = std::move(value);
                }
            }
        } else {
            ImGui::TextDisabled("Fixed and nested schema references apply only to nested columns.");
        }

        if (!pending.has_value()) {
            auto record_module_index{std::optional<std::size_t>{}};
            auto fallback_record_module_index{std::optional<std::size_t>{}};
            auto const& modules{document_->manifest().modules};
            for (std::size_t module_index{}; module_index < modules.size(); ++module_index) {
                auto const* record_module{
                    std::get_if<codegen::RecordModuleSchema>(&modules[module_index])};
                if (record_module == nullptr) {
                    continue;
                }
                if (!fallback_record_module_index.has_value()) {
                    fallback_record_module_index = module_index;
                }
                if (record_module->settings.namespace_name.value_or("") ==
                    node.identity.namespace_name) {
                    record_module_index = module_index;
                    break;
                }
            }
            if (!record_module_index.has_value()) {
                record_module_index = fallback_record_module_index;
            }
            auto const mask_storage{schema->field_mask_name.has_value() &&
                                    member.type.name == *schema->field_mask_name};
            auto const can_create_record{record_module_index.has_value() &&
                                         resolved_column != nullptr && !mask_storage &&
                                         member.kind == codegen::SoaMemberKind::array};
            ImGui::BeginDisabled(!can_create_record);
            if (ImGui::Button("Create record for selected column...")) {
                new_record_module_index_ = *record_module_index;
                auto const suggested_name{suggested_type_name(member.name, "Record")};
                auto& record_module{
                    std::get<codegen::RecordModuleSchema>(modules[new_record_module_index_])};
                auto unique_name{suggested_name};
                auto suffix_number{std::size_t{1}};
                while (std::ranges::find(
                           record_module.records, unique_name, &codegen::RecordSchema::name) !=
                       record_module.records.end()) {
                    unique_name = suggested_name + std::to_string(suffix_number++);
                }
                std::snprintf(
                    new_record_name_.data(), new_record_name_.size(), "%s", unique_name.c_str());
                auto first_member_type{member.type.name};
                auto const& current_type{
                    workspace_.types().type(resolved_column->semantic_type.type)};
                if (!first_member_type.starts_with('@') &&
                    current_type.identity.origin == TypeOrigin::declaration) {
                    first_member_type = current_type.cpp_spelling;
                }
                std::snprintf(new_record_member_type_.data(),
                              new_record_member_type_.size(),
                              "%s",
                              first_member_type.c_str());
                pending_soa_record_binding_ = PendingSoaRecordBinding{
                    .soa_declaration = *declaration, .column_name = member.name};
                open_new_record_dialog_ = true;
            }
            ImGui::EndDisabled();
            if (!record_module_index.has_value()) {
                ImGui::SameLine();
                ImGui::TextDisabled("No record module is available.");
            } else if (resolved_column == nullptr) {
                ImGui::SameLine();
                ImGui::TextDisabled("The selected column type is unresolved.");
            } else if (mask_storage || member.kind != codegen::SoaMemberKind::array) {
                ImGui::SameLine();
                ImGui::TextDisabled("Available for ordinary array columns.");
            } else {
                ImGui::SameLine();
                ImGui::TextDisabled("Creates a shared record, then binds this column.");
            }

            auto const current_kind{semantic_relationship_kinds[static_cast<std::size_t>(
                std::clamp(soa_relationship_kind_,
                           0,
                           static_cast<int>(semantic_relationship_kinds.size() - 1)))]};
            auto const current_unit{semantic_relationship_units[static_cast<std::size_t>(
                std::clamp(soa_relationship_unit_,
                           0,
                           static_cast<int>(semantic_relationship_units.size() - 1)))]};
            ImGui::SeparatorText("Selected column relationship");
            ImGui::SetNextItemWidth(180.0F);
            if (ImGui::BeginCombo("Kind##soa-column-relationship",
                                  codegen::semantic_relation_kind_name(current_kind).data())) {
                for (std::size_t kind_index{}; kind_index < semantic_relationship_kinds.size();
                     ++kind_index) {
                    auto const kind{semantic_relationship_kinds[kind_index]};
                    auto const chosen{soa_relationship_kind_ == static_cast<int>(kind_index)};
                    if (ImGui::Selectable(codegen::semantic_relation_kind_name(kind).data(),
                                          chosen)) {
                        soa_relationship_kind_ = static_cast<int>(kind_index);
                        if (member.relationship.has_value()) {
                            auto replacement{*schema};
                            replacement.members[*selected_index].relationship->kind = kind;
                            replacement.members[*selected_index].relationship->unit =
                                kind == codegen::SemanticRelationKind::offset_into
                                    ? std::optional{current_unit}
                                    : std::nullopt;
                            if (apply_document_edit(ReplaceSoa{.declaration = *declaration,
                                                               .schema = std::move(replacement)})) {
                                soa_editor_declaration_.reset();
                                selected_field_ = member.name;
                                ImGui::EndCombo();
                                return true;
                            }
                        }
                    }
                }
                ImGui::EndCombo();
            }
            if (current_kind == codegen::SemanticRelationKind::offset_into) {
                ImGui::SetNextItemWidth(180.0F);
                if (ImGui::BeginCombo("Unit##soa-column-relationship",
                                      codegen::semantic_relation_unit_name(current_unit).data())) {
                    for (std::size_t unit_index{}; unit_index < semantic_relationship_units.size();
                         ++unit_index) {
                        auto const unit{semantic_relationship_units[unit_index]};
                        auto const chosen{soa_relationship_unit_ == static_cast<int>(unit_index)};
                        if (ImGui::Selectable(codegen::semantic_relation_unit_name(unit).data(),
                                              chosen)) {
                            soa_relationship_unit_ = static_cast<int>(unit_index);
                            if (member.relationship.has_value()) {
                                auto replacement{*schema};
                                replacement.members[*selected_index].relationship->unit = unit;
                                if (apply_document_edit(
                                        ReplaceSoa{.declaration = *declaration,
                                                   .schema = std::move(replacement)})) {
                                    soa_editor_declaration_.reset();
                                    selected_field_ = member.name;
                                    ImGui::EndCombo();
                                    return true;
                                }
                            }
                        }
                    }
                    ImGui::EndCombo();
                }
            }

            ImGui::SetNextItemWidth(std::max(80.0F, ImGui::GetContentRegionAvail().x - 132.0F));
            auto const target_submitted{ImGui::InputText("Target##soa-column-relationship",
                                                         soa_relationship_target_.data(),
                                                         soa_relationship_target_.size(),
                                                         ImGuiInputTextFlags_EnterReturnsTrue)};
            if (member.relationship.has_value() &&
                (target_submitted || ImGui::IsItemDeactivatedAfterEdit())) {
                if (soa_relationship_target_.front() == '\0') {
                    schema_edit_message_ = "Relationship target cannot be empty.";
                } else {
                    auto replacement{*schema};
                    replacement.members[*selected_index].relationship->target.name =
                        soa_relationship_target_.data();
                    if (apply_document_edit(ReplaceSoa{.declaration = *declaration,
                                                       .schema = std::move(replacement)})) {
                        soa_editor_declaration_.reset();
                        selected_field_ = member.name;
                        return true;
                    }
                }
            }
            ImGui::SameLine();
            ImGui::PushID("soa-column-relationship-target");
            auto picked_relationship_target{
                draw_type_picker(node.identity.module_name, node.identity)};
            ImGui::PopID();
            if (picked_relationship_target.has_value()) {
                auto replacement{*schema};
                replacement.members[*selected_index].relationship = codegen::SemanticRelationSchema{
                    .kind = current_kind,
                    .target = codegen::TypeRef{.name = *picked_relationship_target,
                                               .suffix = {},
                                               .nested = std::nullopt},
                    .unit = current_kind == codegen::SemanticRelationKind::offset_into
                              ? std::optional{current_unit}
                              : std::nullopt};
                if (apply_document_edit(ReplaceSoa{.declaration = *declaration,
                                                   .schema = std::move(replacement)})) {
                    soa_editor_declaration_.reset();
                    selected_field_ = member.name;
                    return true;
                }
            }
            ImGui::SameLine();
            if (member.relationship.has_value()) {
                if (ImGui::SmallButton("Clear##soa-column-relationship")) {
                    auto replacement{*schema};
                    replacement.members[*selected_index].relationship.reset();
                    if (apply_document_edit(ReplaceSoa{.declaration = *declaration,
                                                       .schema = std::move(replacement)})) {
                        soa_editor_declaration_.reset();
                        selected_field_ = member.name;
                        return true;
                    }
                }
                ImGui::SameLine();
                if (resolved_column != nullptr && resolved_column->relationship.has_value() &&
                    ImGui::SmallButton(">##soa-column-relationship")) {
                    navigate_to = resolved_column->relationship->target.type;
                }
            } else {
                ImGui::BeginDisabled(soa_relationship_target_.front() == '\0');
                if (ImGui::SmallButton("Add##soa-column-relationship")) {
                    auto replacement{*schema};
                    replacement.members[*selected_index].relationship =
                        codegen::SemanticRelationSchema{
                            .kind = current_kind,
                            .target = codegen::TypeRef{.name = soa_relationship_target_.data(),
                                                       .suffix = {},
                                                       .nested = std::nullopt},
                            .unit = current_kind == codegen::SemanticRelationKind::offset_into
                                      ? std::optional{current_unit}
                                      : std::nullopt};
                    if (apply_document_edit(ReplaceSoa{.declaration = *declaration,
                                                       .schema = std::move(replacement)})) {
                        soa_editor_declaration_.reset();
                        selected_field_ = member.name;
                        return true;
                    }
                }
                ImGui::EndDisabled();
            }
            ImGui::TextDisabled(
                "The relationship is durable semantic metadata; SoA storage, capacity, and "
                "allocator placement remain separate physical/session concerns.");
        }

        auto const mask_configured{schema->field_mask_name.has_value() &&
                                   schema->field_enum_name.has_value()};
        if (!mask_configured) {
            auto const eligible{member.kind == codegen::SoaMemberKind::array};
            ImGui::BeginDisabled(!eligible);
            if (ImGui::Button("Enable generated field mask for this column")) {
                auto replacement{*schema};
                replacement.field_mask_name = schema->name + "FieldMask";
                replacement.field_enum_name = schema->name + "Field";
                replacement.members[*selected_index].mask_field = true;
                auto const storage_name{unique_member_name(replacement.members, "field_mask")};
                replacement.members.push_back(codegen::SoaMemberSchema{
                    .name = storage_name,
                    .kind = codegen::SoaMemberKind::array,
                    .type = codegen::TypeRef{.name = *replacement.field_mask_name,
                                             .suffix = {},
                                             .nested = std::nullopt},
                    .fixed_schema = std::nullopt,
                    .nested_schema = std::nullopt,
                    .mask_field = false,
                    .mask_dimensions = {},
                    .relationship = std::nullopt});
                if (apply_document_edit(ReplaceSoa{.declaration = *declaration,
                                                   .schema = std::move(replacement)})) {
                    selected_field_ = member.name;
                    return true;
                }
            }
            ImGui::EndDisabled();
            if (!eligible) {
                ImGui::TextDisabled("Generated mask fields must be array columns.");
            }
        } else {
            ImGui::Text("Generated mask: %s", schema->field_mask_name->c_str());
            ImGui::Text("Generated field enum: %s", schema->field_enum_name->c_str());
            auto const storage_member{member.type.name == *schema->field_mask_name};
            auto const mask_field_count{std::ranges::count_if(
                schema->members, [](auto const& candidate) { return candidate.mask_field; })};
            auto included{member.mask_field};
            auto const can_toggle{member.kind == codegen::SoaMemberKind::array && !storage_member &&
                                  (!included || mask_field_count > 1)};
            ImGui::BeginDisabled(!can_toggle);
            if (ImGui::Checkbox("Included in generated field mask", &included)) {
                auto replacement{*schema};
                replacement.members[*selected_index].mask_field = included;
                if (!included) {
                    replacement.members[*selected_index].mask_dimensions.clear();
                }
                if (apply_document_edit(ReplaceSoa{.declaration = *declaration,
                                                   .schema = std::move(replacement)})) {
                    soa_editor_declaration_.reset();
                    return true;
                }
            }
            ImGui::EndDisabled();
            if (storage_member) {
                ImGui::TextDisabled("This is the generated mask storage column.");
            } else if (member.kind != codegen::SoaMemberKind::array) {
                ImGui::TextDisabled("Generated mask fields must be array columns.");
            } else if (member.mask_field && mask_field_count == 1) {
                ImGui::TextDisabled("At least one generated mask field is required.");
            }

            if (ImGui::Button("Disable generated field mask")) {
                auto replacement{*schema};
                auto const mask_type{*replacement.field_mask_name};
                auto selected_after_disable{selected_field_};
                auto const selected_is_storage{member.type.name == mask_type};
                std::erase_if(replacement.members, [&](auto const& candidate) {
                    return candidate.type.name == mask_type;
                });
                for (auto& candidate : replacement.members) {
                    candidate.mask_field = false;
                    candidate.mask_dimensions.clear();
                }
                replacement.field_mask_name.reset();
                replacement.field_enum_name.reset();
                if (selected_is_storage && !replacement.members.empty()) {
                    selected_after_disable = replacement.members.front().name;
                }
                if (apply_document_edit(ReplaceSoa{.declaration = *declaration,
                                                   .schema = std::move(replacement)})) {
                    selected_field_ = std::move(selected_after_disable);
                    return true;
                }
            }
        }

        if (member.mask_field) {
            ImGui::SeparatorText("Mask dimensions");
            if (ImGui::Button("+ Dimension")) {
                auto replacement{*schema};
                auto& dimensions{replacement.members[*selected_index].mask_dimensions};
                auto const name{unique_mask_dimension_name(dimensions, "index")};
                dimensions.push_back(
                    codegen::SoaMaskDimensionSchema{.index_name = name, .extent = "1"});
                auto const new_index{dimensions.size() - 1};
                if (apply_document_edit(ReplaceSoa{.declaration = *declaration,
                                                   .schema = std::move(replacement)})) {
                    soa_mask_dimension_index_ = new_index;
                    soa_editor_declaration_.reset();
                    return true;
                }
            }
            ImGui::SameLine();
            auto const dimension_index{soa_mask_dimension_index_};
            auto const has_dimension{dimension_index.has_value() &&
                                     *dimension_index < member.mask_dimensions.size()};
            ImGui::BeginDisabled(!has_dimension);
            if (ImGui::Button("Duplicate dimension")) {
                auto replacement{*schema};
                auto& dimensions{replacement.members[*selected_index].mask_dimensions};
                auto copy{dimensions[*dimension_index]};
                copy.index_name = unique_mask_dimension_name(dimensions, copy.index_name + "_copy");
                dimensions.insert(
                    dimensions.begin() + static_cast<std::ptrdiff_t>(*dimension_index + 1), copy);
                if (apply_document_edit(ReplaceSoa{.declaration = *declaration,
                                                   .schema = std::move(replacement)})) {
                    soa_mask_dimension_index_ = *dimension_index + 1;
                    soa_editor_declaration_.reset();
                    return true;
                }
            }
            ImGui::SameLine();
            ImGui::BeginDisabled(!has_dimension || *dimension_index == 0);
            if (ImGui::Button("Dimension up")) {
                auto replacement{*schema};
                auto& dimensions{replacement.members[*selected_index].mask_dimensions};
                std::swap(dimensions[*dimension_index], dimensions[*dimension_index - 1]);
                if (apply_document_edit(ReplaceSoa{.declaration = *declaration,
                                                   .schema = std::move(replacement)})) {
                    soa_mask_dimension_index_ = *dimension_index - 1;
                    soa_editor_declaration_.reset();
                    return true;
                }
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::BeginDisabled(!has_dimension ||
                                 *dimension_index + 1 >= member.mask_dimensions.size());
            if (ImGui::Button("Dimension down")) {
                auto replacement{*schema};
                auto& dimensions{replacement.members[*selected_index].mask_dimensions};
                std::swap(dimensions[*dimension_index], dimensions[*dimension_index + 1]);
                if (apply_document_edit(ReplaceSoa{.declaration = *declaration,
                                                   .schema = std::move(replacement)})) {
                    soa_mask_dimension_index_ = *dimension_index + 1;
                    soa_editor_declaration_.reset();
                    return true;
                }
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            if (ImGui::Button("Delete dimension")) {
                auto replacement{*schema};
                auto& dimensions{replacement.members[*selected_index].mask_dimensions};
                dimensions.erase(dimensions.begin() +
                                 static_cast<std::ptrdiff_t>(*dimension_index));
                auto const next_index{
                    dimensions.empty()
                        ? std::optional<std::size_t>{}
                        : std::optional{std::min(*dimension_index, dimensions.size() - 1)}};
                if (apply_document_edit(ReplaceSoa{.declaration = *declaration,
                                                   .schema = std::move(replacement)})) {
                    soa_mask_dimension_index_ = next_index;
                    soa_editor_declaration_.reset();
                    return true;
                }
            }
            ImGui::EndDisabled();

            if (soa_mask_dimension_names_.size() == member.mask_dimensions.size() &&
                soa_mask_dimension_extents_.size() == member.mask_dimensions.size() &&
                ImGui::BeginTable("soa-mask-dimensions",
                                  3,
                                  ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                      ImGuiTableFlags_Resizable |
                                      ImGuiTableFlags_SizingStretchProp)) {
                ImGui::TableSetupColumn("Edit", ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableSetupColumn("Index name");
                ImGui::TableSetupColumn("Extent");
                ImGui::TableHeadersRow();
                for (std::size_t index{}; index < member.mask_dimensions.size(); ++index) {
                    ImGui::PushID(static_cast<int>(index));
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    auto const row_selected{soa_mask_dimension_index_ == index};
                    if (ImGui::Selectable(
                            "::", row_selected, ImGuiSelectableFlags_SpanAllColumns)) {
                        soa_mask_dimension_index_ = index;
                    }
                    if (ImGui::BeginDragDropSource()) {
                        ImGui::SetDragDropPayload("SOA_MASK_DIMENSION_ROW", &index, sizeof(index));
                        ImGui::Text("Move %s", member.mask_dimensions[index].index_name.c_str());
                        ImGui::EndDragDropSource();
                    }
                    if (ImGui::BeginDragDropTarget()) {
                        if (auto const* payload{
                                ImGui::AcceptDragDropPayload("SOA_MASK_DIMENSION_ROW")}) {
                            auto const source_index{
                                *static_cast<std::size_t const*>(payload->Data)};
                            if (source_index < member.mask_dimensions.size() &&
                                source_index != index) {
                                pending = *schema;
                                move_element(pending->members[*selected_index].mask_dimensions,
                                             source_index,
                                             index);
                                soa_mask_dimension_index_ = index;
                            }
                        }
                        ImGui::EndDragDropTarget();
                    }

                    ImGui::TableNextColumn();
                    ImGui::SetNextItemWidth(-1.0F);
                    auto const name_submitted{
                        ImGui::InputText("##name",
                                         soa_mask_dimension_names_[index].data(),
                                         soa_mask_dimension_names_[index].size(),
                                         ImGuiInputTextFlags_EnterReturnsTrue)};
                    if (name_submitted || ImGui::IsItemDeactivatedAfterEdit()) {
                        if (!pending.has_value()) {
                            pending = *schema;
                        }
                        pending->members[*selected_index].mask_dimensions[index].index_name =
                            soa_mask_dimension_names_[index].data();
                    }

                    ImGui::TableNextColumn();
                    ImGui::SetNextItemWidth(-1.0F);
                    auto const extent_submitted{
                        ImGui::InputText("##extent",
                                         soa_mask_dimension_extents_[index].data(),
                                         soa_mask_dimension_extents_[index].size(),
                                         ImGuiInputTextFlags_EnterReturnsTrue)};
                    if (extent_submitted || ImGui::IsItemDeactivatedAfterEdit()) {
                        if (!pending.has_value()) {
                            pending = *schema;
                        }
                        pending->members[*selected_index].mask_dimensions[index].extent =
                            soa_mask_dimension_extents_[index].data();
                    }
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }
        }
    }

    ImGui::SeparatorText("Storage operations");
    auto const all_storage_operations{codegen::all_storage_operations()};
    auto const all_operations_enabled{
        std::ranges::all_of(all_storage_operations, [&](auto const operation) {
            return has_storage_operation(schema->operations, operation);
        })};
    ImGui::BeginDisabled(pending.has_value() || all_operations_enabled);
    auto const enable_all_operations{ImGui::Button("Enable all operations")};
    ImGui::EndDisabled();
    if (enable_all_operations) {
        auto replacement{*schema};
        replacement.operations = all_storage_operations;
        if (apply_document_edit(
                ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})) {
            return true;
        }
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(pending.has_value() || schema->operations.empty());
    auto const disable_all_operations{ImGui::Button("Disable all operations")};
    ImGui::EndDisabled();
    if (disable_all_operations) {
        auto replacement{*schema};
        replacement.operations.clear();
        if (apply_document_edit(
                ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})) {
            return true;
        }
    }
    auto operation_applied{false};
    ImGui::BeginDisabled(pending.has_value());
    if (ImGui::BeginTable("soa-storage-operations", 2, ImGuiTableFlags_SizingStretchSame)) {
        for (auto const operation : all_storage_operations) {
            auto const descriptor{storage_operation_descriptor(operation)};
            ImGui::TableNextColumn();
            auto enabled{has_storage_operation(schema->operations, operation)};
            auto const toggled{ImGui::Checkbox(descriptor.source_name, &enabled)};
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", descriptor.description);
            }
            if (toggled) {
                auto replacement{*schema};
                replacement.operations =
                    with_storage_operation(schema->operations, operation, enabled);
                if (apply_document_edit(ReplaceSoa{.declaration = *declaration,
                                                   .schema = std::move(replacement)})) {
                    operation_applied = true;
                    break;
                }
            }
        }
        ImGui::EndTable();
    }
    ImGui::EndDisabled();
    if (operation_applied) {
        return true;
    }

    ImGui::SeparatorText("Generation policy");
    ImGui::SetNextItemWidth(-1.0F);
    auto const export_submitted{ImGui::InputText("Export specifier (optional)",
                                                 soa_export_specifier_.data(),
                                                 soa_export_specifier_.size(),
                                                 ImGuiInputTextFlags_EnterReturnsTrue)};
    if (export_submitted || ImGui::IsItemDeactivatedAfterEdit()) {
        if (!pending.has_value()) {
            pending = *schema;
        }
        pending->export_specifier = optional_text(soa_export_specifier_);
    }

    ImGui::TextUnformatted("Equivalent row type (optional)");
    ImGui::SetNextItemWidth(std::max(60.0F, ImGui::GetContentRegionAvail().x - 58.0F));
    auto const equivalent_submitted{ImGui::InputText("##soa-equivalent-type",
                                                     soa_equivalent_type_.data(),
                                                     soa_equivalent_type_.size(),
                                                     ImGuiInputTextFlags_EnterReturnsTrue)};
    if (equivalent_submitted || ImGui::IsItemDeactivatedAfterEdit()) {
        if (!pending.has_value()) {
            pending = *schema;
        }
        if (soa_equivalent_type_.front() == '\0') {
            pending->equivalent_type.reset();
        } else if (pending->equivalent_type.has_value()) {
            pending->equivalent_type->name = soa_equivalent_type_.data();
        } else {
            pending->equivalent_type = codegen::TypeRef{
                .name = soa_equivalent_type_.data(), .suffix = {}, .nested = std::nullopt};
        }
    }
    ImGui::SameLine();
    ImGui::PushID("soa-equivalent-type");
    if (auto picked{draw_type_picker(node.identity.module_name, node.identity)}) {
        if (!pending.has_value()) {
            pending = *schema;
        }
        if (pending->equivalent_type.has_value()) {
            pending->equivalent_type->name = *picked;
        } else {
            pending->equivalent_type =
                codegen::TypeRef{.name = *picked, .suffix = {}, .nested = std::nullopt};
        }
    }
    ImGui::PopID();
    ImGui::SameLine();
    ImGui::BeginDisabled(pending.has_value() || !soa.equivalent_type.has_value());
    if (ImGui::SmallButton(">##equivalent-type")) {
        navigate_to = soa.equivalent_type->type;
    }
    ImGui::EndDisabled();

    auto layout_only{schema->layout_only};
    ImGui::BeginDisabled(pending.has_value());
    auto const layout_only_toggled{ImGui::Checkbox("Layout only", &layout_only)};
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Keep this declaration available as a nested layout without emitting its "
                          "standalone storage/view API.");
    }
    ImGui::EndDisabled();
    if (layout_only_toggled) {
        auto replacement{*schema};
        replacement.layout_only = layout_only;
        if (apply_document_edit(
                ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})) {
            return true;
        }
    }
    auto copy_element_memberwise{schema->copy_element_memberwise};
    ImGui::BeginDisabled(pending.has_value());
    auto const copy_memberwise_toggled{
        ImGui::Checkbox("Copy elements memberwise", &copy_element_memberwise)};
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Generate direct member assignment for copy-element; this has no effect "
                          "unless the copy-element storage operation is enabled.");
    }
    ImGui::EndDisabled();
    if (copy_memberwise_toggled) {
        auto replacement{*schema};
        replacement.copy_element_memberwise = copy_element_memberwise;
        if (apply_document_edit(
                ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})) {
            return true;
        }
    }

    ImGui::SeparatorText("Using declarations");
    ImGui::BeginDisabled(pending.has_value());
    auto const add_using_declaration{ImGui::Button("+ Using declaration")};
    ImGui::EndDisabled();
    if (add_using_declaration) {
        auto replacement{*schema};
        replacement.using_declarations.push_back(
            unique_soa_using_declaration(replacement.using_declarations));
        auto const new_index{replacement.using_declarations.size() - 1};
        if (apply_document_edit(
                ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})) {
            soa_using_declaration_index_ = new_index;
            soa_editor_declaration_.reset();
            return true;
        }
    }
    ImGui::SameLine();
    auto const using_index{soa_using_declaration_index_};
    auto const has_using{using_index.has_value() &&
                         *using_index < schema->using_declarations.size()};
    ImGui::BeginDisabled(pending.has_value() || !has_using || *using_index == 0);
    auto const move_using_up{ImGui::Button("Using up")};
    ImGui::EndDisabled();
    if (move_using_up) {
        auto replacement{*schema};
        std::swap(replacement.using_declarations[*using_index],
                  replacement.using_declarations[*using_index - 1]);
        if (apply_document_edit(
                ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})) {
            soa_using_declaration_index_ = *using_index - 1;
            soa_editor_declaration_.reset();
            return true;
        }
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(pending.has_value() || !has_using ||
                         *using_index + 1 >= schema->using_declarations.size());
    auto const move_using_down{ImGui::Button("Using down")};
    ImGui::EndDisabled();
    if (move_using_down) {
        auto replacement{*schema};
        std::swap(replacement.using_declarations[*using_index],
                  replacement.using_declarations[*using_index + 1]);
        if (apply_document_edit(
                ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})) {
            soa_using_declaration_index_ = *using_index + 1;
            soa_editor_declaration_.reset();
            return true;
        }
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(pending.has_value() || !has_using);
    auto const delete_using{ImGui::Button("Delete using")};
    ImGui::EndDisabled();
    if (delete_using) {
        auto replacement{*schema};
        replacement.using_declarations.erase(replacement.using_declarations.begin() +
                                             static_cast<std::ptrdiff_t>(*using_index));
        auto const next_index{
            replacement.using_declarations.empty()
                ? std::optional<std::size_t>{}
                : std::optional{std::min(*using_index, replacement.using_declarations.size() - 1)}};
        if (apply_document_edit(
                ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})) {
            soa_using_declaration_index_ = next_index;
            soa_editor_declaration_.reset();
            return true;
        }
    }

    ImGui::BeginDisabled(pending.has_value());
    if (soa_using_declarations_.size() == schema->using_declarations.size() &&
        ImGui::BeginTable("soa-using-declarations",
                          2,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                              ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Edit", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Declaration after 'using'");
        ImGui::TableHeadersRow();
        for (std::size_t index{}; index < schema->using_declarations.size(); ++index) {
            ImGui::PushID(static_cast<int>(index));
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            auto const row_selected{soa_using_declaration_index_ == index};
            if (ImGui::Selectable("::", row_selected, ImGuiSelectableFlags_SpanAllColumns)) {
                soa_using_declaration_index_ = index;
            }
            if (ImGui::BeginDragDropSource()) {
                ImGui::SetDragDropPayload("SOA_USING_DECLARATION_ROW", &index, sizeof(index));
                ImGui::Text("Move %s", schema->using_declarations[index].c_str());
                ImGui::EndDragDropSource();
            }
            if (ImGui::BeginDragDropTarget()) {
                if (auto const* payload{
                        ImGui::AcceptDragDropPayload("SOA_USING_DECLARATION_ROW")}) {
                    auto const source_index{*static_cast<std::size_t const*>(payload->Data)};
                    if (source_index < schema->using_declarations.size() && source_index != index) {
                        pending = *schema;
                        move_element(pending->using_declarations, source_index, index);
                        soa_using_declaration_index_ = index;
                    }
                }
                ImGui::EndDragDropTarget();
            }

            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-1.0F);
            auto const submitted{ImGui::InputText("##declaration",
                                                  soa_using_declarations_[index].data(),
                                                  soa_using_declarations_[index].size(),
                                                  ImGuiInputTextFlags_EnterReturnsTrue)};
            if (submitted || ImGui::IsItemDeactivatedAfterEdit()) {
                if (!pending.has_value()) {
                    pending = *schema;
                }
                pending->using_declarations[index] = soa_using_declarations_[index].data();
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    ImGui::EndDisabled();

    ImGui::SeparatorText("Custom functions");
    ImGui::BeginDisabled(pending.has_value());
    auto const add_function{ImGui::Button("+ Function")};
    ImGui::EndDisabled();
    if (add_function) {
        auto replacement{*schema};
        auto function{codegen::FunctionSchema{}};
        function.name = unique_soa_function_name(replacement.functions, "custom_function");
        function.return_type =
            codegen::TypeRef{.name = "void", .suffix = {}, .nested = std::nullopt};
        function.is_inline = true;
        replacement.functions.push_back(std::move(function));
        auto const new_index{replacement.functions.size() - 1};
        if (apply_document_edit(
                ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})) {
            soa_function_index_ = new_index;
            soa_editor_declaration_.reset();
            return true;
        }
    }
    ImGui::SameLine();
    auto const function_index{soa_function_index_};
    auto const has_function{function_index.has_value() &&
                            *function_index < schema->functions.size()};
    ImGui::BeginDisabled(pending.has_value() || !has_function);
    auto const duplicate_function{ImGui::Button("Duplicate function")};
    ImGui::EndDisabled();
    if (duplicate_function) {
        auto replacement{*schema};
        auto copy{replacement.functions[*function_index]};
        copy.name = unique_soa_function_name(replacement.functions, copy.name + "_copy");
        replacement.functions.insert(replacement.functions.begin() +
                                         static_cast<std::ptrdiff_t>(*function_index + 1),
                                     std::move(copy));
        if (apply_document_edit(
                ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})) {
            soa_function_index_ = *function_index + 1;
            soa_editor_declaration_.reset();
            return true;
        }
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(pending.has_value() || !has_function || *function_index == 0);
    auto const move_function_up{ImGui::Button("Function up")};
    ImGui::EndDisabled();
    if (move_function_up) {
        auto replacement{*schema};
        std::swap(replacement.functions[*function_index],
                  replacement.functions[*function_index - 1]);
        if (apply_document_edit(
                ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})) {
            soa_function_index_ = *function_index - 1;
            soa_editor_declaration_.reset();
            return true;
        }
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(pending.has_value() || !has_function ||
                         *function_index + 1 >= schema->functions.size());
    auto const move_function_down{ImGui::Button("Function down")};
    ImGui::EndDisabled();
    if (move_function_down) {
        auto replacement{*schema};
        std::swap(replacement.functions[*function_index],
                  replacement.functions[*function_index + 1]);
        if (apply_document_edit(
                ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})) {
            soa_function_index_ = *function_index + 1;
            soa_editor_declaration_.reset();
            return true;
        }
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(pending.has_value() || !has_function);
    auto const delete_function{ImGui::Button("Delete function")};
    ImGui::EndDisabled();
    if (delete_function) {
        auto replacement{*schema};
        replacement.functions.erase(replacement.functions.begin() +
                                    static_cast<std::ptrdiff_t>(*function_index));
        auto const next_index{
            replacement.functions.empty()
                ? std::optional<std::size_t>{}
                : std::optional{std::min(*function_index, replacement.functions.size() - 1)}};
        if (apply_document_edit(
                ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})) {
            soa_function_index_ = next_index;
            soa_editor_declaration_.reset();
            return true;
        }
    }

    ImGui::BeginDisabled(pending.has_value());
    if (soa_function_names_.size() == schema->functions.size() &&
        soa_function_return_types_.size() == schema->functions.size() &&
        ImGui::BeginTable("soa-functions",
                          8,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                              ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Edit", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Name");
        ImGui::TableSetupColumn("Return");
        ImGui::TableSetupColumn("const", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("noexcept", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("static", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("inline", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("source", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableHeadersRow();
        for (std::size_t index{}; index < schema->functions.size(); ++index) {
            auto const& function{schema->functions[index]};
            ImGui::PushID(static_cast<int>(index));
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            auto const row_selected{soa_function_index_ == index};
            if (ImGui::Selectable("::", row_selected, ImGuiSelectableFlags_SpanAllColumns)) {
                soa_function_index_ = index;
                soa_editor_declaration_.reset();
            }
            if (ImGui::BeginDragDropSource()) {
                ImGui::SetDragDropPayload("SOA_FUNCTION_ROW", &index, sizeof(index));
                ImGui::Text("Move %s", function.name.c_str());
                ImGui::EndDragDropSource();
            }
            if (ImGui::BeginDragDropTarget()) {
                if (auto const* payload{ImGui::AcceptDragDropPayload("SOA_FUNCTION_ROW")}) {
                    auto const source_index{*static_cast<std::size_t const*>(payload->Data)};
                    if (source_index < schema->functions.size() && source_index != index) {
                        pending = *schema;
                        move_element(pending->functions, source_index, index);
                        soa_function_index_ = index;
                    }
                }
                ImGui::EndDragDropTarget();
            }

            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-1.0F);
            auto const name_submitted{ImGui::InputText("##name",
                                                       soa_function_names_[index].data(),
                                                       soa_function_names_[index].size(),
                                                       ImGuiInputTextFlags_EnterReturnsTrue)};
            if (name_submitted || ImGui::IsItemDeactivatedAfterEdit()) {
                if (!pending.has_value()) {
                    pending = *schema;
                }
                pending->functions[index].name = soa_function_names_[index].data();
            }

            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-1.0F);
            auto const return_submitted{ImGui::InputText("##return",
                                                         soa_function_return_types_[index].data(),
                                                         soa_function_return_types_[index].size(),
                                                         ImGuiInputTextFlags_EnterReturnsTrue)};
            if (return_submitted || ImGui::IsItemDeactivatedAfterEdit()) {
                if (!pending.has_value()) {
                    pending = *schema;
                }
                pending->functions[index].return_type.name =
                    soa_function_return_types_[index].data();
            }

            auto edit_flag = [&](char const* label, bool const current, auto&& apply) {
                ImGui::TableNextColumn();
                auto value{current};
                if (ImGui::Checkbox(label, &value)) {
                    if (!pending.has_value()) {
                        pending = *schema;
                    }
                    apply(pending->functions[index], value);
                }
            };
            edit_flag("##const", function.is_const, [](auto& edited, bool const value) {
                edited.is_const = value;
                if (value) {
                    edited.is_static = false;
                }
            });
            edit_flag("##noexcept", function.is_noexcept, [](auto& edited, bool const value) {
                edited.is_noexcept = value;
            });
            edit_flag("##static", function.is_static, [](auto& edited, bool const value) {
                edited.is_static = value;
                if (value) {
                    edited.is_const = false;
                }
            });
            edit_flag("##inline", function.is_inline, [](auto& edited, bool const value) {
                edited.is_inline = value;
                if (value) {
                    edited.definition_in_source = false;
                }
            });
            edit_flag(
                "##source", function.definition_in_source, [](auto& edited, bool const value) {
                    edited.definition_in_source = value;
                    if (value) {
                        edited.is_inline = false;
                    }
                });
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    ImGui::EndDisabled();

    if (has_function) {
        auto const& function{schema->functions[*function_index]};

        ImGui::TextUnformatted("Parameters");
        ImGui::BeginDisabled(pending.has_value());
        auto const add_parameter{ImGui::Button("+ Parameter")};
        ImGui::EndDisabled();
        if (add_parameter) {
            auto replacement{*schema};
            auto& parameters{replacement.functions[*function_index].parameters};
            auto parameter{codegen::ParameterSchema{}};
            parameter.name = unique_function_parameter_name(parameters, "parameter");
            parameter.type =
                codegen::TypeRef{.name = "std::uint32_t", .suffix = {}, .nested = std::nullopt};
            if (std::ranges::any_of(parameters, [](auto const& value) {
                    return value.default_value.has_value();
                })) {
                parameter.default_value = "{}";
            }
            parameters.push_back(std::move(parameter));
            auto const new_index{parameters.size() - 1};
            if (apply_document_edit(
                    ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})) {
                soa_parameter_index_ = new_index;
                soa_editor_declaration_.reset();
                return true;
            }
        }
        ImGui::SameLine();
        auto const parameter_index{soa_parameter_index_};
        auto const has_parameter{parameter_index.has_value() &&
                                 *parameter_index < function.parameters.size()};
        ImGui::BeginDisabled(pending.has_value() || !has_parameter);
        auto const duplicate_parameter{ImGui::Button("Duplicate parameter")};
        ImGui::EndDisabled();
        if (duplicate_parameter) {
            auto replacement{*schema};
            auto& parameters{replacement.functions[*function_index].parameters};
            auto copy{parameters[*parameter_index]};
            copy.name = unique_function_parameter_name(parameters, copy.name + "_copy");
            parameters.insert(parameters.begin() +
                                  static_cast<std::ptrdiff_t>(*parameter_index + 1),
                              std::move(copy));
            if (apply_document_edit(
                    ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})) {
                soa_parameter_index_ = *parameter_index + 1;
                soa_editor_declaration_.reset();
                return true;
            }
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(pending.has_value() || !has_parameter || *parameter_index == 0);
        auto const move_parameter_up{ImGui::Button("Parameter up")};
        ImGui::EndDisabled();
        if (move_parameter_up) {
            auto replacement{*schema};
            auto& parameters{replacement.functions[*function_index].parameters};
            std::swap(parameters[*parameter_index], parameters[*parameter_index - 1]);
            if (apply_document_edit(
                    ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})) {
                soa_parameter_index_ = *parameter_index - 1;
                soa_editor_declaration_.reset();
                return true;
            }
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(pending.has_value() || !has_parameter ||
                             *parameter_index + 1 >= function.parameters.size());
        auto const move_parameter_down{ImGui::Button("Parameter down")};
        ImGui::EndDisabled();
        if (move_parameter_down) {
            auto replacement{*schema};
            auto& parameters{replacement.functions[*function_index].parameters};
            std::swap(parameters[*parameter_index], parameters[*parameter_index + 1]);
            if (apply_document_edit(
                    ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})) {
                soa_parameter_index_ = *parameter_index + 1;
                soa_editor_declaration_.reset();
                return true;
            }
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(pending.has_value() || !has_parameter);
        auto const delete_parameter{ImGui::Button("Delete parameter")};
        ImGui::EndDisabled();
        if (delete_parameter) {
            auto replacement{*schema};
            auto& parameters{replacement.functions[*function_index].parameters};
            parameters.erase(parameters.begin() + static_cast<std::ptrdiff_t>(*parameter_index));
            auto const next_index{
                parameters.empty()
                    ? std::optional<std::size_t>{}
                    : std::optional{std::min(*parameter_index, parameters.size() - 1)}};
            if (apply_document_edit(
                    ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})) {
                soa_parameter_index_ = next_index;
                soa_editor_declaration_.reset();
                return true;
            }
        }

        ImGui::BeginDisabled(pending.has_value());
        if (soa_parameter_names_.size() == function.parameters.size() &&
            soa_parameter_types_.size() == function.parameters.size() &&
            soa_parameter_defaults_.size() == function.parameters.size() &&
            ImGui::BeginTable("soa-function-parameters",
                              5,
                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                  ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp)) {
            ImGui::TableSetupColumn("Edit", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableSetupColumn("Name");
            ImGui::TableSetupColumn("Type");
            ImGui::TableSetupColumn("Pick", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableSetupColumn("Default");
            ImGui::TableHeadersRow();
            for (std::size_t index{}; index < function.parameters.size(); ++index) {
                auto const& parameter{function.parameters[index]};
                ImGui::PushID(static_cast<int>(index));
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                auto const row_selected{soa_parameter_index_ == index};
                if (ImGui::Selectable("::", row_selected, ImGuiSelectableFlags_SpanAllColumns)) {
                    soa_parameter_index_ = index;
                }
                if (ImGui::BeginDragDropSource()) {
                    ImGui::SetDragDropPayload("SOA_FUNCTION_PARAMETER_ROW", &index, sizeof(index));
                    ImGui::Text("Move %s", parameter.name.c_str());
                    ImGui::EndDragDropSource();
                }
                if (ImGui::BeginDragDropTarget()) {
                    if (auto const* payload{
                            ImGui::AcceptDragDropPayload("SOA_FUNCTION_PARAMETER_ROW")}) {
                        auto const source_index{*static_cast<std::size_t const*>(payload->Data)};
                        if (source_index < function.parameters.size() && source_index != index) {
                            pending = *schema;
                            move_element(pending->functions[*function_index].parameters,
                                         source_index,
                                         index);
                            soa_parameter_index_ = index;
                        }
                    }
                    ImGui::EndDragDropTarget();
                }

                ImGui::TableNextColumn();
                ImGui::SetNextItemWidth(-1.0F);
                auto const name_submitted{ImGui::InputText("##name",
                                                           soa_parameter_names_[index].data(),
                                                           soa_parameter_names_[index].size(),
                                                           ImGuiInputTextFlags_EnterReturnsTrue)};
                if (name_submitted || ImGui::IsItemDeactivatedAfterEdit()) {
                    if (!pending.has_value()) {
                        pending = *schema;
                    }
                    pending->functions[*function_index].parameters[index].name =
                        soa_parameter_names_[index].data();
                }

                ImGui::TableNextColumn();
                ImGui::SetNextItemWidth(-1.0F);
                auto const type_submitted{ImGui::InputText("##type",
                                                           soa_parameter_types_[index].data(),
                                                           soa_parameter_types_[index].size(),
                                                           ImGuiInputTextFlags_EnterReturnsTrue)};
                if (type_submitted || ImGui::IsItemDeactivatedAfterEdit()) {
                    if (!pending.has_value()) {
                        pending = *schema;
                    }
                    pending->functions[*function_index].parameters[index].type.name =
                        soa_parameter_types_[index].data();
                }

                ImGui::TableNextColumn();
                ImGui::PushID("type-picker");
                if (auto picked{draw_type_picker(node.identity.module_name, node.identity)}) {
                    if (!pending.has_value()) {
                        pending = *schema;
                    }
                    pending->functions[*function_index].parameters[index].type.name = *picked;
                }
                ImGui::PopID();

                ImGui::TableNextColumn();
                auto has_default{parameter.default_value.has_value()};
                if (ImGui::Checkbox("##has-default", &has_default)) {
                    if (!pending.has_value()) {
                        pending = *schema;
                    }
                    auto& edited{
                        pending->functions[*function_index].parameters[index].default_value};
                    edited = has_default ? std::optional<std::string>{"{}"} : std::nullopt;
                }
                if (parameter.default_value.has_value()) {
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(-1.0F);
                    auto const default_submitted{
                        ImGui::InputText("##default",
                                         soa_parameter_defaults_[index].data(),
                                         soa_parameter_defaults_[index].size(),
                                         ImGuiInputTextFlags_EnterReturnsTrue)};
                    if (default_submitted || ImGui::IsItemDeactivatedAfterEdit()) {
                        if (!pending.has_value()) {
                            pending = *schema;
                        }
                        pending->functions[*function_index].parameters[index].default_value =
                            soa_parameter_defaults_[index].data();
                    }
                }
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        ImGui::EndDisabled();

        ImGui::PushID(static_cast<int>(*function_index));
        ImGui::SeparatorText("Body fragments");
        ImGui::BeginDisabled(pending.has_value());
        auto const add_body_line{ImGui::Button("+ Body fragment")};
        ImGui::EndDisabled();
        if (add_body_line) {
            auto replacement{*schema};
            auto& body_lines{replacement.functions[*function_index].body_lines};
            body_lines.emplace_back("// TODO");
            auto const new_index{body_lines.size() - 1};
            if (apply_document_edit(
                    ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})) {
                soa_function_body_index_ = new_index;
                soa_editor_declaration_.reset();
                ImGui::PopID();
                return true;
            }
        }
        ImGui::SameLine();
        auto const body_index{soa_function_body_index_};
        auto const has_body_line{body_index.has_value() &&
                                 *body_index < function.body_lines.size()};
        ImGui::BeginDisabled(pending.has_value() || !has_body_line);
        auto const duplicate_body_line{ImGui::Button("Duplicate body")};
        ImGui::EndDisabled();
        if (duplicate_body_line) {
            auto replacement{*schema};
            auto& body_lines{replacement.functions[*function_index].body_lines};
            body_lines.insert(body_lines.begin() + static_cast<std::ptrdiff_t>(*body_index + 1),
                              body_lines[*body_index]);
            if (apply_document_edit(
                    ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})) {
                soa_function_body_index_ = *body_index + 1;
                soa_editor_declaration_.reset();
                ImGui::PopID();
                return true;
            }
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(pending.has_value() || !has_body_line || *body_index == 0);
        auto const move_body_up{ImGui::Button("Body up")};
        ImGui::EndDisabled();
        if (move_body_up) {
            auto replacement{*schema};
            auto& body_lines{replacement.functions[*function_index].body_lines};
            std::swap(body_lines[*body_index], body_lines[*body_index - 1]);
            if (apply_document_edit(
                    ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})) {
                soa_function_body_index_ = *body_index - 1;
                soa_editor_declaration_.reset();
                ImGui::PopID();
                return true;
            }
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(pending.has_value() || !has_body_line ||
                             *body_index + 1 >= function.body_lines.size());
        auto const move_body_down{ImGui::Button("Body down")};
        ImGui::EndDisabled();
        if (move_body_down) {
            auto replacement{*schema};
            auto& body_lines{replacement.functions[*function_index].body_lines};
            std::swap(body_lines[*body_index], body_lines[*body_index + 1]);
            if (apply_document_edit(
                    ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})) {
                soa_function_body_index_ = *body_index + 1;
                soa_editor_declaration_.reset();
                ImGui::PopID();
                return true;
            }
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(pending.has_value() || !has_body_line);
        auto const delete_body_line{ImGui::Button("Delete body")};
        ImGui::EndDisabled();
        if (delete_body_line) {
            auto replacement{*schema};
            auto& body_lines{replacement.functions[*function_index].body_lines};
            body_lines.erase(body_lines.begin() + static_cast<std::ptrdiff_t>(*body_index));
            auto const next_index{
                body_lines.empty() ? std::optional<std::size_t>{}
                                   : std::optional{std::min(*body_index, body_lines.size() - 1)}};
            if (apply_document_edit(
                    ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})) {
                soa_function_body_index_ = next_index;
                soa_editor_declaration_.reset();
                ImGui::PopID();
                return true;
            }
        }

        ImGui::BeginDisabled(pending.has_value());
        if (soa_function_body_lines_.size() == function.body_lines.size() &&
            ImGui::BeginTable("function-body-lines",
                              2,
                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                  ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp)) {
            ImGui::TableSetupColumn("Edit", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableSetupColumn("C++ fragment");
            ImGui::TableHeadersRow();
            for (std::size_t index{}; index < function.body_lines.size(); ++index) {
                ImGui::PushID(static_cast<int>(index));
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                auto const row_selected{soa_function_body_index_ == index};
                if (ImGui::Selectable("::", row_selected, ImGuiSelectableFlags_SpanAllColumns)) {
                    soa_function_body_index_ = index;
                }
                if (ImGui::BeginDragDropSource()) {
                    ImGui::SetDragDropPayload("SOA_FUNCTION_BODY_ROW", &index, sizeof(index));
                    ImGui::TextUnformatted("Move body fragment");
                    ImGui::EndDragDropSource();
                }
                if (ImGui::BeginDragDropTarget()) {
                    if (auto const* payload{
                            ImGui::AcceptDragDropPayload("SOA_FUNCTION_BODY_ROW")}) {
                        auto const source_index{*static_cast<std::size_t const*>(payload->Data)};
                        if (source_index < function.body_lines.size() && source_index != index) {
                            pending = *schema;
                            move_element(pending->functions[*function_index].body_lines,
                                         source_index,
                                         index);
                            soa_function_body_index_ = index;
                        }
                    }
                    ImGui::EndDragDropTarget();
                }

                ImGui::TableNextColumn();
                input_text_multiline("##body",
                                     soa_function_body_lines_[index],
                                     ImVec2{-1.0F, ImGui::GetTextLineHeight() * 2.5F});
                if (ImGui::IsItemDeactivatedAfterEdit()) {
                    if (!pending.has_value()) {
                        pending = *schema;
                    }
                    pending->functions[*function_index].body_lines[index] =
                        soa_function_body_lines_[index];
                }
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        ImGui::EndDisabled();

        ImGui::SeparatorText("Declared dependencies");
        ImGui::SetNextItemWidth(220.0F);
        ImGui::BeginDisabled(pending.has_value());
        input_text("Dependency key", soa_new_function_dependency_);
        ImGui::SameLine();
        auto const add_dependency{ImGui::Button("+ Dependency")};
        ImGui::EndDisabled();
        if (add_dependency) {
            if (soa_new_function_dependency_.find_first_not_of(" \t\r\n") == std::string::npos) {
                schema_edit_message_ = "SoA function dependency key cannot be empty.";
                ImGui::PopID();
                return false;
            }
            auto replacement{*schema};
            auto& dependencies{replacement.functions[*function_index].dependencies};
            dependencies.push_back(soa_new_function_dependency_);
            auto const new_index{dependencies.size() - 1};
            if (apply_document_edit(
                    ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})) {
                soa_new_function_dependency_.clear();
                soa_function_dependency_index_ = new_index;
                soa_editor_declaration_.reset();
                ImGui::PopID();
                return true;
            }
        }
        auto const dependency_index{soa_function_dependency_index_};
        auto const has_dependency{dependency_index.has_value() &&
                                  *dependency_index < function.dependencies.size()};
        ImGui::BeginDisabled(pending.has_value() || !has_dependency);
        auto const duplicate_dependency{ImGui::Button("Duplicate dependency")};
        ImGui::EndDisabled();
        if (duplicate_dependency) {
            auto replacement{*schema};
            auto& dependencies{replacement.functions[*function_index].dependencies};
            dependencies.insert(dependencies.begin() +
                                    static_cast<std::ptrdiff_t>(*dependency_index + 1),
                                dependencies[*dependency_index]);
            if (apply_document_edit(
                    ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})) {
                soa_function_dependency_index_ = *dependency_index + 1;
                soa_editor_declaration_.reset();
                ImGui::PopID();
                return true;
            }
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(pending.has_value() || !has_dependency || *dependency_index == 0);
        auto const move_dependency_up{ImGui::Button("Dependency up")};
        ImGui::EndDisabled();
        if (move_dependency_up) {
            auto replacement{*schema};
            auto& dependencies{replacement.functions[*function_index].dependencies};
            std::swap(dependencies[*dependency_index], dependencies[*dependency_index - 1]);
            if (apply_document_edit(
                    ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})) {
                soa_function_dependency_index_ = *dependency_index - 1;
                soa_editor_declaration_.reset();
                ImGui::PopID();
                return true;
            }
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(pending.has_value() || !has_dependency ||
                             *dependency_index + 1 >= function.dependencies.size());
        auto const move_dependency_down{ImGui::Button("Dependency down")};
        ImGui::EndDisabled();
        if (move_dependency_down) {
            auto replacement{*schema};
            auto& dependencies{replacement.functions[*function_index].dependencies};
            std::swap(dependencies[*dependency_index], dependencies[*dependency_index + 1]);
            if (apply_document_edit(
                    ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})) {
                soa_function_dependency_index_ = *dependency_index + 1;
                soa_editor_declaration_.reset();
                ImGui::PopID();
                return true;
            }
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(pending.has_value() || !has_dependency);
        auto const delete_dependency{ImGui::Button("Delete dependency")};
        ImGui::EndDisabled();
        if (delete_dependency) {
            auto replacement{*schema};
            auto& dependencies{replacement.functions[*function_index].dependencies};
            dependencies.erase(dependencies.begin() +
                               static_cast<std::ptrdiff_t>(*dependency_index));
            auto const next_index{
                dependencies.empty()
                    ? std::optional<std::size_t>{}
                    : std::optional{std::min(*dependency_index, dependencies.size() - 1)}};
            if (apply_document_edit(
                    ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})) {
                soa_function_dependency_index_ = next_index;
                soa_editor_declaration_.reset();
                ImGui::PopID();
                return true;
            }
        }

        ImGui::BeginDisabled(pending.has_value());
        if (soa_function_dependencies_.size() == function.dependencies.size() &&
            ImGui::BeginTable("function-dependencies",
                              2,
                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                  ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp)) {
            ImGui::TableSetupColumn("Edit", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableSetupColumn("Registered dependency key");
            ImGui::TableHeadersRow();
            for (std::size_t index{}; index < function.dependencies.size(); ++index) {
                ImGui::PushID(static_cast<int>(index));
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                auto const row_selected{soa_function_dependency_index_ == index};
                if (ImGui::Selectable("::", row_selected, ImGuiSelectableFlags_SpanAllColumns)) {
                    soa_function_dependency_index_ = index;
                }
                if (ImGui::BeginDragDropSource()) {
                    ImGui::SetDragDropPayload("SOA_FUNCTION_DEPENDENCY_ROW", &index, sizeof(index));
                    ImGui::Text("Move %s", function.dependencies[index].c_str());
                    ImGui::EndDragDropSource();
                }
                if (ImGui::BeginDragDropTarget()) {
                    if (auto const* payload{
                            ImGui::AcceptDragDropPayload("SOA_FUNCTION_DEPENDENCY_ROW")}) {
                        auto const source_index{*static_cast<std::size_t const*>(payload->Data)};
                        if (source_index < function.dependencies.size() && source_index != index) {
                            pending = *schema;
                            move_element(pending->functions[*function_index].dependencies,
                                         source_index,
                                         index);
                            soa_function_dependency_index_ = index;
                        }
                    }
                    ImGui::EndDragDropTarget();
                }

                ImGui::TableNextColumn();
                ImGui::SetNextItemWidth(-1.0F);
                auto const submitted{input_text("##dependency",
                                                soa_function_dependencies_[index],
                                                ImGuiInputTextFlags_EnterReturnsTrue)};
                if (submitted || ImGui::IsItemDeactivatedAfterEdit()) {
                    if (!pending.has_value()) {
                        pending = *schema;
                    }
                    pending->functions[*function_index].dependencies[index] =
                        soa_function_dependencies_[index];
                }
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        ImGui::EndDisabled();

        ImGui::SeparatorText("Advanced signature");
        auto has_trailing_return{function.trailing_return_type.has_value()};
        ImGui::BeginDisabled(pending.has_value());
        auto const trailing_toggled{
            ImGui::Checkbox("Use trailing return type", &has_trailing_return)};
        ImGui::EndDisabled();
        if (trailing_toggled) {
            auto replacement{*schema};
            auto& edited{replacement.functions[*function_index]};
            if (has_trailing_return) {
                edited.trailing_return_type =
                    edited.return_type.name == "auto"
                        ? codegen::TypeRef{.name = "void", .suffix = {}, .nested = std::nullopt}
                        : edited.return_type;
                edited.return_type =
                    codegen::TypeRef{.name = "auto", .suffix = {}, .nested = std::nullopt};
            } else {
                edited.return_type = *edited.trailing_return_type;
                edited.trailing_return_type.reset();
            }
            if (apply_document_edit(
                    ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})) {
                soa_editor_declaration_.reset();
                ImGui::PopID();
                return true;
            }
        }
        if (function.trailing_return_type.has_value()) {
            ImGui::SetNextItemWidth(220.0F);
            auto const submitted{input_text("Trailing return type",
                                            soa_function_trailing_return_type_,
                                            ImGuiInputTextFlags_EnterReturnsTrue)};
            if (submitted || ImGui::IsItemDeactivatedAfterEdit()) {
                if (!pending.has_value()) {
                    pending = *schema;
                }
                pending->functions[*function_index].trailing_return_type->name =
                    soa_function_trailing_return_type_;
            }
            ImGui::SameLine();
            ImGui::PushID("trailing-return-picker");
            if (auto picked{draw_type_picker(node.identity.module_name, node.identity)}) {
                if (!pending.has_value()) {
                    pending = *schema;
                }
                pending->functions[*function_index].trailing_return_type->name = *picked;
            }
            ImGui::PopID();
        }

        auto has_template_parameters{function.template_parameters.has_value()};
        ImGui::BeginDisabled(pending.has_value());
        auto const template_toggled{
            ImGui::Checkbox("Template parameters", &has_template_parameters)};
        ImGui::EndDisabled();
        if (template_toggled) {
            auto replacement{*schema};
            replacement.functions[*function_index].template_parameters =
                has_template_parameters ? std::optional<std::string>{"typename T"} : std::nullopt;
            if (apply_document_edit(
                    ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})) {
                soa_editor_declaration_.reset();
                ImGui::PopID();
                return true;
            }
        }
        if (function.template_parameters.has_value()) {
            ImGui::SetNextItemWidth(-1.0F);
            auto const submitted{input_text("Template parameter text",
                                            soa_function_template_parameters_,
                                            ImGuiInputTextFlags_EnterReturnsTrue)};
            if (submitted || ImGui::IsItemDeactivatedAfterEdit()) {
                if (!pending.has_value()) {
                    pending = *schema;
                }
                pending->functions[*function_index].template_parameters =
                    soa_function_template_parameters_;
            }
        }

        auto has_requires_clause{function.requires_clause.has_value()};
        ImGui::BeginDisabled(pending.has_value());
        auto const requires_toggled{ImGui::Checkbox("Requires clause", &has_requires_clause)};
        ImGui::EndDisabled();
        if (requires_toggled) {
            auto replacement{*schema};
            replacement.functions[*function_index].requires_clause =
                has_requires_clause ? std::optional<std::string>{"true"} : std::nullopt;
            if (apply_document_edit(
                    ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})) {
                soa_editor_declaration_.reset();
                ImGui::PopID();
                return true;
            }
        }
        if (function.requires_clause.has_value()) {
            ImGui::SetNextItemWidth(-1.0F);
            auto const submitted{input_text("Requires expression",
                                            soa_function_requires_clause_,
                                            ImGuiInputTextFlags_EnterReturnsTrue)};
            if (submitted || ImGui::IsItemDeactivatedAfterEdit()) {
                if (!pending.has_value()) {
                    pending = *schema;
                }
                pending->functions[*function_index].requires_clause = soa_function_requires_clause_;
            }
        }
        ImGui::PopID();
    }

    ImGui::SeparatorText("View types");
    auto explicit_view_name{schema->view_name.has_value()};
    if (ImGui::Checkbox("Explicit mutable view type", &explicit_view_name)) {
        auto replacement{*schema};
        replacement.view_name =
            explicit_view_name ? std::optional{schema->name + "View"} : std::nullopt;
        if (apply_document_edit(
                ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})) {
            soa_editor_declaration_.reset();
            return true;
        }
    }
    if (schema->view_name.has_value()) {
        ImGui::SetNextItemWidth(-1.0F);
        auto const submitted{ImGui::InputText("Mutable view type",
                                              soa_view_name_.data(),
                                              soa_view_name_.size(),
                                              ImGuiInputTextFlags_EnterReturnsTrue)};
        if (submitted || ImGui::IsItemDeactivatedAfterEdit()) {
            if (soa_view_name_.front() == '\0') {
                schema_edit_message_ = "Explicit mutable view type cannot be empty; disable it to "
                                       "use the derived name.";
                return false;
            }
            auto replacement{*schema};
            replacement.view_name = soa_view_name_.data();
            if (apply_document_edit(
                    ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})) {
                soa_editor_declaration_.reset();
                return true;
            }
        }
    } else {
        ImGui::TextDisabled("Derived mutable view: %sView", schema->name.c_str());
    }

    auto explicit_const_view_name{schema->const_view_name.has_value()};
    if (ImGui::Checkbox("Explicit const view type", &explicit_const_view_name)) {
        auto replacement{*schema};
        replacement.const_view_name =
            explicit_const_view_name ? std::optional{schema->name + "ConstView"} : std::nullopt;
        if (apply_document_edit(
                ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})) {
            soa_editor_declaration_.reset();
            return true;
        }
    }
    if (schema->const_view_name.has_value()) {
        ImGui::SetNextItemWidth(-1.0F);
        auto const submitted{ImGui::InputText("Const view type",
                                              soa_const_view_name_.data(),
                                              soa_const_view_name_.size(),
                                              ImGuiInputTextFlags_EnterReturnsTrue)};
        if (submitted || ImGui::IsItemDeactivatedAfterEdit()) {
            if (soa_const_view_name_.front() == '\0') {
                schema_edit_message_ =
                    "Explicit const view type cannot be empty; disable it to use the derived name.";
                return false;
            }
            auto replacement{*schema};
            replacement.const_view_name = soa_const_view_name_.data();
            if (apply_document_edit(
                    ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})) {
                soa_editor_declaration_.reset();
                return true;
            }
        }
    } else {
        ImGui::TextDisabled("Derived const view: %sConstView", schema->name.c_str());
    }

    ImGui::SeparatorText("Fixed layout");
    if (!schema->fixed.has_value()) {
        if (ImGui::Button("Enable fixed layout")) {
            auto storage_name{document_->unique_soa_generated_type_name(
                *declaration, schema->name + "FixedStorage")};
            if (!storage_name.has_value()) {
                schema_edit_message_ = storage_name.error().message;
                return false;
            }
            auto replacement{*schema};
            replacement.fixed =
                codegen::FixedSoaSchema{.storage_name = std::move(*storage_name), .containers = {}};
            if (apply_document_edit(
                    ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})) {
                soa_editor_declaration_.reset();
                return true;
            }
        }
    } else {
        ImGui::SetNextItemWidth(-1.0F);
        auto const storage_submitted{ImGui::InputText("Storage type",
                                                      soa_fixed_storage_name_.data(),
                                                      soa_fixed_storage_name_.size(),
                                                      ImGuiInputTextFlags_EnterReturnsTrue)};
        if (storage_submitted || ImGui::IsItemDeactivatedAfterEdit()) {
            if (soa_fixed_storage_name_.front() == '\0') {
                schema_edit_message_ = "Fixed-layout storage type cannot be empty.";
                return false;
            }
            auto replacement{*schema};
            replacement.fixed->storage_name = soa_fixed_storage_name_.data();
            if (apply_document_edit(
                    ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})) {
                soa_editor_declaration_.reset();
                return true;
            }
        }

        if (ImGui::Button("+ Fixed container")) {
            auto container_name{
                document_->unique_soa_generated_type_name(*declaration, schema->name + "Fixed")};
            if (!container_name.has_value()) {
                schema_edit_message_ = container_name.error().message;
                return false;
            }
            auto replacement{*schema};
            replacement.fixed->containers.push_back(std::move(*container_name));
            auto const new_index{replacement.fixed->containers.size() - 1};
            if (apply_document_edit(
                    ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})) {
                soa_fixed_container_index_ = new_index;
                soa_editor_declaration_.reset();
                return true;
            }
        }
        ImGui::SameLine();
        auto const container_index{soa_fixed_container_index_};
        auto const has_container{container_index.has_value() &&
                                 *container_index < schema->fixed->containers.size()};
        ImGui::BeginDisabled(!has_container);
        if (ImGui::Button("Duplicate fixed container")) {
            auto container_name{document_->unique_soa_generated_type_name(
                *declaration, schema->fixed->containers[*container_index] + "_copy")};
            if (!container_name.has_value()) {
                schema_edit_message_ = container_name.error().message;
            } else {
                auto replacement{*schema};
                replacement.fixed->containers.insert(
                    replacement.fixed->containers.begin() +
                        static_cast<std::ptrdiff_t>(*container_index + 1),
                    std::move(*container_name));
                if (apply_document_edit(ReplaceSoa{.declaration = *declaration,
                                                   .schema = std::move(replacement)})) {
                    soa_fixed_container_index_ = *container_index + 1;
                    soa_editor_declaration_.reset();
                    return true;
                }
            }
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(!has_container || *container_index == 0);
        if (ImGui::Button("Fixed container up")) {
            auto replacement{*schema};
            std::swap(replacement.fixed->containers[*container_index],
                      replacement.fixed->containers[*container_index - 1]);
            if (apply_document_edit(
                    ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})) {
                soa_fixed_container_index_ = *container_index - 1;
                soa_editor_declaration_.reset();
                return true;
            }
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(!has_container ||
                             *container_index + 1 >= schema->fixed->containers.size());
        if (ImGui::Button("Fixed container down")) {
            auto replacement{*schema};
            std::swap(replacement.fixed->containers[*container_index],
                      replacement.fixed->containers[*container_index + 1]);
            if (apply_document_edit(
                    ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})) {
                soa_fixed_container_index_ = *container_index + 1;
                soa_editor_declaration_.reset();
                return true;
            }
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Delete fixed container")) {
            auto replacement{*schema};
            replacement.fixed->containers.erase(replacement.fixed->containers.begin() +
                                                static_cast<std::ptrdiff_t>(*container_index));
            auto const next_index{
                replacement.fixed->containers.empty()
                    ? std::optional<std::size_t>{}
                    : std::optional{
                          std::min(*container_index, replacement.fixed->containers.size() - 1)}};
            if (apply_document_edit(
                    ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})) {
                soa_fixed_container_index_ = next_index;
                soa_editor_declaration_.reset();
                return true;
            }
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Disable fixed layout")) {
            auto replacement{*schema};
            replacement.fixed.reset();
            if (apply_document_edit(
                    ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})) {
                soa_fixed_container_index_.reset();
                soa_editor_declaration_.reset();
                return true;
            }
        }

        if (soa_fixed_container_names_.size() == schema->fixed->containers.size() &&
            ImGui::BeginTable("soa-fixed-containers",
                              2,
                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                  ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp)) {
            ImGui::TableSetupColumn("Edit", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableSetupColumn("Container type");
            ImGui::TableHeadersRow();
            for (std::size_t index{}; index < schema->fixed->containers.size(); ++index) {
                ImGui::PushID(static_cast<int>(index));
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                auto const row_selected{soa_fixed_container_index_ == index};
                if (ImGui::Selectable("::", row_selected, ImGuiSelectableFlags_SpanAllColumns)) {
                    soa_fixed_container_index_ = index;
                }
                if (ImGui::BeginDragDropSource()) {
                    ImGui::SetDragDropPayload("SOA_FIXED_CONTAINER_ROW", &index, sizeof(index));
                    ImGui::Text("Move %s", schema->fixed->containers[index].c_str());
                    ImGui::EndDragDropSource();
                }
                if (ImGui::BeginDragDropTarget()) {
                    if (auto const* payload{
                            ImGui::AcceptDragDropPayload("SOA_FIXED_CONTAINER_ROW")}) {
                        auto const source_index{*static_cast<std::size_t const*>(payload->Data)};
                        if (source_index < schema->fixed->containers.size() &&
                            source_index != index) {
                            pending = *schema;
                            move_element(pending->fixed->containers, source_index, index);
                            soa_fixed_container_index_ = index;
                        }
                    }
                    ImGui::EndDragDropTarget();
                }
                ImGui::TableNextColumn();
                ImGui::SetNextItemWidth(-1.0F);
                auto const name_submitted{ImGui::InputText("##name",
                                                           soa_fixed_container_names_[index].data(),
                                                           soa_fixed_container_names_[index].size(),
                                                           ImGuiInputTextFlags_EnterReturnsTrue)};
                if (name_submitted || ImGui::IsItemDeactivatedAfterEdit()) {
                    if (!pending.has_value()) {
                        pending = *schema;
                    }
                    pending->fixed->containers[index] = soa_fixed_container_names_[index].data();
                }
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
    }

    ImGui::SeparatorText("Single allocation");
    if (!schema->single_allocation.has_value()) {
        if (ImGui::Button("Enable single allocation")) {
            auto owner_name{
                document_->unique_soa_storage_owner_name(*declaration, schema->name + "Single")};
            if (!owner_name.has_value()) {
                schema_edit_message_ = owner_name.error().message;
                return false;
            }
            auto replacement{*schema};
            replacement.single_allocation = std::move(*owner_name);
            if (apply_document_edit(
                    ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})) {
                soa_editor_declaration_.reset();
                return true;
            }
        }
    } else {
        ImGui::SetNextItemWidth(-1.0F);
        auto const submitted{ImGui::InputText("Owner type",
                                              soa_single_allocation_name_.data(),
                                              soa_single_allocation_name_.size(),
                                              ImGuiInputTextFlags_EnterReturnsTrue)};
        if (submitted || ImGui::IsItemDeactivatedAfterEdit()) {
            if (soa_single_allocation_name_.front() == '\0') {
                schema_edit_message_ = "Single-allocation owner type cannot be empty.";
                return false;
            }
            auto replacement{*schema};
            replacement.single_allocation = soa_single_allocation_name_.data();
            if (apply_document_edit(
                    ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})) {
                soa_editor_declaration_.reset();
                return true;
            }
        }
        ImGui::Text("Generated storage: %sStorage", schema->single_allocation->c_str());
        ImGui::TextUnformatted("New variant allocator");
        ImGui::SetNextItemWidth(std::max(120.0F, ImGui::GetContentRegionAvail().x - 120.0F));
        ImGui::InputText("##new-variant-allocator",
                         soa_new_single_allocation_allocator_.data(),
                         soa_new_single_allocation_allocator_.size());
        ImGui::SameLine();
        if (auto picked{draw_type_picker(node.identity.module_name, node.identity)}) {
            std::snprintf(soa_new_single_allocation_allocator_.data(),
                          soa_new_single_allocation_allocator_.size(),
                          "%s",
                          picked->c_str());
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(soa_new_single_allocation_allocator_.front() == '\0');
        if (ImGui::Button("+ Variant")) {
            auto variant_name{document_->unique_soa_storage_owner_name(
                *declaration, schema->name + "SingleVariant")};
            if (!variant_name.has_value()) {
                schema_edit_message_ = variant_name.error().message;
            } else {
                auto replacement{*schema};
                replacement.single_allocation_variants.push_back(codegen::SingleAllocationVariant{
                    .name = std::move(*variant_name),
                    .allocator =
                        codegen::TypeRef{.name = soa_new_single_allocation_allocator_.data(),
                                         .suffix = {},
                                         .nested = std::nullopt}});
                auto const new_index{replacement.single_allocation_variants.size() - 1};
                if (apply_document_edit(ReplaceSoa{.declaration = *declaration,
                                                   .schema = std::move(replacement)})) {
                    soa_single_allocation_variant_index_ = new_index;
                    soa_editor_declaration_.reset();
                    return true;
                }
            }
        }
        ImGui::EndDisabled();

        auto const variant_index{soa_single_allocation_variant_index_};
        auto const has_variant{variant_index.has_value() &&
                               *variant_index < schema->single_allocation_variants.size()};
        ImGui::BeginDisabled(!has_variant);
        if (ImGui::Button("Duplicate variant")) {
            auto variant_name{document_->unique_soa_storage_owner_name(
                *declaration, schema->single_allocation_variants[*variant_index].name + "_copy")};
            if (!variant_name.has_value()) {
                schema_edit_message_ = variant_name.error().message;
            } else {
                auto replacement{*schema};
                auto copy{replacement.single_allocation_variants[*variant_index]};
                copy.name = std::move(*variant_name);
                replacement.single_allocation_variants.insert(
                    replacement.single_allocation_variants.begin() +
                        static_cast<std::ptrdiff_t>(*variant_index + 1),
                    std::move(copy));
                if (apply_document_edit(ReplaceSoa{.declaration = *declaration,
                                                   .schema = std::move(replacement)})) {
                    soa_single_allocation_variant_index_ = *variant_index + 1;
                    soa_editor_declaration_.reset();
                    return true;
                }
            }
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(!has_variant || *variant_index == 0);
        if (ImGui::Button("Variant up")) {
            auto replacement{*schema};
            std::swap(replacement.single_allocation_variants[*variant_index],
                      replacement.single_allocation_variants[*variant_index - 1]);
            if (apply_document_edit(
                    ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})) {
                soa_single_allocation_variant_index_ = *variant_index - 1;
                soa_editor_declaration_.reset();
                return true;
            }
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(!has_variant ||
                             *variant_index + 1 >= schema->single_allocation_variants.size());
        if (ImGui::Button("Variant down")) {
            auto replacement{*schema};
            std::swap(replacement.single_allocation_variants[*variant_index],
                      replacement.single_allocation_variants[*variant_index + 1]);
            if (apply_document_edit(
                    ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})) {
                soa_single_allocation_variant_index_ = *variant_index + 1;
                soa_editor_declaration_.reset();
                return true;
            }
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Delete variant")) {
            auto replacement{*schema};
            replacement.single_allocation_variants.erase(
                replacement.single_allocation_variants.begin() +
                static_cast<std::ptrdiff_t>(*variant_index));
            auto const next_index{
                replacement.single_allocation_variants.empty()
                    ? std::optional<std::size_t>{}
                    : std::optional{std::min(*variant_index,
                                             replacement.single_allocation_variants.size() - 1)}};
            if (apply_document_edit(
                    ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})) {
                soa_single_allocation_variant_index_ = next_index;
                soa_editor_declaration_.reset();
                return true;
            }
        }
        ImGui::EndDisabled();

        if (soa_single_allocation_variant_names_.size() ==
                schema->single_allocation_variants.size() &&
            soa_single_allocation_variant_allocators_.size() ==
                schema->single_allocation_variants.size() &&
            ImGui::BeginTable("soa-single-allocation-variants",
                              3,
                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                  ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp)) {
            ImGui::TableSetupColumn("Edit", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableSetupColumn("Owner type");
            ImGui::TableSetupColumn("Allocator type");
            ImGui::TableHeadersRow();
            for (std::size_t index{}; index < schema->single_allocation_variants.size(); ++index) {
                ImGui::PushID(static_cast<int>(index));
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                auto const row_selected{soa_single_allocation_variant_index_ == index};
                if (ImGui::Selectable("::", row_selected, ImGuiSelectableFlags_SpanAllColumns)) {
                    soa_single_allocation_variant_index_ = index;
                }
                if (ImGui::BeginDragDropSource()) {
                    ImGui::SetDragDropPayload(
                        "SOA_SINGLE_ALLOCATION_VARIANT_ROW", &index, sizeof(index));
                    ImGui::Text("Move %s", schema->single_allocation_variants[index].name.c_str());
                    ImGui::EndDragDropSource();
                }
                if (ImGui::BeginDragDropTarget()) {
                    if (auto const* payload{
                            ImGui::AcceptDragDropPayload("SOA_SINGLE_ALLOCATION_VARIANT_ROW")}) {
                        auto const source_index{*static_cast<std::size_t const*>(payload->Data)};
                        if (source_index < schema->single_allocation_variants.size() &&
                            source_index != index) {
                            pending = *schema;
                            move_element(pending->single_allocation_variants, source_index, index);
                            soa_single_allocation_variant_index_ = index;
                        }
                    }
                    ImGui::EndDragDropTarget();
                }

                ImGui::TableNextColumn();
                ImGui::SetNextItemWidth(-1.0F);
                auto const name_submitted{
                    ImGui::InputText("##name",
                                     soa_single_allocation_variant_names_[index].data(),
                                     soa_single_allocation_variant_names_[index].size(),
                                     ImGuiInputTextFlags_EnterReturnsTrue)};
                if (name_submitted || ImGui::IsItemDeactivatedAfterEdit()) {
                    if (!pending.has_value()) {
                        pending = *schema;
                    }
                    pending->single_allocation_variants[index].name =
                        soa_single_allocation_variant_names_[index].data();
                }

                ImGui::TableNextColumn();
                ImGui::SetNextItemWidth(std::max(60.0F, ImGui::GetContentRegionAvail().x - 28.0F));
                auto const allocator_submitted{
                    ImGui::InputText("##allocator",
                                     soa_single_allocation_variant_allocators_[index].data(),
                                     soa_single_allocation_variant_allocators_[index].size(),
                                     ImGuiInputTextFlags_EnterReturnsTrue)};
                if (allocator_submitted || ImGui::IsItemDeactivatedAfterEdit()) {
                    if (!pending.has_value()) {
                        pending = *schema;
                    }
                    pending->single_allocation_variants[index].allocator.name =
                        soa_single_allocation_variant_allocators_[index].data();
                }
                ImGui::SameLine();
                if (auto picked{draw_type_picker(node.identity.module_name, node.identity)}) {
                    if (!pending.has_value()) {
                        pending = *schema;
                    }
                    pending->single_allocation_variants[index].allocator.name = std::move(*picked);
                }
                ImGui::PopID();
            }
            ImGui::EndTable();
        }

        ImGui::BeginDisabled(!schema->single_allocation_variants.empty());
        if (ImGui::Button("Disable single allocation")) {
            auto replacement{*schema};
            replacement.single_allocation.reset();
            if (apply_document_edit(
                    ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})) {
                soa_editor_declaration_.reset();
                return true;
            }
        }
        ImGui::EndDisabled();
        if (!schema->single_allocation_variants.empty()) {
            ImGui::TextDisabled("Remove allocator variants before disabling single allocation.");
        }
    }

    if (navigate_to.has_value()) {
        selected_type_ = *navigate_to;
        selected_field_.clear();
        record_access_members_.clear();
        record_access_set_explicit_ = false;
        return true;
    }

    if (pending.has_value()) {
        auto const invalid_member{std::ranges::find_if(pending->members, [](auto const& member) {
            return member.name.empty() || member.type.name.empty();
        })};
        if (invalid_member != pending->members.end()) {
            schema_edit_message_ = invalid_member->name.empty()
                                     ? "SoA column name cannot be empty."
                                     : "SoA column type cannot be empty.";
            return false;
        }
        if (pending->fixed.has_value() &&
            std::ranges::any_of(pending->fixed->containers,
                                [](std::string const& name) { return name.empty(); })) {
            schema_edit_message_ = "Fixed-layout container type cannot be empty.";
            return false;
        }
        if (std::ranges::any_of(pending->using_declarations, [](std::string const& declaration) {
                return declaration.find_first_not_of(" \t\r\n") == std::string::npos;
            })) {
            schema_edit_message_ = "SoA using declaration cannot be empty.";
            return false;
        }
        auto const invalid_function{
            std::ranges::find_if(pending->functions, [](auto const& function) {
                return function.name.empty() || function.return_type.name.empty();
            })};
        if (invalid_function != pending->functions.end()) {
            schema_edit_message_ = invalid_function->name.empty()
                                     ? "SoA function name cannot be empty."
                                     : "SoA function return type cannot be empty.";
            return false;
        }
        for (auto const& function : pending->functions) {
            if (function.trailing_return_type.has_value() &&
                function.trailing_return_type->name.empty()) {
                schema_edit_message_ = "SoA function trailing return type cannot be empty.";
                return false;
            }
            if (function.template_parameters.has_value() &&
                function.template_parameters->find_first_not_of(" \t\r\n") == std::string::npos) {
                schema_edit_message_ = "SoA function template parameters cannot be empty.";
                return false;
            }
            if (function.requires_clause.has_value() &&
                function.requires_clause->find_first_not_of(" \t\r\n") == std::string::npos) {
                schema_edit_message_ = "SoA function requires clause cannot be empty.";
                return false;
            }
            auto const invalid_dependency{
                std::ranges::find_if(function.dependencies, [](auto const& dependency) {
                    return dependency.find_first_not_of(" \t\r\n") == std::string::npos;
                })};
            if (invalid_dependency != function.dependencies.end()) {
                schema_edit_message_ = "SoA function dependency key cannot be empty.";
                return false;
            }
            auto const invalid_parameter{
                std::ranges::find_if(function.parameters, [](auto const& parameter) {
                    return parameter.name.empty() || parameter.type.name.empty() ||
                           (parameter.default_value.has_value() &&
                            parameter.default_value->find_first_not_of(" \t\r\n") ==
                                std::string::npos);
                })};
            if (invalid_parameter != function.parameters.end()) {
                if (invalid_parameter->name.empty()) {
                    schema_edit_message_ = "SoA function parameter name cannot be empty.";
                } else if (invalid_parameter->type.name.empty()) {
                    schema_edit_message_ = "SoA function parameter type cannot be empty.";
                } else {
                    schema_edit_message_ = "SoA function parameter default cannot be empty.";
                }
                return false;
            }
        }
        auto const invalid_variant{
            std::ranges::find_if(pending->single_allocation_variants, [](auto const& variant) {
                return variant.name.empty() || variant.allocator.name.empty();
            })};
        if (invalid_variant != pending->single_allocation_variants.end()) {
            schema_edit_message_ = invalid_variant->name.empty()
                                     ? "Single-allocation variant owner type cannot be empty."
                                     : "Single-allocation variant allocator type cannot be empty.";
            return false;
        }
        if (apply_document_edit(
                ReplaceSoa{.declaration = *declaration, .schema = std::move(*pending)})) {
            if (soa_access_set_explicit_ && renamed_member.has_value()) {
                auto const existing{soa_access_columns_.find(renamed_member->first)};
                if (existing != soa_access_columns_.end()) {
                    auto const operation{existing->second};
                    soa_access_columns_.erase(existing);
                    soa_access_columns_.insert_or_assign(renamed_member->second, operation);
                }
            }
            selected_field_ = std::move(selected_after_edit);
            soa_editor_declaration_.reset();
            return true;
        }
    }
    return false;
}

void PlannerUi::draw_variants_panel() {
    if (!variants_view_open_) {
        return;
    }
    auto const was_open{variants_view_open_};
    ImGui::Begin("Variants", &variants_view_open_);
    persist_view_visibility(was_open, variants_view_open_);
    auto const baseline_before_actions{workspace_.active_variant_id() ==
                                       LayoutWorkspace::baseline_variant_id};
    if (ImGui::BeginTable("variant-actions",
                          2,
                          ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoSavedSettings)) {
        ImGui::TableNextColumn();
        if (ImGui::Button("New experiment", {-1.0F, 0.0F})) {
            create_variant_for_selected_schema();
        }
        ImGui::TableNextColumn();
        if (ImGui::Button("Duplicate", {-1.0F, 0.0F})) {
            auto const name{workspace_.active_variant().name + " copy"};
            workspace_.duplicate_variant(workspace_.active_variant_id(), name);
            sync_variant_name();
        }
        ImGui::TableNextColumn();
        ImGui::BeginDisabled(baseline_before_actions);
        if (ImGui::Button("Reset all overrides", {-1.0F, 0.0F})) {
            workspace_.reset_variant(workspace_.active_variant_id());
        }
        ImGui::TableNextColumn();
        if (ImGui::Button("Delete", {-1.0F, 0.0F})) {
            workspace_.delete_variant(workspace_.active_variant_id());
            sync_variant_name();
            packed_dragged_divider_.reset();
            packed_dragged_variant_id_.reset();
        }
        ImGui::EndDisabled();
        ImGui::EndTable();
    }

    auto const baseline{workspace_.active_variant_id() == LayoutWorkspace::baseline_variant_id};
    ImGui::BeginDisabled(baseline);
    if (variant_name_id_ != workspace_.active_variant_id()) {
        sync_variant_name();
    }
    if (ImGui::InputText("Name",
                         variant_name_.data(),
                         variant_name_.size(),
                         ImGuiInputTextFlags_EnterReturnsTrue)) {
        workspace_.rename_variant(workspace_.active_variant_id(), variant_name_.data());
        sync_variant_name();
    }
    ImGui::EndDisabled();

    ImGui::Separator();
    if (ImGui::BeginChild("variant-list", {0.0F, 0.0F}, false)) {
        for (auto const& variant : workspace_.variants()) {
            auto const selected{workspace_.active_variant_id() == variant.id};
            auto const label{variant.id == LayoutWorkspace::baseline_variant_id
                                 ? "Baseline"
                                 : variant.name + " (" +
                                       std::to_string(detail::override_count(variant.overrides)) +
                                       " overrides)"};
            if (ImGui::Selectable(label.c_str(), selected)) {
                workspace_.select_variant(variant.id);
                sync_variant_name();
                packed_dragged_divider_.reset();
                packed_dragged_variant_id_.reset();
            }
        }
    }
    ImGui::EndChild();
    ImGui::End();
}

} // namespace ioj::layout_planner
