#include "planner_ui.hpp"

#include "platform/file_dialog.hpp"
#include <ioj/layout/abi_profile.hpp>
#include <ioj/layout/analysis_settings.hpp>
#include <ioj/layout/analyzer/access_operation.hpp>
#include <ioj/layout/analyzer/soa_allocation_strategy.hpp>
#include <ioj/layout/editor_session.hpp>

#include <lispb/diagnostics.h>
#include <lispb/schema/declaration_query.h>
#include <lispb/strings.h>

#include <imgui.h>
#include <imgui_internal.h>
#include <misc/cpp/imgui_stdlib.h>

#include <algorithm>
#include <cassert>
#include <charconv>
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

void record_layout(layout::AnalysisStorage const& storage, layout::AggregateLayout const& record) {
    metric("Size (bytes)", record.size_bytes);
    metric("Alignment (bytes)", record.alignment_bytes);
    metric("Internal padding (bytes)", record.internal_padding_bytes);
    metric("Tail padding (bytes)", record.tail_padding_bytes);
    if (ImGui::BeginTable("record", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Member");
        ImGui::TableSetupColumn("Offset");
        ImGui::TableSetupColumn("Extent");
        ImGui::TableSetupColumn("Padding before");
        ImGui::TableHeadersRow();
        auto const end{record.members.begin + record.members.count};
        auto const& members{storage.components};
        for (auto row{record.members.begin}; row < end; ++row) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            label(members.name[row]);
            ImGui::TableNextColumn();
            label(number(members.offset_bytes[row]));
            ImGui::TableNextColumn();
            label(number(members.extent_bytes[row]));
            ImGui::TableNextColumn();
            label(number(members.padding_before_bytes[row]));
        }
        ImGui::EndTable();
    }
}

void packed_layout(layout::AnalysisStorage const& storage, layout::PackedLayout const& packed) {
    ImGui::Text("Storage: %s", packed.storage_type.c_str());
    metric("Storage bits", packed.storage_bits);
    metric("Used bits", packed.bits_used);
    metric("Unused bits", packed.unused_bits);
    metric("Overflow bits", packed.overflow_bits);
    if (ImGui::BeginTable("packed", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Field");
        ImGui::TableSetupColumn("Type");
        ImGui::TableSetupColumn("Bits");
        ImGui::TableSetupColumn("Low bit");
        ImGui::TableHeadersRow();
        auto const end{packed.fields.begin + packed.fields.count};
        auto const& fields{storage.fields};
        for (auto row{packed.fields.begin}; row < end; ++row) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            label(fields.name[row]);
            ImGui::TableNextColumn();
            label(fields.logical_type[row]);
            ImGui::TableNextColumn();
            label(std::to_string(fields.bit_width[row]));
            ImGui::TableNextColumn();
            label(std::to_string(fields.least_significant_bit[row]));
        }
        ImGui::EndTable();
    }
}

void soa_layout(layout::AnalysisStorage const& storage, layout::SoaLayout const& soa) {
    metric("Capacity", soa.capacity);
    metric("Bytes per element", soa.bytes_per_logical_element);
    metric("Payload (bytes)", soa.total_payload_bytes);
    metric("Allocation (bytes)", soa.total_allocation_bytes);
    metric("Alignment padding (bytes)", soa.total_alignment_padding_bytes);
    if (ImGui::BeginTable("soa", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Column");
        ImGui::TableSetupColumn("Type");
        ImGui::TableSetupColumn("Bytes");
        ImGui::TableSetupColumn("Allocation offset");
        ImGui::TableHeadersRow();
        auto const end{soa.columns.begin + soa.columns.count};
        auto const& columns{storage.columns};
        for (auto row{soa.columns.begin}; row < end; ++row) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            label(columns.name[row]);
            ImGui::TableNextColumn();
            label(columns.physical_type[row]);
            ImGui::TableNextColumn();
            label(number(columns.total_bytes[row]));
            ImGui::TableNextColumn();
            label(number(columns.allocation_offset_bytes[row]));
        }
        ImGui::EndTable();
    }
}

}

PlannerUi::PlannerUi(layout::EditorSession& editor,
                     std::filesystem::path project_path,
                     std::string target_name)
    : editor_{editor}
    , controller_{editor}
    , project_path_{std::move(project_path)}
    , target_name_{std::move(target_name)}
    , project_input_{project_path_.string()}
    , target_input_{target_name_} {}

PlannerUi::PlannerUi(layout::EditorSession& editor, layout::Document document)
    : editor_{editor}
    , controller_{editor, std::move(document)}
    , document_{controller_.document()}
    , project_path_{document_->project_path()}
    , target_name_{document_->target_name()}
    , project_input_{project_path_.string()}
    , target_input_{target_name_} {
    assert(&document_->session() == &editor_);
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

void PlannerUi::open_project() {
    if (editor_modified_ || source_modified_) {
        status_ = "Apply or revert the current drafts before opening another project.";
        return;
    }
    controller_.open(project_input_, target_input_);
    creating_declaration_ = false;
    selected_source_.clear();
    new_source_text_.clear();
    new_source_path_.clear();
}

void PlannerUi::new_project() {
    if (editor_modified_ || source_modified_) {
        status_ = "Apply or revert the current drafts before creating another project.";
        return;
    }
    controller_.new_document();
    project_input_.clear();
    target_input_ = "game";
    selected_module_ = "game";
    selected_declaration_.clear();
    selected_source_.clear();
    new_source_text_.clear();
    new_source_path_.clear();
    editor_source_.clear();
    creating_declaration_ = true;
}

void PlannerUi::save_as() {
    controller_.save_as(project_input_, target_input_);
}

void PlannerUi::select_declaration(std::string module, std::string name) {
    controller_.select_declaration(std::move(module), std::move(name));
    creating_declaration_ = false;
    editor_modified_ = false;
}

void PlannerUi::synchronize() {
    document_ = controller_.document();
    if (!document_ || !document_->valid()) {
        return;
    }
    project_path_ = document_->project_path();
    target_name_ = document_->target_name();
    if (!creating_declaration_) {
        selected_module_ = controller_.selected_module();
        selected_declaration_ = controller_.selected_declaration();
        if (!editor_modified_) {
            editor_source_ = document_->query()
                                 .module(selected_module_)
                                 .declaration(selected_declaration_)
                                 .source()
                                 .value_or("");
        }
    }
    if (!selected_source_.empty() && !source_modified_) {
        new_source_text_.clear();
        for (auto const& source : document_->sources()) {
            if (source.path == selected_source_) {
                new_source_text_ = source.text;
                break;
            }
        }
    }
}

void PlannerUi::commit(layout::Transaction transaction) {
    controller_.submit(std::move(transaction));
    apply_requested_ = true;
}

auto PlannerUi::draw() -> bool {
    frame_changed_ = false;
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
        draw_analysis();
    }
    draw_diagnostics();
    draw_close_confirmation();
    if (controller_.drain()) {
        if (apply_requested_) {
            if (controller_.last_edit_succeeded().value_or(false)) {
                if (declaration_apply_requested_) {
                    editor_modified_ = false;
                    creating_declaration_ = false;
                }
                if (source_apply_requested_) {
                    source_modified_ = false;
                }
                if (source_after_edit_) {
                    selected_source_ = std::move(*source_after_edit_);
                }
                status_.clear();
            } else {
                status_ = "The transaction was rejected; see Diagnostics.";
            }
            source_after_edit_.reset();
            apply_requested_ = false;
            declaration_apply_requested_ = false;
            source_apply_requested_ = false;
        }
        synchronize();
        ImGui::MarkIniSettingsDirty();
        frame_changed_ = true;
    }
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
            controller_.save();
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
            controller_.undo();
        }
        if (ImGui::MenuItem("Redo", "Ctrl+Y", false, document_ && document_->can_redo())) {
            controller_.redo();
        }
        ImGui::EndMenu();
    }
    ImGui::EndMainMenuBar();

    if (document_ && !ImGui::GetIO().WantTextInput) {
        if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Z)) {
            controller_.undo();
        }
        if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Y)) {
            controller_.redo();
        }
        if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_S)) {
            controller_.save();
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
            editor_.record_error(result.error());
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
                controller_.select_declaration(name, "");
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
            source_apply_requested_ = true;
            source_after_edit_ = new_source_path_;
            commit(std::move(transaction));
        }
        ImGui::BeginDisabled(selected_source_.empty());
        if (ImGui::Button("Apply source")) {
            auto transaction{document_->transaction()};
            transaction.replace_source(selected_source_, new_source_text_);
            source_apply_requested_ = true;
            commit(std::move(transaction));
        }
        ImGui::SameLine();
        if (ImGui::Button("Revert source draft")) {
            source_modified_ = false;
            synchronize();
        }
        if (ImGui::Button("Rename selected source")) {
            auto transaction{document_->transaction()};
            transaction.rename_source(selected_source_, new_source_path_);
            source_after_edit_ = new_source_path_;
            commit(std::move(transaction));
        }
        if (ImGui::Button("Remove selected source")) {
            auto transaction{document_->transaction()};
            transaction.remove_source(selected_source_);
            source_after_edit_ = "";
            commit(std::move(transaction));
        }
        ImGui::EndDisabled();
    }
    if (ImGui::CollapsingHeader("Create module")) {
        ImGui::InputText("Source file", &new_module_);
        ImGui::InputTextMultiline("Module LispB", &new_module_source_, {-1.0F, 120.0F});
        if (ImGui::Button("Create module")) {
            auto transaction{document_->transaction()};
            transaction.create_module(new_module_source_, new_module_);
            commit(std::move(transaction));
        }
    }
    if (!selected_module_.empty() && ImGui::CollapsingHeader("Selected module")) {
        label(selected_module_);
        ImGui::InputText("Module name", &module_name_input_);
        if (ImGui::Button("Rename module")) {
            auto transaction{document_->transaction()};
            transaction.rename_module(selected_module_, module_name_input_);
            commit(std::move(transaction));
            select_declaration(module_name_input_, selected_declaration_);
        }
        if (ImGui::Button("Delete module")) {
            auto transaction{document_->transaction()};
            transaction.delete_module(selected_module_);
            commit(std::move(transaction));
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
        declaration_apply_requested_ = true;
        auto transaction{document_->transaction()};
        if (creating_declaration_) {
            transaction.create_declaration(selected_module_, editor_source_);
        } else {
            transaction.replace_declaration(
                selected_module_, selected_declaration_, editor_source_);
        }
        commit(std::move(transaction));
        if (creating) {
            controller_.select_created_declaration(selected_module_);
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
            commit(std::move(transaction));
        }
        if (ImGui::CollapsingHeader("Rename, duplicate or move")) {
            ImGui::InputText("New name", &declaration_name_input_);
            if (ImGui::Button("Rename")) {
                auto transaction{document_->transaction()};
                transaction.rename_declaration(
                    selected_module_, selected_declaration_, declaration_name_input_);
                commit(std::move(transaction));
                select_declaration(selected_module_, declaration_name_input_);
            }
            ImGui::SameLine();
            if (ImGui::Button("Duplicate")) {
                auto transaction{document_->transaction()};
                transaction.duplicate_declaration(
                    selected_module_, selected_declaration_, declaration_name_input_);
                commit(std::move(transaction));
                select_declaration(selected_module_, declaration_name_input_);
            }
            ImGui::InputText("Destination module", &destination_module_);
            if (ImGui::Button("Move")) {
                auto transaction{document_->transaction()};
                transaction.move_declaration(
                    selected_module_, selected_declaration_, destination_module_);
                commit(std::move(transaction));
                select_declaration(destination_module_, selected_declaration_);
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
            commit(std::move(transaction));
        }
        ImGui::InputText("Type", &override_type_);
        if (ImGui::Button("Set member type")) {
            auto transaction{document_->transaction()};
            transaction.set_member_type(
                selected_module_, selected_declaration_, override_member_, override_type_);
            commit(std::move(transaction));
        }
        if (ImGui::Button("Set packed storage")) {
            auto transaction{document_->transaction()};
            transaction.set_packed_storage(selected_module_, selected_declaration_, override_type_);
            commit(std::move(transaction));
        }
    }
    ImGui::End();
}

void PlannerUi::draw_analysis() {
    ImGui::Begin("Layout");
    auto const& settings{document_->analysis_settings()};
    auto elements{settings.element_count};
    if (ImGui::InputScalar("Element count", ImGuiDataType_U64, &elements) && elements > 0) {
        auto transaction{document_->transaction()};
        transaction.set_element_count(elements);
        controller_.submit(std::move(transaction));
    }
    auto capacity{settings.default_soa_capacity};
    if (ImGui::InputScalar("Default SoA capacity", ImGuiDataType_U64, &capacity) && capacity > 0) {
        auto transaction{document_->transaction()};
        transaction.set_default_soa_capacity(capacity);
        controller_.submit(std::move(transaction));
    }
    auto contiguous{settings.soa_allocation_strategy ==
                    layout::SoaAllocationStrategy::aligned_contiguous};
    if (ImGui::Checkbox("Contiguous SoA allocation", &contiguous)) {
        auto transaction{document_->transaction()};
        transaction.set_soa_allocation_strategy(
            contiguous ? layout::SoaAllocationStrategy::aligned_contiguous
                       : layout::SoaAllocationStrategy::separate_columns);
        controller_.submit(std::move(transaction));
    }

    auto const& storage{controller_.analysis()};
    if (ImGui::CollapsingHeader("Access workload")) {
        auto operation{static_cast<int>(settings.access_operation)};
        if (ImGui::Combo("Operation", &operation, "Read\0Write\0Read and write\0")) {
            auto transaction{document_->transaction()};
            transaction.set_access_operation(static_cast<layout::AccessOperation>(operation));
            if (auto const selected{controller_.selected_type()}) {
                auto const id{document_->find_declaration(document_->types().identity(*selected))};
                if (id) {
                    if (auto const found{settings.access_sets.find(*id)};
                        found != settings.access_sets.end()) {
                        auto accesses{found->second};
                        for (auto& [name, access] : accesses) {
                            access = static_cast<layout::AccessOperation>(operation);
                        }
                        transaction.set_access_set(
                            selected_module_, selected_declaration_, accesses);
                    }
                }
            }
            controller_.submit(std::move(transaction));
        }
        auto multiplicity{settings.access_multiplicity};
        if (ImGui::InputScalar("Accesses per element", ImGuiDataType_U64, &multiplicity) &&
            multiplicity > 0) {
            auto transaction{document_->transaction()};
            transaction.set_access_multiplicity(multiplicity);
            controller_.submit(std::move(transaction));
        }
        std::string selected_member;
        if (auto const selected{controller_.selected_type()}) {
            auto const id{document_->find_declaration(document_->types().identity(*selected))};
            if (id) {
                if (auto const found{settings.access_sets.find(*id)};
                    found != settings.access_sets.end() && !found->second.empty()) {
                    selected_member = found->second.begin()->first;
                }
            }
        }
        if (ImGui::BeginCombo("Accessed member",
                              selected_member.empty() ? "None" : selected_member.c_str())) {
            auto const choose{[&](std::string_view name) {
                if (ImGui::Selectable(name.empty() ? "None" : std::string{name}.c_str(),
                                      selected_member == name)) {
                    auto transaction{document_->transaction()};
                    layout::AccessSet accesses;
                    if (!name.empty()) {
                        accesses.emplace(name, settings.access_operation);
                    }
                    transaction.set_access_set(selected_module_, selected_declaration_, accesses);
                    controller_.submit(std::move(transaction));
                }
            }};
            if (!selected_declaration_.empty()) {
                choose("");
            }
            if (!storage.runs.empty()) {
                auto const row{storage.runs.front().analysis.value};
                if (auto const& packed{storage.types.packed[row]}) {
                    auto const end{packed->fields.begin + packed->fields.count};
                    for (auto index{packed->fields.begin}; index < end; ++index) {
                        choose(storage.fields.name[index]);
                    }
                }
                if (auto const& record{storage.types.aggregate[row]}) {
                    auto const end{record->members.begin + record->members.count};
                    for (auto index{record->members.begin}; index < end; ++index) {
                        choose(storage.components.name[index]);
                    }
                }
                if (auto const& soa{storage.types.soa[row]}) {
                    auto const end{soa->columns.begin + soa->columns.count};
                    for (auto index{soa->columns.begin}; index < end; ++index) {
                        choose(storage.columns.name[index]);
                    }
                }
            }
            ImGui::EndCombo();
        }
    }
    draw_distribution();
    if (ImGui::CollapsingHeader("Target profiles")) {
        ImGui::InputText("Primary profile", &profile_path_);
        if (ImGui::Button("Load primary profile")) {
            auto profile{layout::load_abi_profile(profile_path_)};
            if (profile) {
                auto transaction{document_->transaction()};
                transaction.set_abi_profile(*profile);
                controller_.submit(std::move(transaction));
            } else {
                editor_.record_error(profile.error().message);
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Use host profile")) {
            auto transaction{document_->transaction()};
            transaction.set_abi_profile(layout::AbiProfile::host_common());
            controller_.submit(std::move(transaction));
        }
        ImGui::InputText("Comparison profile", &comparison_profile_path_);
        if (ImGui::Button("Load comparison profile")) {
            auto profile{layout::load_abi_profile(comparison_profile_path_)};
            if (profile) {
                auto transaction{document_->transaction()};
                transaction.set_comparison_abi_profile(*profile);
                controller_.submit(std::move(transaction));
            } else {
                editor_.record_error(profile.error().message);
            }
        }
    }

    if (!storage.runs.empty()) {
        auto const& run{storage.runs.front()};
        auto const row{run.analysis.value};
        auto const& totals{storage.totals.front()};
        metric("Total storage (bytes)", totals.total_storage_bytes);
        metric("Minimum cache lines", totals.minimum_cache_lines);
        metric("Minimum pages", totals.minimum_pages);
        if (auto const& value{storage.types.packed[row]}) {
            packed_layout(storage, *value);
        }
        if (auto const& value{storage.types.aggregate[row]}) {
            record_layout(storage, *value);
        }
        if (auto const& value{storage.types.soa[row]}) {
            soa_layout(storage, *value);
        }
        if (auto const& value{storage.types.enum_backing[row]}) {
            metric("Enum backing bits", value->backing_bits);
        }

        auto const scalar{storage.scalars.find(storage.types.schema_type[row])};
        {
            if (auto const& value{storage.scalars.enum_domain[scalar]}) {
                metric("Enum live values", value->live_value_count);
                metric("Enum effective width", value->effective_bit_width);
            }
            if (auto const& value{storage.scalars.integer_domain[scalar]}) {
                metric("Integer effective width", value->effective_bit_width);
                metric("Integer required width", value->minimum_required_bits);
                metric("Integer unused codes", value->unused_codes);
            }
            if (auto const& value{storage.scalars.quantized_encoding[scalar]}) {
                metric("Quantized width", value->encoded_storage_bits);
                ImGui::Text("Resolution: %.9g", static_cast<double>(value->resolution));
                ImGui::Text("Maximum rounding error: %.9g",
                            static_cast<double>(value->maximum_rounding_error));
            }
            if (auto const& value{storage.scalars.varint_encoding[scalar]}) {
                metric("Minimum encoded bytes", value->minimum_encoded_bytes);
                metric("Maximum encoded bytes", value->maximum_encoded_bytes);
                metric("Maximum total bytes", totals.maximum_total_bytes);
            }
            if (auto const& value{storage.scalars.fixed_encoding[scalar]}) {
                metric("Fixed point bits", value->total_bits);
                ImGui::Text("Resolution: %.9g", static_cast<double>(value->resolution));
            }
            if (auto const& value{storage.scalars.float_encoding[scalar]}) {
                metric("Mini float bits", value->total_bits);
                ImGui::Text("Maximum relative rounding error: %.9g",
                            static_cast<double>(value->maximum_relative_rounding_error));
            }
            if (auto const& value{storage.scalars.optional_sentinel_encoding[scalar]}) {
                metric("Optional sentinel bits", value->encoded_storage_bits);
            }
            if (auto const& value{storage.scalars.optional_presence_encoding[scalar]}) {
                metric("Optional presence bits", value->encoded_storage_bits);
            }
        }
        if (run.access) {
            auto const access{run.access->value};
            ImGui::SeparatorText("Access footprint");
            metric("Useful bits", storage.workloads.bits[access].useful);
            metric("Useful bytes", storage.workloads.bytes[access].useful);
            metric("Cache bytes touched", storage.workloads.cache[access].bytes);
            metric("Pages touched", storage.workloads.pages[access].count);
        }
        if (run.distribution) {
            auto const distribution{run.distribution->value};
            auto const& values{storage.distributions};
            ImGui::SeparatorText("Distribution");
            metric("Total weight", values.total_weight[distribution]);
            if (values.expected_extent_bytes[distribution]) {
                ImGui::Text("Expected extent (bytes): %.9g",
                            static_cast<double>(*values.expected_extent_bytes[distribution]));
            }
            if (values.expected_slack_bytes[distribution]) {
                ImGui::Text("Expected slack (bytes): %.9g",
                            static_cast<double>(*values.expected_slack_bytes[distribution]));
            }
        }
        if (settings.comparison_abi && ImGui::CollapsingHeader("Target comparison")) {
            ImGui::Text(
                "%s / %s", settings.abi.name().c_str(), settings.comparison_abi->name().c_str());
            comparison("Total storage bytes",
                       totals.total_storage_bytes,
                       storage.totals[1].total_storage_bytes);
            auto const second{storage.runs[1].analysis.value};
            if (storage.types.soa[row] && storage.types.soa[second]) {
                comparison("SoA allocation bytes",
                           storage.types.soa[row]->total_allocation_bytes,
                           storage.types.soa[second]->total_allocation_bytes);
            }
        }
    }
    if (auto const selected{controller_.selected_type()};
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

void PlannerUi::draw_distribution() {
    if (selected_declaration_.empty() || !ImGui::CollapsingHeader("Value distribution")) {
        return;
    }
    ImGui::Combo("Distribution kind",
                 &distribution_kind_,
                 "Varint values\0Union alternatives\0Tagged union tags\0");
    auto const selected{controller_.selected_type()};
    if (selected) {
        auto const id{document_->find_declaration(document_->types().identity(*selected))};
        auto const& settings{document_->analysis_settings()};
        if (id && distribution_kind_ == 0) {
            if (auto const found{settings.varint_distributions.find(*id)};
                found != settings.varint_distributions.end()) {
                for (auto const& entry : found->second) {
                    ImGui::Text("%s: %llu",
                                lispb::codegen::format_packed_integer(entry.value).c_str(),
                                static_cast<unsigned long long>(entry.weight));
                }
            }
        } else if (id && distribution_kind_ == 1) {
            if (auto const found{settings.union_distributions.find(*id)};
                found != settings.union_distributions.end()) {
                for (auto const& entry : found->second) {
                    ImGui::Text("%s: %llu",
                                entry.alternative_name.c_str(),
                                static_cast<unsigned long long>(entry.weight));
                }
            }
        } else if (id) {
            if (auto const found{settings.tagged_union_distributions.find(*id)};
                found != settings.tagged_union_distributions.end()) {
                for (auto const& entry : found->second) {
                    ImGui::Text("%s: %llu",
                                entry.tag.c_str(),
                                static_cast<unsigned long long>(entry.weight));
                }
            }
        }
    }
    ImGui::TextWrapped("Enter one value or name and a positive weight per line. An empty list "
                       "clears the distribution.");
    ImGui::InputTextMultiline("Samples", &distribution_source_, {-1.0F, 100.0F});
    if (!ImGui::Button("Apply distribution")) {
        return;
    }

    std::vector<layout::IntegerVarintDistributionEntry> integers;
    std::vector<layout::UnionDistributionEntry> alternatives;
    std::vector<layout::TaggedUnionDistributionEntry> tags;
    std::istringstream input{distribution_source_};
    std::string line;
    while (std::getline(input, line)) {
        if (line.find_first_not_of(" \t\r") == std::string::npos) {
            continue;
        }
        std::istringstream row{line};
        std::string name;
        std::string weight_text;
        std::uint64_t weight{};
        std::string extra;
        if (!(row >> name >> weight_text) || row >> extra) {
            editor_.record_error(
                "Each distribution row requires a value or name followed by a positive weight.");
            return;
        }
        auto const parsed_weight{
            std::from_chars(weight_text.data(), weight_text.data() + weight_text.size(), weight)};
        if (parsed_weight.ec != std::errc{} ||
            parsed_weight.ptr != weight_text.data() + weight_text.size() || weight == 0) {
            editor_.record_error("Distribution weights must be positive integers.");
            return;
        }
        if (distribution_kind_ == 0) {
            auto digits{std::string_view{name}};
            auto const negative{digits.starts_with('-')};
            if (negative) {
                digits.remove_prefix(1);
            }
            std::uint64_t magnitude{};
            auto const parsed{
                std::from_chars(digits.data(), digits.data() + digits.size(), magnitude)};
            if (parsed.ec != std::errc{} || parsed.ptr != digits.data() + digits.size()) {
                editor_.record_error("Varint distribution values must be integers.");
                return;
            }
            integers.push_back(
                {lispb::codegen::PackedIntegerValue::from_parts(negative, magnitude), weight});
        } else if (distribution_kind_ == 1) {
            alternatives.push_back({name, weight});
        } else {
            tags.push_back({name, weight});
        }
    }

    auto transaction{document_->transaction()};
    if (distribution_kind_ == 0) {
        transaction.set_varint_distribution(selected_module_, selected_declaration_, integers);
    } else if (distribution_kind_ == 1) {
        transaction.set_union_distribution(selected_module_, selected_declaration_, alternatives);
    } else {
        transaction.set_tagged_union_distribution(selected_module_, selected_declaration_, tags);
    }
    controller_.submit(std::move(transaction));
}

void PlannerUi::draw_variants() {
    ImGui::Begin("Variants");
    auto const active{controller_.active_variant()};
    auto const& variants{document_->variants()};
    for (auto const variant : variants.ids()) {
        ImGui::PushID(static_cast<int>(variant.value));
        if (ImGui::Selectable(variants.name(variant).c_str(), active == variant)) {
            controller_.select_variant(variant);
            variant_name_ = variants.name(variant);
        }
        ImGui::PopID();
    }
    ImGui::InputText("Variant name", &variant_name_);
    if (ImGui::Button("Create")) {
        auto transaction{document_->transaction()};
        transaction.create_variant(variant_name_);
        commit(std::move(transaction));
        controller_.select_created_variant();
    }
    ImGui::SameLine();
    if (ImGui::Button("Duplicate")) {
        auto transaction{document_->transaction()};
        transaction.duplicate_variant(active, variant_name_);
        commit(std::move(transaction));
        controller_.select_created_variant();
    }
    ImGui::BeginDisabled(active == layout::VariantId{});
    if (ImGui::Button("Rename")) {
        auto transaction{document_->transaction()};
        transaction.rename_variant(active, variant_name_);
        commit(std::move(transaction));
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset overrides")) {
        auto transaction{document_->transaction()};
        transaction.reset_variant(active);
        commit(std::move(transaction));
    }
    ImGui::SameLine();
    if (ImGui::Button("Delete variant")) {
        auto transaction{document_->transaction()};
        transaction.delete_variant(active);
        commit(std::move(transaction));
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
        commit(std::move(transaction));
    }
    ImGui::EndDisabled();
    ImGui::InputText("Physical type", &override_type_);
    if (ImGui::Button("Override packed storage")) {
        auto transaction{document_->transaction()};
        transaction.set_variant_packed_storage(
            active, selected_module_, selected_declaration_, override_type_);
        commit(std::move(transaction));
    }
    if (ImGui::Button("Override column type")) {
        auto transaction{document_->transaction()};
        transaction.set_variant_member_type(
            active, selected_module_, selected_declaration_, override_member_, override_type_);
        commit(std::move(transaction));
    }
    ImGui::InputScalar("Capacity", ImGuiDataType_U64, &override_capacity_);
    if (ImGui::Button("Override capacity")) {
        auto transaction{document_->transaction()};
        transaction.set_variant_capacity(
            active, selected_module_, selected_declaration_, override_capacity_);
        commit(std::move(transaction));
    }
    ImGui::EndDisabled();
    ImGui::EndDisabled();

    if (ImGui::CollapsingHeader("Compare variants")) {
        auto const draw_picker{[&](char const* name, bool first) {
            auto const selected{controller_.comparison_variant(first)};
            if (ImGui::BeginCombo(name, variants.name(selected).c_str())) {
                for (auto const candidate : variants.ids()) {
                    if (ImGui::Selectable(variants.name(candidate).c_str(),
                                          selected == candidate)) {
                        controller_.select_comparison_variant(first, candidate);
                    }
                }
                ImGui::EndCombo();
            }
        }};
        draw_picker("First", true);
        draw_picker("Second", false);
        auto const& storage{controller_.analysis()};
        if (!storage.runs.empty()) {
            comparison("Total storage bytes",
                       storage.totals[2].total_storage_bytes,
                       storage.totals[3].total_storage_bytes);
            auto const first{storage.runs[2].analysis.value};
            auto const second{storage.runs[3].analysis.value};
            if (storage.types.soa[first] && storage.types.soa[second]) {
                comparison("SoA allocation bytes",
                           storage.types.soa[first]->total_allocation_bytes,
                           storage.types.soa[second]->total_allocation_bytes);
            }
        }
    }
    ImGui::End();
}

void PlannerUi::draw_diagnostics() {
    ImGui::Begin("Diagnostics");
    if (!status_.empty()) {
        label(status_);
    }
    auto const& diagnostics{editor_.diagnostics()};
    auto const count{diagnostics.size()};
    for (std::size_t index{}; index < count; ++index) {
        auto const id{lispb::DiagnosticId{static_cast<std::uint32_t>(index)}};
        label(editor_.format_diagnostic(id));
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
