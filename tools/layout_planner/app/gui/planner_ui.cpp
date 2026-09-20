#include "planner_ui.hpp"

#include "planner_ui_support.hpp"

#include <imgui.h>
#include <imgui_internal.h>

#include <charconv>
#include <cstdio>
#include <cstring>
#include <set>
#include <string_view>
#include <utility>
#include <variant>

namespace ioj::layout_planner {
namespace {

using namespace layout;
using namespace lispb::schema;

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

void draw_target_profile(AbiProfile const& abi) {
    if (!ImGui::CollapsingHeader("Target profile", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }
    auto const& identity{abi.identity()};
    auto const& memory{abi.memory_facts()};
    auto const primitive_sources{type_fact_sources(abi)};
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
        draw_row("L1 data cache capacity", l1_capacity.c_str());
        draw_row("L2 cache capacity", l2_capacity.c_str());
        draw_row("L3 cache capacity", l3_capacity.c_str());
        draw_row("Primitive fact source", primitive_sources.c_str());
        draw_row("Memory fact source", known_or_unknown(memory.provenance));
        ImGui::EndTable();
    }
}

} // namespace

PlannerUi::PlannerUi(SchemaLoadResult loaded)
    : project_path_{std::move(loaded.project_path)}
    , target_name_{std::move(loaded.target_name)}
    , document_{std::move(loaded.document)}
    , workspace_{document_.has_value() ? document_->types() : TypeGraph{}}
    , load_diagnostics_{std::move(loaded.diagnostics)} {
    auto const types{workspace_.types().types()};
    auto const found{std::ranges::find_if(types, [](auto const& type) {
        if (std::holds_alternative<EnumType>(type.definition) ||
            std::holds_alternative<IntegerScalarType>(type.definition) ||
            std::holds_alternative<LinearQuantizedType>(type.definition) ||
            std::holds_alternative<IntegerVarintType>(type.definition) ||
            std::holds_alternative<FixedPointType>(type.definition) ||
            std::holds_alternative<MiniFloatType>(type.definition) ||
            std::holds_alternative<OptionalSentinelType>(type.definition) ||
            std::holds_alternative<OptionalPresenceBitType>(type.definition) ||
            std::holds_alternative<PackedType>(type.definition) ||
            std::holds_alternative<RecordType>(type.definition) ||
            std::holds_alternative<UnionType>(type.definition) ||
            std::holds_alternative<TaggedUnionType>(type.definition)) {
            return true;
        }
        auto const* soa{std::get_if<SoaType>(&type.definition)};
        return soa != nullptr && soa->backend == codegen::SoaBackend::standard_library;
    })};
    if (found != types.end()) {
        selected_type_ = TypeId{static_cast<std::uint32_t>(found - types.begin())};
    }
    sync_variant_name();
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
            if (load_project(path, true)) {
                return;
            }
        }
    }
    if (!fallback_path.empty()) {
        static_cast<void>(load_project(fallback_path, true));
    }
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
    if (!document_.has_value() || !document_->dirty()) {
        close_confirmed_ = true;
        return;
    }
    open_close_confirmation_ = true;
}

auto PlannerUi::take_close_confirmation() -> bool {
    return std::exchange(close_confirmed_, false);
}

void PlannerUi::validate_comparison_variants() {
    if (workspace_.variant(comparison_a_variant_id_) == nullptr) {
        comparison_a_variant_id_ = LayoutWorkspace::baseline_variant_id;
    }
    if (comparison_b_follows_active_) {
        comparison_b_variant_id_ = workspace_.active_variant_id();
    } else if (workspace_.variant(comparison_b_variant_id_) == nullptr) {
        comparison_b_variant_id_ = LayoutWorkspace::baseline_variant_id;
    }
}

auto PlannerUi::settings_read_open(ImGuiContext*, ImGuiSettingsHandler* handler, char const* name)
    -> void* {
    if (std::strcmp(name, "Settings") != 0) {
        return nullptr;
    }
    auto* ui{static_cast<PlannerUi*>(handler->UserData)};
    ui->recent_projects_.clear();
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
    constexpr std::string_view recent_prefix{"RecentProject="};
    if (value.starts_with(recent_prefix) && value.size() > recent_prefix.size() &&
        ui->recent_projects_.size() < 20) {
        ui->recent_projects_.emplace_back(value.substr(recent_prefix.size()));
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
    output->appendf("GraphOpen=%d\n", ui->graph_view_open_ ? 1 : 0);
    for (auto const& path : ui->recent_projects_) {
        output->appendf("RecentProject=%s\n", path.string().c_str());
    }
    output->append("\n");
}

auto PlannerUi::draw() -> bool {
    ImGui::GetStyle().FontScaleMain = text_scale_;
    auto const view_changed{draw_view_menu()};
    ImGui::GetStyle().FontScaleMain = text_scale_;
    auto const dockspace_id{ImGui::DockSpaceOverViewport()};
    setup_default_dock_layout(dockspace_id);
    refresh_analysis();

    auto const revision_before{workspace_.revision()};
    draw_project_panel();
    refresh_analysis();
    draw_layout_panel();
    refresh_analysis();
    draw_properties_panel();
    draw_variants_panel();
    refresh_analysis();
    draw_comparison_panel();
    draw_graph_panel();
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
    draw_source_preview();
    draw_close_confirmation();
    draw_project_path_dialogs();
    return view_changed || revision_before != workspace_.revision() ||
           std::exchange(project_changed_, false);
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
            changed = true;
        }
        if (ImGui::MenuItem("Graph", nullptr, graph_view_open_)) {
            graph_view_open_ = !graph_view_open_;
            ImGui::MarkIniSettingsDirty();
            changed = true;
        }
        ImGui::EndMenu();
    }
    if (document_.has_value() && document_->dirty()) {
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
    if (ImGui::MenuItem("Open Project...")) {
        std::snprintf(open_project_path_.data(),
                      open_project_path_.size(),
                      "%s",
                      project_path_.string().c_str());
        open_project_dialog_ = true;
    }
    if (ImGui::BeginMenu("Open Recent", !recent_projects_.empty())) {
        for (auto const& path : recent_projects_) {
            auto const label{path.string()};
            if (ImGui::MenuItem(label.c_str(), nullptr, path == project_path_)) {
                changed |= load_project(path);
            }
        }
        ImGui::EndMenu();
    }
    ImGui::Separator();
    ImGui::BeginDisabled(!has_document || !document_->can_undo());
    if (ImGui::MenuItem("Undo")) {
        auto const selection{selected_type_.transform(
            [&](TypeId const type) { return workspace_.types().type(type).identity; })};
        auto result{document_->undo()};
        if (result.has_value() && *result) {
            sync_document_graph(selection);
            changed = true;
        } else if (!result.has_value()) {
            schema_edit_message_ = result.error().message;
        }
    }
    ImGui::EndDisabled();
    ImGui::BeginDisabled(!has_document || !document_->can_redo());
    if (ImGui::MenuItem("Redo")) {
        auto const selection{selected_type_.transform(
            [&](TypeId const type) { return workspace_.types().type(type).identity; })};
        auto result{document_->redo()};
        if (result.has_value() && *result) {
            sync_document_graph(selection);
            changed = true;
        } else if (!result.has_value()) {
            schema_edit_message_ = result.error().message;
        }
    }
    ImGui::EndDisabled();
    ImGui::Separator();
    ImGui::BeginDisabled(!has_document || !document_->dirty());
    if (ImGui::MenuItem("Preview LispB changes")) {
        open_source_preview_ = true;
    }
    if (ImGui::MenuItem("Save")) {
        auto const selection{selected_type_.transform(
            [&](TypeId const type) { return workspace_.types().type(type).identity; })};
        auto result{document_->save()};
        if (result.has_value()) {
            sync_document_graph(selection);
            schema_edit_message_ =
                "Saved and reloaded " + std::to_string(result->size()) + " LispB source file(s).";
            changed = true;
        } else {
            schema_edit_message_ = result.error().message;
        }
    }
    ImGui::EndDisabled();
    ImGui::BeginDisabled(!has_document);
    if (ImGui::MenuItem("Save As...")) {
        auto destination{project_path_.parent_path() /
                         (project_path_.stem().string() + "_copy.lispb")};
        std::snprintf(save_as_project_path_.data(),
                      save_as_project_path_.size(),
                      "%s",
                      destination.string().c_str());
        open_save_as_dialog_ = true;
    }
    ImGui::EndDisabled();
    ImGui::EndMenu();

    return changed;
}

void PlannerUi::draw_project_path_dialogs() {
    if (open_project_dialog_) {
        ImGui::OpenPopup("Open LispB project");
        open_project_dialog_ = false;
    }
    if (ImGui::BeginPopupModal("Open LispB project", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("Project manifest path");
        ImGui::SetNextItemWidth(720.0F);
        ImGui::InputText(
            "##open-project-path", open_project_path_.data(), open_project_path_.size());
        ImGui::BeginDisabled(open_project_path_.front() == '\0');
        if (ImGui::Button("Open")) {
            if (load_project(open_project_path_.data())) {
                ImGui::CloseCurrentPopup();
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

    if (open_save_as_dialog_) {
        ImGui::OpenPopup("Save LispB project as");
        open_save_as_dialog_ = false;
    }
    if (ImGui::BeginPopupModal(
            "Save LispB project as", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("New project manifest path");
        ImGui::SetNextItemWidth(720.0F);
        ImGui::InputText(
            "##save-as-project-path", save_as_project_path_.data(), save_as_project_path_.size());
        ImGui::TextDisabled("A sibling <name>_schema directory will contain the cloned sources.");
        ImGui::BeginDisabled(save_as_project_path_.front() == '\0' || !document_.has_value());
        if (ImGui::Button("Save As")) {
            auto cloned{clone_lispb_schema(*document_, save_as_project_path_.data(), target_name_)};
            if (cloned.loaded) {
                adopt_loaded_schema(std::move(cloned));
                schema_edit_message_ = "Saved and opened the cloned LispB project.";
                ImGui::CloseCurrentPopup();
            } else {
                schema_edit_message_ = cloned.diagnostics.empty()
                                         ? "Could not clone the LispB project."
                                         : cloned.diagnostics.front().message;
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
}

void PlannerUi::draw_source_preview() {
    if (open_source_preview_) {
        ImGui::OpenPopup("LispB source preview");
        open_source_preview_ = false;
    }
    if (!ImGui::BeginPopupModal(
            "LispB source preview", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }
    auto updates{document_->preview_source_updates()};
    if (!updates.has_value()) {
        ImGui::TextWrapped("%s", updates.error().message.c_str());
    } else {
        for (auto const& update : *updates) {
            ImGui::SeparatorText(update.path.string().c_str());
            if (ImGui::BeginTabBar(update.path.string().c_str())) {
                if (ImGui::BeginTabItem("Updated")) {
                    ImGui::BeginChild("updated-source", {760.0F, 360.0F}, true);
                    ImGui::TextUnformatted(update.updated.c_str());
                    ImGui::EndChild();
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Original")) {
                    ImGui::BeginChild("original-source", {760.0F, 360.0F}, true);
                    ImGui::TextUnformatted(update.original.c_str());
                    ImGui::EndChild();
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }
        }
    }
    if (ImGui::Button("Close")) {
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
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
    ImGui::TextWrapped("The editable schema contains unsaved LispB changes.");
    ImGui::TextUnformatted("Save them before closing?");
    if (ImGui::Button("Save and close")) {
        auto result{document_->save()};
        if (result.has_value()) {
            close_confirmed_ = true;
            ImGui::CloseCurrentPopup();
        } else {
            schema_edit_message_ = result.error().message;
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

auto PlannerUi::apply_document_edit(SchemaEditCommand command,
                                    std::optional<TypeIdentity> selection) -> bool {
    if (!document_.has_value()) {
        schema_edit_message_ = "No editable LispB document is loaded.";
        return false;
    }
    if (!selection.has_value() && selected_type_.has_value()) {
        selection = workspace_.types().type(*selected_type_).identity;
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
    workspace_.replace_types(document_->types());
    selected_type_.reset();
    if (selection.has_value()) {
        selected_type_ = workspace_.types().find(*selection);
    }
    selected_field_.clear();
    enum_editor_declaration_.reset();
    enum_editor_value_.clear();
    packed_editor_declaration_.reset();
    packed_editor_field_.clear();
    packed_code_editor_declaration_.reset();
    packed_code_editor_field_.clear();
    packed_code_editor_name_.clear();
    selected_packed_code_.clear();
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
}

auto PlannerUi::load_project(std::filesystem::path const& path, bool const allow_dirty) -> bool {
    if (!allow_dirty && document_.has_value() && document_->dirty()) {
        schema_edit_message_ =
            "Save or undo the current LispB changes before opening another project.";
        return false;
    }
    auto loaded{load_lispb_schema(path, target_name_)};
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
    project_path_ = std::move(loaded.project_path);
    target_name_ = std::move(loaded.target_name);
    document_ = std::move(loaded.document);
    load_diagnostics_ = std::move(loaded.diagnostics);
    workspace_ = LayoutWorkspace{document_.has_value() ? document_->types() : TypeGraph{}};
    selected_type_.reset();
    auto const types{workspace_.types().types()};
    for (std::size_t index{}; index < types.size(); ++index) {
        auto const& definition{types[index].definition};
        auto const* soa{std::get_if<SoaType>(&definition)};
        if (std::holds_alternative<EnumType>(definition) ||
            std::holds_alternative<IntegerScalarType>(definition) ||
            std::holds_alternative<LinearQuantizedType>(definition) ||
            std::holds_alternative<IntegerVarintType>(definition) ||
            std::holds_alternative<FixedPointType>(definition) ||
            std::holds_alternative<MiniFloatType>(definition) ||
            std::holds_alternative<OptionalSentinelType>(definition) ||
            std::holds_alternative<OptionalPresenceBitType>(definition) ||
            std::holds_alternative<PackedType>(definition) ||
            std::holds_alternative<RecordType>(definition) ||
            std::holds_alternative<UnionType>(definition) ||
            std::holds_alternative<TaggedUnionType>(definition) ||
            (soa != nullptr && soa->backend == codegen::SoaBackend::standard_library)) {
            selected_type_ = TypeId{static_cast<std::uint32_t>(index)};
            break;
        }
    }
    selected_field_.clear();
    selected_enumerator_.clear();
    record_access_members_.clear();
    soa_access_columns_.clear();
    varint_distributions_.clear();
    tagged_union_distributions_.clear();
    ++tagged_distribution_revision_;
    new_varint_distribution_value_.fill('\0');
    new_varint_distribution_value_[0] = '0';
    new_varint_distribution_weight_ = 1;
    record_access_set_explicit_ = false;
    soa_access_set_explicit_ = false;
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
    cached_type_.reset();
    cached_revision_ = std::numeric_limits<std::uint64_t>::max();
    quantized_comparison_type_.reset();
    varint_comparison_type_.reset();
    optional_comparison_type_.reset();
    comparison_a_variant_id_ = LayoutWorkspace::baseline_variant_id;
    comparison_b_variant_id_ = LayoutWorkspace::baseline_variant_id;
    comparison_b_follows_active_ = true;
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
    if (recent_projects_.size() > 20) {
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
    ImGui::DockBuilderDockWindow("Layout", center_id);
    ImGui::DockBuilderDockWindow("Properties", right_id);
    ImGui::DockBuilderDockWindow("Comparison", comparison_id);
    ImGui::DockBuilderFinish(dockspace_id);
    dock_layout_initialized_ = true;
    reset_dock_layout_requested_ = false;
}

void PlannerUi::refresh_analysis() {
    validate_comparison_variants();

    if (cached_type_ != selected_type_) {
        soa_access_columns_.clear();
        soa_access_set_explicit_ = false;
    }
    if (selected_type_.has_value()) {
        auto const* selected_soa{
            std::get_if<SoaType>(&workspace_.types().type(*selected_type_).definition)};
        if (selected_soa != nullptr &&
            selected_soa->backend == codegen::SoaBackend::standard_library) {
            std::erase_if(soa_access_columns_, [&](std::string const& name) {
                return std::ranges::none_of(
                    selected_soa->columns, [&](auto const& column) { return column.name == name; });
            });
        }
    }

    if (selected_type_.has_value()) {
        auto const* selected_quantized{
            std::get_if<LinearQuantizedType>(&workspace_.types().type(*selected_type_).definition)};
        if (selected_quantized != nullptr) {
            auto valid_comparison_type = [&](TypeId const candidate) {
                if (candidate == *selected_type_) {
                    return false;
                }
                auto const* candidate_quantized{std::get_if<LinearQuantizedType>(
                    &workspace_.types().type(candidate).definition)};
                return candidate_quantized != nullptr &&
                       candidate_quantized->source.type == selected_quantized->source.type;
            };

            std::optional<TypeId> comparison_id;
            if (quantized_comparison_type_.has_value()) {
                comparison_id = workspace_.types().find(*quantized_comparison_type_);
            }
            if (!comparison_id.has_value() || !valid_comparison_type(*comparison_id)) {
                quantized_comparison_type_.reset();
                auto const types{workspace_.types().types()};
                for (std::size_t index{}; index < types.size(); ++index) {
                    auto const candidate{TypeId{static_cast<std::uint32_t>(index)}};
                    if (valid_comparison_type(candidate)) {
                        quantized_comparison_type_ = types[index].identity;
                        break;
                    }
                }
            }
        } else {
            quantized_comparison_type_.reset();
        }
    } else {
        quantized_comparison_type_.reset();
    }

    if (selected_type_.has_value()) {
        auto const* selected_varint{
            std::get_if<IntegerVarintType>(&workspace_.types().type(*selected_type_).definition)};
        if (selected_varint != nullptr) {
            auto valid_comparison_type = [&](TypeId const candidate) {
                if (candidate == *selected_type_) {
                    return false;
                }
                auto const* candidate_varint{
                    std::get_if<IntegerVarintType>(&workspace_.types().type(candidate).definition)};
                return candidate_varint != nullptr &&
                       candidate_varint->source.type == selected_varint->source.type;
            };

            std::optional<TypeId> comparison_id;
            if (varint_comparison_type_.has_value()) {
                comparison_id = workspace_.types().find(*varint_comparison_type_);
            }
            if (!comparison_id.has_value() || !valid_comparison_type(*comparison_id)) {
                varint_comparison_type_.reset();
                auto const types{workspace_.types().types()};
                for (std::size_t index{}; index < types.size(); ++index) {
                    auto const candidate{TypeId{static_cast<std::uint32_t>(index)}};
                    if (valid_comparison_type(candidate)) {
                        varint_comparison_type_ = types[index].identity;
                        break;
                    }
                }
            }
        } else {
            varint_comparison_type_.reset();
        }
    } else {
        varint_comparison_type_.reset();
    }

    auto optional_source = [&](TypeId const type) -> std::optional<TypeId> {
        auto const& definition{workspace_.types().type(type).definition};
        if (auto const* sentinel{std::get_if<OptionalSentinelType>(&definition)}) {
            return sentinel->source.type;
        }
        if (auto const* presence{std::get_if<OptionalPresenceBitType>(&definition)}) {
            return presence->source.type;
        }
        return std::nullopt;
    };
    if (selected_type_.has_value()) {
        auto const selected_source{optional_source(*selected_type_)};
        if (selected_source.has_value()) {
            auto valid_comparison_type = [&](TypeId const candidate) {
                return candidate != *selected_type_ &&
                       optional_source(candidate) == selected_source;
            };

            std::optional<TypeId> comparison_id;
            if (optional_comparison_type_.has_value()) {
                comparison_id = workspace_.types().find(*optional_comparison_type_);
            }
            if (!comparison_id.has_value() || !valid_comparison_type(*comparison_id)) {
                optional_comparison_type_.reset();
                auto const types{workspace_.types().types()};
                for (std::size_t index{}; index < types.size(); ++index) {
                    auto const candidate{TypeId{static_cast<std::uint32_t>(index)}};
                    if (valid_comparison_type(candidate)) {
                        optional_comparison_type_ = types[index].identity;
                        break;
                    }
                }
            }
        } else {
            optional_comparison_type_.reset();
        }
    } else {
        optional_comparison_type_.reset();
    }

    if (cached_revision_ == workspace_.revision() && cached_type_ == selected_type_ &&
        cached_selected_field_ == selected_field_ &&
        cached_record_access_members_ == record_access_members_ &&
        cached_record_access_set_explicit_ == record_access_set_explicit_ &&
        cached_soa_access_columns_ == soa_access_columns_ &&
        cached_soa_access_set_explicit_ == soa_access_set_explicit_ &&
        cached_comparison_a_variant_id_ == comparison_a_variant_id_ &&
        cached_comparison_b_variant_id_ == comparison_b_variant_id_ &&
        cached_quantized_comparison_type_ == quantized_comparison_type_ &&
        cached_varint_comparison_type_ == varint_comparison_type_ &&
        cached_optional_comparison_type_ == optional_comparison_type_ &&
        cached_tagged_distribution_revision_ == tagged_distribution_revision_) {
        return;
    }
    enum_domain_.reset();
    integer_scalar_analysis_.reset();
    linear_quantized_analysis_.reset();
    linear_quantized_comparison_.reset();
    integer_varint_analysis_.reset();
    integer_varint_comparison_.reset();
    fixed_point_analysis_.reset();
    mini_float_analysis_.reset();
    optional_sentinel_analysis_.reset();
    optional_presence_bit_analysis_.reset();
    optional_encoding_comparison_.reset();
    baseline_packed_.reset();
    active_packed_.reset();
    packed_variants_.clear();
    baseline_soa_.reset();
    active_soa_.reset();
    soa_access_analysis_.reset();
    soa_variants_.clear();
    comparison_a_packed_.reset();
    comparison_b_packed_.reset();
    comparison_a_soa_.reset();
    comparison_b_soa_.reset();
    record_analysis_.reset();
    record_access_analysis_.reset();
    union_analysis_.reset();
    tagged_union_analysis_.reset();
    tagged_union_distribution_analysis_.reset();
    cached_revision_ = workspace_.revision();
    cached_type_ = selected_type_;
    cached_selected_field_ = selected_field_;
    cached_record_access_members_ = record_access_members_;
    cached_record_access_set_explicit_ = record_access_set_explicit_;
    cached_soa_access_columns_ = soa_access_columns_;
    cached_soa_access_set_explicit_ = soa_access_set_explicit_;
    cached_comparison_a_variant_id_ = comparison_a_variant_id_;
    cached_comparison_b_variant_id_ = comparison_b_variant_id_;
    cached_quantized_comparison_type_ = quantized_comparison_type_;
    cached_varint_comparison_type_ = varint_comparison_type_;
    cached_optional_comparison_type_ = optional_comparison_type_;
    cached_tagged_distribution_revision_ = tagged_distribution_revision_;
    if (!selected_type_.has_value()) {
        return;
    }
    auto const& definition{workspace_.types().type(*selected_type_).definition};
    auto const& baseline{*workspace_.variant(LayoutWorkspace::baseline_variant_id)};
    auto const& active{workspace_.active_variant()};
    auto const& comparison_a{*workspace_.variant(comparison_a_variant_id_)};
    auto const& comparison_b{*workspace_.variant(comparison_b_variant_id_)};
    auto const element_count{workspace_.element_count()};
    if (std::holds_alternative<EnumType>(definition)) {
        enum_domain_ = Analyzer::analyze_enum(workspace_.types(), *selected_type_, abi_);
    } else if (std::holds_alternative<IntegerScalarType>(definition)) {
        integer_scalar_analysis_ =
            Analyzer::analyze_integer_scalar(workspace_.types(), *selected_type_);
    } else if (std::holds_alternative<LinearQuantizedType>(definition)) {
        linear_quantized_analysis_ =
            Analyzer::analyze_linear_quantized(workspace_.types(), *selected_type_);
        if (quantized_comparison_type_.has_value()) {
            auto const comparison_type{workspace_.types().find(*quantized_comparison_type_)};
            if (comparison_type.has_value()) {
                linear_quantized_comparison_ = Analyzer::compare_linear_quantized(
                    workspace_.types(), *selected_type_, *comparison_type, element_count);
            }
        }
    } else if (std::holds_alternative<IntegerVarintType>(definition)) {
        integer_varint_analysis_ =
            Analyzer::analyze_integer_varint(workspace_.types(), *selected_type_, element_count);
        if (varint_comparison_type_.has_value()) {
            auto const comparison_type{workspace_.types().find(*varint_comparison_type_)};
            if (comparison_type.has_value()) {
                integer_varint_comparison_ = Analyzer::compare_integer_varint(
                    workspace_.types(), *selected_type_, *comparison_type, element_count);
            }
        }
    } else if (std::holds_alternative<FixedPointType>(definition)) {
        fixed_point_analysis_ =
            Analyzer::analyze_fixed_point(workspace_.types(), *selected_type_, element_count);
    } else if (std::holds_alternative<MiniFloatType>(definition)) {
        mini_float_analysis_ =
            Analyzer::analyze_mini_float(workspace_.types(), *selected_type_, element_count);
    } else if (std::holds_alternative<OptionalSentinelType>(definition)) {
        optional_sentinel_analysis_ =
            Analyzer::analyze_optional_sentinel(workspace_.types(), *selected_type_, element_count);
        if (optional_comparison_type_.has_value()) {
            auto const comparison_type{workspace_.types().find(*optional_comparison_type_)};
            if (comparison_type.has_value()) {
                optional_encoding_comparison_ = Analyzer::compare_optional_encodings(
                    workspace_.types(), *selected_type_, *comparison_type, element_count);
            }
        }
    } else if (std::holds_alternative<OptionalPresenceBitType>(definition)) {
        optional_presence_bit_analysis_ = Analyzer::analyze_optional_presence_bit(
            workspace_.types(), *selected_type_, element_count);
        if (optional_comparison_type_.has_value()) {
            auto const comparison_type{workspace_.types().find(*optional_comparison_type_)};
            if (comparison_type.has_value()) {
                optional_encoding_comparison_ = Analyzer::compare_optional_encodings(
                    workspace_.types(), *selected_type_, *comparison_type, element_count);
            }
        }
    } else if (std::holds_alternative<RecordType>(definition)) {
        record_analysis_ =
            Analyzer::analyze_record(workspace_.types(), *selected_type_, abi_, element_count);
        std::vector<std::string> access_members{record_access_members_.begin(),
                                                record_access_members_.end()};
        if (!record_access_set_explicit_ && !selected_field_.empty()) {
            access_members.clear();
            access_members.push_back(selected_field_);
        }
        if (!access_members.empty()) {
            record_access_analysis_ =
                Analyzer::analyze_record_access(*record_analysis_, access_members, abi_);
        }
    } else if (std::holds_alternative<UnionType>(definition)) {
        union_analysis_ =
            Analyzer::analyze_union(workspace_.types(), *selected_type_, abi_, element_count);
    } else if (std::holds_alternative<TaggedUnionType>(definition)) {
        tagged_union_analysis_ = Analyzer::analyze_tagged_union(
            workspace_.types(), *selected_type_, abi_, element_count);
        auto const declaration{
            document_.has_value()
                ? document_->find_declaration(workspace_.types().type(*selected_type_).identity)
                : std::optional<DeclarationId>{}};
        auto const found{declaration.has_value() ? tagged_union_distributions_.find(*declaration)
                                                 : tagged_union_distributions_.end()};
        if (found != tagged_union_distributions_.end()) {
            std::vector<TaggedUnionDistributionEntry> entries;
            for (auto const& [tag, weight] : found->second) {
                if (weight != 0) {
                    entries.push_back({.tag = tag, .weight = weight});
                }
            }
            if (!entries.empty()) {
                tagged_union_distribution_analysis_ = Analyzer::analyze_tagged_union_distribution(
                    *tagged_union_analysis_, entries, element_count);
            }
        }
    } else if (std::holds_alternative<PackedType>(definition)) {
        baseline_packed_ = Analyzer::analyze_packed(
            workspace_.types(), *selected_type_, baseline, abi_, element_count);
        active_packed_ = Analyzer::analyze_packed(
            workspace_.types(), *selected_type_, active, abi_, element_count);
        for (auto const& variant : workspace_.variants()) {
            if (variant.id != LayoutWorkspace::baseline_variant_id) {
                packed_variants_.emplace_back(
                    variant.id,
                    Analyzer::analyze_packed(
                        workspace_.types(), *selected_type_, variant, abi_, element_count));
            }
        }
        comparison_a_packed_ = Analyzer::analyze_packed(
            workspace_.types(), *selected_type_, comparison_a, abi_, element_count);
        comparison_b_packed_ = Analyzer::analyze_packed(
            workspace_.types(), *selected_type_, comparison_b, abi_, element_count);
    } else if (auto const* soa{std::get_if<SoaType>(&definition)};
               soa != nullptr && soa->backend == codegen::SoaBackend::standard_library) {
        baseline_soa_ = Analyzer::analyze_soa(
            workspace_.types(), *selected_type_, baseline, abi_, workspace_.default_capacity());
        active_soa_ = Analyzer::analyze_soa(
            workspace_.types(), *selected_type_, active, abi_, workspace_.default_capacity());
        std::vector<std::string> access_columns{soa_access_columns_.begin(),
                                                soa_access_columns_.end()};
        if (!soa_access_set_explicit_ && !selected_field_.empty()) {
            access_columns.clear();
            access_columns.push_back(selected_field_);
        }
        if (!access_columns.empty()) {
            soa_access_analysis_ = Analyzer::analyze_soa_access(
                *active_soa_, access_columns, abi_, workspace_.element_count());
        }
        for (auto const& variant : workspace_.variants()) {
            if (variant.id != LayoutWorkspace::baseline_variant_id) {
                soa_variants_.emplace_back(variant.id,
                                           Analyzer::analyze_soa(workspace_.types(),
                                                                 *selected_type_,
                                                                 variant,
                                                                 abi_,
                                                                 workspace_.default_capacity()));
            }
        }
        comparison_a_soa_ = Analyzer::analyze_soa(
            workspace_.types(), *selected_type_, comparison_a, abi_, workspace_.default_capacity());
        comparison_b_soa_ = Analyzer::analyze_soa(
            workspace_.types(), *selected_type_, comparison_b, abi_, workspace_.default_capacity());
    }
}

void PlannerUi::draw_layout_panel() {
    ImGui::Begin("Layout");
    draw_target_profile(abi_);
    ImGui::Separator();
    if (ImGui::Button("+ Add variant")) {
        create_variant_for_selected_schema();
    }
    ImGui::Separator();
    if (!selected_type_.has_value()) {
        ImGui::TextDisabled("Select a supported schema.");
    } else if (auto const& definition{workspace_.types().type(*selected_type_).definition};
               auto const* packed = std::get_if<PackedType>(&definition)) {
        draw_packed_layout(*packed, *baseline_packed_);
    } else if (auto const* soa{std::get_if<SoaType>(&definition)};
               soa != nullptr && soa->backend == codegen::SoaBackend::standard_library) {
        draw_soa_layout(*soa, *baseline_soa_);
    } else if (std::holds_alternative<RecordType>(definition)) {
        draw_record_layout(*record_analysis_);
    } else if (std::holds_alternative<UnionType>(definition)) {
        auto const& analysis{*union_analysis_};
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
                auto const selected{selected_field_ == alternative.name};
                auto const extent_color{selected ? ImVec4{0.24F, 0.65F, 0.90F, 1.0F}
                                                 : ImVec4{0.62F, 0.39F, 0.20F, 1.0F}};
                draw_list->AddRectFilled(origin,
                                         {origin.x + extent_width, origin.y + bar_height},
                                         ImGui::GetColorU32(extent_color),
                                         3.0F);
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
                    selected_field_ = alternative.name;
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("%s: %s extent, %s slack in each %s object",
                                      alternative.name.c_str(),
                                      detail::format_bytes(alternative.extent_bytes).c_str(),
                                      detail::format_bytes(alternative.slack_bytes).c_str(),
                                      detail::format_bytes(analysis.size_bytes).c_str());
                }
                ImGui::PopID();
            }
            ImGui::TextDisabled("Colored bytes belong to the alternative; dark bytes are union "
                                "slack, including any tail alignment.");
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
        ImGui::TextDisabled(
            "Each scaled slack value assumes every object uses that alternative; no tag "
            "distribution is implied.");
        draw_diagnostics(analysis.diagnostics);
    } else if (auto const* tagged{std::get_if<TaggedUnionType>(&definition)}) {
        auto const& analysis{*tagged_union_analysis_};
        ImGui::SeparatorText("Analysis scale");
        if (draw_element_count()) {
            ImGui::End();
            return;
        }
        ImGui::Text("Tagged union: %s",
                    workspace_.types().type(*selected_type_).identity.name.c_str());
        ImGui::Text("Discriminant: %s",
                    workspace_.types().type(tagged->discriminant.type).identity.name.c_str());
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
            draw_stat("Internal padding",
                      detail::format_bytes(aggregate.total_internal_padding_bytes));
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
            draw_stat("Internal padding", detail::format_bytes(analysis.internal_padding_bytes));
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
            draw_list->AddRect(origin,
                               {origin.x + width, origin.y + bar_height},
                               ImGui::GetColorU32(ImGuiCol_Border),
                               3.0F);
            draw_list->AddText(
                {origin.x + 5.0F, origin.y + 10.0F}, ImGui::GetColorU32(ImGuiCol_Text), "tag");
            draw_list->AddText({origin.x + payload_begin + 5.0F, origin.y + 10.0F},
                               ImGui::GetColorU32(ImGuiCol_Text),
                               "payload union");
            ImGui::InvisibleButton("##tagged-union-object-map", {width, bar_height});
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip(
                    "Tag: %s; alignment gap: %s; payload: %s; tail padding: %s",
                    detail::format_bytes(analysis.discriminant_facts->size_bytes).c_str(),
                    detail::format_bytes(analysis.internal_padding_bytes).c_str(),
                    detail::format_bytes(analysis.payload_size_bytes).c_str(),
                    detail::format_bytes(analysis.tail_padding_bytes).c_str());
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
                ImGui::TextUnformatted(
                    workspace_.types().type(alternative.semantic_type).cpp_spelling.c_str());
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
        ImGui::TextDisabled(
            "Semantic integer scalars have no standalone physical layout. Reference one from a "
            "physical representation to analyze storage.");
    } else if (std::holds_alternative<LinearQuantizedType>(definition)) {
        auto const& analysis{*linear_quantized_analysis_};
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
        auto const& analysis{*integer_varint_analysis_};
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
        ImGui::SeparatorText("Analysis scale");
        if (draw_element_count()) {
            ImGui::End();
            return;
        }
        auto const& analysis{*fixed_point_analysis_};
        ImGui::Text("Encoded width: %u bits/value", analysis.total_bits);
        ImGui::Text("At %llu values: %s bits",
                    static_cast<unsigned long long>(analysis.element_count),
                    detail::format_number(analysis.total_encoded_bits).c_str());
        ImGui::Text("Resolution: %.12g", static_cast<double>(analysis.resolution));
        ImGui::Text("Representable range: %.12g .. %.12g",
                    static_cast<double>(analysis.minimum_value),
                    static_cast<double>(analysis.maximum_value));
        ImGui::TextDisabled(
            "Encoded payload bits are not a standalone ABI sizeof/alignment or allocation size.");
        draw_diagnostics(analysis.diagnostics);
    } else if (std::holds_alternative<MiniFloatType>(definition)) {
        ImGui::SeparatorText("Analysis scale");
        if (draw_element_count()) {
            ImGui::End();
            return;
        }
        auto const& analysis{*mini_float_analysis_};
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
        auto const& analysis{*optional_sentinel_analysis_};
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
        auto const& analysis{*optional_presence_bit_analysis_};
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
    auto const& name{workspace_.active_variant().name};
    std::snprintf(variant_name_.data(), variant_name_.size(), "%s", name.c_str());
    variant_name_id_ = workspace_.active_variant_id();
}

void PlannerUi::create_variant_for_selected_schema() {
    auto const name{selected_type_.has_value()
                        ? workspace_.types().type(*selected_type_).identity.name + " experiment " +
                              std::to_string(next_variant_number_++)
                        : "Experiment " + std::to_string(next_variant_number_++)};
    workspace_.create_variant(name);
    sync_variant_name();
    refresh_analysis();
}

} // namespace ioj::layout_planner
