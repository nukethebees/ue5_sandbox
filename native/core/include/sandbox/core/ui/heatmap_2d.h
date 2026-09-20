#pragma once

#include "sandbox/core/ui/types.h"

#include <cstdint>
#include <span>
#include <vector>

namespace ml::ui::heatmap_2d {
struct Grid {
    std::int32_t columns{};
    std::int32_t rows{};
    std::vector<float> values;
};

struct ValueRange {
    float minimum{};
    float maximum{1.0f};
};

struct Domain {
    float minimum_x{};
    float maximum_x{1.0f};
    float minimum_y{};
    float maximum_y{1.0f};
};

struct ColorStop {
    float position{};
    Color4f color{};
};

struct LayoutSettings {
    Insets padding{};
    float axis_thickness{1.0f};
    float x_label_area_height{20.0f};
    float y_label_area_width{36.0f};
    bool show_axes{true};
};

struct PlotLayout {
    Vector2f plot_origin{};
    Vector2f plot_size{};
    float x_label_area_height{};
    float y_label_area_width{};
};

struct CellGeometry {
    std::int32_t cell_index{-1};
    std::int32_t column{-1};
    std::int32_t row{-1};
    Vector2f position{};
    Vector2f size{};
    Color4f color{};
};

struct MeshVertex {
    Vector2f position{};
    Vector2f texture_coordinate{};
    Color4f color{};
};

struct MeshBatch {
    std::vector<MeshVertex> vertices;
    std::vector<std::uint16_t> indices;
};

[[nodiscard]] auto is_valid_grid(Grid const& grid) noexcept -> bool;
[[nodiscard]] auto is_valid_value_range(ValueRange range) noexcept -> bool;
[[nodiscard]] auto is_valid_domain(Domain domain) noexcept -> bool;
[[nodiscard]] auto is_valid_color_stops(std::span<ColorStop const> stops) noexcept -> bool;
[[nodiscard]] auto make_plot_layout(Vector2f widget_size, LayoutSettings settings) noexcept
    -> PlotLayout;
[[nodiscard]] auto build_color_lut(std::span<ColorStop const> stops, std::int32_t entry_count = 256)
    -> std::vector<Color4f>;
[[nodiscard]] auto build_cell_geometry(Grid const& grid,
                                       ValueRange range,
                                       std::span<Color4f const> color_lut,
                                       Vector2f plot_size) -> std::vector<CellGeometry>;
[[nodiscard]] auto build_mesh_batches(std::span<CellGeometry const> cells, Vector2f plot_origin)
    -> std::vector<MeshBatch>;

class Data {
  public:
    [[nodiscard]] auto set_grid(Grid grid) -> bool;
    void clear_grid();
    [[nodiscard]] auto set_value_range(ValueRange range) -> bool;
    [[nodiscard]] auto set_domain(Domain domain) -> bool;

    [[nodiscard]] auto grid() const noexcept -> Grid const& { return grid_; }
    [[nodiscard]] auto value_range() const noexcept -> ValueRange { return value_range_; }
    [[nodiscard]] auto domain() const noexcept -> Domain { return domain_; }
  private:
    Grid grid_;
    ValueRange value_range_;
    Domain domain_;
};
}
