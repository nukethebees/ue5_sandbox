#include "planner_ui.hpp"

#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace ioj::layout_planner {
namespace {

using namespace lispb::schema;

auto graph_column(TypeDefinition const& definition) -> std::size_t {
    if (std::holds_alternative<ExternalType>(definition)) {
        return 0;
    }
    if (std::holds_alternative<EnumType>(definition)) {
        return 1;
    }
    if (std::holds_alternative<PackedType>(definition)) {
        return 2;
    }
    if (std::holds_alternative<RecordType>(definition)) {
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
    if (std::holds_alternative<PackedType>(definition)) {
        return "packed";
    }
    if (std::holds_alternative<RecordType>(definition)) {
        return "record";
    }
    return "SoA";
}

auto node_color(TypeDefinition const& definition, bool const selected) -> ImU32 {
    if (selected) {
        return IM_COL32(64, 126, 220, 255);
    }
    if (std::holds_alternative<EnumType>(definition)) {
        return IM_COL32(80, 105, 155, 255);
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

auto edge_label(TypeGraph const& types, TypeId const user, TypeId const dependency) -> std::string {
    auto const& definition{types.type(user).definition};
    std::string result;
    if (auto const* enumeration{std::get_if<EnumType>(&definition)}) {
        if (enumeration->underlying_type.type == dependency) {
            result = "underlying";
        }
    } else if (auto const* packed{std::get_if<PackedType>(&definition)}) {
        if (packed->storage_type.type == dependency) {
            append_edge_label(result, "storage");
        }
        for (auto const& field : packed->fields) {
            if (field.semantic_type.type == dependency) {
                append_edge_label(result, field.name);
            }
        }
    } else if (auto const* soa{std::get_if<SoaType>(&definition)}) {
        for (auto const& column : soa->columns) {
            if (column.semantic_type.type == dependency) {
                append_edge_label(result, column.name);
            }
            if (column.nested_type == dependency) {
                append_edge_label(result, column.name + " nested");
            }
        }
    } else if (auto const* record{std::get_if<RecordType>(&definition)}) {
        for (auto const& member : record->members) {
            if (member.semantic_type.type == dependency) {
                append_edge_label(result, member.name);
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

} // namespace

void PlannerUi::draw_graph_panel() {
    if (!graph_view_open_) {
        return;
    }

    auto const open_before{graph_view_open_};
    if (!ImGui::Begin("Graph", &graph_view_open_)) {
        ImGui::End();
        if (open_before != graph_view_open_) {
            ImGui::MarkIniSettingsDirty();
        }
        return;
    }
    if (open_before != graph_view_open_) {
        ImGui::MarkIniSettingsDirty();
    }

    if (ImGui::Button("Reset view")) {
        graph_pan_x_ = 32.0F;
        graph_pan_y_ = 32.0F;
        graph_zoom_ = 1.0F;
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!selected_type_.has_value());
    if (ImGui::Button("Focus selection")) {
        graph_focus_selected_ = true;
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::TextDisabled("Middle-drag to pan, wheel to zoom (%.0f%%)", graph_zoom_ * 100.0F);

    constexpr auto child_flags{ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse};
    if (ImGui::BeginChild("relationship-graph-canvas", {0.0F, 0.0F}, true, child_flags)) {
        auto const canvas_position{ImGui::GetCursorScreenPos()};
        auto const canvas_size{ImGui::GetContentRegionAvail()};
        auto* const draw_list{ImGui::GetWindowDrawList()};
        draw_list->AddRectFilled(
            canvas_position, add(canvas_position, canvas_size), IM_COL32(24, 27, 32, 255));

        auto const hovered{ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)};
        auto const& io{ImGui::GetIO()};
        if (hovered && ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.0F)) {
            graph_pan_x_ += io.MouseDelta.x;
            graph_pan_y_ += io.MouseDelta.y;
        }
        if (hovered && io.MouseWheel != 0.0F) {
            auto const old_zoom{graph_zoom_};
            auto const new_zoom{std::clamp(old_zoom * std::pow(1.15F, io.MouseWheel), 0.35F, 2.0F)};
            auto const mouse_in_canvas{subtract(io.MousePos, canvas_position)};
            auto const world_under_mouse{ImVec2{(mouse_in_canvas.x - graph_pan_x_) / old_zoom,
                                                (mouse_in_canvas.y - graph_pan_y_) / old_zoom}};
            graph_pan_x_ = mouse_in_canvas.x - world_under_mouse.x * new_zoom;
            graph_pan_y_ = mouse_in_canvas.y - world_under_mouse.y * new_zoom;
            graph_zoom_ = new_zoom;
        }

        auto const types{workspace_.types().types()};
        constexpr ImVec2 node_size{190.0F, 58.0F};
        constexpr float column_spacing{260.0F};
        constexpr float row_spacing{92.0F};
        std::vector<ImVec2> positions(types.size());
        std::array<std::size_t, 5> rows{};
        for (std::size_t index{}; index < types.size(); ++index) {
            auto const column{graph_column(types[index].definition)};
            positions[index] = {32.0F + static_cast<float>(column) * column_spacing,
                                36.0F + static_cast<float>(rows[column]++) * row_spacing};
        }

        if (graph_focus_selected_ && selected_type_.has_value() &&
            selected_type_->value < positions.size()) {
            auto const center{add(positions[selected_type_->value], multiply(node_size, 0.5F))};
            graph_pan_x_ = canvas_size.x * 0.5F - center.x * graph_zoom_;
            graph_pan_y_ = canvas_size.y * 0.5F - center.y * graph_zoom_;
            graph_focus_selected_ = false;
        }

        auto const screen_position{[&](ImVec2 const world) {
            return ImVec2{canvas_position.x + graph_pan_x_ + world.x * graph_zoom_,
                          canvas_position.y + graph_pan_y_ + world.y * graph_zoom_};
        }};
        for (std::size_t user_index{}; user_index < types.size(); ++user_index) {
            auto const user{TypeId{static_cast<std::uint32_t>(user_index)}};
            for (auto const dependency : types[user_index].dependencies) {
                if (dependency.value >= positions.size()) {
                    continue;
                }
                auto const user_center{add(positions[user_index], multiply(node_size, 0.5F))};
                auto const dependency_center{
                    add(positions[dependency.value], multiply(node_size, 0.5F))};
                auto start{user_center};
                auto end{dependency_center};
                if (std::abs(end.x - start.x) >= std::abs(end.y - start.y)) {
                    auto const direction{end.x >= start.x ? 1.0F : -1.0F};
                    start.x += direction * node_size.x * 0.5F;
                    end.x -= direction * node_size.x * 0.5F;
                } else {
                    auto const direction{end.y >= start.y ? 1.0F : -1.0F};
                    start.y += direction * node_size.y * 0.5F;
                    end.y -= direction * node_size.y * 0.5F;
                }
                auto const screen_start{screen_position(start)};
                auto const screen_end{screen_position(end)};
                auto const edge_color{IM_COL32(145, 158, 177, 180)};
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
                auto const label{edge_label(workspace_.types(), user, dependency)};
                auto const midpoint{multiply(add(screen_start, screen_end), 0.5F)};
                draw_list->AddText(ImGui::GetFont(),
                                   ImGui::GetFontSize() * graph_zoom_,
                                   add(midpoint, multiply({4.0F, -14.0F}, graph_zoom_)),
                                   IM_COL32(185, 194, 208, 220),
                                   label.c_str());
            }
        }

        for (std::size_t index{}; index < types.size(); ++index) {
            auto const id{TypeId{static_cast<std::uint32_t>(index)}};
            auto const minimum{screen_position(positions[index])};
            auto const maximum{add(minimum, multiply(node_size, graph_zoom_))};
            ImGui::SetCursorScreenPos(minimum);
            ImGui::PushID(static_cast<int>(index));
            if (ImGui::InvisibleButton("node", subtract(maximum, minimum))) {
                selected_type_ = id;
                selected_field_.clear();
            }
            auto const selected{selected_type_ == id};
            draw_list->AddRectFilled(minimum,
                                     maximum,
                                     node_color(types[index].definition, selected),
                                     6.0F * graph_zoom_);
            draw_list->AddRect(minimum,
                               maximum,
                               selected ? IM_COL32(185, 220, 255, 255)
                                        : IM_COL32(125, 137, 153, 255),
                               6.0F * graph_zoom_,
                               0,
                               selected ? 2.5F : 1.0F);
            auto const title_position{add(minimum, multiply({8.0F, 7.0F}, graph_zoom_))};
            draw_list->AddText(ImGui::GetFont(),
                               ImGui::GetFontSize() * graph_zoom_,
                               title_position,
                               IM_COL32(245, 247, 250, 255),
                               types[index].identity.name.c_str());
            auto const detail{std::string{graph_kind(types[index].definition)} + " · " +
                              (types[index].identity.module_name.empty()
                                   ? std::string{"external"}
                                   : types[index].identity.module_name)};
            draw_list->AddText(ImGui::GetFont(),
                               ImGui::GetFontSize() * graph_zoom_,
                               add(title_position, {0.0F, 22.0F * graph_zoom_}),
                               IM_COL32(205, 211, 220, 255),
                               detail.c_str());
            ImGui::PopID();
        }
    }
    ImGui::EndChild();
    ImGui::End();
}

} // namespace ioj::layout_planner
