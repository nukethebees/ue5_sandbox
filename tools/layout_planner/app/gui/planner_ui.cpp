#include "planner_ui.hpp"

#include "platform/file_dialog.hpp"
#include <ioj/layout/abi_profile.hpp>
#include <ioj/layout/planner_type.hpp>

#include <lispb/diagnostics.h>
#include <lispb/schema/declaration_query.h>
#include <lispb/strings.h>

#include <imgui.h>
#include <imgui_internal.h>
#include <misc/cpp/imgui_stdlib.h>

#include <algorithm>
#include <cassert>
#include <sstream>
#include <utility>

namespace ioj::layout_planner {
namespace {
void label(std::string_view text) {
    ImGui::TextUnformatted(text.data(), text.data() + text.size());
}

auto number(std::optional<std::uint64_t> value) -> std::string {
    return value ? std::to_string(*value) : "Unknown";
}

void metric(char const* name, std::optional<std::uint64_t> value) {
    ImGui::Text("%s: %s", name, number(value).c_str());
}

void comparison(char const* name,
                std::optional<std::uint64_t> first,
                std::optional<std::uint64_t> second) {
    ImGui::Text("%s: %s / %s", name, number(first).c_str(), number(second).c_str());
}

void record_layout(layout::RecordAnalysis const& record) {
    metric("Size (bytes)", record.size_bytes);
    metric("Alignment (bytes)", record.alignment_bytes);
    metric("Internal padding (bytes)", record.internal_padding_bytes);
    metric("Tail padding (bytes)", record.tail_padding_bytes);
    metric("Total storage (bytes)", record.aggregate.total_storage_bytes);
    if (ImGui::BeginTable("record", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Member");
        ImGui::TableSetupColumn("Offset");
        ImGui::TableSetupColumn("Extent");
        ImGui::TableSetupColumn("Padding before");
        ImGui::TableHeadersRow();
        for (auto const& member : record.members) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            label(member.name);
            ImGui::TableNextColumn();
            label(number(member.offset_bytes));
            ImGui::TableNextColumn();
            label(number(member.extent_bytes));
            ImGui::TableNextColumn();
            label(number(member.padding_before_bytes));
        }
        ImGui::EndTable();
    }
}

void packed_layout(layout::PackedAnalysis const& packed) {
    ImGui::Text("Storage: %s", packed.storage_type.c_str());
    metric("Storage bits", packed.storage_bits);
    metric("Used bits", packed.bits_used);
    metric("Unused bits", packed.unused_bits);
    metric("Overflow bits", packed.overflow_bits);
    metric("Total storage (bytes)", packed.aggregate.total_storage_bytes);
    if (ImGui::BeginTable("packed", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Field");
        ImGui::TableSetupColumn("Type");
        ImGui::TableSetupColumn("Bits");
        ImGui::TableSetupColumn("Low bit");
        ImGui::TableHeadersRow();
        for (auto const& field : packed.fields) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            label(field.name);
            ImGui::TableNextColumn();
            label(field.logical_type);
            ImGui::TableNextColumn();
            label(std::to_string(field.bit_width));
            ImGui::TableNextColumn();
            label(std::to_string(field.least_significant_bit));
        }
        ImGui::EndTable();
    }
}

void soa_layout(layout::SoaAnalysis const& soa) {
    metric("Capacity", soa.capacity);
    metric("Bytes per element", soa.bytes_per_logical_element);
    metric("Payload (bytes)", soa.total_payload_bytes);
    metric("Allocation (bytes)", soa.total_allocation_bytes);
    metric("Alignment padding (bytes)", soa.total_alignment_padding_bytes);
    metric("Minimum pages", soa.minimum_pages);
    if (ImGui::BeginTable("soa", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Column");
        ImGui::TableSetupColumn("Type");
        ImGui::TableSetupColumn("Bytes");
        ImGui::TableSetupColumn("Cache lines");
        ImGui::TableHeadersRow();
        for (auto const& column : soa.columns) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            label(column.name);
            ImGui::TableNextColumn();
            label(column.physical_type);
            ImGui::TableNextColumn();
            label(number(column.total_bytes));
            ImGui::TableNextColumn();
            label(number(column.minimum_cache_lines));
        }
        ImGui::EndTable();
    }
}
}

PlannerUi::PlannerUi(std::filesystem::path project_path, std::string target_name)
    : project_path_{std::move(project_path)}
    , target_name_{std::move(target_name)}
    , project_input_{project_path_.string()}
    , target_input_{target_name_} {}

PlannerUi::PlannerUi(layout::Document document)
    : document_{std::move(document)}
    , project_path_{document_->project_path()}
    , target_name_{document_->target_name()}
    , project_input_{project_path_.string()}
    , target_input_{target_name_} {
    synchronize();
}

void PlannerUi::set_file_dialog(FileDialog* dialog) {
    file_dialog_ = dialog;
}

void PlannerUi::register_settings_handler() {
    ImGuiSettingsHandler handler;
    handler.TypeName = "LayoutPlanner";
    handler.TypeHash = ImHashStr(handler.TypeName);
    handler.ReadOpenFn = settings_read_open;
    handler.ReadLineFn = settings_read_line;
    handler.WriteAllFn = settings_write_all;
    handler.UserData = this;
    ImGui::AddSettingsHandler(&handler);
}

void PlannerUi::finish_startup(bool reopen_recent_project) {
    if (reopen_recent_project && !recent_project_.empty()) {
        project_input_ = recent_project_;
        target_input_ = recent_target_;
    }
    if (!document_ && (!project_path_.empty() || reopen_recent_project) &&
        !project_input_.empty()) {
        open_project();
    }
}

auto PlannerUi::saved_window_size() const -> std::optional<WindowSize> {
    return window_size_;
}

void PlannerUi::remember_window_size(WindowSize size) {
    window_size_ = size;
    ImGui::MarkIniSettingsDirty();
}

void PlannerUi::request_close() {
    if (editor_modified_ || source_modified_ || (document_ && document_->dirty())) {
        close_requested_ = true;
    } else {
        close_confirmed_ = true;
    }
}

auto PlannerUi::take_close_confirmation() -> bool {
    return std::exchange(close_confirmed_, false);
}

auto PlannerUi::window_title() const -> std::string {
    auto title{std::string{"Memory Layout Planner"}};
    if (!project_path_.empty()) {
        title += " - " + project_path_.filename().string();
    }
    if (editor_modified_ || source_modified_ || (document_ && document_->dirty())) {
        title += " *";
    }
    return title;
}

auto PlannerUi::settings_read_open(ImGuiContext*, ImGuiSettingsHandler* handler, char const*)
    -> void* {
    return handler->UserData;
}

void PlannerUi::settings_read_line(ImGuiContext*,
                                   ImGuiSettingsHandler*,
                                   void* entry,
                                   char const* line) {
    auto& ui{*static_cast<PlannerUi*>(entry)};
    auto const text{std::string_view{line}};
    if (text.starts_with("Window=")) {
        std::istringstream input{std::string{text.substr(7)}};
        WindowSize size;
        if (input >> size.width >> size.height && size.width > 0 && size.height > 0) {
            ui.window_size_ = size;
        }
    } else if (text.starts_with("Project=")) {
        ui.recent_project_ = text.substr(8);
    } else if (text.starts_with("Target=")) {
        ui.recent_target_ = text.substr(7);
    }
}

void PlannerUi::settings_write_all(ImGuiContext*,
                                   ImGuiSettingsHandler* handler,
                                   ImGuiTextBuffer* output) {
    auto const& ui{*static_cast<PlannerUi*>(handler->UserData)};
    output->append("[LayoutPlanner][Workspace]\n");
    if (ui.window_size_) {
        output->appendf("Window=%d %d\n", ui.window_size_->width, ui.window_size_->height);
    }
    output->appendf(
        "Project=%s\nTarget=%s\n\n", ui.project_input_.c_str(), ui.target_input_.c_str());
}

auto PlannerUi::open_project() -> bool {
    if (editor_modified_ || source_modified_ || (document_ && document_->dirty())) {
        status_ = "Save or undo the current edits before opening another project.";
        return false;
    }

    auto next{layout::Document::load(project_input_, target_input_)};
    auto const valid{next.valid()};
    document_ = std::move(next);
    project_path_ = project_input_;
    target_name_ = target_input_;
    selected_module_.clear();
    selected_declaration_.clear();
    selected_id_.reset();
    selected_source_.clear();
    new_source_text_.clear();
    new_source_path_.clear();
    observed_revision_.reset();
    analysis_ = layout::PlannerAnalysisSession{};
    synchronize();
    ImGui::MarkIniSettingsDirty();
    frame_changed_ = true;
    return valid;
}

void PlannerUi::new_project() {
    if (editor_modified_ || source_modified_ || (document_ && document_->dirty())) {
        status_ = "Save or undo the current edits before creating another project.";
        return;
    }
    document_ = layout::Document::from_source(
        R"((module game :header "Game.h" :backend standard-library))");
    analysis_ = layout::PlannerAnalysisSession{};
    selected_module_ = "game";
    selected_declaration_.clear();
    selected_id_.reset();
    project_path_.clear();
    selected_source_.clear();
    new_source_text_.clear();
    new_source_path_.clear();
    project_input_.clear();
    target_name_ = "game";
    target_input_ = target_name_;
    editor_source_.clear();
    creating_declaration_ = true;
    observed_revision_.reset();
    synchronize();
    status_ = "Set the project path and target, then use Save as.";
}

void PlannerUi::save_as() {
    if (document_->save_as(project_input_, target_input_)) {
        project_path_ = document_->project_path();
        target_name_ = document_->target_name();
        status_ = "Saved.";
        ImGui::MarkIniSettingsDirty();
    } else {
        status_ = "Save failed; see Diagnostics.";
    }
}

void PlannerUi::select_declaration(std::string module, std::string name) {
    selected_module_ = std::move(module);
    selected_declaration_ = std::move(name);
    creating_declaration_ = false;
    editor_modified_ = false;
    auto const declaration{
        document_->query().module(selected_module_).declaration(selected_declaration_)};
    editor_source_ = declaration.source().value_or("");

    auto const& types{document_->types()};
    auto const selected{document_->find_type(selected_module_, selected_declaration_)};
    selected_id_ = selected ? document_->find_declaration(types.identity(*selected)) : std::nullopt;
    analysis_.edit_inputs().selection.select_type(types, selected);
    frame_changed_ = true;
}

void PlannerUi::synchronize() {
    if (!document_ || !document_->valid() || observed_revision_ == document_->revision()) {
        return;
    }

    analysis_.replace_types(*document_, std::nullopt);
    observed_revision_ = document_->revision();
    if (!selected_source_.empty() && !source_modified_) {
        new_source_text_.clear();
        for (auto const& source : document_->sources()) {
            if (source.path == selected_source_) {
                new_source_text_ = source.text;
                break;
            }
        }
    }
    if (!selected_declaration_.empty()) {
        auto const retained{selected_id_};
        if (retained) {
            if (auto const type{document_->declaration_type(*retained)}) {
                auto const& types{document_->types()};
                selected_module_ = types.strings().view(types.module_name(*type));
                selected_declaration_ = types.strings().view(types.name(*type));
            }
        }
        select_declaration(selected_module_, selected_declaration_);
        if (!selected_id_) {
            selected_id_ = retained;
        }
    } else if (!creating_declaration_) {
        for (auto const& module : document_->query().modules()) {
            auto const declarations{module.declarations()};
            if (!declarations.empty()) {
                select_declaration(std::string{module.name()},
                                   std::string{declarations.front().name()});
                break;
            }
        }
    }
    frame_changed_ = true;
}

auto PlannerUi::commit(layout::Transaction const& transaction) -> bool {
    if (!document_->commit(transaction)) {
        status_ = "The transaction was rejected; see Diagnostics.";
        frame_changed_ = true;
        return false;
    }
    status_.clear();
    synchronize();
    return true;
}

auto PlannerUi::draw() -> bool {
    frame_changed_ = false;
    synchronize();
    auto const dockspace{ImGui::DockSpaceOverViewport()};
    if (!std::exchange(layout_initialized_, true) &&
        !ImGui::FindWindowSettingsByID(ImHashStr("Project"))) {
        ImGui::DockBuilderRemoveNode(dockspace);
        ImGui::DockBuilderAddNode(dockspace, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspace, ImGui::GetMainViewport()->WorkSize);
        auto center{dockspace};
        auto const bottom{
            ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, 0.20F, nullptr, &center)};
        auto const left{
            ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, 0.22F, nullptr, &center)};
        auto const right{
            ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.48F, nullptr, &center)};
        ImGui::DockBuilderDockWindow("Project", left);
        ImGui::DockBuilderDockWindow("Declaration", center);
        ImGui::DockBuilderDockWindow("Layout", right);
        ImGui::DockBuilderDockWindow("Variants", right);
        ImGui::DockBuilderDockWindow("Diagnostics", bottom);
        ImGui::DockBuilderFinish(dockspace);
    }
    draw_menu();
    draw_project();
    draw_editor();
    if (document_ && document_->valid()) {
        draw_variants();
        analysis_.refresh(&*document_);
        draw_analysis();
    }
    draw_diagnostics();
    draw_close_confirmation();
    return frame_changed_;
}

void PlannerUi::draw_menu() {
    if (!ImGui::BeginMainMenuBar()) {
        return;
    }
    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("New project")) {
            new_project();
        }
        if (ImGui::MenuItem("Save", "Ctrl+S", false, document_ && document_->valid())) {
            status_ = document_->save() ? "Saved." : "Save failed; see Diagnostics.";
        }
        if (ImGui::MenuItem("Save as", nullptr, false, document_ && document_->valid())) {
            save_as();
        }
        if (ImGui::MenuItem("Close")) {
            request_close();
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Edit")) {
        if (ImGui::MenuItem("Undo", "Ctrl+Z", false, document_ && document_->can_undo())) {
            document_->undo();
            synchronize();
        }
        if (ImGui::MenuItem("Redo", "Ctrl+Y", false, document_ && document_->can_redo())) {
            document_->redo();
            synchronize();
        }
        ImGui::EndMenu();
    }
    ImGui::EndMainMenuBar();

    if (document_ && !ImGui::GetIO().WantTextInput) {
        if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Z) && document_->undo()) {
            synchronize();
        }
        if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Y) && document_->redo()) {
            synchronize();
        }
        if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_S)) {
            status_ = document_->save() ? "Saved." : "Save failed; see Diagnostics.";
        }
    }
}

void PlannerUi::draw_project() {
    ImGui::Begin("Project");
    ImGui::InputText("Project file", &project_input_);
    ImGui::InputText("Target", &target_input_);
    if (file_dialog_ && ImGui::Button("Browse")) {
        auto const result{file_dialog_->open_file(project_input_, "lispb")};
        if (result && *result) {
            project_input_ = (**result).string();
        } else if (!result) {
            if (document_) {
                document_->record_error(result.error());
            } else {
                status_ = result.error();
            }
        }
    }
    if (ImGui::Button("Open project")) {
        open_project();
    }
    ImGui::SameLine();
    if (ImGui::Button("New project")) {
        new_project();
    }
    if (!document_ || !document_->valid()) {
        ImGui::End();
        return;
    }

    ImGui::Separator();
    ImGui::InputText("Filter declarations", &filter_);
    for (auto const& module : document_->query().modules()) {
        auto const name{std::string{module.name()}};
        if (ImGui::TreeNode(name.c_str())) {
            for (auto const& declaration : module.declarations()) {
                auto const declaration_name{std::string{declaration.name()}};
                if (!filter_.empty() && declaration_name.find(filter_) == std::string::npos) {
                    continue;
                }
                if (ImGui::Selectable(declaration_name.c_str(),
                                      selected_module_ == name &&
                                          selected_declaration_ == declaration_name)) {
                    select_declaration(name, declaration_name);
                }
            }
            if (ImGui::Button("New declaration")) {
                selected_module_ = name;
                selected_declaration_.clear();
                selected_id_.reset();
                editor_source_.clear();
                creating_declaration_ = true;
                editor_modified_ = false;
            }
            ImGui::TreePop();
        }
    }
    if (ImGui::CollapsingHeader("Sources")) {
        for (auto const& source : document_->sources()) {
            auto const path{std::string{source.path}};
            if (ImGui::Selectable(path.c_str(), selected_source_ == path)) {
                selected_source_ = path;
                new_source_path_ = path;
                new_source_text_ = source.text;
                source_modified_ = false;
            }
        }
        ImGui::InputText("Source path", &new_source_path_);
        source_modified_ |= ImGui::InputTextMultiline(
            "Source LispB", &new_source_text_, {-1.0F, 260.0F}, ImGuiInputTextFlags_AllowTabInput);
        if (ImGui::Button("Add source")) {
            auto transaction{document_->transaction()};
            transaction.add_source(new_source_path_, new_source_text_);
            if (commit(transaction)) {
                source_modified_ = false;
            }
        }
        ImGui::BeginDisabled(selected_source_.empty());
        if (ImGui::Button("Apply source")) {
            auto transaction{document_->transaction()};
            transaction.replace_source(selected_source_, new_source_text_);
            if (commit(transaction)) {
                source_modified_ = false;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Revert source draft")) {
            source_modified_ = false;
            observed_revision_.reset();
            synchronize();
        }
        if (ImGui::Button("Rename selected source")) {
            auto transaction{document_->transaction()};
            transaction.rename_source(selected_source_, new_source_path_);
            if (commit(transaction)) {
                selected_source_ = new_source_path_;
            }
        }
        if (ImGui::Button("Remove selected source")) {
            auto transaction{document_->transaction()};
            transaction.remove_source(selected_source_);
            if (commit(transaction)) {
                selected_source_.clear();
            }
        }
        ImGui::EndDisabled();
    }
    if (ImGui::CollapsingHeader("Create module")) {
        ImGui::InputText("Source file", &new_module_);
        ImGui::InputTextMultiline("Module LispB", &new_module_source_, {-1.0F, 120.0F});
        if (ImGui::Button("Create module")) {
            auto transaction{document_->transaction()};
            transaction.create_module(new_module_source_, new_module_);
            commit(transaction);
        }
    }
    if (!selected_module_.empty() && ImGui::CollapsingHeader("Selected module")) {
        label(selected_module_);
        ImGui::InputText("Module name", &module_name_input_);
        if (ImGui::Button("Rename module")) {
            auto transaction{document_->transaction()};
            transaction.rename_module(selected_module_, module_name_input_);
            if (commit(transaction)) {
                selected_module_ = module_name_input_;
            }
        }
        if (ImGui::Button("Delete module")) {
            auto transaction{document_->transaction()};
            transaction.delete_module(selected_module_);
            commit(transaction);
        }
    }
    ImGui::End();
}

void PlannerUi::draw_editor() {
    ImGui::Begin("Declaration");
    if (!document_ || !document_->valid() || selected_module_.empty()) {
        ImGui::TextUnformatted("Select a declaration in the project browser.");
        ImGui::End();
        return;
    }

    ImGui::Text("%s / %s",
                selected_module_.c_str(),
                creating_declaration_ ? "New declaration" : selected_declaration_.c_str());
    ImGui::TextWrapped("Edit the LispB declaration, then apply the complete change.");
    editor_modified_ |= ImGui::InputTextMultiline(
        "##declaration", &editor_source_, {-1.0F, 320.0F}, ImGuiInputTextFlags_AllowTabInput);
    if (ImGui::Button("Apply")) {
        auto const creating{creating_declaration_};
        auto transaction{document_->transaction()};
        if (creating_declaration_) {
            transaction.create_declaration(selected_module_, editor_source_);
        } else {
            transaction.replace_declaration(
                selected_module_, selected_declaration_, editor_source_);
        }
        if (commit(transaction)) {
            editor_modified_ = false;
            if (creating) {
                auto const declarations{document_->query().module(selected_module_).declarations()};
                assert(!declarations.empty());
                select_declaration(selected_module_, std::string{declarations.back().name()});
            }
        }
    }
    if (!creating_declaration_) {
        ImGui::SameLine();
        if (ImGui::Button("Revert draft")) {
            select_declaration(selected_module_, selected_declaration_);
        }
        ImGui::SameLine();
        if (ImGui::Button("Delete")) {
            auto transaction{document_->transaction()};
            transaction.delete_declaration(selected_module_, selected_declaration_);
            commit(transaction);
        }
        if (ImGui::CollapsingHeader("Rename, duplicate or move")) {
            ImGui::InputText("New name", &declaration_name_input_);
            if (ImGui::Button("Rename")) {
                auto transaction{document_->transaction()};
                transaction.rename_declaration(
                    selected_module_, selected_declaration_, declaration_name_input_);
                if (commit(transaction)) {
                    select_declaration(selected_module_, declaration_name_input_);
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Duplicate")) {
                auto transaction{document_->transaction()};
                transaction.duplicate_declaration(
                    selected_module_, selected_declaration_, declaration_name_input_);
                if (commit(transaction)) {
                    select_declaration(selected_module_, declaration_name_input_);
                }
            }
            ImGui::InputText("Destination module", &destination_module_);
            if (ImGui::Button("Move")) {
                auto transaction{document_->transaction()};
                transaction.move_declaration(
                    selected_module_, selected_declaration_, destination_module_);
                if (commit(transaction)) {
                    select_declaration(destination_module_, selected_declaration_);
                }
            }
        }
    }

    if (!creating_declaration_ && ImGui::CollapsingHeader("Quick edit")) {
        ImGui::InputText("Field / member", &override_member_);
        ImGui::InputInt("Bits", &override_bits_);
        if (ImGui::Button("Set packed field width")) {
            auto transaction{document_->transaction()};
            transaction.set_field_bits(
                selected_module_, selected_declaration_, override_member_, override_bits_);
            commit(transaction);
        }
        ImGui::InputText("Type", &override_type_);
        if (ImGui::Button("Set member type")) {
            auto transaction{document_->transaction()};
            transaction.set_member_type(
                selected_module_, selected_declaration_, override_member_, override_type_);
            commit(transaction);
        }
        if (ImGui::Button("Set packed storage")) {
            auto transaction{document_->transaction()};
            transaction.set_packed_storage(selected_module_, selected_declaration_, override_type_);
            commit(transaction);
        }
    }
    ImGui::End();
}

void PlannerUi::draw_analysis() {
    ImGui::Begin("Layout");
    auto elements{analysis_.inputs().workspace.element_count()};
    if (ImGui::InputScalar("Element count", ImGuiDataType_U64, &elements) && elements > 0) {
        analysis_.edit_inputs().workspace.set_element_count(elements);
        analysis_.refresh(&*document_);
    }
    auto contiguous{analysis_.inputs().soa_allocation_strategy ==
                    layout::SoaAllocationStrategy::aligned_contiguous};
    if (ImGui::Checkbox("Contiguous SoA allocation", &contiguous)) {
        analysis_.edit_inputs().soa_allocation_strategy =
            contiguous ? layout::SoaAllocationStrategy::aligned_contiguous
                       : layout::SoaAllocationStrategy::separate_columns;
        analysis_.refresh(&*document_);
    }
    if (ImGui::CollapsingHeader("Access workload")) {
        auto operation{static_cast<int>(analysis_.inputs().access_operation)};
        if (ImGui::Combo("Operation", &operation, "Read\0Write\0Read and write\0")) {
            analysis_.edit_inputs().access_operation =
                static_cast<layout::AccessOperation>(operation);
        }
        auto multiplicity{analysis_.inputs().access_multiplicity};
        if (ImGui::InputScalar("Accesses per element", ImGuiDataType_U64, &multiplicity) &&
            multiplicity > 0) {
            analysis_.edit_inputs().access_multiplicity = multiplicity;
        }
        auto const& selection{analysis_.inputs().selection};
        if (ImGui::BeginCombo("Accessed member",
                              selection.field.empty() ? "None" : selection.field.c_str())) {
            auto const& values{analysis_.results()};
            if (ImGui::Selectable("None", selection.field.empty())) {
                analysis_.edit_inputs().selection.field.clear();
            }
            if (values.active_packed) {
                for (auto const& field : values.active_packed->fields) {
                    if (ImGui::Selectable(field.name.c_str(), selection.field == field.name)) {
                        analysis_.edit_inputs().selection.field = field.name;
                    }
                }
            }
            if (values.record_analysis) {
                for (auto const& member : values.record_analysis->members) {
                    if (ImGui::Selectable(member.name.c_str(), selection.field == member.name)) {
                        analysis_.edit_inputs().selection.field = member.name;
                    }
                }
            }
            if (values.active_soa) {
                for (auto const& column : values.active_soa->columns) {
                    if (ImGui::Selectable(column.name.c_str(), selection.field == column.name)) {
                        analysis_.edit_inputs().selection.field = column.name;
                    }
                }
            }
            ImGui::EndCombo();
        }
        analysis_.refresh(&*document_);
    }
    if (ImGui::CollapsingHeader("Target profiles")) {
        ImGui::InputText("Primary profile", &profile_path_);
        if (ImGui::Button("Load primary profile")) {
            auto profile{layout::load_abi_profile(profile_path_)};
            if (profile) {
                analysis_.set_primary_abi(std::move(*profile));
            } else {
                document_->record_error(profile.error().message);
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Use host profile")) {
            analysis_.set_primary_abi(layout::AbiProfile::host_common());
        }
        ImGui::InputText("Comparison profile", &comparison_profile_path_);
        if (ImGui::Button("Load comparison profile")) {
            auto profile{layout::load_abi_profile(comparison_profile_path_)};
            if (profile) {
                analysis_.set_comparison_abi(std::move(*profile));
            } else {
                document_->record_error(profile.error().message);
            }
        }
        analysis_.refresh(&*document_);
    }

    auto const& results{analysis_.results()};
    if (results.active_packed) {
        packed_layout(*results.active_packed);
    }
    if (results.record_analysis) {
        record_layout(*results.record_analysis);
    }
    if (results.active_soa) {
        soa_layout(*results.active_soa);
    }
    if (results.union_analysis) {
        metric("Union size (bytes)", results.union_analysis->size_bytes);
        metric("Union alignment (bytes)", results.union_analysis->alignment_bytes);
    }
    if (results.tagged_union_analysis) {
        metric("Tagged union size (bytes)", results.tagged_union_analysis->size_bytes);
        metric("Tagged union alignment (bytes)", results.tagged_union_analysis->alignment_bytes);
    }
    if (results.enum_domain) {
        metric("Enum live values", results.enum_domain->live_value_count);
        metric("Enum effective width", results.enum_domain->effective_bit_width);
        metric("Enum backing bits", results.enum_domain->backing_bits);
    }
    if (results.integer_scalar_analysis) {
        metric("Integer effective width", results.integer_scalar_analysis->effective_bit_width);
        metric("Integer required width", results.integer_scalar_analysis->minimum_required_bits);
        metric("Integer unused codes", results.integer_scalar_analysis->unused_codes);
    }
    if (results.linear_quantized_analysis) {
        auto const& value{*results.linear_quantized_analysis};
        metric("Quantized width", value.encoded_storage_bits);
        ImGui::Text("Resolution: %.9g", static_cast<double>(value.resolution));
        ImGui::Text("Maximum rounding error: %.9g",
                    static_cast<double>(value.maximum_rounding_error));
    }
    if (results.integer_varint_analysis) {
        metric("Minimum encoded bytes", results.integer_varint_analysis->minimum_encoded_bytes);
        metric("Maximum encoded bytes", results.integer_varint_analysis->maximum_encoded_bytes);
        metric("Maximum total bytes", results.integer_varint_analysis->maximum_total_bytes);
    }
    if (results.fixed_point_analysis) {
        metric("Fixed point bits", results.fixed_point_analysis->total_bits);
        ImGui::Text("Resolution: %.9g",
                    static_cast<double>(results.fixed_point_analysis->resolution));
    }
    if (results.mini_float_analysis) {
        metric("Mini float bits", results.mini_float_analysis->total_bits);
        ImGui::Text(
            "Maximum relative rounding error: %.9g",
            static_cast<double>(results.mini_float_analysis->maximum_relative_rounding_error));
    }
    if (results.optional_sentinel_analysis) {
        metric("Optional sentinel bits", results.optional_sentinel_analysis->encoded_storage_bits);
    }
    if (results.optional_presence_bit_analysis) {
        metric("Optional presence bits",
               results.optional_presence_bit_analysis->encoded_storage_bits);
    }
    if (results.packed_access_analysis) {
        ImGui::SeparatorText("Packed access");
        metric("Useful bits", results.packed_access_analysis->useful_bits);
        metric("Cache bytes touched", results.packed_access_analysis->minimum_cache_bytes_touched);
        metric("Pages touched", results.packed_access_analysis->minimum_pages_touched);
    }
    if (results.record_access_analysis) {
        ImGui::SeparatorText("Record access");
        metric("Useful bytes", results.record_access_analysis->useful_bytes);
        metric("Cache bytes touched", results.record_access_analysis->cache_bytes_touched);
        metric("Pages touched", results.record_access_analysis->pages_touched);
    }
    if (results.soa_access_analysis) {
        ImGui::SeparatorText("SoA access");
        metric("Useful bytes", results.soa_access_analysis->useful_bytes);
        metric("Cache bytes touched", results.soa_access_analysis->minimum_cache_bytes_touched);
        metric("Pages touched", results.soa_access_analysis->minimum_pages_touched);
    }

    if (ImGui::CollapsingHeader("Target comparison")) {
        ImGui::Text("%s / %s",
                    analysis_.primary_abi().name().c_str(),
                    analysis_.comparison_abi().name().c_str());
        if (results.packed_target_comparison) {
            comparison("Packed storage bytes",
                       results.packed_target_comparison->first.aggregate.total_storage_bytes,
                       results.packed_target_comparison->second.aggregate.total_storage_bytes);
        }
        if (results.record_target_comparison) {
            comparison("Record size",
                       results.record_target_comparison->first.size_bytes,
                       results.record_target_comparison->second.size_bytes);
        }
        if (results.soa_target_comparison) {
            comparison("SoA allocation bytes",
                       results.soa_target_comparison->first.total_allocation_bytes,
                       results.soa_target_comparison->second.total_allocation_bytes);
        }
        if (results.union_target_comparison) {
            comparison("Union size",
                       results.union_target_comparison->first.size_bytes,
                       results.union_target_comparison->second.size_bytes);
        }
        if (results.tagged_union_target_comparison) {
            comparison("Tagged union size",
                       results.tagged_union_target_comparison->first.size_bytes,
                       results.tagged_union_target_comparison->second.size_bytes);
        }
    }

    if (auto const selected{analysis_.inputs().selection.type};
        selected && ImGui::CollapsingHeader("Dependencies")) {
        auto const& types{document_->types()};
        for (auto const dependency : types.dependencies_of(*selected)) {
            label(types.strings().view(types.cpp_spelling(dependency)));
        }
        ImGui::SeparatorText("Used by");
        for (auto const user : types.users_of(*selected)) {
            label(types.strings().view(types.cpp_spelling(user)));
        }
    }
    ImGui::End();
}

void PlannerUi::draw_variants() {
    ImGui::Begin("Variants");
    auto const active{analysis_.inputs().workspace.active_variant_id()};
    for (auto const& variant : analysis_.inputs().workspace.variants()) {
        ImGui::PushID(static_cast<int>(variant.id.value));
        if (ImGui::Selectable(variant.name.c_str(), active == variant.id)) {
            analysis_.edit_inputs().workspace.select_variant(variant.id);
            variant_name_ = variant.name;
        }
        ImGui::PopID();
    }
    ImGui::InputText("Variant name", &variant_name_);
    if (ImGui::Button("Create")) {
        auto transaction{document_->transaction()};
        transaction.create_variant(variant_name_);
        if (commit(transaction)) {
            auto const id{analysis_.inputs().workspace.variants().back().id};
            analysis_.edit_inputs().workspace.select_variant(id);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Duplicate")) {
        auto transaction{document_->transaction()};
        transaction.duplicate_variant(active, variant_name_);
        if (commit(transaction)) {
            auto const id{analysis_.inputs().workspace.variants().back().id};
            analysis_.edit_inputs().workspace.select_variant(id);
        }
    }
    ImGui::BeginDisabled(active == layout::VariantId{});
    if (ImGui::Button("Rename")) {
        auto transaction{document_->transaction()};
        transaction.rename_variant(active, variant_name_);
        commit(transaction);
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset overrides")) {
        auto transaction{document_->transaction()};
        transaction.reset_variant(active);
        commit(transaction);
    }
    ImGui::SameLine();
    if (ImGui::Button("Delete variant")) {
        auto transaction{document_->transaction()};
        transaction.delete_variant(active);
        commit(transaction);
    }

    ImGui::BeginDisabled(selected_declaration_.empty());
    ImGui::SeparatorText("Selected declaration overrides");
    ImGui::InputText("Field / column", &override_member_);
    ImGui::InputInt("Width", &override_bits_);
    ImGui::BeginDisabled(override_bits_ <= 0);
    if (ImGui::Button("Override field width")) {
        auto transaction{document_->transaction()};
        transaction.set_variant_field_bits(active,
                                           selected_module_,
                                           selected_declaration_,
                                           override_member_,
                                           static_cast<std::uint32_t>(override_bits_));
        commit(transaction);
    }
    ImGui::EndDisabled();
    ImGui::InputText("Physical type", &override_type_);
    if (ImGui::Button("Override packed storage")) {
        auto transaction{document_->transaction()};
        transaction.set_variant_packed_storage(
            active, selected_module_, selected_declaration_, override_type_);
        commit(transaction);
    }
    if (ImGui::Button("Override column type")) {
        auto transaction{document_->transaction()};
        transaction.set_variant_member_type(
            active, selected_module_, selected_declaration_, override_member_, override_type_);
        commit(transaction);
    }
    ImGui::InputScalar("Capacity", ImGuiDataType_U64, &override_capacity_);
    if (ImGui::Button("Override capacity")) {
        auto transaction{document_->transaction()};
        transaction.set_variant_capacity(
            active, selected_module_, selected_declaration_, override_capacity_);
        commit(transaction);
    }
    ImGui::EndDisabled();
    ImGui::EndDisabled();

    if (ImGui::CollapsingHeader("Compare variants")) {
        auto const draw_picker{[&](char const* name, layout::VariantId selected, bool first) {
            auto const* variant{analysis_.inputs().workspace.variant(selected)};
            assert(variant);
            if (ImGui::BeginCombo(name, variant->name.c_str())) {
                for (auto const& candidate : analysis_.inputs().workspace.variants()) {
                    if (ImGui::Selectable(candidate.name.c_str(), selected == candidate.id)) {
                        auto& inputs{analysis_.edit_inputs()};
                        if (first) {
                            inputs.comparison_a_variant_id = candidate.id;
                        } else {
                            inputs.comparison_b_variant_id = candidate.id;
                            inputs.comparison_b_follows_active = false;
                        }
                    }
                }
                ImGui::EndCombo();
            }
        }};
        draw_picker("First", analysis_.inputs().comparison_a_variant_id, true);
        draw_picker("Second", analysis_.inputs().comparison_b_variant_id, false);
        analysis_.refresh(&*document_);
        auto const& result{analysis_.results()};
        if (result.comparison_a_packed && result.comparison_b_packed) {
            comparison("Packed storage bytes",
                       result.comparison_a_packed->aggregate.total_storage_bytes,
                       result.comparison_b_packed->aggregate.total_storage_bytes);
        }
        if (result.comparison_a_soa && result.comparison_b_soa) {
            comparison("SoA allocation bytes",
                       result.comparison_a_soa->total_allocation_bytes,
                       result.comparison_b_soa->total_allocation_bytes);
        }
    }
    ImGui::End();
}

void PlannerUi::draw_diagnostics() {
    ImGui::Begin("Diagnostics");
    if (!status_.empty()) {
        label(status_);
    }
    if (document_) {
        auto const& diagnostics{document_->diagnostics()};
        auto const count{diagnostics.size()};
        for (std::size_t index{}; index < count; ++index) {
            auto const id{lispb::DiagnosticId{static_cast<std::uint32_t>(index)}};
            auto const severity{diagnostics.severity(id)};
            ImGui::TextUnformatted(severity == lispb::DiagnosticSeverity::error     ? "Error:"
                                   : severity == lispb::DiagnosticSeverity::warning ? "Warning:"
                                                                                    : "Note:");
            ImGui::SameLine();
            label(diagnostics.message(id));
        }
    }
    ImGui::End();
}

void PlannerUi::draw_close_confirmation() {
    if (std::exchange(close_requested_, false)) {
        ImGui::OpenPopup("Unsaved changes");
    }
    if (ImGui::BeginPopupModal("Unsaved changes", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("There are unsaved changes.");
        if (ImGui::Button("Discard and close")) {
            close_confirmed_ = true;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}
}
