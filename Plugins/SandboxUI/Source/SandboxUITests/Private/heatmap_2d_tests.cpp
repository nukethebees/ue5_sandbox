#include "SandboxUI/widgets/SHeatmap2D.h"

#include <CQTest.h>

#include <limits>

namespace {
void test_heatmap_float(FAutomationTestBase& test,
                        TCHAR const* const message,
                        float const actual,
                        float const expected) {
    test.TestTrue(message, FMath::IsNearlyEqual(actual, expected));
}

void test_heatmap_color(FAutomationTestBase& test,
                        TCHAR const* const message,
                        FLinearColor const& actual,
                        FLinearColor const& expected) {
    test.TestTrue(message, actual.Equals(expected));
}

auto make_grayscale_stops() -> TArray<FHeatmapColorStop> {
    return {{.position = 0.0f, .color = FLinearColor::Black},
            {.position = 1.0f, .color = FLinearColor::White}};
}
}

TEST_CLASS(Heatmap2DValidation, "SandboxUI.UnitTests")
{
    TEST_METHOD(ValidatesGridRangesDomainsAndColorStops)
    {
        TestRunner->TestTrue(TEXT("An empty grid is valid"), is_valid_heatmap_grid({}));
        TestRunner->TestTrue(
            TEXT("A complete dense grid is valid"),
            is_valid_heatmap_grid({.columns = 2, .rows = 2, .values = {0.0f, 1.0f, 2.0f, 3.0f}}));
        TestRunner->TestFalse(
            TEXT("Mismatched value counts are rejected"),
            is_valid_heatmap_grid({.columns = 2, .rows = 2, .values = {0.0f, 1.0f}}));
        TestRunner->TestFalse(TEXT("Partially empty dimensions are rejected"),
                              is_valid_heatmap_grid({.columns = 2}));

        TestRunner->TestTrue(TEXT("Increasing finite value ranges are valid"),
                             is_valid_heatmap_value_range({-1.0f, 1.0f}));
        TestRunner->TestFalse(TEXT("Flat value ranges are rejected"),
                              is_valid_heatmap_value_range({1.0f, 1.0f}));
        TestRunner->TestTrue(TEXT("Increasing finite domains are valid"),
                             is_valid_heatmap_domain({-1.0f, 1.0f, -2.0f, 2.0f}));
        TestRunner->TestFalse(TEXT("Reversed domains are rejected"),
                              is_valid_heatmap_domain({1.0f, -1.0f, -2.0f, 2.0f}));

        auto const valid_stops{make_grayscale_stops()};
        TestRunner->TestTrue(TEXT("Ordered endpoint color stops are valid"),
                             is_valid_heatmap_color_stops(valid_stops));
        TArray<FHeatmapColorStop> const missing_endpoint{
            {.position = 0.0f, .color = FLinearColor::Black},
            {.position = 0.8f, .color = FLinearColor::White}};
        TestRunner->TestFalse(TEXT("Color ramps must span zero to one"),
                              is_valid_heatmap_color_stops(missing_endpoint));
    }
};

TEST_CLASS(Heatmap2DPreparation, "SandboxUI.UnitTests")
{
    TEST_METHOD(BuildsColorLookupTable)
    {
        auto const stops{make_grayscale_stops()};

        auto const color_lut{build_heatmap_color_lut(stops, 5)};

        TestRunner->TestEqual(TEXT("The requested LUT size is produced"), color_lut.Num(), 5);
        test_heatmap_color(
            *TestRunner, TEXT("The first color is preserved"), color_lut[0], FLinearColor::Black);
        test_heatmap_color(*TestRunner,
                           TEXT("Intermediate colors are interpolated"),
                           color_lut[2],
                           FLinearColor{0.5f, 0.5f, 0.5f, 1.0f});
        test_heatmap_color(
            *TestRunner, TEXT("The last color is preserved"), color_lut[4], FLinearColor::White);
    }

    TEST_METHOD(MapsRowsFromBottomToSlateCoordinates)
    {
        FHeatmapGrid const grid{.columns = 2, .rows = 2, .values = {1.0f, 2.0f, 3.0f, 4.0f}};
        auto const color_lut{build_heatmap_color_lut(make_grayscale_stops(), 5)};

        auto const cells{
            build_heatmap_cell_geometry(grid, {0.0f, 4.0f}, color_lut, {100.0f, 80.0f})};

        TestRunner->TestEqual(TEXT("Every value above the minimum emits a cell"), cells.Num(), 4);
        test_heatmap_float(
            *TestRunner, TEXT("Columns divide the plot width"), cells[0].size.X, 50.0f);
        test_heatmap_float(
            *TestRunner, TEXT("Rows divide the plot height"), cells[0].size.Y, 40.0f);
        test_heatmap_float(*TestRunner,
                           TEXT("Row zero renders against the bottom edge"),
                           cells[0].position.Y,
                           40.0f);
        test_heatmap_float(*TestRunner,
                           TEXT("The highest logical row renders at the top"),
                           cells[2].position.Y,
                           0.0f);
        TestRunner->TestEqual(TEXT("Row-major cell indices are retained"), cells[2].cell_index, 2);
        test_heatmap_color(*TestRunner,
                           TEXT("The maximum value uses the last LUT color"),
                           cells[3].color,
                           FLinearColor::White);
    }

    TEST_METHOD(SkipsTransparentAndNonFiniteCellsAndClampsHighValues)
    {
        FHeatmapGrid const grid{.columns = 3,
                                .rows = 1,
                                .values = {0.0f, std::numeric_limits<float>::quiet_NaN(), 2.0f}};
        auto const color_lut{build_heatmap_color_lut(make_grayscale_stops(), 5)};

        auto const cells{
            build_heatmap_cell_geometry(grid, {0.0f, 1.0f}, color_lut, {90.0f, 30.0f})};

        TestRunner->TestEqual(TEXT("Only visible finite cells emit geometry"), cells.Num(), 1);
        TestRunner->TestEqual(
            TEXT("The surviving source index is retained"), cells[0].cell_index, 2);
        test_heatmap_color(*TestRunner,
                           TEXT("Values above maximum clamp to the final color"),
                           cells[0].color,
                           FLinearColor::White);
    }

    TEST_METHOD(BuildsOneBatchedMeshForAFull128Grid)
    {
        FHeatmapGrid grid{.columns = 128, .rows = 128};
        grid.values.Init(1.0f, grid.columns * grid.rows);
        auto const color_lut{build_heatmap_color_lut(make_grayscale_stops(), 5)};
        auto const cells{
            build_heatmap_cell_geometry(grid, {0.0f, 1.0f}, color_lut, {256.0f, 256.0f})};

        auto const batches{build_heatmap_mesh_batches(cells, {10.0f, 20.0f})};

        TestRunner->TestEqual(TEXT("A 128 by 128 heatmap fits in one batch"), batches.Num(), 1);
        TestRunner->TestEqual(
            TEXT("Each cell emits four vertices"), batches[0].vertices.Num(), 128 * 128 * 4);
        TestRunner->TestEqual(
            TEXT("Each cell emits six indices"), batches[0].indices.Num(), 128 * 128 * 6);
        test_heatmap_float(*TestRunner,
                           TEXT("The plot origin offsets local vertices"),
                           batches[0].vertices[0].position.X,
                           10.0f);
    }
};

TEST_CLASS(Heatmap2DLayout, "SandboxUI.UnitTests")
{
    TEST_METHOD(ReservesAxisLabelsOrUsesTheBarePaddedArea)
    {
        FHeatmap2DStyle style;
        style.chart_padding = FMargin{10.0f};
        style.axis_thickness = 1.0f;
        style.x_label_area_height = 20.0f;
        style.y_label_area_width = 36.0f;

        auto const axis_layout{make_heatmap_plot_layout({200.0f, 100.0f}, style)};

        test_heatmap_float(
            *TestRunner, TEXT("Y labels shift the plot right"), axis_layout.plot_origin.X, 47.0f);
        test_heatmap_float(
            *TestRunner, TEXT("Top padding sets the plot Y"), axis_layout.plot_origin.Y, 10.0f);
        test_heatmap_float(
            *TestRunner, TEXT("Axes reserve horizontal space"), axis_layout.plot_size.X, 143.0f);
        test_heatmap_float(
            *TestRunner, TEXT("Axes reserve vertical space"), axis_layout.plot_size.Y, 59.0f);

        style.show_axes = false;
        auto const bare_layout{make_heatmap_plot_layout({200.0f, 100.0f}, style)};
        test_heatmap_float(*TestRunner,
                           TEXT("Bare plots start at left padding"),
                           bare_layout.plot_origin.X,
                           10.0f);
        test_heatmap_float(
            *TestRunner, TEXT("Bare plots use padded width"), bare_layout.plot_size.X, 180.0f);
        test_heatmap_float(
            *TestRunner, TEXT("Bare plots use padded height"), bare_layout.plot_size.Y, 80.0f);
        test_heatmap_float(*TestRunner,
                           TEXT("Bare plots reserve no X labels"),
                           bare_layout.x_label_area_height,
                           0.0f);
    }
};

TEST_CLASS(Heatmap2DData, "SandboxUI.UnitTests")
{
    TEST_METHOD(OwnsReplacesRejectsAndClearsGridSnapshots)
    {
        auto widget{SNew(SHeatmap2D)};
        FHeatmapGrid grid{.columns = 2, .rows = 1, .values = {1.0f, 2.0f}};
        TestRunner->TestTrue(TEXT("Valid grids are accepted"), widget->set_grid(grid));
        grid.values[0] = 100.0f;
        test_heatmap_float(
            *TestRunner, TEXT("Lvalue snapshots are copied"), widget->get_grid().values[0], 1.0f);

        FHeatmapGrid replacement{.columns = 1, .rows = 1, .values = {3.0f}};
        TestRunner->TestTrue(TEXT("Replacement grids are accepted"),
                             widget->set_grid(MoveTemp(replacement)));
        TestRunner->TestTrue(TEXT("Rvalue snapshots move from the caller"),
                             replacement.values.IsEmpty());

        FHeatmapGrid invalid{.columns = 2, .rows = 2, .values = {9.0f}};
        TestRunner->TestFalse(TEXT("Invalid replacements are rejected"),
                              widget->set_grid(MoveTemp(invalid)));
        TestRunner->TestEqual(
            TEXT("Rejected updates preserve valid state"), widget->get_grid().values.Num(), 1);
        test_heatmap_float(*TestRunner,
                           TEXT("Rejected updates preserve values"),
                           widget->get_grid().values[0],
                           3.0f);

        widget->clear_grid();
        TestRunner->TestTrue(TEXT("Owned grid data can be cleared"),
                             widget->get_grid().values.IsEmpty());
    }
};
