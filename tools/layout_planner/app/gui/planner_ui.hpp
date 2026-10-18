#pragma once

#include "window_size.hpp"
#include <ioj/layout/document.hpp>
#include <ioj/layout/planner_controller.hpp>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

struct ImGuiContext;
struct ImGuiSettingsHandler;
struct ImGuiTextBuffer;

namespace ioj::layout {
class EditorSession;
}

namespace ioj::layout_planner {
class FileDialog;

class PlannerUi {
  public:
    // The editor must outlive the UI; a supplied document must belong to that editor.
    explicit PlannerUi(layout::EditorSession& editor,
                       std::filesystem::path project_path = {},
                       std::string target_name = {});
    PlannerUi(layout::EditorSession& editor, layout::Document document);

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

    void open_project();
    void new_project();
    void save_as();
    void select_declaration(std::string module, std::string name);
    void synchronize();
    void commit(layout::Transaction transaction);
    void draw_menu();
    void draw_project();
    void draw_editor();
    void draw_analysis();
    void draw_distribution();
    void draw_variants();
    void draw_diagnostics();
    void draw_close_confirmation();

    layout::EditorSession& editor_;
    layout::PlannerController controller_;
    layout::Document* document_{};
    FileDialog* file_dialog_{};
    std::filesystem::path project_path_;
    std::string target_name_;
    std::optional<WindowSize> window_size_;
    std::string selected_module_;
    std::string selected_declaration_;
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
    std::string distribution_source_;
    int distribution_kind_{};
    std::string variant_name_;
    std::string override_member_;
    std::string override_type_;
    int override_bits_{8};
    std::uint64_t override_capacity_{65536};
    bool creating_declaration_{};
    bool editor_modified_{};
    bool source_modified_{};
    bool apply_requested_{};
    bool declaration_apply_requested_{};
    bool source_apply_requested_{};
    std::optional<std::string> source_after_edit_;
    bool close_requested_{};
    bool close_confirmed_{};
    bool frame_changed_{};
    bool layout_initialized_{};
    std::string status_;
};
}
