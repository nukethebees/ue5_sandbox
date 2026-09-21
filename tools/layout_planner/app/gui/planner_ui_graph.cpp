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

    auto const types{workspace_.types().types()};

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
    ImGui::BeginDisabled(!selected_type_.has_value());
    if (ImGui::Button("Focus selection")) {
        graph_focus_selected_ = true;
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::TextDisabled("Left-drag nodes, middle-drag to pan, wheel to zoom (%.0f%%)",
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
        if (selected_type_.has_value()) {
            auto const current{
                std::find(search_matches.begin(), search_matches.end(), *selected_type_)};
            if (current != search_matches.end()) {
                match = std::next(current);
                if (match == search_matches.end()) {
                    match = search_matches.begin();
                }
            }
        }

        selected_type_ = *match;
        selected_field_.clear();
        packed_access_fields_.clear();
        packed_access_set_explicit_ = false;
        record_access_members_.clear();
        record_access_set_explicit_ = false;
        graph_focus_selected_ = true;
    }

    if (selected_type_.has_value() && selected_type_->value < types.size()) {
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
        if (hovered && ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.0F)) {
            graph_pan_x_ += io.MouseDelta.x;
            graph_pan_y_ += io.MouseDelta.y;
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

        constexpr ImVec2 node_size{190.0F, 58.0F};
        constexpr float column_spacing{260.0F};
        constexpr float row_spacing{92.0F};
        std::vector<ImVec2> positions(types.size());
        std::array<std::size_t, 5> rows{};
        for (std::size_t index{}; index < types.size(); ++index) {
            auto const column{graph_column(types[index].definition)};
            positions[index] = {32.0F + static_cast<float>(column) * column_spacing,
                                36.0F + static_cast<float>(rows[column]++) * row_spacing};
            if (auto const manual{graph_node_positions_.find(types[index].identity)};
                manual != graph_node_positions_.end()) {
                positions[index] = {manual->second[0], manual->second[1]};
            }
        }

        if (graph_fit_all_) {
            if (!positions.empty()) {
                auto minimum{positions.front()};
                auto maximum{add(positions.front(), node_size)};
                for (auto const position : positions) {
                    minimum.x = std::min(minimum.x, position.x);
                    minimum.y = std::min(minimum.y, position.y);
                    maximum.x = std::max(maximum.x, position.x + node_size.x);
                    maximum.y = std::max(maximum.y, position.y + node_size.y);
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
        auto const mouse_over_node{std::ranges::any_of(positions, [&](ImVec2 const position) {
            auto const minimum{screen_position(position)};
            auto const maximum{add(minimum, multiply(node_size, graph_zoom_))};
            return io.MousePos.x >= minimum.x && io.MousePos.x <= maximum.x &&
                   io.MousePos.y >= minimum.y && io.MousePos.y <= maximum.y;
        })};
        auto edge_hover_claimed{false};
        auto const neighborhood_active{selected_type_.has_value() &&
                                       selected_type_->value < types.size()};
        std::vector<bool> dependency_nodes(types.size());
        std::vector<bool> user_nodes(types.size());
        if (neighborhood_active) {
            for (auto const dependency : workspace_.types().dependencies_of(*selected_type_)) {
                if (dependency.value < dependency_nodes.size()) {
                    dependency_nodes[dependency.value] = true;
                }
            }
            for (auto const user : workspace_.types().users_of(*selected_type_)) {
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
                auto const selected_is_user{neighborhood_active &&
                                            selected_type_->value == user_index};
                auto const selected_is_dependency{neighborhood_active &&
                                                  *selected_type_ == dependency};
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
                auto const label{edge_label(workspace_.types(), user, dependency)};
                auto const midpoint{multiply(add(screen_start, screen_end), 0.5F)};
                auto const label_position{add(midpoint, multiply({4.0F, -14.0F}, graph_zoom_))};
                auto const unscaled_label_size{ImGui::CalcTextSize(label.c_str())};
                auto const label_size{multiply(unscaled_label_size, graph_zoom_)};
                auto const label_padding{multiply({4.0F, 2.0F}, graph_zoom_)};
                auto const label_minimum{subtract(label_position, label_padding)};
                auto const label_maximum{add(add(label_position, label_size), label_padding)};
                auto const label_hovered{
                    hovered && !mouse_over_node && !edge_hover_claimed &&
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
                        selected_type_ = dependency;
                        selected_field_.clear();
                        packed_access_fields_.clear();
                        packed_access_set_explicit_ = false;
                        record_access_members_.clear();
                        record_access_set_explicit_ = false;
                    }
                }
            }
        }

        for (std::size_t index{}; index < types.size(); ++index) {
            auto const id{TypeId{static_cast<std::uint32_t>(index)}};
            auto minimum{screen_position(positions[index])};
            auto maximum{add(minimum, multiply(node_size, graph_zoom_))};
            ImGui::SetCursorScreenPos(minimum);
            ImGui::PushID(static_cast<int>(index));
            if (ImGui::InvisibleButton("node", subtract(maximum, minimum))) {
                selected_type_ = id;
                selected_field_.clear();
                packed_access_fields_.clear();
                packed_access_set_explicit_ = false;
                record_access_members_.clear();
                record_access_set_explicit_ = false;
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
                maximum = add(minimum, multiply(node_size, graph_zoom_));
                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
            } else if (ImGui::IsItemHovered()) {
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            }
            auto const selected{selected_type_ == id};
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
                               types[index].identity.name.c_str());
            auto const detail{std::string{graph_kind(types[index].definition)} + " · " +
                              (types[index].identity.module_name.empty()
                                   ? std::string{"external"}
                                   : types[index].identity.module_name)};
            draw_list->AddText(ImGui::GetFont(),
                               ImGui::GetFontSize() * graph_zoom_,
                               add(title_position, {0.0F, 22.0F * graph_zoom_}),
                               neighborhood_active && !selected && !dependency && !user
                                   ? IM_COL32(150, 156, 165, 110)
                                   : IM_COL32(205, 211, 220, 255),
                               detail.c_str());
            ImGui::PopID();
        }
    }
    ImGui::EndChild();
    ImGui::End();
}

} // namespace ioj::layout_planner
