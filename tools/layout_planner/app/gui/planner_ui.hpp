#pragma once

#include "window_size.hpp"
#include <ioj/layout/document.hpp>
#include <ioj/layout/planner_session.hpp>
#include <ioj/layout/revision.hpp>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

struct ImGuiContext;
struct ImGuiSettingsHandler;
struct ImGuiTextBuffer;

namespace ioj::layout_planner {
class FileDialog;

class PlannerUi {
  public:
    explicit PlannerUi(std::filesystem::path project_path = {}, std::string target_name = {});
    explicit PlannerUi(layout::Document document);

    void set_file_dialog(FileDialog* dialog);
    void register_settings_handler();
    void finish_startup(bool reopen_recent_project);
    auto saved_window_size() const -> std::optional<WindowSize>;
    void remember_window_size(WindowSize size);
    void request_close();
    auto take_close_confirmation() -> bool;
    auto draw() -> bool;
    auto window_title() const -> std::string;
  private:
    static auto settings_read_open(ImGuiContext*, ImGuiSettingsHandler*, char const*) -> void*;
    static void settings_read_line(ImGuiContext*, ImGuiSettingsHandler*, void*, char const*);
    static void settings_write_all(ImGuiContext*, ImGuiSettingsHandler*, ImGuiTextBuffer*);

    auto open_project() -> bool;
    void new_project();
    void save_as();
    void select_declaration(std::string module, std::string name);
    void synchronize();
    auto commit(layout::Transaction const& transaction) -> bool;
    void draw_menu();
    void draw_project();
    void draw_editor();
    void draw_analysis();
    void draw_variants();
    void draw_diagnostics();
    void draw_close_confirmation();

    std::optional<layout::Document> document_;
    layout::PlannerAnalysisSession analysis_;
    FileDialog* file_dialog_{};
    std::filesystem::path project_path_;
    std::string target_name_;
    std::optional<WindowSize> window_size_;
    std::optional<layout::Revision> observed_revision_;
    std::string selected_module_;
    std::string selected_declaration_;
    std::optional<lispb::schema::DeclarationId> selected_id_;
    std::string editor_source_;
    std::string project_input_;
    std::string target_input_;
    std::string recent_project_;
    std::string recent_target_;
    std::string new_module_;
    std::string new_module_source_;
    std::string module_name_input_;
    std::string declaration_name_input_;
    std::string destination_module_;
    std::string new_source_path_;
    std::string new_source_text_;
    std::string selected_source_;
    std::string filter_;
    std::string profile_path_;
    std::string comparison_profile_path_;
    std::string variant_name_;
    std::string override_member_;
    std::string override_type_;
    int override_bits_{8};
    std::uint64_t override_capacity_{65536};
    bool creating_declaration_{};
    bool editor_modified_{};
    bool source_modified_{};
    bool close_requested_{};
    bool close_confirmed_{};
    bool frame_changed_{};
    bool layout_initialized_{};
    std::string status_;
};
}
