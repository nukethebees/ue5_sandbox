#include "SandboxUI/EntityOverlay/EntityOverlayTypes.h"
#include "SandboxUI/widgets/SHeatmap2D.h"
#include "SandboxUI/widgets/SHistogram.h"
#include "SandboxUI/widgets/SRadar2D.h"
#include "SandboxUI/widgets/SStackedBarChart.h"

#include <CQTest.h>
#include <Input/Events.h>
#include <sandbox/core/ui/entity_overlay.h>
#include <Widgets/DeclarativeSyntaxSupport.h>

TEST_CLASS(SandboxUISlateAdapters, "Sandbox.UnitTests")
{
    TEST_METHOD(HeatmapConvertsAndCommitsPublicStateTransactionally)
    {
        auto widget{SNew(SHeatmap2D)};
        FHeatmapGrid grid{.columns = 2, .rows = 1, .values = {1.0f, 2.0f}};
        TestRunner->TestTrue(TEXT("Valid Unreal heatmap data is accepted"), widget->set_grid(grid));
        grid.values[0] = 100.0f;

        auto stored{widget->get_grid()};
        TestRunner->TestEqual(
            TEXT("The adapter owns a converted grid snapshot"), stored.values[0], 1.0f);

        FHeatmapGrid invalid{.columns = 2, .rows = 2, .values = {9.0f}};
        TestRunner->TestFalse(TEXT("Invalid Unreal heatmap data is rejected"),
                              widget->set_grid(MoveTemp(invalid)));
        stored = widget->get_grid();
        TestRunner->TestEqual(
            TEXT("Rejected heatmap data preserves dimensions"), stored.columns, 2);
        TestRunner->TestEqual(
            TEXT("Rejected heatmap data preserves values"), stored.values[1], 2.0f);

        auto style{FHeatmap2DStyle{}};
        style.desired_size = {480.0f, 240.0f};
        TestRunner->TestTrue(TEXT("Valid heatmap styles are accepted"),
                             widget->set_style(MoveTemp(style)));
        auto invalid_style{FHeatmap2DStyle{}};
        invalid_style.axis_thickness = 0.0f;
        TestRunner->TestFalse(TEXT("Invalid heatmap styles are rejected"),
                              widget->set_style(MoveTemp(invalid_style)));
        TestRunner->TestEqual(TEXT("Rejected styles preserve the desired width"),
                              widget->ComputeDesiredSize(1.0f).X,
                              480.0);

        widget->clear_grid();
        TestRunner->TestTrue(TEXT("Clearing reaches the native heatmap model"),
                             widget->get_grid().values.IsEmpty());
    }

    TEST_METHOD(HistogramConvertsDataAndMaintainsSlateHoverState)
    {
        auto widget{SNew(SHistogram).DomainMinimum(0.0f).DomainMaximum(4.0f).BinCount(4)};
        TArray<float> samples{0.25f, 1.25f, 3.75f, 4.0f};
        widget->set_samples(samples);
        samples[0] = 3.0f;

        TestRunner->TestEqual(
            TEXT("Histogram copies Unreal sample arrays"), widget->get_samples()[0], 0.25f);
        TestRunner->TestEqual(
            TEXT("Histogram exposes converted bin results"), widget->get_bins()[3], 2);
        TestRunner->TestFalse(TEXT("Invalid configurations are rejected"),
                              widget->set_bin_configuration(4.0f, 4.0f, 0));
        TestRunner->TestEqual(
            TEXT("Rejected configurations preserve the bin count"), widget->get_bin_count(), 4);

        auto const geometry{FGeometry::MakeRoot({320.0f, 200.0f}, FSlateLayoutTransform{})};
        TSet<FKey> const pressed_buttons;
        FPointerEvent const event{
            0, {20.0f, 20.0f}, {19.0f, 20.0f}, pressed_buttons, FKey{}, 0.0f, {}};
        widget->OnMouseMove(geometry, event);
        TestRunner->TestEqual(TEXT("Slate geometry is converted for histogram hit testing"),
                              widget->get_hovered_bin(),
                              0);
        TestRunner->TestTrue(TEXT("Hovering a bin creates a Slate tooltip"),
                             widget->GetToolTip().IsValid());

        widget->OnMouseLeave(event);
        TestRunner->TestEqual(TEXT("Mouse leave clears the wrapper hover state"),
                              widget->get_hovered_bin(),
                              INDEX_NONE);
        TestRunner->TestFalse(TEXT("Mouse leave clears the Slate tooltip"),
                              widget->GetToolTip().IsValid());
    }

    TEST_METHOD(RadarKeepsStylesAndNativePositionBucketsAligned)
    {
        auto widget{SNew(SRadar2D)};
        TArray<FRadar2DStyleBucket> buckets;
        buckets.SetNum(2);
        buckets[0].positions.add(1.0f, 2.0f);
        buckets[1].positions.add(3.0f, 4.0f);
        TestRunner->TestTrue(TEXT("A complete Unreal radar snapshot is accepted"),
                             widget->set_buckets(MoveTemp(buckets)));

        auto stored{widget->get_buckets()};
        TestRunner->TestEqual(TEXT("Every position bucket retains a style"), stored.Num(), 2);
        TestRunner->TestEqual(
            TEXT("The first bucket retains its position"), stored[0].positions.xs[0], 1.0f);

        FRadar2DStyleBucket invalid_bucket;
        invalid_bucket.positions.xs.Add(9.0f);
        TArray<FRadar2DStyleBucket> invalid_buckets;
        invalid_buckets.Add(MoveTemp(invalid_bucket));
        TestRunner->TestFalse(TEXT("Mismatched Unreal SOA data is rejected"),
                              widget->set_buckets(MoveTemp(invalid_buckets)));
        TestRunner->TestEqual(
            TEXT("Rejected snapshots preserve both buckets"), widget->get_buckets().Num(), 2);

        FVectors2f replacement;
        replacement.add(5.0f, 6.0f);
        TestRunner->TestTrue(TEXT("One position bucket can be replaced"),
                             widget->set_positions(0, replacement));
        replacement.xs[0] = 100.0f;
        stored = widget->get_buckets();
        TestRunner->TestEqual(
            TEXT("Radar position updates own their Unreal input"), stored[0].positions.xs[0], 5.0f);
        TestRunner->TestEqual(TEXT("Replacing positions does not disturb other styles"),
                              stored[1].positions.xs[0],
                              3.0f);

        TestRunner->TestTrue(TEXT("Individual native position buckets can be cleared"),
                             widget->clear_positions(0));
        TestRunner->TestTrue(TEXT("Clearing positions retains the corresponding style"),
                             widget->get_buckets()[0].positions.is_empty());
        widget->clear_styles();
        TestRunner->TestTrue(TEXT("Clearing styles clears both sides of the adapter"),
                             widget->get_buckets().IsEmpty());
    }

    TEST_METHOD(StackedBarsKeepLabelsColorsAndValuesAligned)
    {
        auto widget{SNew(SStackedBarChart)};
        TArray<FStackedBar> bars{
            {.label = FText::FromString(TEXT("First")),
             .segments = {{.value = 1.0f, .color = FLinearColor::Red},
                          {.value = 2.0f, .color = FLinearColor::Green}}},
            {.label = FText::FromString(TEXT("Second")),
             .segments = {{.value = 3.0f, .color = FLinearColor::Blue}}},
        };
        widget->set_bars(bars);
        bars[0].label = FText::FromString(TEXT("Mutated"));
        bars[0].segments[0] = {.value = 100.0f, .color = FLinearColor::White};

        auto stored{widget->get_bars()};
        TestRunner->TestEqual(
            TEXT("The adapter owns Unreal bar labels"), stored[0].label.ToString(), TEXT("First"));
        TestRunner->TestEqual(
            TEXT("Native values remain aligned with labels"), stored[1].segments[0].value, 3.0f);
        TestRunner->TestTrue(TEXT("Unreal colors remain aligned with native segments"),
                             stored[0].segments[1].color.Equals(FLinearColor::Green));

        TArray<FStackedBar> replacement{
            {.label = FText::FromString(TEXT("Replacement")),
             .segments = {{.value = 4.0f, .color = FLinearColor::Yellow}}},
        };
        widget->set_bars(MoveTemp(replacement));
        stored = widget->get_bars();
        TestRunner->TestEqual(
            TEXT("Replacing bars replaces every parallel array"), stored.Num(), 1);
        TestRunner->TestEqual(TEXT("Replacement labels and values stay paired"),
                              stored[0].label.ToString(),
                              TEXT("Replacement"));

        widget->clear_bars();
        TestRunner->TestTrue(TEXT("Clearing bars clears every parallel array"),
                             widget->get_bars().IsEmpty());
    }

    TEST_METHOD(EntityOverlayConvertsOrderingAndPackedColor)
    {
        FEntityOverlayCollector collector;
        TArray<FEntityOverlayInstance> output;
        collector.begin(FVector3f::ZeroVector, 100.0f, output);
        TestRunner->TestTrue(
            TEXT("An objective is accepted"),
            collector.try_add(
                {1.0f, 0.0f, 0.0f}, 0.25f, 10.0f, EEntityOverlayObjectiveRole::Defend));
        TestRunner->TestTrue(TEXT("A later ordinary contact is accepted"),
                             collector.try_add({2.0f, 0.0f, 0.0f}, 0.5f, 20.0f));
        auto const fill_color{FLinearColor{-1.0f, 0.5f, 2.0f, 0.25f}};
        TestRunner->TestTrue(TEXT("A colored objective is accepted"),
                             collector.try_add_colored({3.0f, 0.0f, 0.0f},
                                                       0.75f,
                                                       30.0f,
                                                       fill_color,
                                                       EEntityOverlayObjectiveRole::Destroy));

        TestRunner->TestEqual(TEXT("The Unreal output keeps ordinary contacts first"),
                              output[0].world_position.X,
                              2.0f);
        TestRunner->TestEqual(TEXT("Objective swapping preserves the earlier objective"),
                              output[1].world_position.X,
                              1.0f);
        auto const expected{ml::ui::entity_overlay::pack_display_data(
            ml::ui::entity_overlay::ObjectiveRole::Destroy,
            {fill_color.R, fill_color.G, fill_color.B, fill_color.A})};
        TestRunner->TestEqual(TEXT("FLinearColor conversion matches native color packing"),
                              output[2].display_data,
                              expected);
    }
};
