#include "sandbox/core/ui/heatmap_2d.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace ml::ui::heatmap_2d {
namespace {
auto is_finite(Vector2f const value) noexcept -> bool {
    return std::isfinite(value.x) && std::isfinite(value.y);
}

auto is_finite(Color4f const color) noexcept -> bool {
    return std::isfinite(color.r) && std::isfinite(color.g) && std::isfinite(color.b) &&
           std::isfinite(color.a);
}

auto lerp(Color4f const from, Color4f const to, float const alpha) noexcept -> Color4f {
    return {.r = from.r + (to.r - from.r) * alpha,
            .g = from.g + (to.g - from.g) * alpha,
            .b = from.b + (to.b - from.b) * alpha,
            .a = from.a + (to.a - from.a) * alpha};
}
}

auto is_valid_grid(Grid const& grid) noexcept -> bool {
    if (grid.columns == 0 && grid.rows == 0) {
        return grid.values.empty();
    }
    if (grid.columns <= 0 || grid.rows <= 0) {
        return false;
    }
    auto const expected{static_cast<std::int64_t>(grid.columns) * grid.rows};
    return expected <= std::numeric_limits<std::int32_t>::max() &&
           expected == static_cast<std::int64_t>(grid.values.size());
}

auto is_valid_value_range(ValueRange const range) noexcept -> bool {
    return std::isfinite(range.minimum) && std::isfinite(range.maximum) &&
           range.maximum > range.minimum;
}

auto is_valid_domain(Domain const domain) noexcept -> bool {
    return std::isfinite(domain.minimum_x) && std::isfinite(domain.maximum_x) &&
           std::isfinite(domain.minimum_y) && std::isfinite(domain.maximum_y) &&
           domain.maximum_x > domain.minimum_x && domain.maximum_y > domain.minimum_y;
}

auto is_valid_color_stops(std::span<ColorStop const> const stops) noexcept -> bool {
    if (stops.size() < 2 || stops.front().position != 0.0f || stops.back().position != 1.0f) {
        return false;
    }
    auto previous{stops.front().position};
    if (!is_finite(stops.front().color)) {
        return false;
    }
    for (std::size_t i{1}; i < stops.size(); ++i) {
        auto const& stop{stops[i]};
        if (!std::isfinite(stop.position) || stop.position <= previous || stop.position > 1.0f ||
            !is_finite(stop.color)) {
            return false;
        }
        previous = stop.position;
    }
    return true;
}

auto make_plot_layout(Vector2f const widget_size, LayoutSettings const settings) noexcept
    -> PlotLayout {
    auto const available_width{
        std::max(widget_size.x - settings.padding.left - settings.padding.right, 0.0f)};
    auto const available_height{
        std::max(widget_size.y - settings.padding.top - settings.padding.bottom, 0.0f)};

    PlotLayout result;
    if (!settings.show_axes) {
        result.plot_origin = {settings.padding.left, settings.padding.top};
        result.plot_size = {available_width, available_height};
        return result;
    }
    result.x_label_area_height = std::min(settings.x_label_area_height, available_height);
    result.y_label_area_width = std::min(settings.y_label_area_width, available_width);
    result.plot_origin = {settings.padding.left + result.y_label_area_width +
                              settings.axis_thickness,
                          settings.padding.top};
    result.plot_size = {
        std::max(available_width - result.y_label_area_width - settings.axis_thickness, 0.0f),
        std::max(available_height - result.x_label_area_height - settings.axis_thickness, 0.0f)};
    return result;
}

auto build_color_lut(std::span<ColorStop const> const stops, std::int32_t const entry_count)
    -> std::vector<Color4f> {
    if (!is_valid_color_stops(stops) || entry_count < 2) {
        return {};
    }
    std::vector<Color4f> result;
    result.reserve(static_cast<std::size_t>(entry_count));
    std::size_t upper_index{1};
    for (std::int32_t entry{}; entry < entry_count; ++entry) {
        auto const position{static_cast<float>(entry) / static_cast<float>(entry_count - 1)};
        while (upper_index < stops.size() - 1 && position > stops[upper_index].position) {
            ++upper_index;
        }
        auto const& lower{stops[upper_index - 1]};
        auto const& upper{stops[upper_index]};
        auto const alpha{(position - lower.position) / (upper.position - lower.position)};
        result.push_back(lerp(lower.color, upper.color, alpha));
    }
    return result;
}

auto build_cell_geometry(Grid const& grid,
                         ValueRange const range,
                         std::span<Color4f const> const color_lut,
                         Vector2f const plot_size) -> std::vector<CellGeometry> {
    if (!is_valid_grid(grid) || grid.values.empty() || !is_valid_value_range(range) ||
        color_lut.empty() || !is_finite(plot_size) || plot_size.x <= 0.0f || plot_size.y <= 0.0f) {
        return {};
    }
    std::vector<CellGeometry> result;
    result.reserve(grid.values.size());
    auto const cell_size{Vector2f{plot_size.x / static_cast<float>(grid.columns),
                                  plot_size.y / static_cast<float>(grid.rows)}};
    auto const range_size{range.maximum - range.minimum};
    auto const lut_max{static_cast<std::int32_t>(color_lut.size() - 1)};
    for (std::size_t i{}; i < grid.values.size(); ++i) {
        auto const value{grid.values[i]};
        if (!std::isfinite(value) || value <= range.minimum) {
            continue;
        }
        auto const normalized{std::clamp((value - range.minimum) / range_size, 0.0f, 1.0f)};
        auto const lut_index{std::clamp(
            static_cast<std::int32_t>(std::lround(normalized * static_cast<float>(lut_max))),
            std::int32_t{},
            lut_max)};
        auto const index{static_cast<std::int32_t>(i)};
        auto const column{index % grid.columns};
        auto const row{index / grid.columns};
        result.push_back({.cell_index = index,
                          .column = column,
                          .row = row,
                          .position = {static_cast<float>(column) * cell_size.x,
                                       plot_size.y - static_cast<float>(row + 1) * cell_size.y},
                          .size = cell_size,
                          .color = color_lut[static_cast<std::size_t>(lut_index)]});
    }
    return result;
}

auto build_mesh_batches(std::span<CellGeometry const> const cells, Vector2f const plot_origin)
    -> std::vector<MeshBatch> {
    if (cells.empty() || !is_finite(plot_origin)) {
        return {};
    }
    auto const batch_count{(cells.size() + maximum_cells_per_batch - 1) / maximum_cells_per_batch};
    std::vector<MeshBatch> result;
    result.reserve(batch_count);
    for (std::size_t batch_index{}; batch_index < batch_count; ++batch_index) {
        auto const first{batch_index * maximum_cells_per_batch};
        auto const count{std::min<std::size_t>(maximum_cells_per_batch, cells.size() - first)};
        auto& batch{result.emplace_back()};
        batch.vertices.reserve(count * 4);
        batch.indices.reserve(count * 6);
        for (std::size_t local{}; local < count; ++local) {
            auto const& cell{cells[first + local]};
            auto const top_left{
                Vector2f{plot_origin.x + cell.position.x, plot_origin.y + cell.position.y}};
            auto const bottom_right{Vector2f{top_left.x + cell.size.x, top_left.y + cell.size.y}};
            auto const base{static_cast<std::uint16_t>(batch.vertices.size())};
            batch.vertices.push_back({top_left, {0.0f, 0.0f}, cell.color});
            batch.vertices.push_back({{bottom_right.x, top_left.y}, {1.0f, 0.0f}, cell.color});
            batch.vertices.push_back({bottom_right, {1.0f, 1.0f}, cell.color});
            batch.vertices.push_back({{top_left.x, bottom_right.y}, {0.0f, 1.0f}, cell.color});
            batch.indices.insert(batch.indices.end(),
                                 {base,
                                  static_cast<std::uint16_t>(base + 1),
                                  static_cast<std::uint16_t>(base + 2),
                                  base,
                                  static_cast<std::uint16_t>(base + 2),
                                  static_cast<std::uint16_t>(base + 3)});
        }
    }
    return result;
}

auto Data::set_grid(Grid grid) -> bool {
    if (!is_valid_grid(grid)) {
        return false;
    }
    grid_ = std::move(grid);
    return true;
}

void Data::clear_grid() {
    grid_ = {};
}

auto Data::set_value_range(ValueRange const range) -> bool {
    if (!is_valid_value_range(range)) {
        return false;
    }
    value_range_ = range;
    return true;
}

auto Data::set_domain(Domain const domain) -> bool {
    if (!is_valid_domain(domain)) {
        return false;
    }
    domain_ = domain;
    return true;
}
}
