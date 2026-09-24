#include "planner_ui_support.hpp"

#include <imgui_internal.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdio>
#include <ranges>

namespace ioj::layout_planner::detail {

void pane_section_menu() {
    auto& storage{ImGui::GetCurrentWindow()->RootWindow->StateStorage};
    if (!ImGui::BeginMenuBar()) {
        return;
    }

    auto const width{ImGui::CalcTextSize("...").x + 2.0F * ImGui::GetStyle().FramePadding.x};
    auto position{ImGui::GetCursorScreenPos()};
    position.x = std::max(position.x,
                          ImGui::GetWindowPos().x + ImGui::GetWindowWidth() -
                              ImGui::GetStyle().WindowPadding.x - width);
    ImGui::SetCursorScreenPos(position);
    if (ImGui::SmallButton("...##section-actions")) {
        ImGui::OpenPopup("section-actions");
    }
    ImGui::SetItemTooltip("Section actions");
    if (ImGui::BeginPopup("section-actions")) {
        auto open{std::optional<bool>{}};
        if (ImGui::MenuItem("Collapse all sections")) {
            open = false;
        }
        if (ImGui::MenuItem("Expand all sections")) {
            open = true;
        }
        if (open.has_value()) {
            auto const revision_key{ImHashStr("planner-section-action-revision")};
            storage.SetInt(revision_key, storage.GetInt(revision_key) + 1);
            storage.SetBool(ImHashStr("planner-section-action-open"), *open);
        }
        ImGui::EndPopup();
    }
    ImGui::EndMenuBar();
}

auto section(char const* label, bool const default_open) -> bool {
    auto const* window{ImGui::GetCurrentWindow()};
    if (window->SkipItems) {
        return false;
    }

    auto const& pane_storage{window->RootWindow->StateStorage};
    auto const revision{pane_storage.GetInt(ImHashStr("planner-section-action-revision"))};
    auto* storage{ImGui::GetStateStorage()};
    ImGui::PushID(label);
    auto const applied_key{ImGui::GetID("section-action-applied")};
    ImGui::PopID();

    // Hidden nested sections apply the last pane action when they are next submitted.
    if (storage->GetInt(applied_key) != revision) {
        ImGui::SetNextItemOpen(pane_storage.GetBool(ImHashStr("planner-section-action-open")));
        storage->SetInt(applied_key, revision);
    }
    return ImGui::CollapsingHeader(
        label, default_open ? ImGuiTreeNodeFlags_DefaultOpen : ImGuiTreeNodeFlags_None);
}

WrappingButtonRow::WrappingButtonRow(bool const follow_previous_item) {
    available_width_ = std::max(1.0F, ImGui::GetContentRegionAvail().x);
    right_edge_ = ImGui::GetCursorScreenPos().x + available_width_;
    if (follow_previous_item) {
        last_right_ = ImGui::GetItemRectMax().x;
        has_previous_ = true;
    }
}

auto WrappingButtonRow::button(char const* label) -> bool {
    auto const natural_width{ImGui::CalcTextSize(label).x +
                             2.0F * ImGui::GetStyle().FramePadding.x};
    auto const width{std::min(natural_width, available_width_)};
    if (has_previous_ && last_right_ + ImGui::GetStyle().ItemSpacing.x + width <= right_edge_) {
        ImGui::SameLine();
    }
    auto const visible_width{std::min(width, std::max(1.0F, ImGui::GetContentRegionAvail().x))};
    auto const clicked{ImGui::Button(label, {visible_width, 0.0F})};
    last_right_ = ImGui::GetItemRectMax().x;
    has_previous_ = true;
    if (visible_width < natural_width &&
        ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled | ImGuiHoveredFlags_ForTooltip)) {
        ImGui::SetTooltip("%s", label);
    }
    return clicked;
}

auto begin_editable_table(char const* id, int const columns, std::size_t const rows) -> bool {
    auto const height{ImGui::GetFrameHeightWithSpacing() * static_cast<float>(rows + 1) +
                      ImGui::GetStyle().ScrollbarSize + 2.0F * ImGui::GetStyle().WindowPadding.y};
    if (!ImGui::BeginTable(id,
                           columns,
                           ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                               ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollX |
                               ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit,
                           {0.0F, height})) {
        return false;
    }
    ImGui::TableSetupScrollFreeze(0, 1);
    return true;
}

void editable_table_column(char const* label, ImGuiTableColumnFlags const flags) {
    auto const natural_width{ImGui::CalcTextSize(label).x +
                             2.0F * ImGui::GetStyle().FramePadding.x};
    auto const width{(flags & ImGuiTableColumnFlags_WidthFixed) != 0
                         ? std::max(32.0F, natural_width)
                         : std::max(std::string_view{label} == "Semantic type" ? 240.0F : 140.0F,
                                    natural_width)};
    ImGui::TableSetupColumn(label, flags | ImGuiTableColumnFlags_WidthFixed, width);
}

void editable_table_content_hint(std::string_view const text, float const trailing_width) {
    // A column-filling InputText otherwise makes separator double-click fit its current width.
    auto* table{ImGui::GetCurrentTable()};
    auto& column{table->Columns[table->CurrentColumn]};
    auto const width{ImGui::CalcTextSize(text.data(), text.data() + text.size()).x +
                     2.0F * ImGui::GetStyle().FramePadding.x + trailing_width};
    auto& content_max_x{table->IsUnfrozenRows ? column.ContentMaxXUnfrozen
                                              : column.ContentMaxXFrozen};
    content_max_x = std::max(content_max_x, column.WorkMinX + width);
    table->InnerWindow->DC.CursorMaxPos.x = column.WorkMinX + width;
}

auto prepare_editable_type_input() -> bool {
    auto const available_width{ImGui::GetContentRegionAvail().x};
    auto const controls_inline{available_width >= 140.0F};
    ImGui::SetNextItemWidth(controls_inline ? available_width - 80.0F : -1.0F);
    return controls_inline;
}

void prepare_property_input(char const* label) {
    auto const available{ImGui::GetContentRegionAvail().x};
    auto const font_size{ImGui::GetFontSize()};
    if (available >= 28.0F * font_size) {
        auto const left{ImGui::GetCursorPosX()};
        auto const label_width{std::min(18.0F * font_size, available * 0.42F)};
        ImGui::AlignTextToFramePadding();
        ImGui::PushTextWrapPos(left + label_width);
        ImGui::TextUnformatted(label);
        ImGui::PopTextWrapPos();
        ImGui::SameLine(left + label_width + ImGui::GetStyle().ItemInnerSpacing.x);
    } else {
        ImGui::PushTextWrapPos(0.0F);
        ImGui::TextUnformatted(label);
        ImGui::PopTextWrapPos();
    }
    ImGui::SetNextItemWidth(-1.0F);
}

auto editable_table_row_handle(bool const selected) -> bool {
    auto const handle_clicked{ImGui::Selectable("::", selected)};
    ImGui::SetItemTooltip("Select or drag to reorder.");
    if (handle_clicked || selected || !ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        return handle_clicked;
    }
    auto const hovered_column{ImGui::TableGetHoveredColumn()};
    if (hovered_column < 0 || hovered_column >= ImGui::TableGetColumnCount()) {
        return false;
    }
    auto const mouse_y{ImGui::GetIO().MousePos.y};
    auto const row_top{ImGui::GetItemRectMin().y};
    auto const row_bottom{std::max(ImGui::GetItemRectMax().y, row_top + ImGui::GetFrameHeight())};
    return mouse_y >= row_top && mouse_y < row_bottom;
}

auto semantic_type_navigation_button() -> bool {
    auto const clicked{ImGui::SmallButton(">")};
    ImGui::SetItemTooltip("Show the referenced semantic type in Properties.");
    return clicked;
}

auto format_bytes(std::optional<std::uint64_t> const bytes) -> std::string {
    if (!bytes.has_value()) {
        return "Unknown";
    }
    constexpr double kibibyte{1024.0};
    constexpr double mebibyte{1024.0 * 1024.0};
    constexpr double gibibyte{1024.0 * 1024.0 * 1024.0};
    std::array<char, 64> buffer{};
    auto const value{static_cast<double>(*bytes)};
    if (value >= gibibyte) {
        std::snprintf(buffer.data(), buffer.size(), "%.2f GiB", value / gibibyte);
    } else if (value >= mebibyte) {
        std::snprintf(buffer.data(), buffer.size(), "%.2f MiB", value / mebibyte);
    } else if (value >= kibibyte) {
        std::snprintf(buffer.data(), buffer.size(), "%.2f KiB", value / kibibyte);
    } else {
        std::snprintf(
            buffer.data(), buffer.size(), "%llu B", static_cast<unsigned long long>(*bytes));
    }
    return buffer.data();
}

auto format_number(std::optional<std::uint64_t> const value) -> std::string {
    return value.has_value() ? std::to_string(*value) : "Unknown";
}

auto format_code_count(layout::ExactCodeCount const value) -> std::string {
    return value.two_to_64 ? "18446744073709551616 (2^64)" : std::to_string(value.value);
}

auto format_fit(std::optional<bool> const fits) -> std::string {
    return fits.has_value() ? (*fits ? "Yes" : "No") : "Unknown";
}

auto access_operation_name(layout::AccessOperation const operation) -> char const* {
    switch (operation) {
        case layout::AccessOperation::read:
            return "R";
        case layout::AccessOperation::write:
            return "W";
        case layout::AccessOperation::read_write:
            return "RW";
    }
    return "Unknown";
}

auto access_operation_summary(std::span<layout::AccessIntent const> const accesses) -> char const* {
    if (accesses.empty()) {
        return "None";
    }
    auto const operation{accesses.front().operation};
    if (std::ranges::any_of(accesses, [&](layout::AccessIntent const& access) {
            return access.operation != operation;
        })) {
        return "Mixed per field";
    }
    return access_operation_name(operation);
}

auto format_delta(std::optional<layout::NumericDelta> const delta, auto const& format_magnitude)
    -> std::string {
    if (!delta.has_value()) {
        return "Unknown";
    }
    auto const prefix{delta->direction == layout::NumericDeltaDirection::increased   ? "+"
                      : delta->direction == layout::NumericDeltaDirection::decreased ? "-"
                                                                                     : ""};
    auto result{std::string{prefix} + format_magnitude(delta->magnitude)};
    if (delta->percentage.has_value()) {
        std::array<char, 32> percentage{};
        std::snprintf(percentage.data(), percentage.size(), " (%.1f%%)", *delta->percentage);
        result += percentage.data();
    }
    return result;
}

auto format_delta_bytes(std::optional<layout::NumericDelta> const delta) -> std::string {
    return format_delta(delta, [](std::uint64_t const magnitude) {
        return format_bytes(std::optional<std::uint64_t>{magnitude});
    });
}

auto format_delta_number(std::optional<layout::NumericDelta> const delta) -> std::string {
    return format_delta(delta,
                        [](std::uint64_t const magnitude) { return std::to_string(magnitude); });
}

auto relationship_extent_term(codegen::SemanticRelationKind const kind) -> std::string_view {
    return kind == codegen::SemanticRelationKind::offset_into ? "extent" : "capacity";
}

auto relationship_extent_unit(codegen::SemanticRelationKind const kind,
                              std::optional<codegen::SemanticRelationUnit> const unit)
    -> std::string_view {
    if (kind == codegen::SemanticRelationKind::offset_into && unit.has_value()) {
        return codegen::semantic_relation_unit_name(*unit);
    }
    return "elements";
}

auto parse_unsigned(std::string_view text) -> std::optional<std::uint64_t> {
    auto base{10};
    if (text.starts_with("0x") || text.starts_with("0X")) {
        base = 16;
        text.remove_prefix(2);
    }
    if (text.empty()) {
        return std::nullopt;
    }
    std::uint64_t value{};
    auto const [end, error]{std::from_chars(text.data(), text.data() + text.size(), value, base)};
    return error == std::errc{} && end == text.data() + text.size() ? std::optional{value}
                                                                    : std::nullopt;
}

auto parse_packed_integer(std::string_view text) -> std::optional<codegen::PackedIntegerValue> {
    auto negative{false};
    if (!text.empty() && (text.front() == '-' || text.front() == '+')) {
        negative = text.front() == '-';
        text.remove_prefix(1);
    }
    auto const magnitude{parse_unsigned(text)};
    return magnitude.has_value()
             ? std::optional{codegen::PackedIntegerValue::from_parts(negative, *magnitude)}
             : std::nullopt;
}

auto diagnostic_color(layout::DiagnosticSeverity const severity) -> ImVec4 {
    switch (severity) {
        case layout::DiagnosticSeverity::info:
            return {0.65F, 0.75F, 0.9F, 1.0F};
        case layout::DiagnosticSeverity::warning:
            return {0.95F, 0.72F, 0.25F, 1.0F};
        case layout::DiagnosticSeverity::error:
            return {0.95F, 0.35F, 0.3F, 1.0F};
    }
    return {1.0F, 1.0F, 1.0F, 1.0F};
}

auto packed_field(lispb::schema::PackedType const& packed, std::string const& name)
    -> lispb::schema::PackedField const* {
    for (auto const& segment : packed.segments) {
        if (auto const* field{std::get_if<lispb::schema::PackedField>(&segment)};
            field != nullptr && field->name == name) {
            return field;
        }
    }
    return nullptr;
}

auto soa_column(lispb::schema::SoaType const& soa, std::string const& name)
    -> lispb::schema::SoaColumn const* {
    auto const found{std::ranges::find(soa.columns, name, &lispb::schema::SoaColumn::name)};
    return found == soa.columns.end() ? nullptr : &*found;
}

auto has_error(std::vector<layout::Diagnostic> const& diagnostics) -> bool {
    return std::ranges::any_of(diagnostics, [](layout::Diagnostic const& diagnostic) {
        return diagnostic.severity == layout::DiagnosticSeverity::error;
    });
}

auto override_count(layout::VariantOverrides const& overrides) -> std::size_t {
    return overrides.packed_storage_types.size() + overrides.packed_field_widths.size() +
           overrides.soa_column_types.size() + overrides.capacities.size();
}

void draw_labeled_gap(ImDrawList* const draw_list,
                      ImVec2 const minimum,
                      ImVec2 const maximum,
                      std::string_view const label,
                      std::string_view const compact_label) {
    if (maximum.x <= minimum.x || maximum.y <= minimum.y) {
        return;
    }

    draw_list->AddRectFilled(
        minimum, maximum, ImGui::GetColorU32(ImVec4{0.34F, 0.38F, 0.43F, 1.0F}));
    draw_list->PushClipRect(minimum, maximum, true);
    auto const height{maximum.y - minimum.y};
    auto const visible_left{std::max(minimum.x, ImGui::GetCurrentWindow()->ClipRect.Min.x)};
    auto const visible_right{std::min(maximum.x, ImGui::GetCurrentWindow()->ClipRect.Max.x)};
    constexpr float hatch_spacing{9.0F};
    for (auto x{visible_left - height}; x < visible_right; x += hatch_spacing) {
        draw_list->AddLine({x, maximum.y},
                           {x + height, minimum.y},
                           ImGui::GetColorU32(ImVec4{0.82F, 0.85F, 0.89F, 0.35F}));
    }

    auto text{label};
    auto size{ImGui::CalcTextSize(text.data(), text.data() + text.size())};
    if (size.x + 8.0F > maximum.x - minimum.x) {
        text = compact_label;
        size = ImGui::CalcTextSize(text.data(), text.data() + text.size());
    }
    if (!text.empty() && size.x + 6.0F <= maximum.x - minimum.x) {
        draw_list->AddText({minimum.x + (maximum.x - minimum.x - size.x) * 0.5F,
                            minimum.y + (height - size.y) * 0.5F},
                           ImGui::GetColorU32(ImGuiCol_Text),
                           text.data(),
                           text.data() + text.size());
    }
    draw_list->PopClipRect();
    draw_list->AddRect(minimum, maximum, ImGui::GetColorU32(ImGuiCol_Border));
}

} // namespace ioj::layout_planner::detail
