#include "SandboxUI/widgets/SGraphPlot.h"

#include <CQTest.h>
#include <Input/Events.h>
#include <Widgets/DeclarativeSyntaxSupport.h>

TEST_CLASS(GraphPlotWidget, "SandboxUI.UnitTests")
{
    TEST_METHOD(FindsNearestVisibleX)
    {
        FGraphSeries series;
        series.x = {0.0f, 2.0f, 5.0f};
        series.y = {1.0f, 2.0f, 3.0f};
        TArray<FGraphSeries> values{series};
        auto const nearest{nearest_graph_x(values, 3.1)};
        TestRunner->TestTrue(TEXT("A nearest coordinate is found"), nearest.IsSet());
        TestRunner->TestEqual(
            TEXT("Nearest lookup uses X rather than sample index"), nearest.GetValue(), 2.0);
    }

    TEST_METHOD(StyleValidationAndOwnedSeries)
    {
        auto widget{SNew(SGraphPlot)};

        auto valid_style{FGraphPlotStyle{}};
        valid_style.desired_size = {480.0f, 240.0f};
        TestRunner->TestTrue(TEXT("Valid graph styles are accepted"),
                             widget->set_style(MoveTemp(valid_style)));
        TestRunner->TestEqual(TEXT("Graph desired width follows its style"),
                              widget->ComputeDesiredSize(1.0f).X,
                              480.0);
        TestRunner->TestEqual(TEXT("Graph desired height follows its style"),
                              widget->ComputeDesiredSize(1.0f).Y,
                              240.0);

        auto invalid_style{FGraphPlotStyle{}};
        invalid_style.left_margin = -1.0f;
        TestRunner->TestFalse(TEXT("Negative graph margins are rejected"),
                              widget->set_style(MoveTemp(invalid_style)));
        TestRunner->TestEqual(TEXT("Rejected styles preserve the desired width"),
                              widget->ComputeDesiredSize(1.0f).X,
                              480.0);

        {
            FGraphSeries series;
            series.name = FText::FromString(TEXT("Counter"));
            series.x = {0.0f, 1.0f};
            series.y = {2.0f, 3.0f};
            series.style.interpolation = EGraphSeriesInterpolation::StepAfter;
            TArray<FGraphSeries> series_snapshot;
            series_snapshot.Add(MoveTemp(series));
            widget->set_series(MoveTemp(series_snapshot));
        }

        auto const stored_series{widget->get_series()};
        TestRunner->TestEqual(TEXT("Graph owns the supplied series"), stored_series.Num(), 1);
        TestRunner->TestEqual(TEXT("Owned series retain caller data"), stored_series[0].y[1], 3.0f);
        TestRunner->TestTrue(TEXT("Graph preserves step interpolation"),
                             stored_series[0].style.interpolation ==
                                 EGraphSeriesInterpolation::StepAfter);

        FGraphSeries replacement;
        replacement.name = FText::FromString(TEXT("Replacement"));
        replacement.y = {8.0f};
        TArray<FGraphSeries> replacement_snapshot;
        replacement_snapshot.Add(MoveTemp(replacement));
        widget->set_series(MoveTemp(replacement_snapshot));
        auto const replaced_series{widget->get_series()};
        TestRunner->TestEqual(
            TEXT("Replacing data does not retain old series"), replaced_series.Num(), 1);
        TestRunner->TestEqual(TEXT("Replacement data is retained"),
                              replaced_series[0].name.ToString(),
                              TEXT("Replacement"));

        widget->clear_series();
        TestRunner->TestTrue(TEXT("Clearing removes all owned series"),
                             widget->get_series().IsEmpty());
    }

    TEST_METHOD(StableHoverDoesNotReplaceTooltipOrRebuildCache)
    {
        auto widget{SNew(SGraphPlot)};
        FGraphSeries series;
        series.name = FText::FromString(TEXT("Value"));
        series.x = {0.0f, 10.0f};
        series.y = {1.0f, 2.0f};
        TArray<FGraphSeries> values;
        values.Add(MoveTemp(series));
        widget->set_series(MoveTemp(values));

        auto const geometry{FGeometry::MakeRoot({320.0f, 180.0f}, FSlateLayoutTransform{})};
        TSet<FKey> const pressed_buttons;
        FPointerEvent const first_event{
            0, {80.0f, 60.0f}, {79.0f, 60.0f}, pressed_buttons, FKey{}, 0.0f, {}};
        widget->OnMouseMove(geometry, first_event);
        auto const first_tooltip{widget->GetToolTip()};
        auto const first_rebuild_count{widget->get_cache_stats().rebuild_count};

        FPointerEvent const second_event{
            0, {82.0f, 60.0f}, {80.0f, 60.0f}, pressed_buttons, FKey{}, 0.0f, {}};
        widget->OnMouseMove(geometry, second_event);
        TestRunner->TestTrue(TEXT("Movement within one nearest-X region retains the tooltip"),
                             widget->GetToolTip() == first_tooltip);
        TestRunner->TestEqual(TEXT("Mouse movement does not rebuild graph data"),
                              widget->get_cache_stats().rebuild_count,
                              first_rebuild_count);

        widget->clear_series();
        TestRunner->TestFalse(TEXT("Replacing graph data clears stale hover state"),
                              widget->get_hovered_x().IsSet());
        TestRunner->TestFalse(TEXT("Replacing graph data clears the stale tooltip"),
                              widget->GetToolTip().IsValid());
    }
};
