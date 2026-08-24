#include "SandboxUI/widgets/SStackedBarChart.h"

#include <CQTest.h>

#include <limits>

namespace {
void test_float(FAutomationTestBase& test,
                TCHAR const* const message,
                float const actual,
                float const expected) {
    test.TestTrue(message, FMath::IsNearlyEqual(actual, expected));
}

auto make_bar(std::initializer_list<float> const values) -> FStackedBar {
    FStackedBar bar;
    for (auto const value : values) {
        bar.segments.Add({.value = value});
    }
    return bar;
}
}

TEST_CLASS(StackedBarChartGeometry, "SandboxUI.UnitTests")
{
    TEST_METHOD(CalculatesPositiveTotalsAndMaximum)
    {
        auto first{make_bar({10.0f, -5.0f, 20.0f, 0.0f})};
        first.segments.Add({.value = std::numeric_limits<float>::quiet_NaN()});
        first.segments.Add({.value = std::numeric_limits<float>::infinity()});
        TArray<FStackedBar> const bars{MoveTemp(first), make_bar({5.0f, 15.0f})};

        test_float(*TestRunner,
                   TEXT("Only positive finite values contribute"),
                   stacked_bar_total(bars[0]),
                   30.0f);
        test_float(*TestRunner,
                   TEXT("The largest stacked total is selected"),
                   maximum_stacked_bar_total(bars),
                   30.0f);
    }

    TEST_METHOD(BuildsProportionalBottomUpSegments)
    {
        TArray<FStackedBar> const bars{make_bar({10.0f, 20.0f}), make_bar({15.0f})};

        auto const geometry{build_stacked_bar_chart_geometry(bars, {100.0f, 120.0f}, 10.0f)};

        test_float(*TestRunner, TEXT("Maximum total is retained"), geometry.maximum_total, 30.0f);
        test_float(*TestRunner, TEXT("Bars receive equal slots"), geometry.slot_width, 50.0f);
        test_float(
            *TestRunner, TEXT("The configured gap reduces bar width"), geometry.bar_width, 40.0f);
        TestRunner->TestEqual(
            TEXT("Every positive segment emits geometry"), geometry.segments.Num(), 3);

        auto const& first{geometry.segments[0]};
        TestRunner->TestEqual(
            TEXT("The first segment preserves its source index"), first.segment_index, 0);
        test_float(
            *TestRunner, TEXT("The first segment occupies one third height"), first.size.Y, 40.0f);
        test_float(
            *TestRunner, TEXT("The first segment starts at the baseline"), first.position.Y, 80.0f);

        auto const& second{geometry.segments[1]};
        TestRunner->TestEqual(
            TEXT("The second segment preserves its source index"), second.segment_index, 1);
        test_float(*TestRunner,
                   TEXT("The second segment occupies two thirds height"),
                   second.size.Y,
                   80.0f);
        test_float(
            *TestRunner, TEXT("The second segment accumulates upward"), second.position.Y, 0.0f);

        auto const& shorter_bar{geometry.segments[2]};
        test_float(
            *TestRunner, TEXT("Other bars share the same Y scale"), shorter_bar.size.Y, 60.0f);
        test_float(*TestRunner,
                   TEXT("Other bars retain their own baseline"),
                   shorter_bar.position.Y,
                   60.0f);
    }

    TEST_METHOD(IgnoresNegativeSegmentsWithoutChangingOrder)
    {
        TArray<FStackedBar> const bars{make_bar({-10.0f, 5.0f, -1.0f, 5.0f})};

        auto const geometry{build_stacked_bar_chart_geometry(bars, {20.0f, 100.0f}, 0.0f)};

        TestRunner->TestEqual(
            TEXT("Only positive segments emit geometry"), geometry.segments.Num(), 2);
        TestRunner->TestEqual(TEXT("The first positive segment keeps its index"),
                              geometry.segments[0].segment_index,
                              1);
        TestRunner->TestEqual(TEXT("The second positive segment keeps its index"),
                              geometry.segments[1].segment_index,
                              3);
        test_float(*TestRunner,
                   TEXT("The first positive segment starts at baseline"),
                   geometry.segments[0].position.Y,
                   50.0f);
        test_float(*TestRunner,
                   TEXT("The second positive segment stacks above it"),
                   geometry.segments[1].position.Y,
                   0.0f);
    }

    TEST_METHOD(HandlesEmptyAndZeroData)
    {
        auto const empty{build_stacked_bar_chart_geometry({}, {100.0f, 100.0f}, 8.0f)};
        TArray<FStackedBar> const zero_bars{make_bar({0.0f, -2.0f})};
        auto const zero{build_stacked_bar_chart_geometry(zero_bars, {100.0f, 100.0f}, 8.0f)};

        test_float(*TestRunner, TEXT("Empty input has a zero maximum"), empty.maximum_total, 0.0f);
        TestRunner->TestTrue(TEXT("Empty input emits no segments"), empty.segments.IsEmpty());
        test_float(*TestRunner, TEXT("Zero input has a zero maximum"), zero.maximum_total, 0.0f);
        TestRunner->TestTrue(TEXT("Zero input emits no segments"), zero.segments.IsEmpty());
        test_float(
            *TestRunner, TEXT("Zero bars still receive a stable slot"), zero.slot_width, 100.0f);
    }
};

TEST_CLASS(StackedBarChartData, "SandboxUI.UnitTests")
{
    TEST_METHOD(OwnsReplacesAndClearsBars)
    {
        auto widget{SNew(SStackedBarChart)};
        TArray<FStackedBar> bars{make_bar({1.0f})};
        widget->set_bars(bars);
        bars[0].segments[0].value = 100.0f;

        test_float(*TestRunner,
                   TEXT("Lvalue snapshots are copied"),
                   widget->get_bars()[0].segments[0].value,
                   1.0f);

        TArray<FStackedBar> replacement{make_bar({2.0f}), make_bar({3.0f})};
        widget->set_bars(MoveTemp(replacement));
        TestRunner->TestTrue(TEXT("Rvalue snapshots move from the caller"), replacement.IsEmpty());
        TestRunner->TestEqual(
            TEXT("Replacement data is owned by the widget"), widget->get_bars().Num(), 2);

        widget->clear_bars();
        TestRunner->TestTrue(TEXT("Owned data can be cleared"), widget->get_bars().IsEmpty());
    }
};
