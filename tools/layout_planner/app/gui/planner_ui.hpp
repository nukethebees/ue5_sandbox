#pragma once

#include <ioj/layout/abi_profile.hpp>
#include <ioj/layout/analyzer.hpp>
#include <ioj/layout/schema_loader.hpp>
#include <ioj/layout/workspace.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <optional>
#include <string>
#include <vector>

struct ImGuiContext;
struct ImGuiSettingsHandler;
struct ImGuiTextBuffer;

namespace ioj::layout_planner {

struct WindowSize {
    int width{};
    int height{};
};

class PlannerUi {
  public:
    explicit PlannerUi(layout::SchemaLoadResult loaded);

    void register_settings_handler();
    void finish_startup(bool reopen_recent_project);
    auto saved_window_size() const -> std::optional<WindowSize>;
    void remember_window_size(WindowSize size);
    void request_close();
    auto take_close_confirmation() -> bool;
    auto draw() -> bool;
  private:
    static auto settings_read_open(ImGuiContext* context,
                                   ImGuiSettingsHandler* handler,
                                   char const* name) -> void*;
    static void settings_read_line(ImGuiContext* context,
                                   ImGuiSettingsHandler* handler,
                                   void* entry,
                                   char const* line);
    static void settings_write_all(ImGuiContext* context,
                                   ImGuiSettingsHandler* handler,
                                   ImGuiTextBuffer* output);
    void validate_comparison_variants();
    void setup_default_dock_layout(unsigned int dockspace_id);
    auto draw_view_menu() -> bool;
    auto draw_file_menu() -> bool;
    void draw_source_preview();
    void draw_close_confirmation();
    void draw_project_path_dialogs();
    void draw_new_enum_dialog();
    void draw_new_packed_value_dialog();
    void draw_enum_editor(lispb::schema::TypeNode const& node,
                          lispb::schema::EnumType const& enumeration);
    auto draw_packed_editor(lispb::schema::TypeNode const& node,
                            lispb::schema::PackedType const& packed) -> bool;
    auto apply_document_edit(lispb::schema::SchemaEditCommand command,
                             std::optional<lispb::schema::TypeIdentity> selection = std::nullopt)
        -> bool;
    void sync_document_graph(std::optional<lispb::schema::TypeIdentity> selection);
    auto load_project(std::filesystem::path const& path, bool allow_dirty = false) -> bool;
    void adopt_loaded_schema(layout::SchemaLoadResult loaded);
    void remember_recent_project(std::filesystem::path const& path);
    void refresh_analysis();
    void draw_project_panel();
    void draw_layout_panel();
    void draw_properties_panel();
    void draw_variants_panel();
    void draw_comparison_panel();
    void draw_packed_layout(lispb::schema::PackedType const& packed,
                            layout::PackedAnalysis const& baseline);
    void draw_soa_layout(lispb::schema::SoaType const& soa, layout::SoaAnalysis const& baseline);
    void draw_diagnostics(std::vector<layout::Diagnostic> const& diagnostics) const;
    void sync_variant_name();
    void create_variant_for_selected_schema();

    std::filesystem::path project_path_;
    std::string target_name_;
    std::vector<std::filesystem::path> recent_projects_;
    std::optional<lispb::schema::EditableSchemaDocument> document_;
    layout::LayoutWorkspace workspace_;
    layout::AbiProfile abi_{layout::AbiProfile::host_common()};
    std::vector<layout::Diagnostic> load_diagnostics_;
    std::optional<lispb::schema::TypeId> selected_type_;
    std::string selected_field_;

    std::uint64_t cached_revision_{};
    std::optional<lispb::schema::TypeId> cached_type_;
    std::uint64_t cached_comparison_a_variant_id_{std::numeric_limits<std::uint64_t>::max()};
    std::uint64_t cached_comparison_b_variant_id_{std::numeric_limits<std::uint64_t>::max()};
    std::optional<layout::PackedAnalysis> baseline_packed_;
    std::optional<layout::PackedAnalysis> active_packed_;
    std::vector<std::pair<std::uint64_t, layout::PackedAnalysis>> packed_variants_;
    std::optional<layout::SoaAnalysis> baseline_soa_;
    std::optional<layout::SoaAnalysis> active_soa_;
    std::vector<std::pair<std::uint64_t, layout::SoaAnalysis>> soa_variants_;
    std::optional<layout::PackedAnalysis> comparison_a_packed_;
    std::optional<layout::PackedAnalysis> comparison_b_packed_;
    std::optional<layout::SoaAnalysis> comparison_a_soa_;
    std::optional<layout::SoaAnalysis> comparison_b_soa_;

    std::array<char, 128> variant_name_{};
    std::array<char, 128> schema_filter_{};
    std::array<char, 1024> open_project_path_{};
    std::array<char, 1024> save_as_project_path_{};
    std::array<char, 128> new_enum_name_{};
    std::array<char, 128> new_enum_underlying_type_{"std::uint8_t"};
    std::array<char, 128> new_packed_value_name_{};
    std::array<char, 128> new_packed_storage_type_{"std::uint32_t"};
    std::array<char, 128> enum_value_name_{};
    std::array<char, 128> enum_value_initializer_{};
    std::array<char, 128> enum_value_display_name_{};
    std::array<char, 128> enum_value_serialized_name_{};
    std::array<char, 128> packed_storage_type_{};
    std::array<char, 64> packed_invalid_value_{};
    std::array<char, 128> packed_field_name_{};
    std::array<char, 128> packed_field_type_{};
    std::string selected_enumerator_;
    std::optional<lispb::schema::DeclarationId> enum_editor_declaration_;
    std::string enum_editor_value_;
    std::string schema_edit_message_;
    std::size_t new_enum_module_index_{};
    std::size_t new_packed_module_index_{};
    std::optional<lispb::schema::DeclarationId> packed_editor_declaration_;
    std::string packed_editor_field_;
    int packed_field_bits_{1};
    std::optional<std::size_t> packed_dragged_divider_;
    std::optional<std::uint64_t> packed_dragged_variant_id_;
    std::optional<std::uint32_t> packed_dragged_left_width_;
    std::optional<std::uint32_t> packed_dragged_right_width_;
    std::uint64_t variant_name_id_{std::numeric_limits<std::uint64_t>::max()};
    std::uint64_t next_variant_number_{1};
    std::uint64_t comparison_a_variant_id_{layout::LayoutWorkspace::baseline_variant_id};
    std::uint64_t comparison_b_variant_id_{layout::LayoutWorkspace::baseline_variant_id};
    float text_scale_{1.0F};
    std::optional<int> window_width_;
    std::optional<int> window_height_;
    bool dock_layout_initialized_{};
    bool reset_dock_layout_requested_{};
    bool comparison_b_follows_active_{true};
    bool open_new_enum_dialog_{};
    bool open_new_packed_value_dialog_{};
    bool open_source_preview_{};
    bool open_project_dialog_{};
    bool open_save_as_dialog_{};
    bool project_changed_{};
    bool open_close_confirmation_{};
    bool close_confirmed_{};
};

} // namespace ioj::layout_planner
