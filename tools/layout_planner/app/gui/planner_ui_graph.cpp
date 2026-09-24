#include "planner_ui.hpp"

#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace ioj::layout_planner {
namespace {

using namespace lispb::schema;

auto graph_column(TypeDefinition const& definition) -> std::size_t {
    if (std::holds_alternative<ExternalType>(definition)) {
        return 0;
    }
    if (std::holds_alternative<EnumType>(definition) ||
        std::holds_alternative<IntegerScalarType>(definition)) {
        return 1;
    }
    if (std::holds_alternative<LinearQuantizedType>(definition) ||
        std::holds_alternative<IntegerVarintType>(definition) ||
        std::holds_alternative<FixedPointType>(definition) ||
        std::holds_alternative<MiniFloatType>(definition) ||
        std::holds_alternative<OptionalSentinelType>(definition) ||
        std::holds_alternative<OptionalPresenceBitType>(definition) ||
        std::holds_alternative<PackedType>(definition)) {
        return 2;
    }
    if (std::holds_alternative<RecordType>(definition) ||
        std::holds_alternative<UnionType>(definition) ||
        std::holds_alternative<TaggedUnionType>(definition)) {
        return 3;
    }
    return 4;
}

auto graph_kind(TypeDefinition const& definition) -> char const* {
    if (std::holds_alternative<ExternalType>(definition)) {
        return "external";
    }
    if (std::holds_alternative<EnumType>(definition)) {
        return "enum";
    }
    if (std::holds_alternative<IntegerScalarType>(definition)) {
        return "integer scalar";
    }
    if (std::holds_alternative<LinearQuantizedType>(definition)) {
        return "linear quantized";
    }
    if (std::holds_alternative<IntegerVarintType>(definition)) {
        return "integer varint";
    }
    if (std::holds_alternative<FixedPointType>(definition)) {
        return "fixed point";
    }
    if (std::holds_alternative<MiniFloatType>(definition)) {
        return "mini float";
    }
    if (std::holds_alternative<OptionalSentinelType>(definition)) {
        return "optional sentinel";
    }
    if (std::holds_alternative<OptionalPresenceBitType>(definition)) {
        return "optional presence bit";
    }
    if (std::holds_alternative<PackedType>(definition)) {
        return "packed";
    }
    if (std::holds_alternative<RecordType>(definition)) {
        return "record";
    }
    if (std::holds_alternative<UnionType>(definition)) {
        return "union";
    }
    if (std::holds_alternative<TaggedUnionType>(definition)) {
        return "tagged union";
    }
    if (std::holds_alternative<StaticTableType>(definition)) {
        return "static table";
    }
    if (std::holds_alternative<FacadeType>(definition)) {
        return "facade";
    }
    if (std::holds_alternative<HomogeneousStorageType>(definition)) {
        return "homogeneous storage";
    }
    return "SoA";
}

auto node_color(TypeDefinition const& definition,
                bool const selected,
                bool const dependency,
                bool const user,
                bool const neighborhood_active) -> ImU32 {
    if (selected) {
        return IM_COL32(64, 126, 220, 255);
    }
    if (dependency && user) {
        return IM_COL32(123, 82, 166, 255);
    }
    if (dependency) {
        return IM_COL32(54, 130, 105, 255);
    }
    if (user) {
        return IM_COL32(164, 103, 55, 255);
    }
    if (neighborhood_active) {
        return IM_COL32(45, 49, 57, 210);
    }
    if (std::holds_alternative<EnumType>(definition)) {
        return IM_COL32(80, 105, 155, 255);
    }
    if (std::holds_alternative<IntegerScalarType>(definition)) {
        return IM_COL32(68, 118, 148, 255);
    }
    if (std::holds_alternative<LinearQuantizedType>(definition)) {
        return IM_COL32(116, 94, 164, 255);
    }
    if (std::holds_alternative<IntegerVarintType>(definition)) {
        return IM_COL32(98, 102, 172, 255);
    }
    if (std::holds_alternative<FixedPointType>(definition)) {
        return IM_COL32(108, 88, 176, 255);
    }
    if (std::holds_alternative<MiniFloatType>(definition)) {
        return IM_COL32(118, 84, 174, 255);
    }
    if (std::holds_alternative<OptionalSentinelType>(definition)) {
        return IM_COL32(128, 82, 168, 255);
    }
    if (std::holds_alternative<OptionalPresenceBitType>(definition)) {
        return IM_COL32(142, 78, 158, 255);
    }
    if (std::holds_alternative<PackedType>(definition)) {
        return IM_COL32(126, 88, 148, 255);
    }
    if (std::holds_alternative<SoaType>(definition)) {
        return IM_COL32(65, 130, 118, 255);
    }
    if (std::holds_alternative<RecordType>(definition)) {
        return IM_COL32(148, 104, 64, 255);
    }
    if (std::holds_alternative<UnionType>(definition)) {
        return IM_COL32(156, 84, 72, 255);
    }
    if (std::holds_alternative<TaggedUnionType>(definition)) {
        return IM_COL32(174, 76, 96, 255);
    }
    return IM_COL32(80, 86, 96, 255);
}

void append_edge_label(std::string& result, std::string value) {
    if (value.empty() || result.find(value) != std::string::npos) {
        return;
    }
    if (!result.empty()) {
        result += ", ";
    }
    result += std::move(value);
}

auto relationship_label(lispb::schema::SemanticRelationship const& relationship) -> std::string {
    auto result{std::string{codegen::semantic_relation_kind_name(relationship.kind)}};
    if (relationship.unit.has_value()) {
        result += " (";
        result += codegen::semantic_relation_unit_name(*relationship.unit);
        result += ')';
    }
    return result;
}

auto edge_label(TypeGraph const& types, TypeId const user, TypeId const dependency) -> std::string {
    auto const& definition{types.type(user).definition};
    std::string result;
    if (auto const* enumeration{std::get_if<EnumType>(&definition)}) {
        if (enumeration->underlying_type.has_value() &&
            enumeration->underlying_type->type == dependency) {
            result = "underlying";
        }
    } else if (auto const* scalar{std::get_if<IntegerScalarType>(&definition)}) {
        if (scalar->relationship.has_value() && scalar->relationship->target.type == dependency) {
            result = relationship_label(*scalar->relationship);
        }
    } else if (auto const* quantized{std::get_if<LinearQuantizedType>(&definition)}) {
        if (quantized->source.type == dependency) {
            result = "quantises";
        }
    } else if (auto const* varint{std::get_if<IntegerVarintType>(&definition)}) {
        if (varint->source.type == dependency) {
            result = "encodes";
        }
    } else if (auto const* optional{std::get_if<OptionalSentinelType>(&definition)}) {
        if (optional->source.type == dependency) {
            result = "optional via " + optional->sentinel_name;
        }
    } else if (auto const* optional{std::get_if<OptionalPresenceBitType>(&definition)}) {
        if (optional->source.type == dependency) {
            result = "optional via presence bit";
        }
    } else if (auto const* packed{std::get_if<PackedType>(&definition)}) {
        if (packed->storage_type.type == dependency) {
            append_edge_label(result, "storage");
        }
        for (auto const& segment : packed->segments) {
            if (auto const* field{std::get_if<PackedField>(&segment)}; field != nullptr) {
                if (field->semantic_type.type == dependency) {
                    append_edge_label(result, field->name);
                }
                if (field->relationship.has_value() &&
                    field->relationship->target.type == dependency) {
                    append_edge_label(result,
                                      field->name + " " + relationship_label(*field->relationship));
                }
            }
        }
    } else if (auto const* soa{std::get_if<SoaType>(&definition)}) {
        if (soa->equivalent_type.has_value() && soa->equivalent_type->type == dependency) {
            append_edge_label(result, "equivalent row");
        }
        for (auto const& column : soa->columns) {
            if (column.semantic_type.type == dependency) {
                append_edge_label(result, column.name);
            }
            if (column.nested_type == dependency) {
                append_edge_label(result, column.name + " nested");
            }
            if (column.relationship.has_value() && column.relationship->target.type == dependency) {
                append_edge_label(result,
                                  column.name + " " + relationship_label(*column.relationship));
            }
        }
    } else if (auto const* record{std::get_if<RecordType>(&definition)}) {
        for (auto const& member : record->members) {
            if (member.semantic_type.type == dependency) {
                append_edge_label(result, member.name);
            }
            if (member.relationship.has_value() && member.relationship->target.type == dependency) {
                append_edge_label(result,
                                  member.name + " " + relationship_label(*member.relationship));
            }
        }
    } else if (auto const* union_type{std::get_if<UnionType>(&definition)}) {
        for (auto const& alternative : union_type->alternatives) {
            if (alternative.semantic_type.type == dependency) {
                append_edge_label(result, alternative.name);
            }
        }
    } else if (auto const* tagged{std::get_if<TaggedUnionType>(&definition)}) {
        if (tagged->discriminant.type == dependency) {
            append_edge_label(result, "discriminates");
        }
        for (auto const& alternative : tagged->alternatives) {
            if (alternative.semantic_type.type == dependency) {
                append_edge_label(result, alternative.name + " [" + alternative.tag + "]");
            }
        }
    }
    if (result.empty()) {
        auto const& node{types.type(user)};
        auto const owner{node.owning_declaration.value_or(node.identity)};
        for (auto const& use : types.type_uses()) {
            if (use.declaration == owner && use.target.type == dependency) {
                append_edge_label(result, use.role);
            }
        }
    }
    return result.empty() ? "references" : result;
}

auto add(ImVec2 const left, ImVec2 const right) -> ImVec2 {
    return {left.x + right.x, left.y + right.y};
}

auto subtract(ImVec2 const left, ImVec2 const right) -> ImVec2 {
    return {left.x - right.x, left.y - right.y};
}

auto multiply(ImVec2 const value, float const scale) -> ImVec2 {
    return {value.x * scale, value.y * scale};
}

auto lowercase(std::string_view const value) -> std::string {
    std::string result;
    result.reserve(value.size());
    for (auto const character : value) {
        result.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(character))));
    }
    return result;
}

auto graph_matches(TypeNode const& node, std::string_view const lowercase_query) -> bool {
    if (lowercase_query.empty()) {
        return false;
    }

    auto const haystack{lowercase(node.identity.name + " " + node.identity.module_name + " " +
                                  node.identity.namespace_name + " " +
                                  graph_kind(node.definition))};
    return haystack.find(lowercase_query) != std::string::npos;
}

auto graph_detail(TypeNode const& node) -> std::string {
    return std::string{graph_kind(node.definition)} + " · " +
           (node.identity.module_name.empty() ? std::string{"external"}
                                              : node.identity.module_name);
}

auto graph_display_text(std::string_view const text, float const available_width) -> std::string {
    auto result{std::string{text}};
    if (ImGui::CalcTextSize(result.c_str()).x <= available_width) {
        return result;
    }

    constexpr std::string_view ellipsis{"…"};
    auto const ellipsis_width{ImGui::CalcTextSize(ellipsis.data()).x};
    while (!result.empty() &&
           ImGui::CalcTextSize(result.c_str()).x + ellipsis_width > available_width) {
        auto first_byte{result.size() - 1};
        while (first_byte > 0 &&
               (static_cast<unsigned char>(result[first_byte]) & 0xc0U) == 0x80U) {
            --first_byte;
        }
        result.resize(first_byte);
    }
    result += ellipsis;
    return result;
}

auto automatic_graph_positions(std::span<TypeNode const> const types,
                               std::span<ImVec2 const> const node_sizes) -> std::vector<ImVec2> {
    auto const count{types.size()};
    std::vector<std::vector<std::size_t>> related(count);
    std::vector<std::vector<std::size_t>> external_users(count);
    for (std::size_t user{}; user < count; ++user) {
        if (std::holds_alternative<ExternalType>(types[user].definition)) {
            continue;
        }
        for (auto const dependency : types[user].dependencies) {
            auto const target{static_cast<std::size_t>(dependency.value)};
            if (target >= count) {
                continue;
            }
            if (std::holds_alternative<ExternalType>(types[target].definition)) {
                external_users[target].push_back(user);
            } else {
                related[user].push_back(target);
                related[target].push_back(user);
            }
        }
    }

    std::vector<int> group_of(count, -1);
    std::vector<std::vector<std::size_t>> groups;
    std::vector<std::size_t> group_rank;
    for (std::size_t index{}; index < count; ++index) {
        if (group_of[index] != -1 ||
            std::holds_alternative<ExternalType>(types[index].definition)) {
            continue;
        }
        auto const group{groups.size()};
        groups.emplace_back();
        group_rank.push_back(0);
        std::vector<std::size_t> pending{index};
        group_of[index] = static_cast<int>(group);
        while (!pending.empty()) {
            auto const current{pending.back()};
            pending.pop_back();
            groups[group].push_back(current);
            group_rank[group] =
                std::max(group_rank[group], graph_column(types[current].definition));
            for (auto const neighbor : related[current]) {
                if (group_of[neighbor] == -1) {
                    group_of[neighbor] = static_cast<int>(group);
                    pending.push_back(neighbor);
                }
            }
        }
    }

    std::vector<std::size_t> orphan_externals;
    for (std::size_t index{}; index < count; ++index) {
        if (!std::holds_alternative<ExternalType>(types[index].definition)) {
            continue;
        }
        std::vector<std::size_t> references(groups.size());
        for (auto const user : external_users[index]) {
            ++references[static_cast<std::size_t>(group_of[user])];
        }
        auto best_group{groups.size()};
        for (std::size_t group{}; group < groups.size(); ++group) {
            if (references[group] == 0) {
                continue;
            }
            if (best_group == groups.size() || references[group] > references[best_group] ||
                (references[group] == references[best_group] &&
                 group_rank[group] > group_rank[best_group])) {
                best_group = group;
            }
        }
        if (best_group == groups.size()) {
            orphan_externals.push_back(index);
        } else {
            groups[best_group].push_back(index);
        }
    }
    if (!orphan_externals.empty()) {
        groups.push_back(std::move(orphan_externals));
        group_rank.push_back(0);
    }

    std::array<float, 5> column_widths{};
    for (std::size_t index{}; index < count; ++index) {
        auto const column{graph_column(types[index].definition)};
        column_widths[column] = std::max(column_widths[column], node_sizes[index].x);
    }
    std::array<float, 5> column_x{};
    auto next_x{32.0F};
    for (std::size_t column{}; column < column_x.size(); ++column) {
        if (column_widths[column] == 0.0F) {
            continue;
        }
        column_x[column] = next_x;
        next_x += column_widths[column] + 70.0F;
    }

    std::vector<std::size_t> group_order(groups.size());
    for (std::size_t index{}; index < group_order.size(); ++index) {
        group_order[index] = index;
    }
    std::stable_sort(
        group_order.begin(), group_order.end(), [&](auto const left, auto const right) {
            return group_rank[left] > group_rank[right];
        });

    auto const row_height{node_sizes.empty() ? 58.0F : node_sizes.front().y};
    auto const row_step{row_height + 34.0F};
    auto next_y{36.0F};
    std::vector<ImVec2> positions(count);
    for (auto const group : group_order) {
        std::array<std::vector<std::size_t>, 5> columns;
        for (auto const index : groups[group]) {
            columns[graph_column(types[index].definition)].push_back(index);
        }
        auto rows{std::size_t{0}};
        for (auto& column : columns) {
            std::sort(column.begin(), column.end());
            rows = std::max(rows, column.size());
        }
        auto const height{row_height + static_cast<float>(rows - 1) * row_step};
        for (std::size_t column{}; column < columns.size(); ++column) {
            auto const column_rows{columns[column].size()};
            if (column_rows == 0) {
                continue;
            }
            auto const column_height{row_height + static_cast<float>(column_rows - 1) * row_step};
            auto const first_y{next_y + (height - column_height) * 0.5F};
            for (std::size_t row{}; row < column_rows; ++row) {
                positions[columns[column][row]] = {column_x[column],
                                                   first_y + static_cast<float>(row) * row_step};
            }
        }
        next_y += height + 88.0F;
    }
    return positions;
}

} // namespace

void PlannerUi::draw_graph_panel() {
    if (!graph_view_open_) {
        return;
    }

    auto const open_before{graph_view_open_};
    if (!ImGui::Begin("Graph", &graph_view_open_, ImGuiWindowFlags_HorizontalScrollbar)) {
        ImGui::End();
        if (open_before != graph_view_open_) {
            ImGui::MarkIniSettingsDirty();
        }
        return;
    }
    if (open_before != graph_view_open_) {
        ImGui::MarkIniSettingsDirty();
    }

    auto const& graph{analysis_session_.inputs.workspace.types()};
    auto const types{graph.types()};
    auto graph_selection{analysis_session_.inputs.selection.type};
    if (!graph_selection.has_value() && analysis_session_.inputs.selection.identity().has_value()) {
        auto const generated{
            graph.types_for_declaration(*analysis_session_.inputs.selection.identity())};
        if (!generated.empty()) {
            graph_selection = generated.front();
            ImGui::TextWrapped("Declaration %s: focusing its first generated storage type, %s.",
                               analysis_session_.inputs.selection.identity()->name.c_str(),
                               graph.type(*graph_selection).cpp_spelling.c_str());
        }
    }

    if (ImGui::Button("Reset view")) {
        graph_pan_x_ = 32.0F;
        graph_pan_y_ = 32.0F;
        graph_zoom_ = 1.0F;
    }
    ImGui::SameLine();
    if (ImGui::Button("Automatic layout")) {
        auto const had_positions{!graph_node_positions_.empty()};
        graph_node_positions_.clear();
        if (!project_path_.empty()) {
            auto const erased{persisted_graph_node_positions_.erase(
                project_path_.lexically_normal().generic_string())};
            if (had_positions || erased != 0) {
                ImGui::MarkIniSettingsDirty();
            }
        }
        graph_fit_all_ = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Fit all")) {
        graph_fit_all_ = true;
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!graph_selection.has_value());
    if (ImGui::Button("Focus selection")) {
        graph_focus_selected_ = true;
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::TextDisabled("Left-drag nodes, right/middle-drag to pan, wheel to zoom (%.0f%%)",
                        graph_zoom_ * 100.0F);

    ImGui::SetNextItemWidth(280.0F);
    auto const search_submitted{ImGui::InputTextWithHint("##graph-search",
                                                         "Find type, module, namespace, or kind",
                                                         graph_search_.data(),
                                                         graph_search_.size(),
                                                         ImGuiInputTextFlags_EnterReturnsTrue)};
    auto const search_query{lowercase(graph_search_.data())};
    std::vector<TypeId> search_matches;
    for (std::size_t index{}; index < types.size(); ++index) {
        if (graph_matches(types[index], search_query)) {
            search_matches.push_back(TypeId{static_cast<std::uint32_t>(index)});
        }
    }

    ImGui::SameLine();
    ImGui::BeginDisabled(search_matches.empty());
    auto const next_match_requested{ImGui::Button("Next match")};
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (search_query.empty()) {
        ImGui::TextDisabled("Search keeps the full graph visible");
    } else if (search_matches.empty()) {
        ImGui::TextDisabled("No matches");
    } else {
        ImGui::TextDisabled(
            "%zu %s", search_matches.size(), search_matches.size() == 1 ? "match" : "matches");
    }

    if ((search_submitted || next_match_requested) && !search_matches.empty()) {
        auto match{search_matches.begin()};
        if (graph_selection.has_value()) {
            auto const current{
                std::find(search_matches.begin(), search_matches.end(), *graph_selection)};
            if (current != search_matches.end()) {
                match = std::next(current);
                if (match == search_matches.end()) {
                    match = search_matches.begin();
                }
            }
        }

        select_type(*match);
        graph_selection = *match;
        graph_focus_selected_ = true;
    }

    if (graph_selection.has_value() && graph_selection->value < types.size()) {
        ImGui::ColorButton("graph-dependency-color",
                           ImVec4{0.21F, 0.51F, 0.41F, 1.0F},
                           ImGuiColorEditFlags_NoTooltip,
                           {10.0F, 10.0F});
        ImGui::SameLine();
        ImGui::TextDisabled("Selected type depends on");
        ImGui::SameLine();
        ImGui::ColorButton("graph-user-color",
                           ImVec4{0.64F, 0.4F, 0.22F, 1.0F},
                           ImGuiColorEditFlags_NoTooltip,
                           {10.0F, 10.0F});
        ImGui::SameLine();
        ImGui::TextDisabled("Uses selected type");
        ImGui::SameLine();
        ImGui::ColorButton("graph-bidirectional-color",
                           ImVec4{0.48F, 0.32F, 0.65F, 1.0F},
                           ImGuiColorEditFlags_NoTooltip,
                           {10.0F, 10.0F});
        ImGui::SameLine();
        ImGui::TextDisabled("Both / cycle");
    }

    constexpr auto child_flags{ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse};
    if (ImGui::BeginChild("relationship-graph-canvas", {0.0F, 0.0F}, true, child_flags)) {
        auto const canvas_position{ImGui::GetCursorScreenPos()};
        auto const canvas_size{ImGui::GetContentRegionAvail()};
        auto* const draw_list{ImGui::GetWindowDrawList()};
        draw_list->AddRectFilled(
            canvas_position, add(canvas_position, canvas_size), IM_COL32(24, 27, 32, 255));

        auto const hovered{ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)};
        auto const& io{ImGui::GetIO()};
        auto const panning{hovered && (ImGui::IsMouseDragging(ImGuiMouseButton_Right, 0.0F) ||
                                       ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.0F))};
        if (panning) {
            graph_pan_x_ += io.MouseDelta.x;
            graph_pan_y_ += io.MouseDelta.y;
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
        }
        if (hovered && io.MouseWheel != 0.0F) {
            auto const old_zoom{graph_zoom_};
            auto const new_zoom{std::clamp(old_zoom * std::pow(1.15F, io.MouseWheel), 0.1F, 2.0F)};
            auto const mouse_in_canvas{subtract(io.MousePos, canvas_position)};
            auto const world_under_mouse{ImVec2{(mouse_in_canvas.x - graph_pan_x_) / old_zoom,
                                                (mouse_in_canvas.y - graph_pan_y_) / old_zoom}};
            graph_pan_x_ = mouse_in_canvas.x - world_under_mouse.x * new_zoom;
            graph_pan_y_ = mouse_in_canvas.y - world_under_mouse.y * new_zoom;
            graph_zoom_ = new_zoom;
        }

        auto const font_size{ImGui::GetFontSize()};
        auto const minimum_width{std::max(190.0F, font_size * 10.0F)};
        auto const maximum_width{std::max(320.0F, font_size * 16.0F)};
        auto const node_height{std::max(58.0F, font_size * 2.0F + 20.0F)};
        std::vector<ImVec2> node_sizes;
        std::vector<std::string> node_details;
        std::vector<std::string> visible_names;
        std::vector<std::string> visible_details;
        node_sizes.reserve(types.size());
        node_details.reserve(types.size());
        visible_names.reserve(types.size());
        visible_details.reserve(types.size());
        for (auto const& node : types) {
            auto const detail{graph_detail(node)};
            auto const text_width{std::max(ImGui::CalcTextSize(node.identity.name.c_str()).x,
                                           ImGui::CalcTextSize(detail.c_str()).x)};
            auto const width{std::clamp(text_width + 16.0F, minimum_width, maximum_width)};
            node_sizes.push_back({width, node_height});
            visible_names.push_back(graph_display_text(node.identity.name, width - 16.0F));
            visible_details.push_back(graph_display_text(detail, width - 16.0F));
            node_details.push_back(detail);
        }

        auto positions{automatic_graph_positions(types, node_sizes)};
        for (std::size_t index{}; index < types.size(); ++index) {
            if (auto const manual{graph_node_positions_.find(types[index].identity)};
                manual != graph_node_positions_.end()) {
                positions[index] = {manual->second[0], manual->second[1]};
            }
        }

        if (graph_fit_all_) {
            if (!positions.empty()) {
                auto minimum{positions.front()};
                auto maximum{add(positions.front(), node_sizes.front())};
                for (std::size_t index{}; index < positions.size(); ++index) {
                    auto const position{positions[index]};
                    minimum.x = std::min(minimum.x, position.x);
                    minimum.y = std::min(minimum.y, position.y);
                    maximum.x = std::max(maximum.x, position.x + node_sizes[index].x);
                    maximum.y = std::max(maximum.y, position.y + node_sizes[index].y);
                }
                auto const extent{subtract(maximum, minimum)};
                constexpr float margin{32.0F};
                auto const horizontal_zoom{
                    extent.x > 0.0F ? (canvas_size.x - margin * 2.0F) / extent.x : 2.0F};
                auto const vertical_zoom{
                    extent.y > 0.0F ? (canvas_size.y - margin * 2.0F) / extent.y : 2.0F};
                graph_zoom_ = std::clamp(std::min(horizontal_zoom, vertical_zoom), 0.1F, 2.0F);
                graph_pan_x_ =
                    (canvas_size.x - extent.x * graph_zoom_) * 0.5F - minimum.x * graph_zoom_;
                graph_pan_y_ =
                    (canvas_size.y - extent.y * graph_zoom_) * 0.5F - minimum.y * graph_zoom_;
            }
            graph_fit_all_ = false;
        }

        if (graph_focus_selected_ && graph_selection.has_value() &&
            graph_selection->value < positions.size()) {
            auto const center{add(positions[graph_selection->value],
                                  multiply(node_sizes[graph_selection->value], 0.5F))};
            graph_pan_x_ = canvas_size.x * 0.5F - center.x * graph_zoom_;
            graph_pan_y_ = canvas_size.y * 0.5F - center.y * graph_zoom_;
            graph_focus_selected_ = false;
        }

        auto const screen_position{[&](ImVec2 const world) {
            return ImVec2{canvas_position.x + graph_pan_x_ + world.x * graph_zoom_,
                          canvas_position.y + graph_pan_y_ + world.y * graph_zoom_};
        }};
        auto mouse_over_node{false};
        for (std::size_t index{}; index < positions.size(); ++index) {
            auto const minimum{screen_position(positions[index])};
            auto const maximum{add(minimum, multiply(node_sizes[index], graph_zoom_))};
            mouse_over_node |= io.MousePos.x >= minimum.x && io.MousePos.x <= maximum.x &&
                               io.MousePos.y >= minimum.y && io.MousePos.y <= maximum.y;
        }
        auto edge_hover_claimed{false};
        auto const neighborhood_active{graph_selection.has_value() &&
                                       graph_selection->value < types.size()};
        std::vector<bool> dependency_nodes(types.size());
        std::vector<bool> user_nodes(types.size());
        if (neighborhood_active) {
            for (auto const dependency :
                 analysis_session_.inputs.workspace.types().dependencies_of(*graph_selection)) {
                if (dependency.value < dependency_nodes.size()) {
                    dependency_nodes[dependency.value] = true;
                }
            }
            for (auto const user :
                 analysis_session_.inputs.workspace.types().users_of(*graph_selection)) {
                if (user.value < user_nodes.size()) {
                    user_nodes[user.value] = true;
                }
            }
        }
        for (std::size_t user_index{}; user_index < types.size(); ++user_index) {
            auto const user{TypeId{static_cast<std::uint32_t>(user_index)}};
            for (auto const dependency : types[user_index].dependencies) {
                if (dependency.value >= positions.size()) {
                    continue;
                }
                auto const& user_size{node_sizes[user_index]};
                auto const& dependency_size{node_sizes[dependency.value]};
                auto const user_center{add(positions[user_index], multiply(user_size, 0.5F))};
                auto const dependency_center{
                    add(positions[dependency.value], multiply(dependency_size, 0.5F))};
                auto start{user_center};
                auto end{dependency_center};
                if (std::abs(end.x - start.x) >= std::abs(end.y - start.y)) {
                    auto const direction{end.x >= start.x ? 1.0F : -1.0F};
                    start.x += direction * user_size.x * 0.5F;
                    end.x -= direction * dependency_size.x * 0.5F;
                } else {
                    auto const direction{end.y >= start.y ? 1.0F : -1.0F};
                    start.y += direction * user_size.y * 0.5F;
                    end.y -= direction * dependency_size.y * 0.5F;
                }
                auto const screen_start{screen_position(start)};
                auto const screen_end{screen_position(end)};
                auto const selected_is_user{neighborhood_active &&
                                            graph_selection->value == user_index};
                auto const selected_is_dependency{neighborhood_active &&
                                                  *graph_selection == dependency};
                auto const edge_color{selected_is_user && selected_is_dependency
                                          ? IM_COL32(184, 126, 232, 255)
                                      : selected_is_user       ? IM_COL32(91, 202, 161, 255)
                                      : selected_is_dependency ? IM_COL32(232, 151, 78, 255)
                                      : neighborhood_active    ? IM_COL32(94, 101, 113, 85)
                                                               : IM_COL32(145, 158, 177, 180)};
                draw_list->AddLine(screen_start, screen_end, edge_color, 1.5F);

                auto const delta{subtract(screen_end, screen_start)};
                auto const length{std::sqrt(delta.x * delta.x + delta.y * delta.y)};
                if (length > 0.0F) {
                    auto const unit{ImVec2{delta.x / length, delta.y / length}};
                    auto const normal{ImVec2{-unit.y, unit.x}};
                    auto const arrow_length{8.0F};
                    auto const arrow_width{4.0F};
                    auto const arrow_base{subtract(screen_end, multiply(unit, arrow_length))};
                    draw_list->AddTriangleFilled(
                        screen_end,
                        add(arrow_base, multiply(normal, arrow_width)),
                        subtract(arrow_base, multiply(normal, arrow_width)),
                        edge_color);
                }
                auto const label{
                    edge_label(analysis_session_.inputs.workspace.types(), user, dependency)};
                auto const midpoint{multiply(add(screen_start, screen_end), 0.5F)};
                auto const label_position{add(midpoint, multiply({4.0F, -14.0F}, graph_zoom_))};
                auto const unscaled_label_size{ImGui::CalcTextSize(label.c_str())};
                auto const label_size{multiply(unscaled_label_size, graph_zoom_)};
                auto const label_padding{multiply({4.0F, 2.0F}, graph_zoom_)};
                auto const label_minimum{subtract(label_position, label_padding)};
                auto const label_maximum{add(add(label_position, label_size), label_padding)};
                auto const label_hovered{
                    hovered && !panning && !mouse_over_node && !edge_hover_claimed &&
                    io.MousePos.x >= label_minimum.x && io.MousePos.x <= label_maximum.x &&
                    io.MousePos.y >= label_minimum.y && io.MousePos.y <= label_maximum.y};
                draw_list->AddRectFilled(label_minimum,
                                         label_maximum,
                                         label_hovered ? IM_COL32(54, 65, 82, 245)
                                         : neighborhood_active && !selected_is_user &&
                                                 !selected_is_dependency
                                             ? IM_COL32(31, 36, 44, 90)
                                             : IM_COL32(31, 36, 44, 210),
                                         3.0F * graph_zoom_);
                draw_list->AddText(ImGui::GetFont(),
                                   ImGui::GetFontSize() * graph_zoom_,
                                   label_position,
                                   label_hovered ? IM_COL32(225, 235, 249, 255)
                                   : neighborhood_active && !selected_is_user &&
                                           !selected_is_dependency
                                       ? IM_COL32(145, 151, 161, 90)
                                       : edge_color,
                                   label.c_str());
                if (label_hovered) {
                    edge_hover_claimed = true;
                    ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                    ImGui::BeginTooltip();
                    ImGui::Text("%s -> %s",
                                types[user_index].identity.name.c_str(),
                                types[dependency.value].identity.name.c_str());
                    ImGui::TextDisabled("%s", label.c_str());
                    ImGui::Separator();
                    ImGui::TextUnformatted("Click to navigate to the target type.");
                    ImGui::EndTooltip();
                    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                        select_type(dependency);
                    }
                }
            }
        }

        for (std::size_t index{}; index < types.size(); ++index) {
            auto const id{TypeId{static_cast<std::uint32_t>(index)}};
            auto minimum{screen_position(positions[index])};
            auto maximum{add(minimum, multiply(node_sizes[index], graph_zoom_))};
            ImGui::SetCursorScreenPos(minimum);
            ImGui::PushID(static_cast<int>(index));
            if (ImGui::InvisibleButton("node", subtract(maximum, minimum))) {
                select_type(id);
            }
            auto const dependency{dependency_nodes[index]};
            auto const user{user_nodes[index]};
            if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 2.0F)) {
                positions[index].x += io.MouseDelta.x / graph_zoom_;
                positions[index].y += io.MouseDelta.y / graph_zoom_;
                graph_node_positions_[types[index].identity] = {positions[index].x,
                                                                positions[index].y};
                if (!project_path_.empty()) {
                    persisted_graph_node_positions_[project_path_.lexically_normal()
                                                        .generic_string()][types[index].identity] =
                        {positions[index].x, positions[index].y};
                    ImGui::MarkIniSettingsDirty();
                }
                minimum = screen_position(positions[index]);
                maximum = add(minimum, multiply(node_sizes[index], graph_zoom_));
                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
            } else if (!panning && ImGui::IsItemHovered()) {
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                ImGui::SetItemTooltip(
                    "%s\n%s", types[index].identity.name.c_str(), node_details[index].c_str());
            }
            auto const selected{graph_selection == id};
            draw_list->AddRectFilled(
                minimum,
                maximum,
                node_color(
                    types[index].definition, selected, dependency, user, neighborhood_active),
                6.0F * graph_zoom_);
            draw_list->AddRect(minimum,
                               maximum,
                               selected ? IM_COL32(185, 220, 255, 255)
                               : neighborhood_active && !dependency && !user
                                   ? IM_COL32(92, 99, 109, 130)
                                   : IM_COL32(145, 162, 178, 255),
                               6.0F * graph_zoom_,
                               0,
                               selected ? 2.5F : 1.0F);
            auto const title_position{add(minimum, multiply({8.0F, 7.0F}, graph_zoom_))};
            draw_list->AddText(ImGui::GetFont(),
                               ImGui::GetFontSize() * graph_zoom_,
                               title_position,
                               neighborhood_active && !selected && !dependency && !user
                                   ? IM_COL32(178, 183, 191, 130)
                                   : IM_COL32(245, 247, 250, 255),
                               visible_names[index].c_str());
            draw_list->AddText(ImGui::GetFont(),
                               ImGui::GetFontSize() * graph_zoom_,
                               add(title_position, {0.0F, (font_size + 4.0F) * graph_zoom_}),
                               neighborhood_active && !selected && !dependency && !user
                                   ? IM_COL32(150, 156, 165, 110)
                                   : IM_COL32(205, 211, 220, 255),
                               visible_details[index].c_str());
            ImGui::PopID();
        }
    }
    ImGui::EndChild();
    ImGui::End();
}

} // namespace ioj::layout_planner
