#include "planner_ui.hpp"
#include "planner_ui_support.hpp"

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

} // namespace

void PlannerUi::draw_graph_panel() {
    if (!graph_view_open_) {
        return;
    }
    auto const open_before{graph_view_open_};
    auto const visible{ImGui::Begin("Graph", &graph_view_open_)};
    if (open_before != graph_view_open_) {
        ImGui::MarkIniSettingsDirty();
    }
    if (!visible) {
        ImGui::End();
        return;
    }
    auto const& workspace{analysis_session_.inputs.workspace};
    auto const& graph{workspace.types()};
    auto selection{analysis_session_.inputs.selection.type};
    if (!selection && analysis_session_.inputs.selection.identity()) {
        auto const generated{
            graph.types_for_declaration(*analysis_session_.inputs.selection.identity())};
        if (!generated.empty()) {
            selection = generated.front();
        }
    }
    std::set<std::string> modules;
    for (auto const& node : graph.types()) {
        if (!node.identity.module_name.empty()) {
            modules.insert(node.identity.module_name);
        }
    }
    ImGui::SetNextItemWidth(240.0F);
    if (ImGui::BeginCombo("Scope", graph_scope_.module.value_or("Entire project").c_str())) {
        if (ImGui::Selectable("Entire project", !graph_scope_.module)) {
            graph_scope_ = {};
        }
        for (auto const& module : modules) {
            if (ImGui::Selectable(module.c_str(), graph_scope_.module == module)) {
                graph_scope_.module = module;
            }
        }
        ImGui::EndCombo();
    }
    detail::WrappingButtonRow actions;
    if (actions.button("View selected module") && selection) {
        auto const& module{graph.type(*selection).identity.module_name};
        if (!module.empty()) {
            graph_scope_.module = module;
        }
    }
    if (actions.button("Fit all")) {
        graph_fit_all_ = true;
    }
    if (actions.button("Automatic layout")) {
        if (graph_scope_.module) {
            module_graph_positions_[*graph_scope_.module].clear();
        } else {
            graph_node_positions_.clear();
            persisted_graph_node_positions_.erase(
                project_path_.lexically_normal().generic_string());
            ImGui::MarkIniSettingsDirty();
        }
        graph_cache_revision_ = 0;
        graph_fit_all_ = true;
    }
    if (actions.button("Focus selection") && selection) {
        graph_focus_ = graph.type(*selection).identity;
        graph_focus_selected_ = true;
    }
    if (actions.button("Clear focus")) {
        graph_focus_.reset();
    }
    if (actions.button("Reset view")) {
        graph_zoom_ = 1.0F;
        graph_pan_x_ = 32.0F;
        graph_pan_y_ = 32.0F;
    }
    ImGui::SetNextItemWidth(280.0F);
    auto const search{ImGui::InputTextWithHint("##graph-search",
                                               "Find type or module",
                                               graph_search_.data(),
                                               graph_search_.size(),
                                               ImGuiInputTextFlags_EnterReturnsTrue)};
    ImGui::SameLine();
    auto const next{ImGui::Button("Next match")};

    auto const font_size{ImGui::GetFontSize()};
    if (graph_cache_revision_ != workspace.graph_revision() ||
        cached_graph_scope_ != graph_scope_ || graph_cache_font_size_ != font_size) {
        graph_fit_all_ |= cached_graph_scope_ != graph_scope_;
        graph_projection_ = layout::project_graph(graph, graph_scope_);
        graph_sizes_.clear();
        graph_names_.clear();
        graph_details_.clear();
        for (auto const& occurrence : graph_projection_.nodes) {
            auto const& node{graph.type(occurrence.type)};
            auto const detail{occurrence.boundary ? "boundary: " + node.identity.module_name
                                                  : graph_detail(node)};
            auto const width{
                occurrence.boundary
                    ? std::max(150.0F, font_size * 9.0F)
                    : std::clamp(std::max(ImGui::CalcTextSize(node.identity.name.c_str()).x,
                                          ImGui::CalcTextSize(detail.c_str()).x) +
                                     16.0F,
                                 font_size * 10.0F,
                                 font_size * 18.0F)};
            graph_sizes_.push_back({width, font_size * 2.0F + 24.0F});
            graph_names_.push_back(graph_display_text(node.identity.name, width - 16.0F));
            graph_details_.push_back(graph_display_text(detail, width - 16.0F));
        }
        graph_positions_ = layout::layout_graph(graph_projection_, graph_sizes_);
        auto const& manual{graph_scope_.module ? module_graph_positions_[*graph_scope_.module]
                                               : graph_node_positions_};
        for (std::size_t index{}; index < graph_projection_.nodes.size(); ++index) {
            auto const found{manual.find(graph.type(graph_projection_.nodes[index].type).identity)};
            if (found != manual.end()) {
                graph_positions_[index] = found->second;
            }
        }
        graph_cache_revision_ = workspace.graph_revision();
        graph_cache_font_size_ = font_size;
        cached_graph_scope_ = graph_scope_;
    }
    auto const& nodes{graph_projection_.nodes};
    auto const count{nodes.size()};
    auto const query{lowercase(graph_search_.data())};
    if ((search || next) && !query.empty()) {
        std::vector<TypeId> matches;
        for (auto const& node : nodes) {
            if (graph_matches(graph.type(node.type), query)) {
                matches.push_back(node.type);
            }
        }
        if (!matches.empty()) {
            auto found{selection ? std::ranges::find(matches, *selection) : matches.end()};
            if (found == matches.end() || ++found == matches.end()) {
                found = matches.begin();
            }
            select_type(*found);
            selection = *found;
            graph_focus_ = graph.type(*found).identity;
            graph_focus_selected_ = true;
        }
    }
    auto const focus{graph_focus_ ? graph.find(*graph_focus_) : std::optional<TypeId>{}};
    auto const focused{
        focus && std::ranges::any_of(nodes, [&](auto const& node) { return node.type == *focus; })};
    ImGui::TextDisabled(
        "%zu nodes | %s | %.1f%% | drag nodes; right/middle-drag to pan; wheel to zoom",
        count,
        focused ? "Graph focus active (Escape clears)" : "All semantic colours",
        graph_zoom_ * 100.0F);
    if (ImGui::BeginChild("relationship-graph-canvas",
                          {},
                          true,
                          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
        auto const origin{ImGui::GetCursorScreenPos()};
        auto const canvas{ImGui::GetContentRegionAvail()};
        auto* draw{ImGui::GetWindowDrawList()};
        draw->AddRectFilled(origin, add(origin, canvas), IM_COL32(24, 27, 32, 255));
        auto const& io{ImGui::GetIO()};
        auto const hovered{ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)};
        auto const panning{hovered && (ImGui::IsMouseDragging(ImGuiMouseButton_Right) ||
                                       ImGui::IsMouseDragging(ImGuiMouseButton_Middle))};
        if (panning) {
            graph_pan_x_ += io.MouseDelta.x;
            graph_pan_y_ += io.MouseDelta.y;
        }
        if (hovered && io.MouseWheel != 0.0F) {
            auto const zoom{std::clamp(
                graph_zoom_ * std::pow(1.15F, io.MouseWheel), std::min(0.1F, graph_zoom_), 2.0F)};
            auto const mouse{subtract(io.MousePos, origin)};
            graph_pan_x_ = mouse.x - (mouse.x - graph_pan_x_) * zoom / graph_zoom_;
            graph_pan_y_ = mouse.y - (mouse.y - graph_pan_y_) * zoom / graph_zoom_;
            graph_zoom_ = zoom;
        }
        if (graph_fit_all_ && canvas.x > 0.0F && canvas.y > 0.0F) {
            auto const fit{layout::fit_graph(graph_positions_, graph_sizes_, {canvas.x, canvas.y})};
            graph_zoom_ = fit.zoom;
            graph_pan_x_ = fit.pan[0];
            graph_pan_y_ = fit.pan[1];
            graph_fit_all_ = false;
        }
        if (graph_focus_selected_ && selection) {
            for (std::size_t index{}; index < count; ++index) {
                if (nodes[index].type == *selection) {
                    graph_pan_x_ =
                        canvas.x * 0.5F -
                        (graph_positions_[index][0] + graph_sizes_[index][0] * 0.5F) * graph_zoom_;
                    graph_pan_y_ =
                        canvas.y * 0.5F -
                        (graph_positions_[index][1] + graph_sizes_[index][1] * 0.5F) * graph_zoom_;
                }
            }
            graph_focus_selected_ = false;
        }
        auto const screen{[&](layout::GraphPoint point) {
            return ImVec2{origin.x + graph_pan_x_ + point[0] * graph_zoom_,
                          origin.y + graph_pan_y_ + point[1] * graph_zoom_};
        }};
        auto const size{[&](std::size_t index) {
            return ImVec2{graph_sizes_[index][0] * graph_zoom_,
                          graph_sizes_[index][1] * graph_zoom_};
        }};
        bool over_node{};
        for (std::size_t index{}; index < count; ++index) {
            auto const min{screen(graph_positions_[index])};
            auto const max{add(min, size(index))};
            over_node |= ImGui::IsMouseHoveringRect(min, max);
        }
        std::vector<bool> dependencies(count), users(count);
        for (auto const& edge : graph_projection_.edges) {
            if (focused && nodes[edge.user].type == *focus) {
                dependencies[edge.dependency] = true;
            }
            if (focused && nodes[edge.dependency].type == *focus) {
                users[edge.user] = true;
            }
        }
        bool over_edge{};
        for (auto const& edge : graph_projection_.edges) {
            auto start{add(screen(graph_positions_[edge.user]), multiply(size(edge.user), 0.5F))};
            auto end{add(screen(graph_positions_[edge.dependency]),
                         multiply(size(edge.dependency), 0.5F))};
            auto const direction{end.x >= start.x ? 1.0F : -1.0F};
            start.x += direction * size(edge.user).x * 0.5F;
            end.x -= direction * size(edge.dependency).x * 0.5F;
            auto const relevant{!focused || nodes[edge.user].type == *focus ||
                                nodes[edge.dependency].type == *focus};
            auto const color{relevant ? IM_COL32(145, 180, 205, 210) : IM_COL32(94, 101, 113, 65)};
            draw->AddLine(start, end, color, 1.5F);
            auto const delta{subtract(end, start)};
            auto const length{std::sqrt(delta.x * delta.x + delta.y * delta.y)};
            if (length > 0.0F) {
                auto const unit{multiply(delta, 1.0F / length)};
                auto const base{subtract(end, multiply(unit, 8.0F))};
                auto const normal{ImVec2{-unit.y * 4.0F, unit.x * 4.0F}};
                draw->AddTriangleFilled(end, add(base, normal), subtract(base, normal), color);
            }
            auto const midpoint{multiply(add(start, end), 0.5F)};
            auto const label_size{multiply(ImGui::CalcTextSize(edge.summary.c_str()), graph_zoom_)};
            auto const label_position{subtract(midpoint, multiply(label_size, 0.5F))};
            auto const show_label{graph_zoom_ >= 0.65F && relevant};
            if (show_label) {
                draw->AddRectFilled(subtract(label_position, {3.0F, 2.0F}),
                                    add(add(label_position, label_size), {3.0F, 2.0F}),
                                    IM_COL32(31, 36, 44, 235));
                draw->AddText(ImGui::GetFont(),
                              font_size * graph_zoom_,
                              label_position,
                              color,
                              edge.summary.c_str());
            }
            auto const hit{
                hovered && !panning && !over_node && !over_edge &&
                (layout::graph_segment_hit(
                     {io.MousePos.x, io.MousePos.y}, {start.x, start.y}, {end.x, end.y}, 5.0F) ||
                 (show_label &&
                  ImGui::IsMouseHoveringRect(label_position, add(label_position, label_size))))};
            if (hit) {
                over_edge = true;
                ImGui::BeginTooltip();
                ImGui::PushTextWrapPos(ImGui::GetFontSize() * 32.0F);
                ImGui::TextWrapped("%s -> %s",
                                   graph.type(nodes[edge.user].type).identity.name.c_str(),
                                   graph.type(nodes[edge.dependency].type).identity.name.c_str());
                for (auto const& label : edge.labels) {
                    ImGui::TextWrapped("%s", label.c_str());
                }
                ImGui::TextUnformatted("Click to inspect target");
                ImGui::PopTextWrapPos();
                ImGui::EndTooltip();
                if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                    select_type(nodes[edge.dependency].type);
                }
            }
        }
        for (std::size_t index{}; index < count; ++index) {
            auto const& occurrence{nodes[index]};
            auto const& node{graph.type(occurrence.type)};
            auto min{screen(graph_positions_[index])};
            auto max{add(min, size(index))};
            ImGui::SetCursorScreenPos(min);
            ImGui::PushID(static_cast<int>(index));
            if (ImGui::InvisibleButton("node", size(index))) {
                select_type(occurrence.type);
                graph_focus_ = node.identity;
            }
            if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 2.0F)) {
                graph_positions_[index][0] += io.MouseDelta.x / graph_zoom_;
                graph_positions_[index][1] += io.MouseDelta.y / graph_zoom_;
                auto& manual{graph_scope_.module ? module_graph_positions_[*graph_scope_.module]
                                                 : graph_node_positions_};
                manual[node.identity] = graph_positions_[index];
                if (!graph_scope_.module && !project_path_.empty()) {
                    persisted_graph_node_positions_[project_path_.lexically_normal()
                                                        .generic_string()][node.identity] =
                        graph_positions_[index];
                    ImGui::MarkIniSettingsDirty();
                }
                min = screen(graph_positions_[index]);
                max = add(min, size(index));
            }
            if (ImGui::IsItemHovered() && !panning) {
                ImGui::SetItemTooltip("%s\n%s%s",
                                      node.cpp_spelling.c_str(),
                                      node.identity.module_name.c_str(),
                                      occurrence.boundary ? " (scope boundary)" : "");
            }
            auto const selected{selection == occurrence.type};
            auto const is_focus{focused && focus == occurrence.type};
            auto const dim{focused && !is_focus && !dependencies[index] && !users[index]};
            draw->AddRectFilled(
                min,
                max,
                node_color(node.definition, is_focus, dependencies[index], users[index], focused),
                6.0F * graph_zoom_);
            draw->AddRect(min,
                          max,
                          selected ? IM_COL32(220, 240, 255, 255) : IM_COL32(145, 162, 178, 180),
                          6.0F * graph_zoom_,
                          0,
                          selected ? 2.5F : 1.0F);
            auto const title{add(min, multiply({8.0F, 7.0F}, graph_zoom_))};
            draw->AddText(ImGui::GetFont(),
                          font_size * graph_zoom_,
                          title,
                          dim ? IM_COL32(178, 183, 191, 130) : IM_COL32(245, 247, 250, 255),
                          graph_names_[index].c_str());
            draw->AddText(ImGui::GetFont(),
                          font_size * graph_zoom_,
                          add(title, {0.0F, (font_size + 4.0F) * graph_zoom_}),
                          dim ? IM_COL32(150, 156, 165, 110) : IM_COL32(205, 211, 220, 255),
                          graph_details_[index].c_str());
            ImGui::PopID();
        }
        if ((hovered && !over_node && !over_edge && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) ||
            (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
             ImGui::IsKeyPressed(ImGuiKey_Escape))) {
            graph_focus_.reset();
        }
    }
    ImGui::EndChild();
    ImGui::End();
}

} // namespace ioj::layout_planner
