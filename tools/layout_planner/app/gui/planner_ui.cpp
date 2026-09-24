#include "planner_ui.hpp"

#include "../platform/file_dialog.hpp"
#include "planner_ui_support.hpp"

#include <codegen/schema/fixed_point_value.h>
#include <ioj/layout/planner_type.hpp>
#include <lispb/target_compiler.h>

#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <exception>
#include <iterator>
#include <set>
#include <string_view>
#include <tuple>
#include <utility>
#include <variant>

namespace ioj::layout_planner {

void PlannerUi::set_file_dialog(FileDialog* const dialog) {
    file_dialog_ = dialog;
}

namespace {

using namespace layout;
using namespace lispb::schema;

auto format_cache_capacity(std::uint64_t const bytes, int const unit) -> std::string {
    auto const divisor{std::uint64_t{1} << (unit * 10)};
    auto result{std::to_string(bytes / divisor)};
    auto remainder{bytes % divisor};
    if (remainder == 0) {
        return result;
    }
    result += '.';
    while (remainder != 0) {
        remainder *= 10;
        result += static_cast<char>('0' + remainder / divisor);
        remainder %= divisor;
    }
    return result;
}

auto parse_cache_capacity(std::string_view const text, int const unit)
    -> std::optional<std::uint64_t> {
    auto parsed{codegen::parse_fixed_point_value(text, static_cast<std::uint32_t>(unit * 10))};
    if (!parsed.has_value() || parsed->negative) {
        return std::nullopt;
    }
    return parsed->magnitude;
}

void draw_docked_panel_outlines() {
    constexpr std::array names{"Project / Schema",
                               "Layout",
                               "Target Profile",
                               "Properties",
                               "Variants",
                               "Comparison",
                               "Graph",
                               "Source",
                               "Diagnostics"};
    auto const thickness{std::max(2.0F, ImGui::GetStyle().WindowBorderSize)};
    for (auto const* name : names) {
        auto* window{ImGui::FindWindowByName(name)};
        if (window == nullptr || window->LastFrameActive != ImGui::GetFrameCount() ||
            !window->DockIsActive || !window->DockTabIsVisible || window->DockNode == nullptr) {
            continue;
        }

        auto const& node{*window->DockNode};
        auto const inset{thickness * 0.5F};
        auto const minimum{ImVec2{node.Pos.x + inset, node.Pos.y + inset}};
        auto const maximum{
            ImVec2{node.Pos.x + node.Size.x - inset, node.Pos.y + node.Size.y - inset}};
        auto const color{node.IsFocused ? IM_COL32(130, 186, 238, 255)
                                        : IM_COL32(113, 137, 163, 255)};
        window->DrawList->PushClipRect(
            node.Pos, {node.Pos.x + node.Size.x, node.Pos.y + node.Size.y}, false);
        window->DrawList->AddRect(minimum, maximum, color, 0.0F, ImDrawFlags_None, thickness);
        window->DrawList->PopClipRect();
    }
}

auto parse_float_setting(std::string_view const line, std::string_view const prefix)
    -> std::optional<float> {
    if (!line.starts_with(prefix)) {
        return std::nullopt;
    }
    auto const value{line.substr(prefix.size())};
    float parsed{};
    auto const [end, error]{std::from_chars(value.data(), value.data() + value.size(), parsed)};
    return error == std::errc{} && end == value.data() + value.size() ? std::optional{parsed}
                                                                      : std::nullopt;
}

auto percent_encode(std::string_view const value) -> std::string {
    constexpr char hex[]{"0123456789ABCDEF"};
    std::string result;
    result.reserve(value.size());
    for (auto const byte : value) {
        auto const character{static_cast<unsigned char>(byte)};
        auto const unreserved{(character >= 'a' && character <= 'z') ||
                              (character >= 'A' && character <= 'Z') ||
                              (character >= '0' && character <= '9') || character == '-' ||
                              character == '_' || character == '.' || character == '~'};
        if (unreserved) {
            result.push_back(static_cast<char>(character));
        } else {
            result.push_back('%');
            result.push_back(hex[character >> 4U]);
            result.push_back(hex[character & 0x0fU]);
        }
    }
    return result;
}

auto percent_decode(std::string_view const value) -> std::optional<std::string> {
    auto hex_value = [](char const character) -> std::optional<unsigned char> {
        if (character >= '0' && character <= '9') {
            return static_cast<unsigned char>(character - '0');
        }
        if (character >= 'A' && character <= 'F') {
            return static_cast<unsigned char>(10 + character - 'A');
        }
        if (character >= 'a' && character <= 'f') {
            return static_cast<unsigned char>(10 + character - 'a');
        }
        return std::nullopt;
    };

    std::string result;
    result.reserve(value.size());
    for (std::size_t index{}; index < value.size(); ++index) {
        if (value[index] != '%') {
            result.push_back(value[index]);
            continue;
        }
        if (index + 2 >= value.size()) {
            return std::nullopt;
        }
        auto const high{hex_value(value[index + 1])};
        auto const low{hex_value(value[index + 2])};
        if (!high.has_value() || !low.has_value()) {
            return std::nullopt;
        }
        result.push_back(static_cast<char>((*high << 4U) | *low));
        index += 2;
    }
    return result;
}

auto parse_graph_position(std::string_view const value)
    -> std::optional<std::tuple<std::string, TypeIdentity, std::array<float, 2>>> {
    std::array<std::string_view, 7> parts;
    auto remaining{value};
    for (std::size_t index{}; index + 1 < parts.size(); ++index) {
        auto const separator{remaining.find('|')};
        if (separator == std::string_view::npos) {
            return std::nullopt;
        }
        parts[index] = remaining.substr(0, separator);
        remaining.remove_prefix(separator + 1);
    }
    parts.back() = remaining;

    int origin{};
    auto const [origin_end, origin_error]{
        std::from_chars(parts[1].data(), parts[1].data() + parts[1].size(), origin)};
    float x{};
    auto const [x_end,
                x_error]{std::from_chars(parts[5].data(), parts[5].data() + parts[5].size(), x)};
    float y{};
    auto const [y_end,
                y_error]{std::from_chars(parts[6].data(), parts[6].data() + parts[6].size(), y)};
    auto const project{percent_decode(parts[0])};
    auto const module{percent_decode(parts[2])};
    auto const namespace_name{percent_decode(parts[3])};
    auto const name{percent_decode(parts[4])};
    if (origin_error != std::errc{} || origin_end != parts[1].data() + parts[1].size() ||
        origin < static_cast<int>(TypeOrigin::declaration) ||
        origin > static_cast<int>(TypeOrigin::cpp_spelling) || x_error != std::errc{} ||
        x_end != parts[5].data() + parts[5].size() || y_error != std::errc{} ||
        y_end != parts[6].data() + parts[6].size() || !std::isfinite(x) || !std::isfinite(y) ||
        std::abs(x) > 10'000'000.0F || std::abs(y) > 10'000'000.0F || !project.has_value() ||
        !module.has_value() || !namespace_name.has_value() || !name.has_value() ||
        project->empty() || name->empty()) {
        return std::nullopt;
    }
    return std::tuple{*project,
                      TypeIdentity{.origin = static_cast<TypeOrigin>(origin),
                                   .module_name = *module,
                                   .namespace_name = *namespace_name,
                                   .name = *name},
                      std::array{x, y}};
}

auto graph_project_key(std::filesystem::path const& path) -> std::string {
    return path.lexically_normal().generic_string();
}

auto parse_target_profile_mapping(std::string_view const value)
    -> std::optional<std::pair<std::string, std::filesystem::path>> {
    auto const separator{value.find('|')};
    if (separator == std::string_view::npos ||
        value.find('|', separator + 1) != std::string_view::npos) {
        return std::nullopt;
    }
    auto const project{percent_decode(value.substr(0, separator))};
    auto const profile{percent_decode(value.substr(separator + 1))};
    if (!project.has_value() || project->empty() || !profile.has_value() || profile->empty()) {
        return std::nullopt;
    }
    return std::pair{*project, std::filesystem::path{*profile}};
}

auto parse_recent_target_mapping(std::string_view const value)
    -> std::optional<std::pair<std::string, std::string>> {
    auto const separator{value.find('|')};
    if (separator == std::string_view::npos ||
        value.find('|', separator + 1) != std::string_view::npos) {
        return std::nullopt;
    }
    auto const project{percent_decode(value.substr(0, separator))};
    auto const target{percent_decode(value.substr(separator + 1))};
    if (!project.has_value() || project->empty() || !target.has_value() || target->empty()) {
        return std::nullopt;
    }
    return std::pair{*project, *target};
}

template <std::size_t Size>
void set_text_buffer(std::array<char, Size>& buffer, std::string_view const value) {
    static_assert(Size > 0);
    buffer.fill('\0');
    auto const count{std::min(value.size(), Size - 1)};
    std::copy_n(value.begin(), count, buffer.begin());
}

auto parse_int_setting(std::string_view const line, std::string_view const prefix)
    -> std::optional<int> {
    if (!line.starts_with(prefix)) {
        return std::nullopt;
    }
    auto const value{line.substr(prefix.size())};
    int parsed{};
    auto const [end, error]{std::from_chars(value.data(), value.data() + value.size(), parsed)};
    return error == std::errc{} && end == value.data() + value.size() ? std::optional{parsed}
                                                                      : std::nullopt;
}

auto known_or_unknown(std::optional<std::string> const& value) -> char const* {
    return value.has_value() && !value->empty() ? value->c_str() : "Unknown";
}

auto known_or_unknown(std::string const& value) -> char const* {
    return value.empty() ? "Unknown" : value.c_str();
}

auto type_fact_sources(AbiProfile const& abi) -> std::string {
    std::set<std::string, std::less<>> sources;
    bool has_unknown{};
    for (auto const& [spelling, facts] : abi.types()) {
        static_cast<void>(spelling);
        if (facts.provenance.empty()) {
            has_unknown = true;
        } else {
            sources.insert(facts.provenance);
        }
    }
    std::string result;
    for (auto const& source : sources) {
        if (!result.empty()) {
            result += "; ";
        }
        result += source;
    }
    if (has_unknown) {
        if (!result.empty()) {
            result += "; ";
        }
        result += "Unknown";
    }
    return result.empty() ? "Unknown" : result;
}

void draw_target_profile_summary(AbiProfile const& abi) {
    auto const& identity{abi.identity()};
    auto const& memory{abi.memory_facts()};
    auto const primitive_sources{type_fact_sources(abi)};
    auto const cache_line_bytes{detail::format_bytes(memory.cache_line_bytes)};
    auto const page_bytes{detail::format_bytes(memory.page_bytes)};
    auto const l1_capacity{detail::format_bytes(memory.l1_data_cache_bytes)};
    auto const l2_capacity{detail::format_bytes(memory.l2_cache_bytes)};
    auto const l3_capacity{detail::format_bytes(memory.l3_cache_bytes)};
    if (ImGui::BeginTable("target-profile",
                          2,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                              ImGuiTableFlags_SizingStretchProp)) {
        auto draw_row{[](char const* const label, char const* const value) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(label);
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(value);
        }};
        draw_row("Profile", known_or_unknown(abi.name()));
        draw_row("Platform", known_or_unknown(identity.platform));
        draw_row("Architecture", known_or_unknown(identity.architecture));
        draw_row("ABI", known_or_unknown(identity.abi));
        draw_row("Compiler", known_or_unknown(identity.compiler));
        draw_row("Build configuration", known_or_unknown(identity.build_configuration));
        draw_row("Cache-line size", cache_line_bytes.c_str());
        draw_row("Page size", page_bytes.c_str());
        draw_row("L1 data cache capacity", l1_capacity.c_str());
        draw_row("L2 cache capacity", l2_capacity.c_str());
        draw_row("L3 cache capacity", l3_capacity.c_str());
        draw_row("Primitive fact source", primitive_sources.c_str());
        draw_row("Memory fact source", known_or_unknown(memory.provenance));
        ImGui::EndTable();
    }

    if (ImGui::TreeNodeEx("Primitive facts", ImGuiTreeNodeFlags_SpanAvailWidth)) {
        if (abi.types().empty()) {
            ImGui::TextDisabled("No primitive facts; physical layout remains Unknown.");
        } else if (ImGui::BeginTable("target-primitive-facts",
                                     6,
                                     ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                         ImGuiTableFlags_SizingStretchProp)) {
            ImGui::TableSetupColumn("Type");
            ImGui::TableSetupColumn("Size");
            ImGui::TableSetupColumn("Alignment");
            ImGui::TableSetupColumn("Kind");
            ImGui::TableSetupColumn("Value bits");
            ImGui::TableSetupColumn("Provenance");
            ImGui::TableHeadersRow();
            for (auto const& [spelling, facts] : abi.types()) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(spelling.c_str());
                ImGui::TableNextColumn();
                ImGui::Text("%llu", static_cast<unsigned long long>(facts.size_bytes));
                ImGui::TableNextColumn();
                ImGui::Text("%llu", static_cast<unsigned long long>(facts.alignment_bytes));
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(!facts.integer_signed.has_value()
                                           ? "Non-integer"
                                           : (*facts.integer_signed ? "Signed" : "Unsigned"));
                ImGui::TableNextColumn();
                if (facts.unsigned_value_bits.has_value()) {
                    ImGui::Text("%u", *facts.unsigned_value_bits);
                } else {
                    ImGui::TextDisabled("Unknown");
                }
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(known_or_unknown(facts.provenance));
            }
            ImGui::EndTable();
        }

        if (!abi.representations().empty() &&
            ImGui::BeginTable("target-representation-aliases",
                              2,
                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                  ImGuiTableFlags_SizingStretchProp)) {
            ImGui::TableSetupColumn("Representation alias");
            ImGui::TableSetupColumn("Represented by");
            ImGui::TableHeadersRow();
            for (auto const& [spelling, represented_by] : abi.representations()) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(spelling.c_str());
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(represented_by.c_str());
            }
            ImGui::EndTable();
        }
        ImGui::TreePop();
    }
}

} // namespace

PlannerUi::PlannerUi(SchemaLoadResult loaded)
    : project_path_{std::move(loaded.project_path)}
    , target_name_{std::move(loaded.target_name)}
    , project_document_{std::move(loaded.project_document)}
    , document_{std::move(loaded.document)}
    , analysis_session_{document_.has_value() ? document_->types() : TypeGraph{}}
    , load_diagnostics_{std::move(loaded.diagnostics)} {
    target_memory_fact_defaults_ = analysis_session_.primary_abi().memory_facts();
    sync_target_memory_fact_inputs();
    auto const types{analysis_session_.inputs.workspace.types().types()};
    auto const found{std::ranges::find_if(
        types, [](auto const& type) { return declaration_capabilities(type).visible; })};
    if (found != types.end()) {
        select_type(TypeId{static_cast<std::uint32_t>(found - types.begin())});
    }
    sync_variant_name();
}

void PlannerUi::sync_target_memory_fact_inputs() {
    auto write_value = [](auto& buffer, std::optional<std::uint64_t> const value) {
        buffer.fill('\0');
        if (!value.has_value()) {
            return;
        }
        auto const [end,
                    error]{std::to_chars(buffer.data(), buffer.data() + buffer.size() - 1, *value)};
        if (error == std::errc{}) {
            *end = '\0';
        }
    };
    auto const& memory{analysis_session_.primary_abi().memory_facts()};
    write_value(target_cache_line_bytes_, memory.cache_line_bytes);
    write_value(target_page_bytes_, memory.page_bytes);
    auto write_cache = [](auto& buffer, int& unit, std::optional<std::uint64_t> const value) {
        buffer.fill('\0');
        unit = 1;
        if (!value.has_value()) {
            return;
        }
        if (*value != 0) {
            for (auto candidate{3}; candidate >= 1; --candidate) {
                if (*value % (std::uint64_t{1} << (candidate * 10)) == 0) {
                    unit = candidate;
                    break;
                }
            }
        }
        auto const text{format_cache_capacity(*value, unit)};
        std::snprintf(buffer.data(), buffer.size(), "%s", text.c_str());
    };
    write_cache(
        target_l1_data_cache_bytes_, target_l1_data_cache_unit_, memory.l1_data_cache_bytes);
    write_cache(target_l2_cache_bytes_, target_l2_cache_unit_, memory.l2_cache_bytes);
    write_cache(target_l3_cache_bytes_, target_l3_cache_unit_, memory.l3_cache_bytes);
    target_memory_fact_error_.clear();
}

auto PlannerUi::load_target_profile(std::filesystem::path const& path, bool const persist) -> bool {
    auto loaded{load_abi_profile(path)};
    if (!loaded.has_value()) {
        target_profile_load_error_ =
            loaded.error().line == 0
                ? loaded.error().message
                : "Line " + std::to_string(loaded.error().line) + ": " + loaded.error().message;
        return false;
    }

    auto stored_path{path.lexically_normal()};
    std::error_code path_error;
    auto const absolute_path{std::filesystem::absolute(path, path_error)};
    if (!path_error) {
        stored_path = absolute_path.lexically_normal();
    }
    auto const path_text{stored_path.string()};
    set_text_buffer(target_profile_path_, path_text);
    analysis_session_.set_primary_abi(std::move(*loaded));
    target_memory_fact_defaults_ = analysis_session_.primary_abi().memory_facts();
    sync_target_memory_fact_inputs();
    target_profile_load_error_.clear();

    if (persist && !project_path_.empty()) {
        persisted_target_profile_paths_.insert_or_assign(graph_project_key(project_path_),
                                                         std::move(stored_path));
        ImGui::MarkIniSettingsDirty();
    }
    return true;
}

void PlannerUi::use_builtin_target_profile(bool const clear_persisted) {
    analysis_session_.set_primary_abi(AbiProfile::host_common());
    target_memory_fact_defaults_ = analysis_session_.primary_abi().memory_facts();
    sync_target_memory_fact_inputs();
    target_profile_load_error_.clear();
    target_profile_path_.fill('\0');

    if (clear_persisted && !project_path_.empty() &&
        persisted_target_profile_paths_.erase(graph_project_key(project_path_)) != 0) {
        ImGui::MarkIniSettingsDirty();
    }
}

auto PlannerUi::load_comparison_target_profile(std::filesystem::path const& path) -> bool {
    auto loaded{load_abi_profile(path)};
    if (!loaded.has_value()) {
        comparison_target_profile_error_ =
            loaded.error().line == 0
                ? loaded.error().message
                : "Line " + std::to_string(loaded.error().line) + ": " + loaded.error().message;
        return false;
    }

    auto stored_path{path.lexically_normal()};
    std::error_code path_error;
    auto const absolute_path{std::filesystem::absolute(path, path_error)};
    if (!path_error) {
        stored_path = absolute_path.lexically_normal();
    }
    set_text_buffer(comparison_target_profile_path_, stored_path.string());
    analysis_session_.set_comparison_abi(std::move(*loaded));
    comparison_target_profile_error_.clear();
    return true;
}

void PlannerUi::use_builtin_comparison_target_profile() {
    analysis_session_.set_comparison_abi(AbiProfile::host_common());
    comparison_target_profile_path_.fill('\0');
    comparison_target_profile_error_.clear();
}

auto PlannerUi::draw_target_profile() -> bool {
    draw_target_profile_summary(analysis_session_.primary_abi());

    ImGui::SeparatorText("Generated target profile");
    ImGui::TextDisabled(
        "Loads explicit compiler/configuration facts for this analysis session; it never modifies "
        "LispB.");
    ImGui::TextWrapped("Profile: %s",
                       target_profile_path_.front() == '\0' ? "None" : target_profile_path_.data());

    bool changed{};
    if (ImGui::Button("Load profile")) {
        auto chosen{file_dialog_->open_file(target_profile_path_.data(), "")};
        if (!chosen.has_value()) {
            target_profile_load_error_ = chosen.error();
        } else if (chosen->has_value()) {
            changed = load_target_profile(**chosen, true);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Use built-in profile")) {
        use_builtin_target_profile(true);
        changed = true;
    }
    if (!target_profile_load_error_.empty()) {
        ImGui::TextColored(
            ImVec4{0.95F, 0.45F, 0.35F, 1.0F}, "%s", target_profile_load_error_.c_str());
    }

    ImGui::SeparatorText("Session memory facts");
    ImGui::TextDisabled(
        "Values apply to analysis only. Empty fields are Unknown; line/page sizes must be "
        "non-zero.");
    constexpr auto flags{ImGuiInputTextFlags_CharsDecimal};
    ImGui::SetNextItemWidth(180.0F);
    ImGui::InputText("Cache-line bytes",
                     target_cache_line_bytes_.data(),
                     target_cache_line_bytes_.size(),
                     flags);
    ImGui::SetNextItemWidth(180.0F);
    ImGui::InputText("Page bytes", target_page_bytes_.data(), target_page_bytes_.size(), flags);
    auto draw_cache_input = [&](char const* label, auto& buffer, int& unit) {
        ImGui::TextUnformatted(label);
        ImGui::SetNextItemWidth(180.0F);
        auto const input_id{std::string{"##value-"} + label};
        ImGui::InputText(input_id.c_str(), buffer.data(), buffer.size(), flags);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(85.0F);
        auto selected_unit{unit};
        auto const unit_id{std::string{"##unit-"} + label};
        if (ImGui::Combo(unit_id.c_str(), &selected_unit, "B\0KiB\0MiB\0GiB\0")) {
            auto const bytes{buffer.front() == '\0' ? std::optional<std::uint64_t>{}
                                                    : parse_cache_capacity(buffer.data(), unit)};
            if (buffer.front() != '\0' && !bytes.has_value()) {
                target_memory_fact_error_ =
                    std::string{label} + " must represent a whole number of bytes.";
                return;
            }
            unit = selected_unit;
            if (bytes.has_value()) {
                auto const text{format_cache_capacity(*bytes, unit)};
                std::snprintf(buffer.data(), buffer.size(), "%s", text.c_str());
            }
        }
    };
    draw_cache_input("L1 data cache", target_l1_data_cache_bytes_, target_l1_data_cache_unit_);
    draw_cache_input("L2 cache", target_l2_cache_bytes_, target_l2_cache_unit_);
    draw_cache_input("L3 cache", target_l3_cache_bytes_, target_l3_cache_unit_);

    if (ImGui::Button("Apply memory facts")) {
        MemoryFacts candidate;
        auto parse_value = [&](char const* const text,
                               char const* const label,
                               bool const require_non_zero,
                               std::optional<std::uint64_t>& output) {
            auto const value{std::string_view{text}};
            if (value.empty()) {
                output.reset();
                return true;
            }
            std::uint64_t parsed{};
            auto const [end,
                        error]{std::from_chars(value.data(), value.data() + value.size(), parsed)};
            if (error != std::errc{} || end != value.data() + value.size()) {
                target_memory_fact_error_ = std::string{label} + " must be an unsigned integer.";
                return false;
            }
            if (require_non_zero && parsed == 0) {
                target_memory_fact_error_ = std::string{label} + " must be non-zero or empty.";
                return false;
            }
            output = parsed;
            return true;
        };
        auto parse_cache = [&](char const* const text,
                               char const* const label,
                               int const unit,
                               std::optional<std::uint64_t>& output) {
            if (*text == '\0') {
                output.reset();
                return true;
            }
            output = parse_cache_capacity(text, unit);
            if (!output.has_value()) {
                target_memory_fact_error_ =
                    std::string{label} + " must represent a whole number of bytes within uint64.";
                return false;
            }
            return true;
        };
        auto const valid{
            parse_value(target_cache_line_bytes_.data(),
                        "Cache-line bytes",
                        true,
                        candidate.cache_line_bytes) &&
            parse_value(target_page_bytes_.data(), "Page bytes", true, candidate.page_bytes) &&
            parse_cache(target_l1_data_cache_bytes_.data(),
                        "L1 data cache",
                        target_l1_data_cache_unit_,
                        candidate.l1_data_cache_bytes) &&
            parse_cache(target_l2_cache_bytes_.data(),
                        "L2 cache",
                        target_l2_cache_unit_,
                        candidate.l2_cache_bytes) &&
            parse_cache(target_l3_cache_bytes_.data(),
                        "L3 cache",
                        target_l3_cache_unit_,
                        candidate.l3_cache_bytes)};
        if (valid) {
            auto const matches_defaults{
                candidate.cache_line_bytes == target_memory_fact_defaults_.cache_line_bytes &&
                candidate.page_bytes == target_memory_fact_defaults_.page_bytes &&
                candidate.l1_data_cache_bytes == target_memory_fact_defaults_.l1_data_cache_bytes &&
                candidate.l2_cache_bytes == target_memory_fact_defaults_.l2_cache_bytes &&
                candidate.l3_cache_bytes == target_memory_fact_defaults_.l3_cache_bytes};
            candidate.provenance =
                matches_defaults ? target_memory_fact_defaults_.provenance : "Session override";
            if (candidate != analysis_session_.primary_abi().memory_facts()) {
                auto profile{analysis_session_.primary_abi()};
                profile.set_memory_facts(std::move(candidate));
                analysis_session_.set_primary_abi(std::move(profile));
                changed = true;
            }
            target_memory_fact_error_.clear();
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Restore profile facts")) {
        if (analysis_session_.primary_abi().memory_facts() != target_memory_fact_defaults_) {
            auto profile{analysis_session_.primary_abi()};
            profile.set_memory_facts(target_memory_fact_defaults_);
            analysis_session_.set_primary_abi(std::move(profile));
            changed = true;
        }
        sync_target_memory_fact_inputs();
    }
    if (!target_memory_fact_error_.empty()) {
        ImGui::TextColored(
            ImVec4{0.95F, 0.45F, 0.35F, 1.0F}, "%s", target_memory_fact_error_.c_str());
    }
    return changed;
}

void PlannerUi::register_settings_handler() {
    ImGuiSettingsHandler handler;
    handler.TypeName = "MemoryLayoutPlanner";
    handler.TypeHash = ImHashStr(handler.TypeName);
    handler.ReadOpenFn = settings_read_open;
    handler.ReadLineFn = settings_read_line;
    handler.WriteAllFn = settings_write_all;
    handler.UserData = this;
    ImGui::AddSettingsHandler(&handler);
}

void PlannerUi::finish_startup(bool const reopen_recent_project) {
    auto const fallback_path{project_path_};
    if (reopen_recent_project) {
        auto const candidates{recent_projects_};
        for (auto const& path : candidates) {
            if (load_project(path, true, true)) {
                return;
            }
        }
    }
    if (!fallback_path.empty()) {
        static_cast<void>(load_project(fallback_path, true, true));
    }
}

auto PlannerUi::window_title() const -> std::string {
    if (project_path_.empty()) {
        return "Memory Layout Planner";
    }
    return project_path_.stem().string() + " - Memory Layout Planner";
}

auto PlannerUi::saved_window_size() const -> std::optional<WindowSize> {
    if (!window_width_.has_value() || !window_height_.has_value() || *window_width_ <= 0 ||
        *window_height_ <= 0) {
        return std::nullopt;
    }
    return WindowSize{.width = *window_width_, .height = *window_height_};
}

void PlannerUi::remember_window_size(WindowSize const size) {
    if (size.width <= 0 || size.height <= 0 ||
        (window_width_ == size.width && window_height_ == size.height)) {
        return;
    }
    window_width_ = size.width;
    window_height_ = size.height;
    ImGui::MarkIniSettingsDirty();
}

void PlannerUi::request_close() {
    if (!has_dirty_changes()) {
        close_confirmed_ = true;
        return;
    }
    open_close_confirmation_ = true;
}

auto PlannerUi::take_close_confirmation() -> bool {
    return std::exchange(close_confirmed_, false);
}

void PlannerUi::validate_comparison_variants() {
    analysis_session_.validate_comparison_variants();
}

void PlannerUi::persist_view_visibility(bool const previous, bool const current) {
    if (previous != current) {
        ImGui::MarkIniSettingsDirty();
    }
}

auto PlannerUi::settings_read_open(ImGuiContext*, ImGuiSettingsHandler* handler, char const* name)
    -> void* {
    if (std::strcmp(name, "Settings") != 0) {
        return nullptr;
    }
    auto* ui{static_cast<PlannerUi*>(handler->UserData)};
    ui->recent_projects_.clear();
    ui->recent_project_targets_.clear();
    ui->graph_node_positions_.clear();
    ui->persisted_graph_node_positions_.clear();
    ui->persisted_target_profile_paths_.clear();
    return ui;
}

void PlannerUi::settings_read_line(ImGuiContext*,
                                   ImGuiSettingsHandler*,
                                   void* entry,
                                   char const* line) {
    auto* ui{static_cast<PlannerUi*>(entry)};
    auto const value{std::string_view{line}};
    if (auto const text_scale{parse_float_setting(value, "TextScale=")}) {
        ui->text_scale_ = std::clamp(*text_scale, 0.75F, 1.75F);
        return;
    }
    if (auto const width{parse_int_setting(value, "WindowWidth=")}) {
        ui->window_width_ = *width;
        return;
    }
    if (auto const height{parse_int_setting(value, "WindowHeight=")}) {
        ui->window_height_ = *height;
        return;
    }
    constexpr std::string_view graph_open_prefix{"GraphOpen="};
    if (value.starts_with(graph_open_prefix)) {
        ui->graph_view_open_ = value.substr(graph_open_prefix.size()) != "0";
        return;
    }
    constexpr std::string_view project_open_prefix{"ProjectOpen="};
    if (value.starts_with(project_open_prefix)) {
        ui->project_view_open_ = value.substr(project_open_prefix.size()) != "0";
        return;
    }
    constexpr std::string_view layout_open_prefix{"LayoutOpen="};
    if (value.starts_with(layout_open_prefix)) {
        ui->layout_view_open_ = value.substr(layout_open_prefix.size()) != "0";
        return;
    }
    constexpr std::string_view target_profile_open_prefix{"TargetProfileOpen="};
    if (value.starts_with(target_profile_open_prefix)) {
        ui->target_profile_view_open_ = value.substr(target_profile_open_prefix.size()) != "0";
        return;
    }
    constexpr std::string_view properties_open_prefix{"PropertiesOpen="};
    if (value.starts_with(properties_open_prefix)) {
        ui->properties_view_open_ = value.substr(properties_open_prefix.size()) != "0";
        return;
    }
    constexpr std::string_view variants_open_prefix{"VariantsOpen="};
    if (value.starts_with(variants_open_prefix)) {
        ui->variants_view_open_ = value.substr(variants_open_prefix.size()) != "0";
        return;
    }
    constexpr std::string_view comparison_open_prefix{"ComparisonOpen="};
    if (value.starts_with(comparison_open_prefix)) {
        ui->comparison_view_open_ = value.substr(comparison_open_prefix.size()) != "0";
        return;
    }
    constexpr std::string_view source_open_prefix{"SourceOpen="};
    if (value.starts_with(source_open_prefix)) {
        ui->source_view_open_ = value.substr(source_open_prefix.size()) != "0";
        return;
    }
    constexpr std::string_view diagnostics_open_prefix{"DiagnosticsOpen="};
    if (value.starts_with(diagnostics_open_prefix)) {
        ui->diagnostics_view_open_ = value.substr(diagnostics_open_prefix.size()) != "0";
        return;
    }
    constexpr std::string_view target_profile_prefix{"TargetProfile="};
    if (value.starts_with(target_profile_prefix)) {
        auto const parsed{parse_target_profile_mapping(value.substr(target_profile_prefix.size()))};
        if (!parsed.has_value()) {
            return;
        }
        auto const& [project, profile]{*parsed};
        ui->persisted_target_profile_paths_.insert_or_assign(project, profile);
        if (project == graph_project_key(ui->project_path_)) {
            ui->use_builtin_target_profile(false);
            set_text_buffer(ui->target_profile_path_, profile.string());
            static_cast<void>(ui->load_target_profile(profile, false));
        }
        return;
    }
    constexpr std::string_view graph_node_prefix{"GraphNode="};
    if (value.starts_with(graph_node_prefix)) {
        auto const parsed{parse_graph_position(value.substr(graph_node_prefix.size()))};
        if (!parsed.has_value()) {
            return;
        }
        auto const& [project, identity, position]{*parsed};
        ui->persisted_graph_node_positions_[project][identity] = position;
        if (project == graph_project_key(ui->project_path_)) {
            auto const types{ui->analysis_session_.inputs.workspace.types().types()};
            if (std::ranges::find(types, identity, &TypeNode::identity) != types.end()) {
                ui->graph_node_positions_[identity] = position;
            }
        }
        return;
    }
    constexpr std::string_view recent_prefix{"RecentProject="};
    if (value.starts_with(recent_prefix) && value.size() > recent_prefix.size() &&
        ui->recent_projects_.size() < 20) {
        ui->recent_projects_.emplace_back(value.substr(recent_prefix.size()));
        return;
    }
    constexpr std::string_view recent_target_prefix{"RecentTarget="};
    if (value.starts_with(recent_target_prefix)) {
        auto const mapping{parse_recent_target_mapping(value.substr(recent_target_prefix.size()))};
        if (mapping.has_value()) {
            ui->recent_project_targets_.insert_or_assign(mapping->first, mapping->second);
        }
    }
}

void PlannerUi::settings_write_all(ImGuiContext*,
                                   ImGuiSettingsHandler* handler,
                                   ImGuiTextBuffer* output) {
    auto const* ui{static_cast<PlannerUi const*>(handler->UserData)};
    output->appendf("[%s][Settings]\n", handler->TypeName);
    output->appendf("TextScale=%g\n", ui->text_scale_);
    if (auto const size{ui->saved_window_size()}) {
        output->appendf("WindowWidth=%d\n", size->width);
        output->appendf("WindowHeight=%d\n", size->height);
    }
    output->appendf("ProjectOpen=%d\n", ui->project_view_open_ ? 1 : 0);
    output->appendf("LayoutOpen=%d\n", ui->layout_view_open_ ? 1 : 0);
    output->appendf("TargetProfileOpen=%d\n", ui->target_profile_view_open_ ? 1 : 0);
    output->appendf("PropertiesOpen=%d\n", ui->properties_view_open_ ? 1 : 0);
    output->appendf("VariantsOpen=%d\n", ui->variants_view_open_ ? 1 : 0);
    output->appendf("ComparisonOpen=%d\n", ui->comparison_view_open_ ? 1 : 0);
    output->appendf("GraphOpen=%d\n", ui->graph_view_open_ ? 1 : 0);
    output->appendf("SourceOpen=%d\n", ui->source_view_open_ ? 1 : 0);
    output->appendf("DiagnosticsOpen=%d\n", ui->diagnostics_view_open_ ? 1 : 0);
    for (auto const& path : ui->recent_projects_) {
        output->appendf("RecentProject=%s\n", path.string().c_str());
        if (auto const found{ui->recent_project_targets_.find(graph_project_key(path))};
            found != ui->recent_project_targets_.end()) {
            auto const project{percent_encode(found->first)};
            auto const target{percent_encode(found->second)};
            output->appendf("RecentTarget=%s|%s\n", project.c_str(), target.c_str());
        }
    }
    for (auto const& [project, profile] : ui->persisted_target_profile_paths_) {
        auto const encoded_project{percent_encode(project)};
        auto const encoded_profile{percent_encode(profile.generic_string())};
        output->appendf("TargetProfile=%s|%s\n", encoded_project.c_str(), encoded_profile.c_str());
    }
    for (auto const& [project, positions] : ui->persisted_graph_node_positions_) {
        auto const encoded_project{percent_encode(project)};
        for (auto const& [identity, position] : positions) {
            auto const encoded_module{percent_encode(identity.module_name)};
            auto const encoded_namespace{percent_encode(identity.namespace_name)};
            auto const encoded_name{percent_encode(identity.name)};
            output->appendf("GraphNode=%s|%d|%s|%s|%s|%g|%g\n",
                            encoded_project.c_str(),
                            static_cast<int>(identity.origin),
                            encoded_module.c_str(),
                            encoded_namespace.c_str(),
                            encoded_name.c_str(),
                            position[0],
                            position[1]);
        }
    }
    output->append("\n");
}

auto PlannerUi::draw() -> bool {
    auto const save_shortcut_now{std::exchange(save_shortcut_pending_, false)};
    ImGui::GetStyle().FontScaleMain = text_scale_;
    auto const view_changed{draw_view_menu()};
    ImGui::GetStyle().FontScaleMain = text_scale_;
    auto const dockspace_id{ImGui::DockSpaceOverViewport()};
    setup_default_dock_layout(dockspace_id);
    auto const save_requested{
        ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_S, ImGuiInputFlags_RouteGlobal)};
    if (save_requested) {
        ImGui::ClearActiveID();
        save_shortcut_pending_ = true;
    }
    refresh_analysis();

    auto const revision_before{analysis_session_.inputs.workspace.revision()};
    draw_project_panel();
    refresh_analysis();
    draw_layout_panel();
    refresh_analysis();
    draw_properties_panel();
    draw_variants_panel();
    draw_target_profile_panel();
    refresh_analysis();
    draw_comparison_panel();
    draw_graph_panel();
    draw_source_panel();
    gate_new_declaration_dialogs();
    draw_diagnostics_panel();
    draw_docked_panel_outlines();
    draw_new_module_dialog();
    draw_new_enum_dialog();
    draw_new_packed_value_dialog();
    draw_new_integer_scalar_dialog();
    draw_new_linear_quantized_dialog();
    draw_new_integer_varint_dialog();
    draw_new_fixed_point_dialog();
    draw_new_mini_float_dialog();
    draw_new_optional_sentinel_dialog();
    draw_new_optional_presence_bit_dialog();
    draw_new_record_dialog();
    draw_new_union_dialog();
    draw_new_tagged_union_dialog();
    draw_new_soa_dialog();
    draw_close_confirmation();
    draw_project_path_dialogs();
    auto const saved{save_shortcut_now && has_dirty_changes() && save_changes()};
    return view_changed || revision_before != analysis_session_.inputs.workspace.revision() ||
           std::exchange(project_changed_, false) || saved;
}

void PlannerUi::gate_new_declaration_dialogs() {
    if (!document_.has_value()) {
        return;
    }

    bool has_module{};
    bool has_soa{};
    for (auto const& module : document_->manifest().modules) {
        if (auto const* normal{std::get_if<codegen::NormalModuleSchema>(&module)}) {
            has_module = true;
            has_soa |= normal->soa_backend == codegen::SoaBackend::standard_library;
        }
    }

    auto require_module = [&](bool& requested,
                              bool const available,
                              std::string_view const suggested_name,
                              NewDeclarationDialog const declaration) {
        if (!requested || available) {
            return;
        }

        requested = false;
        declaration_after_new_module_ = declaration;
        new_module_backend_ = declaration == NewDeclarationDialog::soa ? 1 : 0;
        new_module_header_.fill('\0');
        new_module_namespace_.fill('\0');
        confirm_unchecked_module_header_ = false;
        auto name{std::string{suggested_name}};
        auto suffix{2};
        auto const& modules{document_->manifest().modules};
        while (std::ranges::any_of(modules, [&](auto const& module) {
            return std::visit([&](auto const& value) { return value.settings.name == name; },
                              module);
        })) {
            name = std::string{suggested_name} + "_" + std::to_string(suffix++);
        }
        std::snprintf(new_module_name_.data(), new_module_name_.size(), "%s", name.c_str());
        schema_warning_message_.clear();
        open_new_module_dialog_ = true;
    };

    require_module(open_new_enum_dialog_, has_module, "enums", NewDeclarationDialog::enumeration);
    require_module(open_new_packed_value_dialog_,
                   has_module,
                   "packed_values",
                   NewDeclarationDialog::packed_value);
    require_module(open_new_integer_scalar_dialog_,
                   has_module,
                   "scalars",
                   NewDeclarationDialog::integer_scalar);
    require_module(open_new_linear_quantized_dialog_,
                   has_module,
                   "representations",
                   NewDeclarationDialog::quantization);
    require_module(open_new_integer_varint_dialog_,
                   has_module,
                   "representations",
                   NewDeclarationDialog::varint);
    require_module(open_new_fixed_point_dialog_,
                   has_module,
                   "representations",
                   NewDeclarationDialog::fixed_point);
    require_module(open_new_mini_float_dialog_,
                   has_module,
                   "representations",
                   NewDeclarationDialog::mini_float);
    require_module(open_new_optional_sentinel_dialog_,
                   has_module,
                   "representations",
                   NewDeclarationDialog::optional_sentinel);
    require_module(open_new_optional_presence_bit_dialog_,
                   has_module,
                   "representations",
                   NewDeclarationDialog::optional_presence_bit);
    require_module(open_new_record_dialog_, has_module, "records", NewDeclarationDialog::record);
    require_module(open_new_union_dialog_, has_module, "unions", NewDeclarationDialog::union_value);
    require_module(
        open_new_tagged_union_dialog_, has_module, "unions", NewDeclarationDialog::tagged_union);
    require_module(open_new_soa_dialog_, has_soa, "soa", NewDeclarationDialog::soa);
}

auto PlannerUi::draw_view_menu() -> bool {
    bool changed{};
    if (!ImGui::BeginMainMenuBar()) {
        return false;
    }
    changed |= draw_file_menu();
    if (ImGui::BeginMenu("View")) {
        auto percentage{text_scale_ * 100.0F};
        if (ImGui::SliderFloat("Text size", &percentage, 75.0F, 175.0F, "%.0f%%")) {
            text_scale_ = percentage / 100.0F;
            ImGui::MarkIniSettingsDirty();
            changed = true;
        }
        if (ImGui::MenuItem("Reset text size")) {
            text_scale_ = 1.0F;
            ImGui::MarkIniSettingsDirty();
            changed = true;
        }
        if (ImGui::MenuItem("Reset panel layout")) {
            reset_dock_layout_requested_ = true;
            project_view_open_ = true;
            layout_view_open_ = true;
            target_profile_view_open_ = true;
            properties_view_open_ = true;
            variants_view_open_ = true;
            comparison_view_open_ = true;
            graph_view_open_ = true;
            source_view_open_ = false;
            diagnostics_view_open_ = true;
            ImGui::MarkIniSettingsDirty();
            changed = true;
        }
        auto toggle_view = [&](char const* const label, bool& open) {
            if (ImGui::MenuItem(label, nullptr, open)) {
                open = !open;
                ImGui::MarkIniSettingsDirty();
                changed = true;
            }
        };
        toggle_view("Project / Schema", project_view_open_);
        toggle_view("Layout", layout_view_open_);
        toggle_view("Target Profile", target_profile_view_open_);
        toggle_view("Properties", properties_view_open_);
        toggle_view("Variants", variants_view_open_);
        toggle_view("Comparison", comparison_view_open_);
        if (ImGui::MenuItem("Graph", nullptr, graph_view_open_)) {
            graph_view_open_ = !graph_view_open_;
            ImGui::MarkIniSettingsDirty();
            changed = true;
        }
        if (ImGui::MenuItem("Source", nullptr, source_view_open_)) {
            source_view_open_ = !source_view_open_;
            ImGui::MarkIniSettingsDirty();
            changed = true;
        }
        if (ImGui::MenuItem("Diagnostics", nullptr, diagnostics_view_open_)) {
            diagnostics_view_open_ = !diagnostics_view_open_;
            ImGui::MarkIniSettingsDirty();
            changed = true;
        }
        ImGui::EndMenu();
    }
    if (has_dirty_changes()) {
        ImGui::SameLine();
        ImGui::TextColored({0.95F, 0.72F, 0.25F, 1.0F}, "Unsaved LispB changes");
    }
    ImGui::EndMainMenuBar();
    return changed;
}

auto PlannerUi::draw_file_menu() -> bool {
    bool changed{};
    if (!ImGui::BeginMenu("File")) {
        return false;
    }
    auto const has_document{document_.has_value()};
    auto const use_project_history{project_history_active()};
    auto const can_undo{use_project_history ? project_document_->can_undo()
                                            : has_document && document_->can_undo()};
    auto const can_redo{use_project_history ? project_document_->can_redo()
                                            : has_document && document_->can_redo()};
    if (ImGui::MenuItem("New Project...")) {
        auto const suggested{std::filesystem::current_path() / "lispb" / "new_project.lispb"};
        auto chosen{file_dialog_->save_file(suggested, "lispb")};
        if (!chosen.has_value()) {
            schema_edit_message_ = chosen.error();
        } else if (chosen->has_value()) {
            set_text_buffer(new_project_path_, chosen->value().string());
            set_text_buffer(new_project_target_, "new-schema");
            schema_edit_message_.clear();
            open_new_project_dialog_ = true;
        }
    }
    if (ImGui::MenuItem("Open Project...")) {
        auto chosen{file_dialog_->open_file(project_path_, "lispb")};
        if (!chosen.has_value()) {
            schema_edit_message_ = chosen.error();
        } else if (chosen->has_value()) {
            changed |= load_project(**chosen, false, true);
        }
    }
    if (ImGui::BeginMenu("Open Recent", !recent_projects_.empty())) {
        for (auto const& path : recent_projects_) {
            auto const label{path.string()};
            if (ImGui::MenuItem(label.c_str(), nullptr, path == project_path_)) {
                changed |= load_project(path, false, true);
            }
        }
        ImGui::EndMenu();
    }
    ImGui::Separator();
    ImGui::BeginDisabled(!can_undo);
    if (ImGui::MenuItem("Undo")) {
        if (use_project_history) {
            auto result{project_document_->undo()};
            if (result.has_value() && *result) {
                schema_edit_message_ = "Undid the staged project source change.";
                changed = true;
            } else if (!result.has_value()) {
                schema_edit_message_ = result.error().message;
            }
        } else {
            auto const selection{
                analysis_session_.inputs.selection.type.transform([&](TypeId const type) {
                    return analysis_session_.inputs.workspace.types().type(type).identity;
                })};
            auto result{document_->undo()};
            if (result.has_value() && *result) {
                sync_document_graph(selection);
                changed = true;
            } else if (!result.has_value()) {
                schema_edit_message_ = result.error().message;
            }
        }
    }
    ImGui::EndDisabled();
    ImGui::BeginDisabled(!can_redo);
    if (ImGui::MenuItem("Redo")) {
        if (use_project_history) {
            auto result{project_document_->redo()};
            if (result.has_value() && *result) {
                schema_edit_message_ = "Redid the staged project source change.";
                changed = true;
            } else if (!result.has_value()) {
                schema_edit_message_ = result.error().message;
            }
        } else {
            auto const selection{
                analysis_session_.inputs.selection.type.transform([&](TypeId const type) {
                    return analysis_session_.inputs.workspace.types().type(type).identity;
                })};
            auto result{document_->redo()};
            if (result.has_value() && *result) {
                sync_document_graph(selection);
                changed = true;
            } else if (!result.has_value()) {
                schema_edit_message_ = result.error().message;
            }
        }
    }
    ImGui::EndDisabled();
    ImGui::Separator();
    ImGui::BeginDisabled(!has_dirty_changes());
    if (ImGui::MenuItem("Preview LispB changes")) {
        source_view_open_ = true;
        focus_source_view_ = true;
        ImGui::MarkIniSettingsDirty();
        changed = true;
    }
    if (ImGui::MenuItem("Save", "Ctrl+S")) {
        if (save_changes()) {
            changed = true;
        }
    }
    ImGui::EndDisabled();
    ImGui::BeginDisabled(!has_document || use_project_history);
    if (ImGui::MenuItem("Save As...")) {
        auto destination{project_path_.parent_path() /
                         (project_path_.stem().string() + "_copy.lispb")};
        auto chosen{file_dialog_->save_file(destination, "lispb")};
        if (!chosen.has_value()) {
            schema_edit_message_ = chosen.error();
        } else if (chosen->has_value()) {
            auto cloned{clone_lispb_schema(*document_, **chosen, target_name_)};
            if (cloned.loaded) {
                adopt_loaded_schema(std::move(cloned));
                schema_edit_message_ = "Saved and opened the cloned LispB project.";
                changed = true;
            } else {
                schema_edit_message_ = cloned.diagnostics.empty()
                                         ? "Could not clone the LispB project."
                                         : cloned.diagnostics.front().message;
            }
        }
    }
    ImGui::EndDisabled();
    if (ImGui::MenuItem("Export C++",
                        nullptr,
                        false,
                        has_document && project_document_.has_value() && !has_dirty_changes())) {
        static_cast<void>(export_cpp(export_build_root_path_.data()));
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip(
            has_dirty_changes()
                ? "Save schema and project changes before exporting C++."
                : "Generate C++ for the saved target into its configured output directory.");
    }
    ImGui::EndMenu();

    return changed;
}

void PlannerUi::draw_project_path_dialogs() {
    if (std::exchange(open_project_target_dialog_, false)) {
        ImGui::OpenPopup("Choose LispB target");
    }
    if (ImGui::BeginPopupModal("Choose LispB target", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped("Choose a C++ schema target in %s",
                           pending_project_path_.string().c_str());
        for (auto const& name : pending_project_targets_) {
            if (ImGui::Selectable(name.c_str())) {
                auto const path{pending_project_path_};
                pending_project_path_.clear();
                pending_project_targets_.clear();
                ImGui::CloseCurrentPopup();
                static_cast<void>(load_project(path, false, false, name));
                break;
            }
        }
        if (ImGui::Button("Cancel")) {
            pending_project_path_.clear();
            pending_project_targets_.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    if (open_new_project_dialog_) {
        ImGui::OpenPopup("New LispB project");
        open_new_project_dialog_ = false;
    }
    if (ImGui::BeginPopupModal("New LispB project", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("Project manifest path");
        ImGui::TextWrapped("%s", new_project_path_.data());
        if (ImGui::Button("Choose location...")) {
            auto chosen{file_dialog_->save_file(new_project_path_.data(), "lispb")};
            if (!chosen.has_value()) {
                schema_edit_message_ = chosen.error();
            } else if (chosen->has_value()) {
                set_text_buffer(new_project_path_, chosen->value().string());
            }
        }
        ImGui::TextUnformatted("C++ schema target name");
        ImGui::SetNextItemWidth(360.0F);
        ImGui::InputText(
            "##new-project-target", new_project_target_.data(), new_project_target_.size());
        ImGui::TextDisabled(
            "Creates an empty types file and a starter module beside the manifest.");
        std::string destination_warning;
        if (new_project_path_.front() != '\0') {
            std::error_code error;
            auto const destination{
                std::filesystem::absolute(new_project_path_.data(), error).lexically_normal()};
            if (error) {
                destination_warning = "Cannot inspect project path: " + error.message();
            } else {
                auto temporary_project{destination};
                temporary_project += ".layout-planner.tmp";
                auto const source_directory{destination.parent_path() /
                                            (destination.stem().string() + "_schema")};
                for (auto const& path : {destination, source_directory, temporary_project}) {
                    auto const status{std::filesystem::symlink_status(path, error)};
                    if (error && error != std::errc::no_such_file_or_directory) {
                        destination_warning =
                            "Cannot inspect " + path.string() + ": " + error.message();
                        break;
                    }
                    if (error == std::errc::no_such_file_or_directory) {
                        error.clear();
                        continue;
                    }
                    error.clear();
                    if (status.type() != std::filesystem::file_type::not_found) {
                        destination_warning = "Already exists: " + path.string() +
                                              ". Choose another project path; existing files "
                                              "will not be overwritten.";
                        break;
                    }
                }
            }
        }
        if (!destination_warning.empty()) {
            ImGui::TextWrapped("%s", destination_warning.c_str());
        }
        ImGui::BeginDisabled(new_project_path_.front() == '\0' ||
                             new_project_target_.front() == '\0' || !destination_warning.empty());
        if (ImGui::Button("Create")) {
            if (has_dirty_changes()) {
                schema_edit_message_ = "Save or undo the current LispB changes before creating "
                                       "another project.";
            } else {
                auto created{create_blank_lispb_schema(new_project_path_.data(),
                                                       new_project_target_.data())};
                if (created.loaded) {
                    adopt_loaded_schema(std::move(created));
                    schema_edit_message_ = "Created and opened the blank LispB project.";
                    ImGui::CloseCurrentPopup();
                } else {
                    schema_edit_message_ = created.diagnostics.empty()
                                             ? "Could not create the LispB project."
                                             : created.diagnostics.front().message;
                }
            }
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            ImGui::CloseCurrentPopup();
        }
        if (!schema_edit_message_.empty()) {
            ImGui::TextWrapped("%s", schema_edit_message_.c_str());
        }
        ImGui::EndPopup();
    }

    if (std::exchange(open_export_build_root_dialog_, false)) {
        ImGui::OpenPopup("Export build directory");
    }
    if (ImGui::BeginPopupModal(
            "Export build directory", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("This target writes output relative to a build directory.");
        ImGui::TextUnformatted("Build directory (absolute path)");
        ImGui::TextWrapped("%s", export_build_root_path_.data());
        if (ImGui::Button("Choose build directory...")) {
            auto chosen{file_dialog_->pick_folder(export_build_root_path_.data())};
            if (!chosen.has_value()) {
                schema_edit_message_ = chosen.error();
            } else if (chosen->has_value()) {
                set_text_buffer(export_build_root_path_, chosen->value().string());
            }
        }
        auto const build_root{std::filesystem::path{export_build_root_path_.data()}};
        ImGui::BeginDisabled(!build_root.is_absolute() || has_dirty_changes());
        if (ImGui::Button("Export")) {
            if (export_cpp(build_root)) {
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            ImGui::CloseCurrentPopup();
        }
        if (!schema_edit_message_.empty()) {
            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 560.0F);
            ImGui::TextUnformatted(schema_edit_message_.c_str());
            ImGui::PopTextWrapPos();
        }
        ImGui::EndPopup();
    }
}

void PlannerUi::draw_source_panel() {
    if (!source_view_open_) {
        return;
    }
    if (std::exchange(focus_source_view_, false)) {
        ImGui::SetNextWindowFocus();
    }
    auto const was_open{source_view_open_};
    if (!ImGui::Begin("Source", &source_view_open_, ImGuiWindowFlags_HorizontalScrollbar)) {
        ImGui::End();
        persist_view_visibility(was_open, source_view_open_);
        return;
    }
    if (!document_.has_value()) {
        ImGui::TextDisabled("Open a LispB project to inspect source changes.");
        ImGui::End();
        persist_view_visibility(was_open, source_view_open_);
        return;
    }
    struct SourceViewFile {
        std::filesystem::path path;
        std::string const* loaded{};
        std::string original;
        std::optional<std::string> updated;
    };
    std::vector<SourceViewFile> files;
    if (project_document_.has_value()) {
        files.push_back({.path = project_document_->path(),
                         .loaded = &project_document_->source_text(),
                         .original = {},
                         .updated = std::nullopt});
    }
    for (auto const& source : document_->source_files()) {
        files.push_back(
            {.path = source.path, .loaded = &source.text, .original = {}, .updated = std::nullopt});
    }
    auto apply_update = [&](std::filesystem::path path, std::string original, std::string updated) {
        auto const normalized{path.lexically_normal()};
        auto const existing{std::ranges::find_if(
            files, [&](auto const& file) { return file.path.lexically_normal() == normalized; })};
        if (existing == files.end()) {
            files.push_back({.path = std::move(path),
                             .loaded = nullptr,
                             .original = std::move(original),
                             .updated = std::move(updated)});
        } else {
            existing->original = std::move(original);
            existing->updated = std::move(updated);
        }
    };
    std::string preview_error;
    if (project_document_.has_value()) {
        auto project_updates{project_document_->preview_source_updates()};
        if (!project_updates.has_value()) {
            preview_error = project_updates.error().message;
        } else {
            for (auto& update : *project_updates) {
                apply_update(
                    std::move(update.path), std::move(update.original), std::move(update.updated));
            }
        }
    }
    auto schema_updates{document_->preview_source_updates()};
    if (!schema_updates.has_value()) {
        if (!preview_error.empty()) {
            preview_error += "\n";
        }
        preview_error += schema_updates.error().message;
    } else {
        for (auto& update : *schema_updates) {
            apply_update(
                std::move(update.path), std::move(update.original), std::move(update.updated));
        }
    }
    if (!preview_error.empty()) {
        ImGui::TextWrapped("%s", preview_error.c_str());
    }
    if (files.empty()) {
        ImGui::TextDisabled("No LispB source files loaded.");
    } else if (ImGui::BeginTabBar("source-files")) {
        auto draw_text = [](char const* id, std::string const& value) {
            if (ImGui::Button("Copy source")) {
                ImGui::SetClipboardText(value.c_str());
            }
            if (ImGui::BeginChild(id, {0.0F, 0.0F}, true, ImGuiWindowFlags_HorizontalScrollbar)) {
                ImGui::TextUnformatted(value.data(), value.data() + value.size());
            }
            ImGui::EndChild();
        };
        for (auto const& file : files) {
            ImGui::PushID(file.path.string().c_str());
            auto const label{file.path.filename().string() +
                             (file.updated.has_value() ? " *" : "") + "###" + file.path.string()};
            if (ImGui::BeginTabItem(label.c_str())) {
                ImGui::TextDisabled("%s", file.path.string().c_str());
                if (ImGui::BeginTabBar("source-version-tabs")) {
                    if (file.updated.has_value()) {
                        if (ImGui::BeginTabItem("Updated")) {
                            draw_text("updated-source", *file.updated);
                            ImGui::EndTabItem();
                        }
                        if (ImGui::BeginTabItem("Original")) {
                            draw_text("original-source", file.original);
                            ImGui::EndTabItem();
                        }
                    } else if (ImGui::BeginTabItem("Current")) {
                        draw_text("current-source", *file.loaded);
                        ImGui::EndTabItem();
                    }
                    ImGui::EndTabBar();
                }
                ImGui::EndTabItem();
            }
            ImGui::PopID();
        }
        ImGui::EndTabBar();
    }
    ImGui::End();
    persist_view_visibility(was_open, source_view_open_);
}

void PlannerUi::draw_diagnostics_panel() {
    if (!diagnostics_view_open_) {
        return;
    }
    if (std::exchange(focus_diagnostics_view_, false)) {
        ImGui::SetNextWindowFocus();
    }
    auto const was_open{diagnostics_view_open_};
    if (!ImGui::Begin(
            "Diagnostics", &diagnostics_view_open_, ImGuiWindowFlags_HorizontalScrollbar)) {
        ImGui::End();
        persist_view_visibility(was_open, diagnostics_view_open_);
        return;
    }

    struct DisplayDiagnostic {
        std::string label;
        DiagnosticSeverity severity;
        std::string message;
    };
    std::vector<DisplayDiagnostic> entries;
    std::string content;
    auto append_message = [&](std::string_view const label,
                              std::string_view const message,
                              DiagnosticSeverity const severity = DiagnosticSeverity::info) {
        if (message.empty()) {
            return;
        }
        entries.push_back({std::string{label}, severity, std::string{message}});
        if (!content.empty()) {
            content += "\n\n";
        }
        content += label;
        content += '\n';
        content += message;
    };
    auto append_diagnostics = [&](std::string_view const label,
                                  std::vector<Diagnostic> const& diagnostics) {
        if (diagnostics.empty()) {
            return;
        }
        for (auto const& diagnostic : diagnostics) {
            append_message(label, diagnostic.message, diagnostic.severity);
        }
    };
    auto append_analysis = [&](char const* const label, auto const& analysis) {
        if (analysis.has_value()) {
            append_diagnostics(label, analysis->diagnostics);
        }
    };
    append_message("Document status", schema_edit_message_);
    append_diagnostics("Project load", load_diagnostics_);
    append_message("Target profile", target_profile_load_error_, DiagnosticSeverity::error);
    append_message("Target profile", target_memory_fact_error_, DiagnosticSeverity::error);
    append_message(
        "Comparison target profile", comparison_target_profile_error_, DiagnosticSeverity::error);
    append_analysis("Semantic domain", analysis_session_.results().enum_domain);
    append_analysis("Enum target comparison", analysis_session_.results().enum_target_comparison);
    append_analysis("Integer scalar", analysis_session_.results().integer_scalar_analysis);
    append_analysis("Varint", analysis_session_.results().integer_varint_analysis);
    append_analysis("Fixed point", analysis_session_.results().fixed_point_analysis);
    append_analysis("Mini-float", analysis_session_.results().mini_float_analysis);
    append_analysis("Sentinel optional", analysis_session_.results().optional_sentinel_analysis);
    append_analysis("Presence-bit optional",
                    analysis_session_.results().optional_presence_bit_analysis);
    append_analysis("Packed layout", analysis_session_.results().active_packed);
    append_analysis("Packed target comparison",
                    analysis_session_.results().packed_target_comparison);
    append_analysis("Packed access", analysis_session_.results().packed_access_analysis);
    append_analysis("Packed target access comparison",
                    analysis_session_.results().packed_target_access_comparison);
    append_analysis("Packed access comparison",
                    analysis_session_.results().packed_access_comparison);
    append_analysis("Record layout", analysis_session_.results().record_analysis);
    append_analysis("Record target comparison",
                    analysis_session_.results().record_target_comparison);
    append_analysis("Record access", analysis_session_.results().record_access_analysis);
    append_analysis("Record target access comparison",
                    analysis_session_.results().record_target_access_comparison);
    append_analysis("Union layout", analysis_session_.results().union_analysis);
    append_analysis("Union target comparison", analysis_session_.results().union_target_comparison);
    append_analysis("Raw-union workload", analysis_session_.results().union_distribution_analysis);
    append_analysis("Raw-union target workload comparison",
                    analysis_session_.results().union_target_distribution_comparison);
    append_analysis("Tagged-union layout", analysis_session_.results().tagged_union_analysis);
    append_analysis("Tagged-union target comparison",
                    analysis_session_.results().tagged_union_target_comparison);
    append_analysis("Tagged-union workload",
                    analysis_session_.results().tagged_union_distribution_analysis);
    append_analysis("Tagged-union target workload comparison",
                    analysis_session_.results().tagged_union_target_distribution_comparison);
    append_analysis("SoA layout", analysis_session_.results().active_soa);
    append_analysis("SoA target comparison", analysis_session_.results().soa_target_comparison);
    append_analysis("SoA access", analysis_session_.results().soa_access_analysis);
    append_analysis("SoA target access comparison",
                    analysis_session_.results().soa_target_access_comparison);
    if (entries.empty() && schema_warning_message_.empty()) {
        ImGui::TextDisabled("No current diagnostics.");
    } else {
        auto copy_text{content};
        if (!schema_warning_message_.empty()) {
            copy_text = "Warning\n" + schema_warning_message_;
            if (!content.empty()) {
                copy_text += "\n\n" + content;
            }
        }
        if (ImGui::Button("Copy all")) {
            ImGui::SetClipboardText(copy_text.c_str());
        }
        if (!schema_warning_message_.empty()) {
            ImGui::SeparatorText("Warning");
            ImGui::PushStyleColor(ImGuiCol_Text,
                                  detail::diagnostic_color(DiagnosticSeverity::warning));
            ImGui::InputTextMultiline("##schema-warning",
                                      schema_warning_message_.data(),
                                      schema_warning_message_.size() + 1,
                                      {0.0F, ImGui::GetTextLineHeightWithSpacing() * 4.0F},
                                      ImGuiInputTextFlags_ReadOnly | ImGuiInputTextFlags_WordWrap);
            ImGui::PopStyleColor();
        }
        std::string previous_label;
        for (std::size_t index{}; index < entries.size(); ++index) {
            auto const& entry{entries[index]};
            ImGui::PushID(static_cast<int>(index));
            if (entry.label != previous_label) {
                ImGui::SeparatorText(entry.label.c_str());
                previous_label = entry.label;
            }
            ImGui::PushStyleColor(ImGuiCol_Text, detail::diagnostic_color(entry.severity));
            ImGui::TextWrapped("%s", entry.message.c_str());
            ImGui::PopStyleColor();
            if (ImGui::BeginPopupContextItem("diagnostic-context")) {
                if (ImGui::MenuItem("Copy message")) {
                    ImGui::SetClipboardText(entry.message.c_str());
                }
                ImGui::EndPopup();
            }
            ImGui::PopID();
        }
    }

    ImGui::End();
    persist_view_visibility(was_open, diagnostics_view_open_);
}

void PlannerUi::draw_close_confirmation() {
    if (open_close_confirmation_) {
        ImGui::OpenPopup("Unsaved LispB changes");
        open_close_confirmation_ = false;
    }
    if (!ImGui::BeginPopupModal(
            "Unsaved LispB changes", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }
    ImGui::TextWrapped("The editable project contains unsaved LispB changes.");
    ImGui::TextUnformatted("Save them before closing?");
    if (ImGui::Button("Save and close")) {
        if (save_changes()) {
            close_confirmed_ = true;
            ImGui::CloseCurrentPopup();
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Discard and close")) {
        close_confirmed_ = true;
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
        ImGui::CloseCurrentPopup();
    }
    if (!schema_edit_message_.empty()) {
        ImGui::TextWrapped("%s", schema_edit_message_.c_str());
    }
    ImGui::EndPopup();
}

auto PlannerUi::project_history_active() const -> bool {
    return project_document_.has_value() &&
           (project_document_->can_undo() || project_document_->can_redo());
}

auto PlannerUi::has_dirty_changes() const -> bool {
    return (project_document_.has_value() && project_document_->dirty()) ||
           (document_.has_value() && document_->dirty());
}

auto PlannerUi::save_changes() -> bool {
    if (project_document_.has_value() && project_document_->dirty()) {
        if (document_.has_value() && document_->dirty()) {
            schema_edit_message_ =
                "Project-source and schema drafts cannot be saved as one ambiguous history.";
            return false;
        }
        auto result{project_document_->save()};
        if (!result.has_value()) {
            schema_edit_message_ = result.error().message;
            return false;
        }
        auto const manifest_changed{*result};
        auto loaded{load_lispb_schema(project_path_, target_name_)};
        if (!loaded.loaded) {
            schema_edit_message_ = loaded.diagnostics.empty()
                                     ? "Saved the project source list but could not reload it."
                                     : loaded.diagnostics.front().message;
            return false;
        }
        adopt_loaded_schema(std::move(loaded));
        schema_edit_message_ = manifest_changed
                                 ? "Saved the project source list and reloaded the editable "
                                   "project."
                                 : "The project source list already matched disk; reloaded and "
                                   "cleared its draft history.";
        return true;
    }
    if (!document_.has_value() || !document_->dirty()) {
        return false;
    }

    auto const selection{analysis_session_.inputs.selection.type.transform([&](TypeId const type) {
        return analysis_session_.inputs.workspace.types().type(type).identity;
    })};
    auto result{document_->save()};
    if (!result.has_value()) {
        schema_edit_message_ = result.error().message;
        return false;
    }
    sync_document_graph(selection);
    schema_edit_message_ =
        "Saved and reloaded " + std::to_string(result->size()) + " LispB source file(s).";
    return true;
}

auto PlannerUi::export_cpp(std::filesystem::path const& build_root) -> bool {
    project_view_open_ = true;
    ImGui::MarkIniSettingsDirty();
    if (!document_.has_value() || !project_document_.has_value()) {
        schema_edit_message_ = "Open a LispB project before exporting C++.";
        return false;
    }
    if (has_dirty_changes()) {
        schema_edit_message_ = "Save schema and project changes before exporting C++.";
        return false;
    }

    try {
        auto const project{lispb::load_project(project_path_)};
        auto const target{project.targets.find(target_name_)};
        if (target == project.targets.end() ||
            !std::holds_alternative<lispb::CppSchemaTarget>(target->second)) {
            schema_edit_message_ = "The saved project no longer contains C++ schema target '" +
                                   target_name_ + "'. Reopen the project.";
            return false;
        }
        auto const& schema{std::get<lispb::CppSchemaTarget>(target->second)};
        if (schema.output_root.base == lispb::PathBase::build && !build_root.is_absolute()) {
            schema_edit_message_ = "Choose an absolute build directory for this export.";
            open_export_build_root_dialog_ = true;
            return false;
        }

        auto const compiled{lispb::compile_target(target->second, project.root, build_root)};
        if (lispb::publish(compiled.compilation, compiled.publication) != 0) {
            schema_edit_message_ = "C++ export failed for target '" + target_name_ + "'.";
            return false;
        }
        schema_edit_message_ = "Exported " + std::to_string(compiled.compilation.artifacts.size()) +
                               " C++ file(s) for '" + target_name_ + "' to " +
                               compiled.publication.output_root.string();
        return true;
    } catch (std::exception const& error) {
        schema_edit_message_ = "C++ export failed: " + std::string{error.what()};
        return false;
    }
}

auto PlannerUi::apply_project_edit(lispb::ProjectEditCommand command) -> bool {
    if (!project_document_.has_value()) {
        schema_edit_message_ = "No editable LispB project manifest is loaded.";
        return false;
    }
    if (document_.has_value() && document_->dirty()) {
        schema_edit_message_ = "Save or undo schema edits before changing the project source list.";
        return false;
    }
    auto result{project_document_->apply(std::move(command))};
    if (!result.has_value()) {
        schema_edit_message_ = result.error().message;
        return false;
    }
    if (!*result) {
        return false;
    }
    schema_edit_message_.clear();
    project_changed_ = true;
    return true;
}

auto PlannerUi::apply_document_edit(SchemaEditCommand command,
                                    std::optional<TypeIdentity> selection) -> bool {
    if (!document_.has_value()) {
        schema_edit_message_ = "No editable LispB document is loaded.";
        return false;
    }
    if (project_history_active()) {
        schema_edit_message_ = "Save, redo, or discard the project source-list draft before "
                               "editing schema declarations.";
        return false;
    }
    if (!selection.has_value() && analysis_session_.inputs.selection.type.has_value()) {
        selection = analysis_session_.inputs.workspace.types()
                        .type(*analysis_session_.inputs.selection.type)
                        .identity;
    }
    auto result{document_->apply(std::move(command))};
    if (!result.has_value()) {
        schema_edit_message_ = result.error().message;
        return false;
    }
    if (!*result) {
        return false;
    }
    schema_edit_message_.clear();
    sync_document_graph(std::move(selection));
    return true;
}

void PlannerUi::sync_document_graph(std::optional<TypeIdentity> selection) {
    std::erase_if(varint_distributions_, [&](auto const& entry) {
        auto const type{document_->types().find(entry.first)};
        return !type.has_value() || !std::holds_alternative<IntegerScalarType>(
                                        document_->types().type(*type).definition);
    });
    analysis_session_.replace_types(*document_, selection);
    invalidate_type_editor_state();
    rename_editor_declaration_.reset();
    delete_declaration_.reset();
    delete_declaration_name_.clear();
    inline_record_rename_.reset();
    focus_inline_record_rename_ = false;
    open_record_module_.reset();
}

void PlannerUi::invalidate_type_editor_state() {
    selected_enumerator_.clear();
    enum_editor_declaration_.reset();
    enum_editor_value_.clear();
    packed_editor_declaration_.reset();
    packed_editor_field_.clear();
    packed_code_editor_declaration_.reset();
    packed_code_editor_field_.clear();
    packed_code_editor_name_.clear();
    selected_packed_code_.clear();
    integer_scalar_editor_declaration_.reset();
    integer_scalar_editor_code_.clear();
    selected_integer_scalar_code_.clear();
    linear_quantized_editor_declaration_.reset();
    integer_varint_editor_declaration_.reset();
    fixed_point_editor_declaration_.reset();
    record_editor_declaration_.reset();
    record_editor_member_.clear();
    union_editor_declaration_.reset();
    union_editor_alternative_.clear();
    tagged_union_editor_declaration_.reset();
    tagged_union_editor_alternative_.clear();
    optional_sentinel_editor_declaration_.reset();
    optional_presence_bit_editor_declaration_.reset();
    soa_editor_declaration_.reset();
    soa_editor_member_.clear();
    packed_dragged_divider_.reset();
    packed_dragged_variant_id_.reset();
    packed_dragged_left_width_.reset();
    packed_dragged_right_width_.reset();
    new_varint_distribution_value_.fill('\0');
    new_varint_distribution_value_[0] = '0';
    new_varint_distribution_weight_ = 1;
}

auto PlannerUi::load_project(std::filesystem::path const& path,
                             bool const allow_dirty,
                             bool const use_recent_target,
                             std::optional<std::string> selected_target) -> bool {
    if (!allow_dirty && has_dirty_changes()) {
        schema_edit_message_ =
            "Save or undo the current LispB changes before opening another project.";
        return false;
    }
    auto target{selected_target.value_or(target_name_)};
    if (use_recent_target && !selected_target.has_value()) {
        auto const normalized{std::filesystem::absolute(path).lexically_normal()};
        try {
            auto const project{lispb::load_project(path)};
            std::vector<std::string> schema_targets;
            for (auto const& [name, candidate] : project.targets) {
                if (std::holds_alternative<lispb::CppSchemaTarget>(candidate)) {
                    schema_targets.push_back(name);
                }
            }
            if (schema_targets.empty()) {
                schema_edit_message_ = "The LispB project has no C++ schema target.";
                return false;
            }
            auto const remembered{recent_project_targets_.find(graph_project_key(normalized))};
            if (remembered != recent_project_targets_.end() &&
                std::ranges::find(schema_targets, remembered->second) != schema_targets.end()) {
                target = remembered->second;
            } else if (schema_targets.size() == 1) {
                target = schema_targets.front();
            } else {
                pending_project_path_ = normalized;
                pending_project_targets_ = std::move(schema_targets);
                open_project_target_dialog_ = true;
                return false;
            }
        } catch (std::exception const& error) {
            schema_edit_message_ = error.what();
            return false;
        }
    }
    auto loaded{load_lispb_schema(path, target)};
    if (!loaded.loaded) {
        schema_edit_message_ = loaded.diagnostics.empty() ? "Could not load the LispB project."
                                                          : loaded.diagnostics.front().message;
        return false;
    }
    adopt_loaded_schema(std::move(loaded));
    schema_edit_message_.clear();
    return true;
}

void PlannerUi::adopt_loaded_schema(SchemaLoadResult loaded) {
    schema_warning_message_.clear();
    if (project_path_ != loaded.project_path || target_name_ != loaded.target_name) {
        export_build_root_path_.fill('\0');
    }
    open_export_build_root_dialog_ = false;
    project_path_ = std::move(loaded.project_path);
    target_name_ = std::move(loaded.target_name);
    analysis_session_ = PlannerAnalysisSession{
        loaded.document.has_value() ? loaded.document->types() : TypeGraph{}};
    use_builtin_target_profile(false);
    use_builtin_comparison_target_profile();
    if (auto const saved_profile{
            persisted_target_profile_paths_.find(graph_project_key(project_path_))};
        saved_profile != persisted_target_profile_paths_.end()) {
        set_text_buffer(target_profile_path_, saved_profile->second.string());
        static_cast<void>(load_target_profile(saved_profile->second, false));
    }
    project_document_ = std::move(loaded.project_document);
    document_ = std::move(loaded.document);
    load_diagnostics_ = std::move(loaded.diagnostics);
    select_type(std::nullopt);
    inline_record_rename_.reset();
    focus_inline_record_rename_ = false;
    open_record_module_.reset();
    graph_node_positions_.clear();
    auto const types{analysis_session_.inputs.workspace.types().types()};
    if (auto const saved{persisted_graph_node_positions_.find(graph_project_key(project_path_))};
        saved != persisted_graph_node_positions_.end()) {
        for (auto const& [identity, position] : saved->second) {
            if (std::ranges::find(types, identity, &TypeNode::identity) != types.end()) {
                graph_node_positions_[identity] = position;
            }
        }
    }
    for (std::size_t index{}; index < types.size(); ++index) {
        if (declaration_capabilities(types[index]).visible) {
            select_type(TypeId{static_cast<std::uint32_t>(index)});
            break;
        }
    }
    analysis_session_.inputs.selection.field.clear();
    selected_enumerator_.clear();
    analysis_session_.inputs.selection.packed_access_fields.clear();
    analysis_session_.inputs.selection.record_access_members.clear();
    analysis_session_.inputs.selection.soa_access_columns.clear();
    varint_distributions_.clear();
    analysis_session_.inputs.selection.packed_access_set_explicit = false;
    analysis_session_.clear_distributions();
    new_varint_distribution_value_.fill('\0');
    new_varint_distribution_value_[0] = '0';
    new_varint_distribution_weight_ = 1;
    new_project_source_path_.fill('\0');
    rename_project_source_path_.fill('\0');
    rename_project_source_.reset();
    focus_rename_project_source_ = false;
    analysis_session_.inputs.selection.record_access_set_explicit = false;
    analysis_session_.inputs.selection.soa_access_set_explicit = false;
    rename_editor_declaration_.reset();
    declaration_name_.fill('\0');
    delete_declaration_.reset();
    delete_declaration_name_.clear();
    enum_editor_declaration_.reset();
    enum_editor_value_.clear();
    packed_editor_declaration_.reset();
    packed_editor_field_.clear();
    packed_code_editor_declaration_.reset();
    packed_code_editor_field_.clear();
    packed_code_editor_name_.clear();
    selected_packed_code_.clear();
    integer_scalar_editor_declaration_.reset();
    integer_scalar_editor_code_.clear();
    selected_integer_scalar_code_.clear();
    linear_quantized_editor_declaration_.reset();
    integer_varint_editor_declaration_.reset();
    fixed_point_editor_declaration_.reset();
    optional_sentinel_editor_declaration_.reset();
    optional_presence_bit_editor_declaration_.reset();
    record_editor_declaration_.reset();
    record_editor_member_.clear();
    union_editor_declaration_.reset();
    union_editor_alternative_.clear();
    tagged_union_editor_declaration_.reset();
    tagged_union_editor_alternative_.clear();
    soa_editor_declaration_.reset();
    soa_editor_member_.clear();
    packed_dragged_divider_.reset();
    packed_dragged_variant_id_.reset();
    packed_dragged_left_width_.reset();
    packed_dragged_right_width_.reset();
    graph_pan_x_ = 32.0F;
    graph_pan_y_ = 32.0F;
    graph_zoom_ = 1.0F;
    graph_focus_selected_ = false;
    analysis_session_.inputs.quantized_comparison_type.reset();
    analysis_session_.inputs.varint_comparison_type.reset();
    analysis_session_.inputs.optional_comparison_type.reset();
    analysis_session_.inputs.comparison_a_variant_id = LayoutWorkspace::baseline_variant_id;
    analysis_session_.inputs.comparison_b_variant_id = LayoutWorkspace::baseline_variant_id;
    analysis_session_.inputs.comparison_b_follows_active = true;
    project_changed_ = true;
    sync_variant_name();
    remember_recent_project(project_path_);
}

void PlannerUi::remember_recent_project(std::filesystem::path const& path) {
    if (path.empty()) {
        return;
    }
    auto const normalized{std::filesystem::absolute(path).lexically_normal()};
    recent_projects_.erase(
        std::remove(recent_projects_.begin(), recent_projects_.end(), normalized),
        recent_projects_.end());
    recent_projects_.insert(recent_projects_.begin(), normalized);
    recent_project_targets_.insert_or_assign(graph_project_key(normalized), target_name_);
    if (recent_projects_.size() > 20) {
        recent_project_targets_.erase(graph_project_key(recent_projects_.back()));
        recent_projects_.resize(20);
    }
    ImGui::MarkIniSettingsDirty();
}

void PlannerUi::setup_default_dock_layout(unsigned int const dockspace_id) {
    auto const* existing_node{ImGui::DockBuilderGetNode(dockspace_id)};
    if (!reset_dock_layout_requested_ &&
        (dock_layout_initialized_ || existing_node == nullptr || existing_node->IsSplitNode())) {
        dock_layout_initialized_ = true;
        return;
    }
    ImGui::DockBuilderRemoveNode(dockspace_id);
    ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspace_id, ImGui::GetMainViewport()->WorkSize);

    auto center_id{dockspace_id};
    auto const left_id{
        ImGui::DockBuilderSplitNode(center_id, ImGuiDir_Left, 0.22F, nullptr, &center_id)};
    auto const right_id{
        ImGui::DockBuilderSplitNode(center_id, ImGuiDir_Right, 0.34F, nullptr, &center_id)};
    auto const comparison_id{
        ImGui::DockBuilderSplitNode(center_id, ImGuiDir_Down, 0.34F, nullptr, &center_id)};
    auto left_top_id{left_id};
    auto const variants_id{
        ImGui::DockBuilderSplitNode(left_top_id, ImGuiDir_Down, 0.34F, nullptr, &left_top_id)};
    ImGui::DockBuilderDockWindow("Project / Schema", left_top_id);
    ImGui::DockBuilderDockWindow("Variants", variants_id);
    ImGui::DockBuilderDockWindow("Graph", center_id);
    ImGui::DockBuilderDockWindow("Target Profile", center_id);
    ImGui::DockBuilderDockWindow("Layout", center_id);
    ImGui::DockBuilderDockWindow("Properties", right_id);
    ImGui::DockBuilderDockWindow("Comparison", comparison_id);
    ImGui::DockBuilderDockWindow("Source", comparison_id);
    ImGui::DockBuilderDockWindow("Diagnostics", comparison_id);
    ImGui::DockBuilderFinish(dockspace_id);
    dock_layout_initialized_ = true;
    reset_dock_layout_requested_ = false;
}

void PlannerUi::refresh_analysis() {
    auto& inputs{analysis_session_.inputs};
    std::optional<TypeIdentity> source_identity;
    std::vector<IntegerVarintDistributionEntry> entries;
    bool rows_present{};
    if (inputs.selection.type.has_value()) {
        auto const& definition{inputs.workspace.types().type(*inputs.selection.type).definition};
        if (auto const* varint{std::get_if<IntegerVarintType>(&definition)}) {
            source_identity = inputs.workspace.types().type(varint->source.type).identity;
            if (auto const found{varint_distributions_.find(*source_identity)};
                found != varint_distributions_.end()) {
                rows_present = !found->second.empty();
                entries.reserve(found->second.size());
                for (auto const& row : found->second) {
                    if (auto const value{detail::parse_packed_integer(row.value.data())}) {
                        entries.push_back({.value = *value, .weight = row.weight});
                    }
                }
            }
        }
    }
    analysis_session_.set_varint_distribution(
        std::move(source_identity), std::move(entries), rows_present);
    analysis_session_.refresh(document_.has_value() ? &*document_ : nullptr);
}

void PlannerUi::select_type(std::optional<TypeId> type) {
    if (!analysis_session_.inputs.selection.select_type(analysis_session_.inputs.workspace.types(),
                                                        type)) {
        return;
    }
    invalidate_type_editor_state();
}

void PlannerUi::draw_target_profile_panel() {
    if (!target_profile_view_open_) {
        return;
    }
    if (auto* layout_window{ImGui::FindWindowByName("Layout")};
        layout_window != nullptr && layout_window->DockId != 0) {
        ImGui::SetNextWindowDockID(layout_window->DockId, ImGuiCond_FirstUseEver);
    }
    if (std::exchange(focus_target_profile_view_, false)) {
        ImGui::SetNextWindowFocus();
    }

    auto const was_open{target_profile_view_open_};
    if (ImGui::Begin(
            "Target Profile", &target_profile_view_open_, ImGuiWindowFlags_HorizontalScrollbar)) {
        if (draw_target_profile()) {
            refresh_analysis();
        }
    }
    persist_view_visibility(was_open, target_profile_view_open_);
    ImGui::End();
}

void PlannerUi::draw_layout_panel() {
    if (!layout_view_open_) {
        return;
    }
    auto const was_open{layout_view_open_};
    ImGui::Begin("Layout", &layout_view_open_, ImGuiWindowFlags_HorizontalScrollbar);
    persist_view_visibility(was_open, layout_view_open_);
    ImGui::TextWrapped("Target: %s", known_or_unknown(analysis_session_.primary_abi().name()));
    if (ImGui::SmallButton("Profile settings...")) {
        target_profile_view_open_ = true;
        focus_target_profile_view_ = true;
        ImGui::MarkIniSettingsDirty();
    }
    ImGui::Separator();
    if (ImGui::Button("+ Add variant")) {
        create_variant_for_selected_schema();
    }
    ImGui::Separator();
    if (!analysis_session_.inputs.selection.type.has_value()) {
        ImGui::TextDisabled("Select a supported schema.");
    } else if (auto const& definition{analysis_session_.inputs.workspace.types()
                                          .type(*analysis_session_.inputs.selection.type)
                                          .definition};
               std::holds_alternative<EnumType>(definition)) {
        auto const& analysis{*analysis_session_.results().enum_domain};
        auto const& aggregate{analysis.aggregate};
        ImGui::SeparatorText("Standalone C++ backing scale");
        if (draw_element_count()) {
            ImGui::End();
            return;
        }
        if (ImGui::BeginTable("enum-backing-aggregate",
                              2,
                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                  ImGuiTableFlags_SizingStretchProp)) {
            auto draw_stat{[](char const* const label, std::string const& value) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(label);
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(value.c_str());
            }};
            draw_stat("C++ backing", analysis.backing_type);
            draw_stat("Physical backing / value",
                      detail::format_bytes(analysis.backing_facts.transform(
                          [](TypeFacts const& facts) { return facts.size_bytes; })));
            draw_stat("Total standalone backing storage",
                      detail::format_bytes(aggregate.total_storage_bytes));
            draw_stat("Minimum cache lines", detail::format_number(aggregate.minimum_cache_lines));
            draw_stat("Complete standalone values / cache line",
                      detail::format_number(aggregate.complete_elements_per_cache_line));
            draw_stat("Values crossing cache-line boundaries",
                      detail::format_number(aggregate.cache_line_straddling_elements));
            draw_stat("Minimum pages", detail::format_number(aggregate.minimum_pages));
            draw_stat("Complete standalone values / page",
                      detail::format_number(aggregate.complete_elements_per_page));
            draw_stat("Values crossing page boundaries",
                      detail::format_number(aggregate.page_straddling_elements));
            draw_stat("Fits L1 data cache",
                      detail::format_fit(aggregate.cache_capacity.fits_l1_data));
            draw_stat("Fits L2 cache", detail::format_fit(aggregate.cache_capacity.fits_l2));
            draw_stat("Fits L3 cache", detail::format_fit(aggregate.cache_capacity.fits_l3));
            ImGui::EndTable();
        }
        ImGui::TextDisabled(
            "This is a contiguous array of the generated standalone C++ backing at a cache-line/"
            "page-aligned origin. Semantic width is not sizeof, and packed-field uses are not "
            "included.");
        draw_diagnostics(analysis.diagnostics);
    } else if (auto const* packed = std::get_if<PackedType>(&definition)) {
        draw_packed_layout(*packed, *analysis_session_.results().baseline_packed);
    } else if (auto const* soa{std::get_if<SoaType>(&definition)};
               soa != nullptr && soa->backend == codegen::SoaBackend::standard_library) {
        draw_soa_layout(*soa, *analysis_session_.results().baseline_soa);
    } else if (std::holds_alternative<RecordType>(definition)) {
        draw_record_layout(*analysis_session_.results().record_analysis);
    } else if (std::holds_alternative<UnionType>(definition)) {
        auto const& analysis{*analysis_session_.results().union_analysis};
        ImGui::SeparatorText("Analysis scale");
        if (draw_element_count()) {
            ImGui::End();
            return;
        }
        auto const& aggregate{analysis.aggregate};
        if (ImGui::BeginTable("union-aggregate",
                              2,
                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                  ImGuiTableFlags_SizingStretchProp)) {
            auto draw_stat{[](char const* const label, std::string const& value) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(label);
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(value.c_str());
            }};
            draw_stat("Physical storage", detail::format_bytes(aggregate.total_storage_bytes));
            draw_stat("Tail padding", detail::format_bytes(aggregate.total_tail_padding_bytes));
            draw_stat("Minimum cache lines", detail::format_number(aggregate.minimum_cache_lines));
            draw_stat("Complete elements / cache line",
                      detail::format_number(aggregate.complete_elements_per_cache_line));
            draw_stat("Elements crossing cache-line boundaries",
                      detail::format_number(aggregate.cache_line_straddling_elements));
            draw_stat("Minimum pages", detail::format_number(aggregate.minimum_pages));
            draw_stat("Complete elements / page",
                      detail::format_number(aggregate.complete_elements_per_page));
            draw_stat("Elements crossing page boundaries",
                      detail::format_number(aggregate.page_straddling_elements));
            draw_stat("Fits L1 data cache",
                      detail::format_fit(aggregate.cache_capacity.fits_l1_data));
            draw_stat("Fits L2 cache", detail::format_fit(aggregate.cache_capacity.fits_l2));
            draw_stat("Fits L3 cache", detail::format_fit(aggregate.cache_capacity.fits_l3));
            ImGui::EndTable();
        }
        ImGui::TextDisabled(
            "Boundary crossing assumes a contiguous array whose base is cache-line/page aligned.");
        ImGui::Text("Size: %s", detail::format_bytes(analysis.size_bytes).c_str());
        ImGui::Text("Alignment: %s", detail::format_bytes(analysis.alignment_bytes).c_str());
        ImGui::Text("Largest alternative: %s",
                    detail::format_bytes(analysis.largest_alternative_bytes).c_str());
        ImGui::Text("Tail padding: %s", detail::format_bytes(analysis.tail_padding_bytes).c_str());
        if (analysis.size_bytes.has_value() && *analysis.size_bytes != 0) {
            ImGui::SeparatorText("Object map");
            auto const width{std::max(1.0F, ImGui::GetContentRegionAvail().x)};
            constexpr auto bar_height{34.0F};
            for (std::size_t index{}; index < analysis.alternatives.size(); ++index) {
                auto const& alternative{analysis.alternatives[index]};
                if (!alternative.extent_bytes.has_value()) {
                    continue;
                }
                ImGui::PushID(static_cast<int>(index));
                ImGui::TextUnformatted(alternative.name.c_str());
                auto const origin{ImGui::GetCursorScreenPos()};
                auto* const draw_list{ImGui::GetWindowDrawList()};
                draw_list->AddRectFilled(origin,
                                         {origin.x + width, origin.y + bar_height},
                                         ImGui::GetColorU32(ImVec4{0.20F, 0.22F, 0.25F, 1.0F}),
                                         3.0F);
                auto const extent_width{width * static_cast<float>(*alternative.extent_bytes) /
                                        static_cast<float>(*analysis.size_bytes)};
                auto const selected{analysis_session_.inputs.selection.field == alternative.name};
                auto const extent_color{selected ? ImVec4{0.24F, 0.65F, 0.90F, 1.0F}
                                                 : ImVec4{0.62F, 0.39F, 0.20F, 1.0F}};
                draw_list->AddRectFilled(origin,
                                         {origin.x + extent_width, origin.y + bar_height},
                                         ImGui::GetColorU32(extent_color),
                                         3.0F);
                if (*alternative.extent_bytes < *analysis.size_bytes) {
                    auto const range{std::to_string(*alternative.extent_bytes) + ".." +
                                     std::to_string(*analysis.size_bytes - 1)};
                    detail::draw_labeled_gap(draw_list,
                                             {origin.x + extent_width, origin.y},
                                             {origin.x + width, origin.y + bar_height},
                                             "slack " + range,
                                             range);
                }
                draw_list->AddRect(origin,
                                   {origin.x + width, origin.y + bar_height},
                                   ImGui::GetColorU32(ImGuiCol_Border),
                                   3.0F);
                auto const extent_label{std::to_string(*alternative.extent_bytes) + " B extent"};
                draw_list->AddText({origin.x + 5.0F, origin.y + 8.0F},
                                   ImGui::GetColorU32(ImGuiCol_Text),
                                   extent_label.c_str());
                ImGui::InvisibleButton("##union-alternative-map", {width, bar_height});
                if (ImGui::IsItemClicked()) {
                    analysis_session_.inputs.selection.field = alternative.name;
                }
                if (ImGui::IsItemHovered()) {
                    if (*alternative.extent_bytes == 0) {
                        ImGui::SetTooltip(
                            "%s: zero-byte extent; bytes 0..%llu are slack",
                            alternative.name.c_str(),
                            static_cast<unsigned long long>(*analysis.size_bytes - 1));
                    } else if (*alternative.extent_bytes < *analysis.size_bytes) {
                        ImGui::SetTooltip(
                            "%s: bytes 0..%llu occupied; bytes %llu..%llu are slack",
                            alternative.name.c_str(),
                            static_cast<unsigned long long>(*alternative.extent_bytes - 1),
                            static_cast<unsigned long long>(*alternative.extent_bytes),
                            static_cast<unsigned long long>(*analysis.size_bytes - 1));
                    } else {
                        ImGui::SetTooltip("%s: all %llu bytes occupied; no slack",
                                          alternative.name.c_str(),
                                          static_cast<unsigned long long>(*analysis.size_bytes));
                    }
                }
                if (*alternative.extent_bytes < *analysis.size_bytes) {
                    ImGui::TextWrapped("Slack: bytes %llu..%llu",
                                       static_cast<unsigned long long>(*alternative.extent_bytes),
                                       static_cast<unsigned long long>(*analysis.size_bytes - 1));
                }
                ImGui::PopID();
            }
            ImGui::TextWrapped("Hatched bytes are inactive union storage, including any tail "
                               "alignment.");
        }
        if (ImGui::BeginTable("union-layout",
                              5,
                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                  ImGuiTableFlags_SizingStretchProp)) {
            ImGui::TableSetupColumn("Alternative");
            ImGui::TableSetupColumn("Count");
            ImGui::TableSetupColumn("Extent");
            ImGui::TableSetupColumn("Slack / object");
            ImGui::TableSetupColumn("Slack at count");
            ImGui::TableHeadersRow();
            for (auto const& alternative : analysis.alternatives) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(alternative.name.c_str());
                ImGui::TableNextColumn();
                ImGui::Text("%llu", static_cast<unsigned long long>(alternative.element_count));
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(detail::format_bytes(alternative.extent_bytes).c_str());
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(detail::format_bytes(alternative.slack_bytes).c_str());
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(detail::format_bytes(alternative.total_slack_bytes).c_str());
            }
            ImGui::EndTable();
        }
        if (analysis_session_.results().union_distribution_analysis.has_value()) {
            auto const& distribution{*analysis_session_.results().union_distribution_analysis};
            ImGui::SeparatorText("Explicit session workload");
            if (ImGui::BeginTable("union-distribution-summary",
                                  2,
                                  ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                      ImGuiTableFlags_SizingStretchProp)) {
                auto draw_stat{[](char const* const label, std::string const& value) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(label);
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(value.c_str());
                }};
                draw_stat("Sample weight", detail::format_number(distribution.total_weight));
                draw_stat("Sample active extent",
                          detail::format_bytes(distribution.total_extent_bytes));
                draw_stat("Sample conditional slack",
                          detail::format_bytes(distribution.total_slack_bytes));
                auto format_decimal{[](std::optional<long double> const value) {
                    if (!value.has_value()) {
                        return std::string{"Unknown"};
                    }
                    std::array<char, 64> text{};
                    std::snprintf(
                        text.data(), text.size(), "%.8g bytes", static_cast<double>(*value));
                    return std::string{text.data()};
                }};
                draw_stat("Expected active extent / value",
                          format_decimal(distribution.expected_extent_bytes_per_value));
                draw_stat("Expected conditional slack / value",
                          format_decimal(distribution.expected_slack_bytes_per_value));
                draw_stat("Expected active extent at selected count",
                          format_decimal(distribution.expected_selected_extent_bytes));
                draw_stat("Expected conditional slack at selected count",
                          format_decimal(distribution.expected_selected_slack_bytes));
                ImGui::EndTable();
            }
            if (ImGui::BeginTable("union-distribution-entries",
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
            ImGui::TextDisabled(
                "Expected values are conditional on the explicit session weights; a raw union "
                "stores no runtime tag and the workload is not persisted to LispB.");
            draw_diagnostics(distribution.diagnostics);
        }
        ImGui::TextDisabled(
            "Each scaled slack value assumes every object uses that alternative; no tag "
            "distribution is implied.");
        draw_diagnostics(analysis.diagnostics);
    } else if (auto const* tagged{std::get_if<TaggedUnionType>(&definition)}) {
        auto const& analysis{*analysis_session_.results().tagged_union_analysis};
        ImGui::SeparatorText("Analysis scale");
        if (draw_element_count()) {
            ImGui::End();
            return;
        }
        ImGui::Text("Tagged union: %s",
                    analysis_session_.inputs.workspace.types()
                        .type(*analysis_session_.inputs.selection.type)
                        .identity.name.c_str());
        ImGui::Text("Discriminant: %s",
                    analysis_session_.inputs.workspace.types()
                        .type(tagged->discriminant.type)
                        .identity.name.c_str());
        ImGui::SeparatorText("Discriminant coverage");
        ImGui::Text("Mapped live tags: %llu",
                    static_cast<unsigned long long>(analysis.mapped_live_tags.size()));
        for (auto const& tag : analysis.mapped_live_tags) {
            ImGui::BulletText("%s -> payload", tag.c_str());
        }
        ImGui::Text("Unmapped live tags: %llu",
                    static_cast<unsigned long long>(analysis.unmapped_live_tags.size()));
        for (auto const& tag : analysis.unmapped_live_tags) {
            ImGui::BulletText("%s -> no payload alternative", tag.c_str());
        }
        ImGui::Text("Named sentinel tags: %llu",
                    static_cast<unsigned long long>(analysis.sentinel_tags.size()));
        for (auto const& tag : analysis.sentinel_tags) {
            ImGui::BulletText("%s -> reserved sentinel", tag.c_str());
        }
        if (analysis.count_sentinel_tag.has_value()) {
            ImGui::Text("Count sentinel: %s", analysis.count_sentinel_tag->c_str());
        } else {
            ImGui::TextDisabled("Count sentinel: None");
        }
        ImGui::TextDisabled("Tag roles are semantic code-space facts, not allocated byte waste.");
        auto const& aggregate{analysis.aggregate};
        if (ImGui::BeginTable("tagged-union-aggregate",
                              2,
                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                  ImGuiTableFlags_SizingStretchProp)) {
            auto draw_stat{[](char const* const label, std::string const& value) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(label);
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(value.c_str());
            }};
            draw_stat("Physical storage", detail::format_bytes(aggregate.total_storage_bytes));
            draw_stat("Discriminant storage",
                      detail::format_bytes(aggregate.total_discriminant_bytes));
            draw_stat("Payload-union storage", detail::format_bytes(aggregate.total_payload_bytes));
            draw_stat("Padding", detail::format_bytes(aggregate.total_internal_padding_bytes));
            draw_stat("Tail padding", detail::format_bytes(aggregate.total_tail_padding_bytes));
            draw_stat("Total padding", detail::format_bytes(aggregate.total_padding_bytes));
            draw_stat("Minimum cache lines", detail::format_number(aggregate.minimum_cache_lines));
            draw_stat("Complete elements / cache line",
                      detail::format_number(aggregate.complete_elements_per_cache_line));
            draw_stat("Elements crossing cache-line boundaries",
                      detail::format_number(aggregate.cache_line_straddling_elements));
            draw_stat("Minimum pages", detail::format_number(aggregate.minimum_pages));
            draw_stat("Complete elements / page",
                      detail::format_number(aggregate.complete_elements_per_page));
            draw_stat("Elements crossing page boundaries",
                      detail::format_number(aggregate.page_straddling_elements));
            draw_stat("Fits L1 data cache",
                      detail::format_fit(aggregate.cache_capacity.fits_l1_data));
            draw_stat("Fits L2 cache", detail::format_fit(aggregate.cache_capacity.fits_l2));
            draw_stat("Fits L3 cache", detail::format_fit(aggregate.cache_capacity.fits_l3));
            ImGui::EndTable();
        }
        ImGui::TextDisabled(
            "Boundary crossing assumes a contiguous array whose base is cache-line/page aligned.");
        if (ImGui::BeginTable("tagged-union-target-layout",
                              2,
                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                  ImGuiTableFlags_SizingStretchProp)) {
            auto draw_stat{[](char const* const label, std::string const& value) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(label);
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(value.c_str());
            }};
            draw_stat("Discriminant storage",
                      analysis.discriminant_facts.has_value()
                          ? detail::format_bytes(analysis.discriminant_facts->size_bytes)
                          : "Unknown");
            draw_stat("Payload offset", detail::format_bytes(analysis.payload_offset_bytes));
            draw_stat("Payload storage", detail::format_bytes(analysis.payload_size_bytes));
            draw_stat("Padding", detail::format_bytes(analysis.internal_padding_bytes));
            draw_stat("Tail padding", detail::format_bytes(analysis.tail_padding_bytes));
            draw_stat("Object size", detail::format_bytes(analysis.size_bytes));
            draw_stat("Object alignment", detail::format_bytes(analysis.alignment_bytes));
            ImGui::EndTable();
        }
        if (analysis.size_bytes.has_value() && *analysis.size_bytes != 0 &&
            analysis.discriminant_facts.has_value() && analysis.payload_offset_bytes.has_value() &&
            analysis.payload_size_bytes.has_value()) {
            ImGui::SeparatorText("Object map");
            auto const width{std::max(1.0F, ImGui::GetContentRegionAvail().x)};
            constexpr auto bar_height{38.0F};
            auto const origin{ImGui::GetCursorScreenPos()};
            auto* const draw_list{ImGui::GetWindowDrawList()};
            auto const total{static_cast<float>(*analysis.size_bytes)};
            auto const tag_end{width * static_cast<float>(analysis.discriminant_facts->size_bytes) /
                               total};
            auto const payload_begin{width * static_cast<float>(*analysis.payload_offset_bytes) /
                                     total};
            auto const payload_end{
                width *
                static_cast<float>(*analysis.payload_offset_bytes + *analysis.payload_size_bytes) /
                total};
            draw_list->AddRectFilled(origin,
                                     {origin.x + width, origin.y + bar_height},
                                     ImGui::GetColorU32(ImVec4{0.20F, 0.22F, 0.25F, 1.0F}),
                                     3.0F);
            draw_list->AddRectFilled(origin,
                                     {origin.x + tag_end, origin.y + bar_height},
                                     ImGui::GetColorU32(ImVec4{0.25F, 0.58F, 0.86F, 1.0F}),
                                     3.0F);
            draw_list->AddRectFilled({origin.x + payload_begin, origin.y},
                                     {origin.x + payload_end, origin.y + bar_height},
                                     ImGui::GetColorU32(ImVec4{0.62F, 0.39F, 0.20F, 1.0F}),
                                     3.0F);
            auto const tag_bytes{analysis.discriminant_facts->size_bytes};
            auto const payload_offset{*analysis.payload_offset_bytes};
            auto const payload_end_bytes{payload_offset + *analysis.payload_size_bytes};
            if (tag_bytes < payload_offset) {
                auto const range{std::to_string(tag_bytes) + ".." +
                                 std::to_string(payload_offset - 1)};
                detail::draw_labeled_gap(draw_list,
                                         {origin.x + tag_end, origin.y},
                                         {origin.x + payload_begin, origin.y + bar_height},
                                         "padding " + range,
                                         "pad " + range);
            }
            if (payload_end_bytes < *analysis.size_bytes) {
                auto const range{std::to_string(payload_end_bytes) + ".." +
                                 std::to_string(*analysis.size_bytes - 1)};
                detail::draw_labeled_gap(draw_list,
                                         {origin.x + payload_end, origin.y},
                                         {origin.x + width, origin.y + bar_height},
                                         "tail padding " + range,
                                         "tail " + range);
            }
            draw_list->AddRect(origin,
                               {origin.x + width, origin.y + bar_height},
                               ImGui::GetColorU32(ImGuiCol_Border),
                               3.0F);
            if (tag_end > ImGui::CalcTextSize("tag").x + 10.0F) {
                draw_list->AddText(
                    {origin.x + 5.0F, origin.y + 10.0F}, ImGui::GetColorU32(ImGuiCol_Text), "tag");
            }
            if (payload_end - payload_begin > ImGui::CalcTextSize("payload union").x + 10.0F) {
                draw_list->AddText({origin.x + payload_begin + 5.0F, origin.y + 10.0F},
                                   ImGui::GetColorU32(ImGuiCol_Text),
                                   "payload union");
            }
            ImGui::InvisibleButton("##tagged-union-object-map", {width, bar_height});
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip(
                    "Tag: %s; alignment gap: %s; payload: %s; tail padding: %s",
                    detail::format_bytes(analysis.discriminant_facts->size_bytes).c_str(),
                    detail::format_bytes(analysis.internal_padding_bytes).c_str(),
                    detail::format_bytes(analysis.payload_size_bytes).c_str(),
                    detail::format_bytes(analysis.tail_padding_bytes).c_str());
            }
            if (tag_bytes < payload_offset) {
                ImGui::TextWrapped("Alignment padding: bytes %llu..%llu",
                                   static_cast<unsigned long long>(tag_bytes),
                                   static_cast<unsigned long long>(payload_offset - 1));
            }
            if (payload_end_bytes < *analysis.size_bytes) {
                ImGui::TextWrapped("Tail padding: bytes %llu..%llu",
                                   static_cast<unsigned long long>(payload_end_bytes),
                                   static_cast<unsigned long long>(*analysis.size_bytes - 1));
            }
        }
        if (ImGui::BeginTable("tagged-union-semantic-layout",
                              7,
                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                  ImGuiTableFlags_SizingStretchProp)) {
            ImGui::TableSetupColumn("Tag");
            ImGui::TableSetupColumn("Alternative");
            ImGui::TableSetupColumn("Semantic type");
            ImGui::TableSetupColumn("Count");
            ImGui::TableSetupColumn("Extent");
            ImGui::TableSetupColumn("Payload slack");
            ImGui::TableSetupColumn("Slack at count");
            ImGui::TableHeadersRow();
            for (auto const& alternative : analysis.alternatives) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(alternative.tag.c_str());
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(alternative.name.c_str());
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(analysis_session_.inputs.workspace.types()
                                           .type(alternative.semantic_type)
                                           .cpp_spelling.c_str());
                ImGui::TableNextColumn();
                ImGui::Text("%llu", static_cast<unsigned long long>(alternative.element_count));
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(detail::format_bytes(alternative.extent_bytes).c_str());
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(
                    detail::format_bytes(alternative.payload_slack_bytes).c_str());
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(
                    detail::format_bytes(alternative.total_payload_slack_bytes).c_str());
            }
            ImGui::EndTable();
        }
        ImGui::TextDisabled("Payload slack is conditional on the active tag; it is not allocated "
                            "outside the shared payload union.");
        draw_diagnostics(analysis.diagnostics);
    } else if (std::holds_alternative<EnumType>(definition)) {
        ImGui::TextDisabled("Enums have semantic metadata but no standalone aggregate layout.");
    } else if (std::holds_alternative<IntegerScalarType>(definition)) {
        auto const& analysis{*analysis_session_.results().integer_scalar_analysis};
        if (analysis.relationship_kind.has_value() && analysis.relationship_target.has_value()) {
            ImGui::Text(
                "Relationship: %s -> %s",
                std::string{codegen::semantic_relation_kind_name(*analysis.relationship_kind)}
                    .c_str(),
                analysis.relationship_target->c_str());
            if (analysis.relationship_target_extent.has_value()) {
                auto const kind{*analysis.relationship_kind};
                auto const term{detail::relationship_extent_term(kind)};
                auto const unit{detail::relationship_extent_unit(kind, analysis.relationship_unit)};
                ImGui::Text("Session target %s: %llu %s",
                            term.data(),
                            static_cast<unsigned long long>(*analysis.relationship_target_extent),
                            unit.data());
                ImGui::Text(
                    "Required live values: %s",
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
                ImGui::Text(
                    "Code-space %s limit: %s",
                    term.data(),
                    detail::format_number(analysis.relationship_code_space_capacity_limit).c_str());
                ImGui::Text("Code-space %s headroom: %s",
                            term.data(),
                            detail::format_number(analysis.relationship_capacity_headroom).c_str());
                ImGui::Text(
                    "Semantic-range %s limit: %s",
                    term.data(),
                    detail::format_number(analysis.relationship_semantic_capacity_limit).c_str());
                ImGui::Text(
                    "Sentinel-placement %s limit: %s",
                    term.data(),
                    detail::format_number(analysis.relationship_sentinel_capacity_limit).c_str());
                ImGui::Text(
                    "Effective valid %s limit: %s",
                    term.data(),
                    detail::format_number(analysis.relationship_effective_capacity_limit).c_str());
                ImGui::Text("Effective valid %s headroom: %s",
                            term.data(),
                            detail::format_number(analysis.relationship_effective_capacity_headroom)
                                .c_str());
            } else {
                ImGui::TextDisabled("No supported session target fact is available.");
            }
        }
        ImGui::TextDisabled(
            "Semantic integer scalars have no standalone physical layout. Target facts explain "
            "the domain requirement but do not create an ABI sizeof or mutate LispB.");
        draw_diagnostics(analysis.diagnostics);
    } else if (std::holds_alternative<LinearQuantizedType>(definition)) {
        auto const& analysis{*analysis_session_.results().linear_quantized_analysis};
        ImGui::Text("Encoded width: %u bits", analysis.encoded_storage_bits);
        ImGui::Text("Usable codes: %s",
                    detail::format_code_count(analysis.usable_code_count).c_str());
        ImGui::Text("Resolution: %.12g", static_cast<double>(analysis.resolution));
        ImGui::Text("Maximum rounding error: %.12g",
                    static_cast<double>(analysis.maximum_rounding_error));
        ImGui::TextDisabled(
            "Encoded bits are representation facts, not a standalone ABI sizeof/alignment.");
    } else if (std::holds_alternative<IntegerVarintType>(definition)) {
        ImGui::SeparatorText("Analysis scale");
        if (draw_element_count()) {
            ImGui::End();
            return;
        }
        auto const& analysis{*analysis_session_.results().integer_varint_analysis};
        ImGui::Text("Encoded size: %u .. %u bytes/value",
                    analysis.minimum_encoded_bytes,
                    analysis.maximum_encoded_bytes);
        ImGui::Text("At %llu values: %s .. %s",
                    static_cast<unsigned long long>(analysis.element_count),
                    detail::format_bytes(analysis.minimum_total_bytes).c_str(),
                    detail::format_bytes(analysis.maximum_total_bytes).c_str());
        ImGui::TextDisabled(
            "Variable-length size is a range; expected size requires a value distribution.");
    } else if (std::holds_alternative<FixedPointType>(definition)) {
        draw_fixed_point_bit_layout(*analysis_session_.results().fixed_point_analysis);
        ImGui::SeparatorText("Analysis scale");
        if (draw_element_count()) {
            ImGui::End();
            return;
        }
        auto const& analysis{*analysis_session_.results().fixed_point_analysis};
        ImGui::Text("Encoded width: %u bits/value", analysis.total_bits);
        ImGui::Text("At %llu values: %s bits",
                    static_cast<unsigned long long>(analysis.element_count),
                    detail::format_number(analysis.total_encoded_bits).c_str());
        ImGui::Text("Resolution: %.12g", static_cast<double>(analysis.resolution));
        ImGui::Text("Representable range: %.12g .. %.12g",
                    static_cast<double>(analysis.minimum_value),
                    static_cast<double>(analysis.maximum_value));
        ImGui::Text("Allowed range: %.12g .. %.12g",
                    static_cast<double>(analysis.minimum_allowed_value),
                    static_cast<double>(analysis.maximum_allowed_value));
        ImGui::TextDisabled(
            "Encoded payload bits are not a standalone ABI sizeof/alignment or allocation size.");
        draw_diagnostics(analysis.diagnostics);
    } else if (std::holds_alternative<MiniFloatType>(definition)) {
        ImGui::SeparatorText("Analysis scale");
        if (draw_element_count()) {
            ImGui::End();
            return;
        }
        auto const& analysis{*analysis_session_.results().mini_float_analysis};
        ImGui::Text("Encoded width: %u bits/value", analysis.total_bits);
        ImGui::Text("At %llu values: %s bits",
                    static_cast<unsigned long long>(analysis.element_count),
                    detail::format_number(analysis.total_encoded_bits).c_str());
        ImGui::Text("Normal exponent range: %d .. %d",
                    analysis.minimum_normal_exponent,
                    analysis.maximum_normal_exponent);
        if (analysis.minimum_positive_normal.has_value()) {
            ImGui::Text("Minimum positive normal: %.12g",
                        static_cast<double>(*analysis.minimum_positive_normal));
        } else {
            ImGui::TextDisabled("Minimum positive normal: Unknown");
        }
        if (analysis.maximum_finite.has_value()) {
            ImGui::Text("Maximum finite: %.12g", static_cast<double>(*analysis.maximum_finite));
        } else {
            ImGui::TextDisabled("Maximum finite: Unknown");
        }
        ImGui::TextDisabled(
            "Encoded payload bits are not a standalone ABI sizeof/alignment or allocation size.");
        draw_diagnostics(analysis.diagnostics);
    } else if (std::holds_alternative<OptionalSentinelType>(definition)) {
        ImGui::SeparatorText("Analysis scale");
        if (draw_element_count()) {
            ImGui::End();
            return;
        }
        auto const& analysis{*analysis_session_.results().optional_sentinel_analysis};
        ImGui::Text("Encoded width: %u bits/value", analysis.encoded_storage_bits);
        ImGui::Text("Present values: %s",
                    detail::format_number(analysis.present_value_count).c_str());
        ImGui::Text("Absence codes: %llu",
                    static_cast<unsigned long long>(analysis.absence_code_count));
        ImGui::Text("Other sentinel codes: %llu",
                    static_cast<unsigned long long>(analysis.other_sentinel_code_count));
        ImGui::Text("Unused codes: %s", detail::format_number(analysis.unused_code_count).c_str());
        ImGui::Text("At %llu values: %s encoded bits",
                    static_cast<unsigned long long>(analysis.element_count),
                    detail::format_number(analysis.total_encoded_bits).c_str());
        ImGui::TextDisabled(
            "Encoded payload bits are not a standalone ABI sizeof or allocation size.");
        draw_diagnostics(analysis.diagnostics);
    } else if (std::holds_alternative<OptionalPresenceBitType>(definition)) {
        ImGui::SeparatorText("Analysis scale");
        if (draw_element_count()) {
            ImGui::End();
            return;
        }
        auto const& analysis{*analysis_session_.results().optional_presence_bit_analysis};
        ImGui::Text("Encoded width: %u bits/value (%u presence + %u payload)",
                    analysis.encoded_storage_bits,
                    analysis.presence_bits,
                    analysis.payload_bits);
        ImGui::Text("Present values: %s",
                    detail::format_number(analysis.present_value_count).c_str());
        ImGui::Text("Canonical absence states: %llu",
                    static_cast<unsigned long long>(analysis.canonical_absence_state_count));
        ImGui::Text("Source sentinel codes: %llu",
                    static_cast<unsigned long long>(analysis.source_sentinel_code_count));
        ImGui::Text("Source unused payload codes: %s",
                    detail::format_number(analysis.source_unused_payload_codes).c_str());
        ImGui::Text("Noncanonical absence bit patterns: %s",
                    detail::format_number(analysis.noncanonical_absence_patterns).c_str());
        ImGui::Text("At %llu values: %s encoded bits",
                    static_cast<unsigned long long>(analysis.element_count),
                    detail::format_number(analysis.total_encoded_bits).c_str());
        ImGui::TextDisabled(
            "Ignored payload patterns when absent are redundant encodings, not additional "
            "semantic absence states or allocated byte waste.");
        ImGui::TextDisabled(
            "Encoded payload bits are not a standalone ABI sizeof or allocation size.");
        draw_diagnostics(analysis.diagnostics);
    } else {
        ImGui::TextDisabled("This type is not supported by the layout analyzer.");
    }
    ImGui::End();
}

void PlannerUi::draw_diagnostics(std::vector<Diagnostic> const& diagnostics) const {
    for (auto const& diagnostic : diagnostics) {
        ImGui::PushStyleColor(ImGuiCol_Text, detail::diagnostic_color(diagnostic.severity));
        ImGui::TextWrapped("%s", diagnostic.message.c_str());
        ImGui::PopStyleColor();
    }
}

void PlannerUi::sync_variant_name() {
    auto const& name{analysis_session_.inputs.workspace.active_variant().name};
    std::snprintf(variant_name_.data(), variant_name_.size(), "%s", name.c_str());
    variant_name_id_ = analysis_session_.inputs.workspace.active_variant_id();
}

void PlannerUi::create_variant_for_selected_schema() {
    auto const name{analysis_session_.inputs.selection.type.has_value()
                        ? analysis_session_.inputs.workspace.types()
                                  .type(*analysis_session_.inputs.selection.type)
                                  .identity.name +
                              " experiment " + std::to_string(next_variant_number_++)
                        : "Experiment " + std::to_string(next_variant_number_++)};
    analysis_session_.inputs.workspace.create_variant(name);
    sync_variant_name();
    refresh_analysis();
}

} // namespace ioj::layout_planner
