#pragma once

#include <ioj/layout/abi_profile.hpp>
#include <ioj/layout/analyzer.hpp>
#include <ioj/layout/schema_loader.hpp>
#include <ioj/layout/workspace.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
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
    auto saved_window_size() const -> std::optional<WindowSize>;
    void remember_window_size(WindowSize size);
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
    void refresh_analysis();
    void draw_project_panel();
    void draw_layout_panel();
    void draw_properties_panel();
    void draw_variants_panel();
    void draw_comparison_panel();
    void draw_packed_layout(lispb::schema::PackedType const& packed,
                            layout::PackedAnalysis const& baseline,
                            layout::PackedAnalysis const& active);
    void draw_soa_layout(lispb::schema::SoaType const& soa,
                         layout::SoaAnalysis const& baseline,
                         layout::SoaAnalysis const& active);
    void draw_diagnostics(std::vector<layout::Diagnostic> const& diagnostics) const;
    void sync_variant_name();
    void create_variant_for_selected_schema();

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
    std::optional<layout::SoaAnalysis> baseline_soa_;
    std::optional<layout::SoaAnalysis> active_soa_;
    std::optional<layout::PackedAnalysis> comparison_a_packed_;
    std::optional<layout::PackedAnalysis> comparison_b_packed_;
    std::optional<layout::SoaAnalysis> comparison_a_soa_;
    std::optional<layout::SoaAnalysis> comparison_b_soa_;

    std::array<char, 128> variant_name_{};
    std::array<char, 128> schema_filter_{};
    std::optional<std::size_t> packed_dragged_divider_;
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
};

} // namespace ioj::layout_planner
