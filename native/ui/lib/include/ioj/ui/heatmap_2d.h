#pragma once

#include "ioj/ui/types.h"
#include <ioj/constrained_numbers.h>
#include <ioj/min_max.h>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <vector>

namespace ioj::ui::heatmap_2d {
inline constexpr std::uint32_t invalid_index{std::numeric_limits<std::uint32_t>::max()};
inline constexpr std::size_t maximum_cells_per_batch{
    (static_cast<std::size_t>(std::numeric_limits<std::uint16_t>::max()) + 1U) / 4U};

struct GridData {
    using ColumnCount = std::int32_t;
    using RowCount = std::int32_t;

    ColumnCount columns{};
    RowCount rows{};
    std::vector<float> values;
};

inline constexpr auto grid_policy{ConstrainedValuePolicy{
    [](GridData const& grid) {
        if (grid.columns == 0 && grid.rows == 0) {
            return grid.values.empty();
        }
        auto const count{static_cast<std::int64_t>(grid.columns) * grid.rows};
        return grid.columns > 0 && grid.rows > 0 &&
               count <= std::numeric_limits<std::int32_t>::max() &&
               count == static_cast<std::int64_t>(grid.values.size());
    },
    "Heatmap dimensions must match its values and fit an int32 cell count"}};
using Grid = ConstrainedValue<GridData, grid_policy>;

inline constexpr auto range_policy{ConstrainedValuePolicy{
    [](MinMax<float> const range) {
        constexpr auto limit{std::numeric_limits<float>::max()};
        return range.min >= -limit && range.min <= limit && range.max >= -limit &&
               range.max <= limit && range.min < range.max;
    },
    "Heatmap range must have finite ordered endpoints"}};
using ValidRange = ConstrainedValue<MinMax<float>, range_policy>;

struct Domain {
    MinMax<float> x{};
    MinMax<float> y{};
};

inline constexpr auto domain_policy{ConstrainedValuePolicy{
    [](Domain const domain) { return ValidRange::valid(domain.x) && ValidRange::valid(domain.y); },
    "Heatmap domains must have finite ordered endpoints"}};
using ValidDomain = ConstrainedValue<Domain, domain_policy>;

class Data;

struct ColorStop {
    float position{};
    Color4f color{};
};

struct CellGeometry {
    std::uint32_t cell_index{invalid_index};
    std::uint32_t column{invalid_index};
    std::uint32_t row{invalid_index};
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

[[nodiscard]] auto is_valid_color_stops(std::span<ColorStop const> stops) noexcept -> bool;
[[nodiscard]] auto build_color_lut(std::span<ColorStop const> stops, std::int32_t entry_count = 256)
    -> std::vector<Color4f>;
[[nodiscard]] auto build_cell_geometry(Data const& data,
                                       std::span<Color4f const> color_lut,
                                       Vector2f plot_size) -> std::vector<CellGeometry>;
[[nodiscard]] auto build_mesh_batches(std::span<CellGeometry const> cells, Vector2f plot_origin)
    -> std::vector<MeshBatch>;

class Data {
  public:
    void set_grid(Grid grid) { grid_ = std::move(grid); }
    void clear_grid();
    void set_value_range(ValidRange range) { value_range_ = range; }
    void set_domain(ValidDomain domain) { domain_ = domain; }

    [[nodiscard]] auto grid() const noexcept -> GridData const& { return grid_.get_ref(); }
    [[nodiscard]] auto value_range() const noexcept -> MinMax<float> { return value_range_.get(); }
    [[nodiscard]] auto domain() const noexcept -> Domain { return domain_.get(); }
  private:
    Grid grid_;
    ValidRange value_range_;
    ValidDomain domain_;
};
}
