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

namespace ioj::layout_planner {

class PlannerUi {
  public:
    explicit PlannerUi(layout::SchemaLoadResult loaded);

    auto draw() -> bool;
  private:
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
    std::optional<layout::PackedAnalysis> baseline_packed_;
    std::optional<layout::PackedAnalysis> active_packed_;
    std::optional<layout::SoaAnalysis> baseline_soa_;
    std::optional<layout::SoaAnalysis> active_soa_;

    std::array<char, 128> variant_name_{};
    std::array<char, 128> schema_filter_{};
    std::optional<std::size_t> packed_dragged_divider_;
    std::uint64_t variant_name_id_{std::numeric_limits<std::uint64_t>::max()};
    std::uint64_t next_variant_number_{1};
    float text_scale_{1.0F};
    bool dock_layout_initialized_{};
};

} // namespace ioj::layout_planner
