#pragma once

#include <ioj/layout/abi_profile.hpp>
#include <ioj/layout/analyzer.hpp>
#include <ioj/layout/lispb_adapter.hpp>
#include <ioj/layout/workspace.hpp>

#include <array>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <vector>

namespace ioj::layout_planner {

class PlannerUi {
  public:
    explicit PlannerUi(layout::CatalogLoadResult loaded);

    auto draw() -> bool;
  private:
    void setup_default_dock_layout(unsigned int dockspace_id);
    void refresh_analysis();
    void draw_project_panel();
    void draw_layout_panel();
    void draw_properties_panel();
    void draw_variants_panel();
    void draw_comparison_panel();
    void draw_packed_layout(layout::PackedLayout const& layout,
                            layout::PackedAnalysis const& baseline,
                            layout::PackedAnalysis const& active);
    void draw_soa_layout(layout::SoaLayout const& layout,
                         layout::SoaAnalysis const& baseline,
                         layout::SoaAnalysis const& active);
    void draw_diagnostics(std::vector<layout::Diagnostic> const& diagnostics) const;
    void sync_variant_name();
    void create_variant_for_selected_schema();

    layout::LayoutWorkspace workspace_;
    layout::AbiProfile abi_{layout::AbiProfile::host_common()};
    std::vector<layout::Diagnostic> load_diagnostics_;
    std::optional<layout::SchemaId> selected_schema_;
    std::string selected_field_;

    std::uint64_t cached_revision_{};
    std::optional<layout::SchemaId> cached_schema_;
    std::optional<layout::PackedAnalysis> baseline_packed_;
    std::optional<layout::PackedAnalysis> active_packed_;
    std::optional<layout::SoaAnalysis> baseline_soa_;
    std::optional<layout::SoaAnalysis> active_soa_;

    std::array<char, 128> variant_name_{};
    std::array<char, 128> schema_filter_{};
    std::uint64_t variant_name_id_{std::numeric_limits<std::uint64_t>::max()};
    std::uint64_t next_variant_number_{1};
    bool dock_layout_initialized_{};
};

} // namespace ioj::layout_planner
