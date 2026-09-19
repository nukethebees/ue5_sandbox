#include "planner_ui.hpp"

#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string_view>
#include <variant>

namespace ioj::layout_planner {
namespace {

using namespace layout;

auto definition_id(LayoutDefinition const& definition) -> SchemaId const& {
    return std::visit([](auto const& value) -> SchemaId const& { return value.id; }, definition);
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

auto diagnostic_color(DiagnosticSeverity const severity) -> ImVec4 {
    switch (severity) {
        case DiagnosticSeverity::info:
            return {0.65F, 0.75F, 0.9F, 1.0F};
        case DiagnosticSeverity::warning:
            return {0.95F, 0.72F, 0.25F, 1.0F};
        case DiagnosticSeverity::error:
            return {0.95F, 0.35F, 0.3F, 1.0F};
    }
    return {1.0F, 1.0F, 1.0F, 1.0F};
}

auto packed_field(PackedLayout const& layout, std::string const& name) -> PackedField const* {
    auto const found{std::ranges::find(layout.fields, name, &PackedField::name)};
    return found == layout.fields.end() ? nullptr : &*found;
}

auto soa_column(SoaLayout const& layout, std::string const& name) -> SoaColumn const* {
    auto const found{std::ranges::find(layout.columns, name, &SoaColumn::name)};
    return found == layout.columns.end() ? nullptr : &*found;
}

} // namespace

PlannerUi::PlannerUi(CatalogLoadResult loaded)
    : workspace_{std::move(loaded.catalog)}
    , load_diagnostics_{std::move(loaded.diagnostics)} {
    auto const& items{workspace_.catalog().items()};
    if (!items.empty()) {
        selected_schema_ = definition_id(items.front());
    }
    sync_variant_name();
}

auto PlannerUi::draw() -> bool {
    auto const dockspace_id{ImGui::DockSpaceOverViewport()};
    setup_default_dock_layout(dockspace_id);
    refresh_analysis();

    auto const revision_before{workspace_.revision()};
    draw_project_panel();
    refresh_analysis();
    draw_layout_panel();
    draw_properties_panel();
    draw_variants_panel();
    draw_analysis_panel();
    return revision_before != workspace_.revision();
}

void PlannerUi::setup_default_dock_layout(unsigned int const dockspace_id) {
    auto const* existing_node{ImGui::DockBuilderGetNode(dockspace_id)};
    if (dock_layout_initialized_ || existing_node == nullptr || existing_node->IsSplitNode()) {
        dock_layout_initialized_ = true;
        return;
    }
    ImGui::DockBuilderRemoveNode(dockspace_id);
    ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspace_id, ImGui::GetMainViewport()->WorkSize);

    auto center_id{dockspace_id};
    auto const left_id{
        ImGui::DockBuilderSplitNode(center_id, ImGuiDir_Left, 0.20F, nullptr, &center_id)};
    auto const right_id{
        ImGui::DockBuilderSplitNode(center_id, ImGuiDir_Right, 0.25F, nullptr, &center_id)};
    auto const bottom_id{
        ImGui::DockBuilderSplitNode(center_id, ImGuiDir_Down, 0.30F, nullptr, &center_id)};
    ImGui::DockBuilderDockWindow("Project / Schema", left_id);
    ImGui::DockBuilderDockWindow("Layout", center_id);
    ImGui::DockBuilderDockWindow("Properties", right_id);
    ImGui::DockBuilderDockWindow("Variants", bottom_id);
    ImGui::DockBuilderDockWindow("Analysis", bottom_id);
    ImGui::DockBuilderFinish(dockspace_id);
    dock_layout_initialized_ = true;
}

void PlannerUi::refresh_analysis() {
    if (cached_revision_ == workspace_.revision() && cached_schema_ == selected_schema_) {
        return;
    }
    baseline_packed_.reset();
    active_packed_.reset();
    baseline_soa_.reset();
    active_soa_.reset();
    cached_revision_ = workspace_.revision();
    cached_schema_ = selected_schema_;
    if (!selected_schema_.has_value()) {
        return;
    }
    auto const* definition{workspace_.catalog().find(*selected_schema_)};
    if (definition == nullptr) {
        return;
    }
    auto const& baseline{*workspace_.variant(LayoutWorkspace::baseline_variant_id)};
    auto const& active{workspace_.active_variant()};
    if (auto const* packed{std::get_if<PackedLayout>(definition)}) {
        baseline_packed_ = Analyzer::analyze(*packed, baseline, abi_);
        active_packed_ = Analyzer::analyze(*packed, active, abi_);
    } else if (auto const* soa{std::get_if<SoaLayout>(definition)}) {
        baseline_soa_ = Analyzer::analyze(*soa, baseline, abi_, workspace_.default_capacity());
        active_soa_ = Analyzer::analyze(*soa, active, abi_, workspace_.default_capacity());
    }
}

void PlannerUi::draw_project_panel() {
    ImGui::Begin("Project / Schema");
    if (workspace_.catalog().items().empty()) {
        ImGui::TextDisabled("No supported schemas loaded.");
    }
    std::string_view current_module;
    for (auto const& definition : workspace_.catalog().items()) {
        auto const& id{definition_id(definition)};
        if (current_module != id.module_name) {
            current_module = id.module_name;
            ImGui::SeparatorText(id.module_name.c_str());
        }
        auto const prefix{id.kind == SchemaKind::packed_value ? "Packed" : "SoA"};
        auto const label{std::string{prefix} + "  " + id.schema_name};
        auto const selected{selected_schema_.has_value() && *selected_schema_ == id};
        ImGui::PushID(id.module_name.c_str());
        if (ImGui::Selectable(label.c_str(), selected)) {
            selected_schema_ = id;
            selected_field_.clear();
        }
        ImGui::PopID();
    }
    if (!load_diagnostics_.empty()) {
        ImGui::SeparatorText("Load diagnostics");
        draw_diagnostics(load_diagnostics_);
    }
    ImGui::End();
}

void PlannerUi::draw_layout_panel() {
    ImGui::Begin("Layout");
    if (!selected_schema_.has_value()) {
        ImGui::TextDisabled("Select a supported schema.");
        ImGui::End();
        return;
    }
    auto const* definition{workspace_.catalog().find(*selected_schema_)};
    if (definition == nullptr) {
        ImGui::TextDisabled("The selected schema is unavailable.");
    } else if (auto const* packed{std::get_if<PackedLayout>(definition)}) {
        draw_packed_layout(*packed, *active_packed_);
    } else if (auto const* soa{std::get_if<SoaLayout>(definition)}) {
        draw_soa_layout(*soa, *active_soa_);
    }
    ImGui::End();
}

void PlannerUi::draw_packed_layout(PackedLayout const&, PackedAnalysis const& analysis) {
    ImGui::Text("%s", analysis.id.schema_name.c_str());
    ImGui::TextDisabled("%s | %s", analysis.id.module_name.c_str(), analysis.storage_type.c_str());
    auto const available{std::max(1.0F, ImGui::GetContentRegionAvail().x)};
    auto const origin{ImGui::GetCursorScreenPos()};
    constexpr float height{76.0F};
    auto const denominator{static_cast<float>(std::max<std::uint64_t>(
        1, std::max(analysis.storage_bits.value_or(0), analysis.bits_used.value_or(0))))};
    auto* draw_list{ImGui::GetWindowDrawList()};
    auto x{origin.x};
    for (std::size_t index{}; index < analysis.fields.size(); ++index) {
        auto const& field{analysis.fields[index]};
        auto const width{available * static_cast<float>(field.bit_width) / denominator};
        auto const right{x + std::max(width, 2.0F)};
        auto const selected{selected_field_ == field.name};
        auto const color{
            ImGui::GetColorU32(selected ? ImVec4{0.25F, 0.55F, 0.85F, 1.0F}
                                        : ImVec4{0.18F + 0.06F * (index % 2), 0.36F, 0.52F, 1.0F})};
        draw_list->AddRectFilled({x, origin.y}, {right, origin.y + height}, color, 3.0F);
        draw_list->AddRect(
            {x, origin.y}, {right, origin.y + height}, ImGui::GetColorU32(ImGuiCol_Border));
        ImGui::SetCursorScreenPos({x, origin.y});
        ImGui::PushID(static_cast<int>(index));
        ImGui::InvisibleButton("field", {std::max(width, 2.0F), height});
        if (ImGui::IsItemClicked()) {
            selected_field_ = field.name;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::Text("%s", field.name.c_str());
            ImGui::Text("Width: %u bits", field.bit_width);
            if (field.most_significant_bit.has_value()) {
                ImGui::Text("Range: [%llu:%llu]",
                            static_cast<unsigned long long>(*field.most_significant_bit),
                            static_cast<unsigned long long>(field.least_significant_bit));
            }
            if (field.maximum_unsigned_value.has_value()) {
                ImGui::Text("Maximum raw value: %llu",
                            static_cast<unsigned long long>(*field.maximum_unsigned_value));
            }
            ImGui::EndTooltip();
        }
        ImGui::PopID();
        if (width > 70.0F) {
            draw_list->AddText(
                {x + 7.0F, origin.y + 9.0F}, ImGui::GetColorU32(ImGuiCol_Text), field.name.c_str());
            auto const bit_text{std::to_string(field.bit_width) + " bits"};
            draw_list->AddText({x + 7.0F, origin.y + 35.0F},
                               ImGui::GetColorU32(ImGuiCol_TextDisabled),
                               bit_text.c_str());
        }
        x = right;
    }
    ImGui::SetCursorScreenPos({origin.x, origin.y + height + ImGui::GetStyle().ItemSpacing.y});
}

void PlannerUi::draw_soa_layout(SoaLayout const&, SoaAnalysis const& analysis) {
    ImGui::Text("%s", analysis.id.schema_name.c_str());
    ImGui::TextDisabled("Capacity: %llu | Payload: %s",
                        static_cast<unsigned long long>(analysis.capacity),
                        format_bytes(analysis.total_payload_bytes).c_str());
    if (ImGui::BeginTable("soa-columns",
                          7,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                              ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Field");
        ImGui::TableSetupColumn("Type");
        ImGui::TableSetupColumn("Element bytes");
        ImGui::TableSetupColumn("Capacity");
        ImGui::TableSetupColumn("Payload");
        ImGui::TableSetupColumn("Min cache lines");
        ImGui::TableSetupColumn("Elements / 64 B");
        ImGui::TableHeadersRow();
        for (auto const& column : analysis.columns) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            auto const selected{selected_field_ == column.name};
            if (ImGui::Selectable(
                    column.name.c_str(), selected, ImGuiSelectableFlags_SpanAllColumns)) {
                selected_field_ = column.name;
            }
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(column.physical_type.c_str());
            ImGui::TableNextColumn();
            if (column.type_facts.has_value()) {
                ImGui::Text("%llu", static_cast<unsigned long long>(column.type_facts->size_bytes));
            } else {
                ImGui::TextDisabled("Unknown");
            }
            ImGui::TableNextColumn();
            ImGui::Text("%llu", static_cast<unsigned long long>(analysis.capacity));
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(format_bytes(column.total_bytes).c_str());
            ImGui::TableNextColumn();
            if (column.minimum_cache_lines.has_value()) {
                ImGui::Text("%llu", static_cast<unsigned long long>(*column.minimum_cache_lines));
            } else {
                ImGui::TextDisabled("Unknown");
            }
            ImGui::TableNextColumn();
            if (column.elements_per_cache_line.has_value()) {
                ImGui::Text("%llu",
                            static_cast<unsigned long long>(*column.elements_per_cache_line));
            } else {
                ImGui::TextDisabled("--");
            }
        }
        ImGui::EndTable();
    }

    auto maximum_bytes{std::uint64_t{1}};
    for (auto const& column : analysis.columns) {
        maximum_bytes = std::max(maximum_bytes, column.total_bytes.value_or(0));
    }
    ImGui::SeparatorText("Relative column payloads");
    if (ImGui::BeginTable("soa-relative-payloads", 2, ImGuiTableFlags_SizingFixedFit)) {
        ImGui::TableSetupColumn("Field", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Payload", ImGuiTableColumnFlags_WidthStretch);
        for (auto const& column : analysis.columns) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(column.name.c_str());
            ImGui::TableNextColumn();
            auto const fraction{static_cast<float>(column.total_bytes.value_or(0)) /
                                static_cast<float>(maximum_bytes)};
            ImGui::ProgressBar(fraction, {-1.0F, 0.0F}, format_bytes(column.total_bytes).c_str());
        }
        ImGui::EndTable();
    }
}

void PlannerUi::draw_properties_panel() {
    ImGui::Begin("Properties");
    if (!selected_schema_.has_value()) {
        ImGui::TextDisabled("No selection.");
        ImGui::End();
        return;
    }
    auto const* definition{workspace_.catalog().find(*selected_schema_)};
    auto const editable{workspace_.active_variant_id() != LayoutWorkspace::baseline_variant_id};
    if (!editable) {
        ImGui::TextWrapped(
            "Baseline is read-only. Create or select a variant to edit planning values.");
        ImGui::Separator();
    }
    ImGui::BeginDisabled(!editable);
    if (auto const* packed{std::get_if<PackedLayout>(definition)}) {
        ImGui::Text("Packed storage");
        auto const current_storage{active_packed_->storage_type};
        if (ImGui::BeginCombo("Physical type", current_storage.c_str())) {
            if (ImGui::Selectable("Schema default", current_storage == packed->storage_type)) {
                workspace_.set_packed_storage_type(packed->id, std::nullopt);
            }
            for (auto const& [type, facts] : abi_.types()) {
                if (!facts.unsigned_value_bits.has_value()) {
                    continue;
                }
                if (ImGui::Selectable(type.c_str(), current_storage == type)) {
                    workspace_.set_packed_storage_type(packed->id, type);
                }
            }
            ImGui::EndCombo();
        }
        if (selected_field_.empty() && !packed->fields.empty()) {
            selected_field_ = packed->fields.front().name;
        }
        if (auto const* field{packed_field(*packed, selected_field_)}) {
            ImGui::SeparatorText(field->name.c_str());
            auto const analyzed{
                std::ranges::find(active_packed_->fields, field->name, &PackedFieldAnalysis::name)};
            auto width{analyzed == active_packed_->fields.end() ? field->bit_width
                                                                : analyzed->bit_width};
            if (ImGui::InputScalar("Planning bit width", ImGuiDataType_U32, &width)) {
                width = std::clamp(width, std::uint32_t{1}, std::uint32_t{64});
                workspace_.set_packed_field_width(packed->id, field->name, width);
            }
            if (ImGui::Button("Use schema width")) {
                workspace_.set_packed_field_width(packed->id, field->name, std::nullopt);
            }
        }
    } else if (auto const* soa{std::get_if<SoaLayout>(definition)}) {
        auto capacity{active_soa_->capacity};
        if (ImGui::InputScalar("Capacity", ImGuiDataType_U64, &capacity)) {
            workspace_.set_capacity(soa->id, capacity);
        }
        for (auto const quick : {std::uint64_t{1},
                                 std::uint64_t{4'096},
                                 std::uint64_t{16'384},
                                 std::uint64_t{65'536}}) {
            ImGui::SameLine();
            auto const label{std::to_string(quick)};
            if (ImGui::SmallButton(label.c_str())) {
                workspace_.set_capacity(soa->id, quick);
            }
        }
        if (ImGui::Button("Use default capacity")) {
            workspace_.set_capacity(soa->id, std::nullopt);
        }
        if (selected_field_.empty() && !soa->columns.empty()) {
            selected_field_ = soa->columns.front().name;
        }
        if (auto const* column{soa_column(*soa, selected_field_)}) {
            ImGui::SeparatorText(column->name.c_str());
            auto const analyzed{
                std::ranges::find(active_soa_->columns, column->name, &SoaColumnAnalysis::name)};
            auto const current_type{analyzed == active_soa_->columns.end()
                                        ? column->logical_type
                                        : analyzed->physical_type};
            if (ImGui::BeginCombo("Planning type", current_type.c_str())) {
                if (ImGui::Selectable("Schema default", current_type == column->logical_type)) {
                    workspace_.set_soa_column_type(soa->id, column->name, std::nullopt);
                }
                for (auto const& [type, facts] : abi_.types()) {
                    static_cast<void>(facts);
                    if (ImGui::Selectable(type.c_str(), current_type == type)) {
                        workspace_.set_soa_column_type(soa->id, column->name, type);
                    }
                }
                ImGui::EndCombo();
            }
        }
    }
    ImGui::EndDisabled();
    ImGui::End();
}

void PlannerUi::draw_variants_panel() {
    ImGui::Begin("Variants");
    for (auto const& variant : workspace_.variants()) {
        auto const selected{workspace_.active_variant_id() == variant.id};
        if (ImGui::Selectable(variant.name.c_str(), selected)) {
            workspace_.select_variant(variant.id);
            sync_variant_name();
        }
    }
    ImGui::Separator();
    if (ImGui::Button("New")) {
        workspace_.create_variant("Variant " + std::to_string(next_variant_number_++));
        sync_variant_name();
    }
    ImGui::SameLine();
    if (ImGui::Button("Duplicate")) {
        workspace_.duplicate_variant(workspace_.active_variant_id(),
                                     "Variant " + std::to_string(next_variant_number_++));
        sync_variant_name();
    }
    auto const baseline{workspace_.active_variant_id() == LayoutWorkspace::baseline_variant_id};
    ImGui::BeginDisabled(baseline);
    ImGui::SameLine();
    if (ImGui::Button("Reset")) {
        workspace_.reset_variant(workspace_.active_variant_id());
    }
    ImGui::SameLine();
    if (ImGui::Button("Delete")) {
        workspace_.delete_variant(workspace_.active_variant_id());
        sync_variant_name();
    }
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
    ImGui::End();
}

void PlannerUi::draw_analysis_panel() {
    ImGui::Begin("Analysis");
    ImGui::TextDisabled("ABI profile: %s", abi_.name().c_str());
    if (baseline_packed_.has_value() && active_packed_.has_value()) {
        if (ImGui::BeginTable(
                "packed-comparison", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("Fact");
            ImGui::TableSetupColumn("Baseline");
            ImGui::TableSetupColumn("Active variant");
            ImGui::TableHeadersRow();
            auto row =
                [](char const* label, std::string const& baseline, std::string const& active) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(label);
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(baseline.c_str());
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(active.c_str());
                };
            row("Storage", baseline_packed_->storage_type, active_packed_->storage_type);
            row("Storage bytes",
                format_bytes(baseline_packed_->storage_facts.transform([](auto const& facts) {
                    return facts.size_bytes;
                })).c_str(),
                format_bytes(active_packed_->storage_facts.transform([](auto const& facts) {
                    return facts.size_bytes;
                })).c_str());
            row("Storage bits",
                baseline_packed_->storage_bits.has_value()
                    ? std::to_string(*baseline_packed_->storage_bits)
                    : "Unknown",
                active_packed_->storage_bits.has_value()
                    ? std::to_string(*active_packed_->storage_bits)
                    : "Unknown");
            row("Bits used",
                baseline_packed_->bits_used.has_value()
                    ? std::to_string(*baseline_packed_->bits_used)
                    : "Unknown",
                active_packed_->bits_used.has_value() ? std::to_string(*active_packed_->bits_used)
                                                      : "Unknown");
            row("Unused bits",
                baseline_packed_->unused_bits.has_value()
                    ? std::to_string(*baseline_packed_->unused_bits)
                    : "Invalid",
                active_packed_->unused_bits.has_value()
                    ? std::to_string(*active_packed_->unused_bits)
                    : "Invalid");
            for (std::size_t index{}; index < baseline_packed_->fields.size(); ++index) {
                auto const& baseline{baseline_packed_->fields[index]};
                auto const& active{active_packed_->fields[index]};
                row((baseline.name + " bits").c_str(),
                    std::to_string(baseline.bit_width),
                    std::to_string(active.bit_width));
            }
            ImGui::EndTable();
        }
        draw_diagnostics(active_packed_->diagnostics);
    } else if (baseline_soa_.has_value() && active_soa_.has_value()) {
        if (ImGui::BeginTable(
                "soa-comparison", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("Fact");
            ImGui::TableSetupColumn("Baseline");
            ImGui::TableSetupColumn("Active variant");
            ImGui::TableHeadersRow();
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted("Capacity");
            ImGui::TableNextColumn();
            ImGui::Text("%llu", static_cast<unsigned long long>(baseline_soa_->capacity));
            ImGui::TableNextColumn();
            ImGui::Text("%llu", static_cast<unsigned long long>(active_soa_->capacity));
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted("Payload");
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(format_bytes(baseline_soa_->total_payload_bytes).c_str());
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(format_bytes(active_soa_->total_payload_bytes).c_str());
            ImGui::EndTable();
        }
        ImGui::TextDisabled(
            "Cache-line counts are minimum payload coverage, not allocator traffic.");
        draw_diagnostics(active_soa_->diagnostics);
    }
    ImGui::End();
}

void PlannerUi::draw_diagnostics(std::vector<Diagnostic> const& diagnostics) const {
    for (auto const& diagnostic : diagnostics) {
        ImGui::PushStyleColor(ImGuiCol_Text, diagnostic_color(diagnostic.severity));
        ImGui::TextWrapped("%s", diagnostic.message.c_str());
        ImGui::PopStyleColor();
    }
}

void PlannerUi::sync_variant_name() {
    auto const& name{workspace_.active_variant().name};
    std::snprintf(variant_name_.data(), variant_name_.size(), "%s", name.c_str());
    variant_name_id_ = workspace_.active_variant_id();
}

} // namespace ioj::layout_planner
