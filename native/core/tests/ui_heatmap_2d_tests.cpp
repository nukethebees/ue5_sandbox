#include "sandbox/core/ui/heatmap_2d.h"

#include <gtest/gtest.h>

#include <limits>
#include <vector>

namespace {
auto grayscale_stops() -> std::vector<ml::ui::heatmap_2d::ColorStop> {
    return {{.position = 0.0f, .color = {0.0f, 0.0f, 0.0f, 1.0f}},
            {.position = 1.0f, .color = {1.0f, 1.0f, 1.0f, 1.0f}}};
}
}

TEST(Heatmap2D, ValidatesGridsRangesDomainsAndColorStops) {
    using namespace ml::ui::heatmap_2d;
    EXPECT_TRUE(is_valid_grid({}));
    EXPECT_TRUE(is_valid_grid({2, 2, {0.0f, 1.0f, 2.0f, 3.0f}}));
    EXPECT_FALSE(is_valid_grid({2, 2, {0.0f, 1.0f}}));
    EXPECT_FALSE(is_valid_grid({.columns = 2, .rows = 0, .values = {}}));
    EXPECT_TRUE(is_valid_value_range({-1.0f, 1.0f}));
    EXPECT_FALSE(is_valid_value_range({1.0f, 1.0f}));
    EXPECT_TRUE(is_valid_domain({-1.0f, 1.0f, -2.0f, 2.0f}));
    EXPECT_FALSE(is_valid_domain({1.0f, -1.0f, -2.0f, 2.0f}));
    EXPECT_TRUE(is_valid_color_stops(grayscale_stops()));
    EXPECT_FALSE(is_valid_color_stops({{{0.0f, {}}, {0.8f, {}}}}));
}

TEST(Heatmap2D, BuildsColorLookupTable) {
    auto const lut{ml::ui::heatmap_2d::build_color_lut(grayscale_stops(), 5)};
    ASSERT_EQ(lut.size(), 5);
    EXPECT_EQ(lut.front(), (ml::ui::Color4f{0.0f, 0.0f, 0.0f, 1.0f}));
    EXPECT_EQ(lut[2], (ml::ui::Color4f{0.5f, 0.5f, 0.5f, 1.0f}));
    EXPECT_EQ(lut.back(), (ml::ui::Color4f{1.0f, 1.0f, 1.0f, 1.0f}));
}

TEST(Heatmap2D, BuildsBottomUpVisibleCellGeometry) {
    using namespace ml::ui::heatmap_2d;
    Grid const grid{2, 2, {1.0f, 2.0f, 3.0f, 4.0f}};
    auto const cells{build_cell_geometry(
        grid, {0.0f, 4.0f}, build_color_lut(grayscale_stops(), 5), {100.0f, 80.0f})};
    ASSERT_EQ(cells.size(), 4);
    EXPECT_EQ(cells[0].size, (ml::ui::Vector2f{50.0f, 40.0f}));
    EXPECT_FLOAT_EQ(cells[0].position.y, 40.0f);
    EXPECT_FLOAT_EQ(cells[2].position.y, 0.0f);
    EXPECT_EQ(cells[2].cell_index, 2);
    EXPECT_EQ(cells[3].color, (ml::ui::Color4f{1.0f, 1.0f, 1.0f, 1.0f}));

    Grid const sparse{3, 1, {0.0f, std::numeric_limits<float>::quiet_NaN(), 2.0f}};
    auto const visible{build_cell_geometry(
        sparse, {0.0f, 1.0f}, build_color_lut(grayscale_stops(), 5), {90.0f, 30.0f})};
    ASSERT_EQ(visible.size(), 1);
    EXPECT_EQ(visible[0].cell_index, 2);
    EXPECT_EQ(visible[0].color, (ml::ui::Color4f{1.0f, 1.0f, 1.0f, 1.0f}));
}

TEST(Heatmap2D, BatchesAFull128GridIntoOneMesh) {
    using namespace ml::ui::heatmap_2d;
    Grid grid{.columns = 128, .rows = 128, .values = {}};
    grid.values.assign(128 * 128, 1.0f);
    auto const cells{build_cell_geometry(
        grid, {0.0f, 1.0f}, build_color_lut(grayscale_stops(), 5), {256.0f, 256.0f})};
    auto const batches{build_mesh_batches(cells, {10.0f, 20.0f})};
    ASSERT_EQ(batches.size(), 1);
    EXPECT_EQ(batches[0].vertices.size(), 128 * 128 * 4);
    EXPECT_EQ(batches[0].indices.size(), 128 * 128 * 6);
    EXPECT_FLOAT_EQ(batches[0].vertices[0].position.x, 10.0f);
}

TEST(Heatmap2D, ReservesAxisLabelsOrUsesBarePaddedArea) {
    using namespace ml::ui::heatmap_2d;
    LayoutSettings settings{.padding = {10.0f, 10.0f, 10.0f, 10.0f},
                            .axis_thickness = 1.0f,
                            .x_label_area_height = 20.0f,
                            .y_label_area_width = 36.0f};
    auto const axes{make_plot_layout({200.0f, 100.0f}, settings)};
    EXPECT_EQ(axes.plot_origin, (ml::ui::Vector2f{47.0f, 10.0f}));
    EXPECT_EQ(axes.plot_size, (ml::ui::Vector2f{143.0f, 59.0f}));
    settings.show_axes = false;
    auto const bare{make_plot_layout({200.0f, 100.0f}, settings)};
    EXPECT_EQ(bare.plot_origin, (ml::ui::Vector2f{10.0f, 10.0f}));
    EXPECT_EQ(bare.plot_size, (ml::ui::Vector2f{180.0f, 80.0f}));
    EXPECT_FLOAT_EQ(bare.x_label_area_height, 0.0f);
}

TEST(Heatmap2D, OwnsRejectsAndClearsGridSnapshots) {
    using namespace ml::ui::heatmap_2d;
    Data data;
    Grid grid{2, 1, {1.0f, 2.0f}};
    ASSERT_TRUE(data.set_grid(grid));
    grid.values[0] = 100.0f;
    EXPECT_FLOAT_EQ(data.grid().values[0], 1.0f);
    ASSERT_TRUE(data.set_grid({1, 1, {3.0f}}));
    EXPECT_FALSE(data.set_grid({2, 2, {9.0f}}));
    EXPECT_EQ(data.grid().values, (std::vector<float>{3.0f}));
    data.clear_grid();
    EXPECT_TRUE(data.grid().values.empty());
}
