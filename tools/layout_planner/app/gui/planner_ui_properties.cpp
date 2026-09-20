#include "planner_ui.hpp"

#include "planner_ui_support.hpp"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <set>
#include <string>
#include <string_view>

namespace ioj::layout_planner {
namespace {

using namespace layout;
using namespace lispb::schema;

inline constexpr std::array packed_relationship_kinds{
    codegen::PackedFieldRelationKind::index_into,
    codegen::PackedFieldRelationKind::count_of,
    codegen::PackedFieldRelationKind::offset_into,
    codegen::PackedFieldRelationKind::discriminates,
    codegen::PackedFieldRelationKind::contains,
    codegen::PackedFieldRelationKind::member_of,
    codegen::PackedFieldRelationKind::quantises,
    codegen::PackedFieldRelationKind::encoded_as,
    codegen::PackedFieldRelationKind::references,
};

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
    ImGui::SeparatorText("Local declarations");
    for (auto const& declaration : document_->declarations()) {
        if (declaration.identity.module_name == module_name && declaration.identity != owner) {
            auto const type{workspace_.types().find(declaration.identity)};
            if (type.has_value()) {
                draw_candidate(declaration.identity.name,
                               workspace_.types().type(*type).cpp_spelling);
            }
        }
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
    ImGui::Begin("Properties");
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
    if (selected_declaration.has_value() && !std::holds_alternative<SoaType>(node.definition)) {
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
        auto const& underlying{workspace_.types().type(enumeration->underlying_type.type)};
        ImGui::TextUnformatted("Underlying type");
        ImGui::SameLine();
        if (ImGui::SmallButton(underlying.cpp_spelling.c_str())) {
            selected_type_ = enumeration->underlying_type.type;
            selected_field_.clear();
            record_access_members_.clear();
            record_access_set_explicit_ = false;
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
        ImGui::TextDisabled("Tagged discriminants are a separate future representation layer.");
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
    if (auto const* schema{document_->soa_schema(*declaration)}) {
        if (schema->view_name.has_value() || schema->const_view_name.has_value() ||
            schema->fixed.has_value() || schema->single_allocation.has_value() ||
            !schema->single_allocation_variants.empty() || schema->field_mask_name.has_value() ||
            schema->field_enum_name.has_value()) {
            schema_edit_message_ =
                "SoAs with explicit generated helper names cannot yet be duplicated safely.";
            return false;
        }
        auto copy{*schema};
        copy.name = name;
        return apply_document_edit(CreateSoa{.declaration = id,
                                             .module_index = info->module_index,
                                             .schema = std::move(copy),
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

    auto result{document_->apply(std::move(*command))};
    if (!result.has_value()) {
        schema_edit_message_ = result.error().message;
        return false;
    }
    if (!*result) {
        return false;
    }
    schema_edit_message_.clear();
    record_access_members_.clear();
    record_access_set_explicit_ = false;
    sync_document_graph(std::nullopt);
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
    if (enum_editor_declaration_ != declaration || enum_editor_value_ != selected_enumerator_) {
        enum_editor_declaration_ = declaration;
        enum_editor_value_ = selected_enumerator_;
        if (selected != schema->values.end()) {
            std::snprintf(
                enum_value_name_.data(), enum_value_name_.size(), "%s", selected->name.c_str());
            set_buffer(enum_value_initializer_, selected->initializer);
            set_buffer(enum_value_display_name_, selected->display_name);
            set_buffer(enum_value_serialized_name_, selected->serialized_name);
        }
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
    ImGui::TextDisabled("Semantic width describes the value domain; the underlying type remains a "
                        "lowering choice.");

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

auto PlannerUi::draw_integer_scalar_editor(TypeNode const& node, IntegerScalarType const&) -> bool {
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
                        ? std::ranges::find(packed_relationship_kinds, field->relationship->kind)
                        : packed_relationship_kinds.end()};
                packed_relationship_kind_ =
                    relationship_kind == packed_relationship_kinds.end()
                        ? 0
                        : static_cast<int>(relationship_kind - packed_relationship_kinds.begin());
            } else {
                packed_field_type_.front() = '\0';
                packed_field_minimum_.front() = '\0';
                packed_field_maximum_.front() = '\0';
                packed_relationship_target_.front() = '\0';
                packed_relationship_kind_ = 0;
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
        replacement.segments.erase(replacement.segments.begin() +
                                   static_cast<std::ptrdiff_t>(*selected_index));
        auto const next_index{std::min(*selected_index, replacement.segments.size() - 1)};
        auto const next_name{codegen::packed_segment_name(replacement.segments[next_index])};
        if (apply_document_edit(ReplacePackedValue{.declaration = *declaration,
                                                   .schema = std::move(replacement)})) {
            selected_field_ = next_name;
            return true;
        }
    }
    ImGui::EndDisabled();
    ImGui::EndDisabled();

    std::optional<codegen::PackedValueSchema> pending;
    std::optional<TypeId> navigate_to;
    auto selected_after_edit{selected_field_};
    if (ImGui::BeginTable("packed-schema-fields",
                          9,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                              ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Edit", ImGuiTableColumnFlags_WidthFixed);
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
                    std::get<codegen::PackedFieldSchema>(pending->segments[index]).type.name =
                        std::move(*picked);
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
            auto const kind_label{field == nullptr ? "reserved"
                                  : field->kind == codegen::PackedFieldKind::enumeration ? "enum"
                                  : field->kind == codegen::PackedFieldKind::signed_integer
                                      ? "signed"
                                      : "unsigned"};
            if (field == nullptr) {
                ImGui::TextDisabled("reserved");
            } else if (row_selected && ImGui::BeginCombo("##kind", kind_label)) {
                if (ImGui::Selectable("unsigned",
                                      field->kind == codegen::PackedFieldKind::unsigned_integer)) {
                    pending = *schema;
                    auto& pending_field{
                        std::get<codegen::PackedFieldSchema>(pending->segments[index])};
                    pending_field.kind = codegen::PackedFieldKind::unsigned_integer;
                    if (auto const mapped{matching_unsigned_type(pending_field.type.name)}) {
                        pending_field.type.name = *mapped;
                    }
                    auto const has_negative_code{
                        std::ranges::any_of(pending_field.named_codes,
                                            [](auto const& code) { return code.value.negative; })};
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
                if (ImGui::Selectable("signed",
                                      field->kind == codegen::PackedFieldKind::signed_integer)) {
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
                            ? std::get<lispb::schema::PackedField>(packed.segments[index]).bit_width
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
                ImGui::EndCombo();
            } else if (!row_selected) {
                ImGui::TextUnformatted(kind_label);
            }

            ImGui::TableNextColumn();
            auto range_helper{field != nullptr && field->range_helper};
            if (field == nullptr) {
                ImGui::TextDisabled("-");
            } else if (row_selected) {
                ImGui::BeginDisabled(field->kind != codegen::PackedFieldKind::unsigned_integer);
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
        ImGui::SeparatorText("Semantic relationship");
        ImGui::SetNextItemWidth(180.0F);
        auto const current_kind{packed_relationship_kinds[static_cast<std::size_t>(
            std::clamp(packed_relationship_kind_,
                       0,
                       static_cast<int>(packed_relationship_kinds.size() - 1)))]};
        if (ImGui::BeginCombo("Kind",
                              codegen::packed_field_relation_kind_name(current_kind).data())) {
            for (std::size_t kind_index{}; kind_index < packed_relationship_kinds.size();
                 ++kind_index) {
                auto const kind{packed_relationship_kinds[kind_index]};
                auto const chosen{packed_relationship_kind_ == static_cast<int>(kind_index)};
                if (ImGui::Selectable(codegen::packed_field_relation_kind_name(kind).data(),
                                      chosen)) {
                    packed_relationship_kind_ = static_cast<int>(kind_index);
                    if (selected_schema_field->relationship.has_value()) {
                        auto replacement{*schema};
                        std::get<codegen::PackedFieldSchema>(replacement.segments[*selected_index])
                            .relationship->kind = kind;
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
            relationship = codegen::PackedFieldRelationSchema{
                .kind = current_kind,
                .target = codegen::TypeRef{
                    .name = *picked_relationship_target, .suffix = {}, .nested = std::nullopt}};
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
                record_access_members_.clear();
                record_access_set_explicit_ = false;
                return true;
            }
        } else {
            ImGui::BeginDisabled(packed_relationship_target_.front() == '\0');
            if (ImGui::SmallButton("Add")) {
                auto replacement{*schema};
                std::get<codegen::PackedFieldSchema>(replacement.segments[*selected_index])
                    .relationship = codegen::PackedFieldRelationSchema{
                    .kind = current_kind,
                    .target = codegen::TypeRef{.name = packed_relationship_target_.data(),
                                               .suffix = {},
                                               .nested = std::nullopt}};
                if (apply_document_edit(ReplacePackedValue{.declaration = *declaration,
                                                           .schema = std::move(replacement)})) {
                    selected_field_ = selected_segment_name;
                    return true;
                }
            }
            ImGui::EndDisabled();
        }
        ImGui::TextDisabled(
            "Relationships are semantic graph edges; width changes only when factual capacity "
            "metadata exists.");
    }

    auto selected_code_after_edit{selected_packed_code_};
    if (!pending.has_value() && selected_schema_field != nullptr && selected_index.has_value()) {
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
        }
    }

    if (ImGui::Button("+ Member")) {
        auto replacement{*schema};
        auto name{unique_record_member_name(replacement.members, "member")};
        replacement.members.push_back(codegen::RecordMemberSchema{
            .name = name,
            .type = codegen::TypeRef{.name = "std::uint32_t", .suffix = {}, .nested = std::nullopt},
            .count = std::nullopt});
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
            record_access_members_.insert(member.name);
        }
        record_access_set_explicit_ = true;
    }

    std::optional<codegen::RecordSchema> pending;
    std::optional<TypeId> navigate_to;
    std::optional<std::pair<std::string, std::string>> renamed_member;
    auto selected_after_edit{selected_field_};
    if (ImGui::BeginTable("record-schema-members",
                          6,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                              ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Edit", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Access", ImGuiTableColumnFlags_WidthFixed);
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
                        record_access_members_.insert(selected_field_);
                    }
                    record_access_set_explicit_ = true;
                }
                if (accessed) {
                    record_access_members_.insert(member.name);
                } else {
                    record_access_members_.erase(member.name);
                }
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
            if (record_access_set_explicit_ && renamed_member.has_value() &&
                record_access_members_.erase(renamed_member->first) != 0) {
                record_access_members_.insert(renamed_member->second);
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

    if (soa_editor_declaration_ != declaration || soa_editor_member_ != selected_field_) {
        soa_editor_declaration_ = declaration;
        soa_editor_member_ = selected_field_;
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
            .mask_dimensions = {}});
        if (apply_document_edit(
                ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})) {
            selected_field_ = std::move(name);
            return true;
        }
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!selected_index.has_value());
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
    ImGui::BeginDisabled(!selected_index.has_value() || schema->members.size() == 1);
    if (ImGui::Button("Delete")) {
        auto replacement{*schema};
        replacement.members.erase(replacement.members.begin() +
                                  static_cast<std::ptrdiff_t>(*selected_index));
        auto const next_index{std::min(*selected_index, replacement.members.size() - 1)};
        auto const next_name{replacement.members[next_index].name};
        if (apply_document_edit(
                ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})) {
            selected_field_ = next_name;
            return true;
        }
    }
    ImGui::EndDisabled();
    ImGui::EndDisabled();

    std::optional<codegen::SoaSchema> pending;
    std::optional<TypeId> navigate_to;
    auto selected_after_edit{selected_field_};
    if (ImGui::BeginTable("soa-schema-members",
                          4,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                              ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Edit", ImGuiTableColumnFlags_WidthFixed);
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
                }
            } else {
                ImGui::TextUnformatted(member.name.c_str());
            }

            ImGui::TableNextColumn();
            if (row_selected) {
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
                if (ImGui::Selectable("nested", member.kind == codegen::SoaMemberKind::nested)) {
                    pending = *schema;
                    pending->members[index].kind = codegen::SoaMemberKind::nested;
                }
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
                    .mask_dimensions = {}});
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
                pending = *schema;
                pending->members[*selected_index].mask_field = included;
                if (!included) {
                    pending->members[*selected_index].mask_dimensions.clear();
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

        if (!member.mask_dimensions.empty()) {
            ImGui::TextDisabled("Mask dimensions");
            for (auto const& dimension : member.mask_dimensions) {
                ImGui::BulletText("%s: %s", dimension.index_name.c_str(), dimension.extent.c_str());
            }
        }
        ImGui::TextDisabled("Mask dimensions remain source-preserved and read only.");
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
        if (apply_document_edit(
                ReplaceSoa{.declaration = *declaration, .schema = std::move(*pending)})) {
            selected_field_ = std::move(selected_after_edit);
            return true;
        }
    }
    return false;
}

void PlannerUi::draw_variants_panel() {
    ImGui::Begin("Variants");
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
