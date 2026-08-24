#include "SandboxUI/widgets/SRadar2D.h"

#include <CQTest.h>

namespace {
void test_vector(FAutomationTestBase& test,
                 TCHAR const* const message,
                 FVector2f const actual,
                 FVector2f const expected) {
    test.TestTrue(message, actual.Equals(expected));
}
}

TEST_CLASS(Radar2DLayout, "SandboxUI.UnitTests")
{
    TEST_METHOD(MapsCentreAndEdges)
    {
        auto const layout{make_radar_2d_layout({200.0f, 100.0f}, 10.0f)};

        test_vector(
            *TestRunner, TEXT("Wide widgets centre a square radar"), layout.origin, {50.0f, 0.0f});
        test_vector(*TestRunner,
                    TEXT("Radar size uses the short widget axis"),
                    layout.size,
                    {100.0f, 100.0f});
        test_vector(*TestRunner,
                    TEXT("Zero maps to the radar centre"),
                    radar_to_local({0.0f, 0.0f}, layout),
                    {100.0f, 50.0f});
        test_vector(*TestRunner,
                    TEXT("Positive X maps to the right edge"),
                    radar_to_local({10.0f, 0.0f}, layout),
                    {150.0f, 50.0f});
        test_vector(*TestRunner,
                    TEXT("Negative X maps to the left edge"),
                    radar_to_local({-10.0f, 0.0f}, layout),
                    {50.0f, 50.0f});
        test_vector(*TestRunner,
                    TEXT("Positive Y maps to the top edge"),
                    radar_to_local({0.0f, 10.0f}, layout),
                    {100.0f, 0.0f});
        test_vector(*TestRunner,
                    TEXT("Negative Y maps to the bottom edge"),
                    radar_to_local({0.0f, -10.0f}, layout),
                    {100.0f, 100.0f});
    }

    TEST_METHOD(PreservesScaleAcrossRangesAndAspectRatios)
    {
        auto const square{make_radar_2d_layout({100.0f, 100.0f}, 20.0f)};
        auto const tall{make_radar_2d_layout({100.0f, 200.0f}, 20.0f)};

        TestRunner->TestEqual(
            TEXT("Range controls pixels per radar unit"), square.pixels_per_unit, 2.5f);
        test_vector(*TestRunner,
                    TEXT("Larger ranges reduce displacement"),
                    radar_to_local({10.0f, 0.0f}, square),
                    {75.0f, 50.0f});
        test_vector(*TestRunner,
                    TEXT("Tall widgets vertically centre the radar"),
                    tall.origin,
                    {0.0f, 50.0f});
        test_vector(
            *TestRunner, TEXT("Tall widget centres remain correct"), tall.centre, {50.0f, 100.0f});
    }

    TEST_METHOD(HandlesZeroSizeAndInvalidRangeSafely)
    {
        auto const zero_size{make_radar_2d_layout({0.0f, 100.0f}, 10.0f)};
        auto const invalid_range{make_radar_2d_layout({100.0f, 100.0f}, 0.0f)};

        TestRunner->TestEqual(
            TEXT("Zero-sized layouts have no scale"), zero_size.pixels_per_unit, 0.0f);
        TestRunner->TestEqual(
            TEXT("Invalid ranges have no scale"), invalid_range.pixels_per_unit, 0.0f);
        test_vector(*TestRunner,
                    TEXT("Zero-sized transforms remain at the centre"),
                    radar_to_local({10.0f, 10.0f}, zero_size),
                    zero_size.centre);
    }
};

TEST_CLASS(Radar2DData, "SandboxUI.UnitTests")
{
    TEST_METHOD(ReplacesSnapshotsAndIndividualBuckets)
    {
        auto widget{SNew(SRadar2D)};
        TArray<FRadar2DStyleBucket> buckets;
        buckets.SetNum(2);
        buckets[0].positions.add(1.0f, 2.0f);
        buckets[1].positions.add(3.0f, 4.0f);

        TestRunner->TestTrue(TEXT("A complete snapshot is accepted"),
                             widget->set_buckets(MoveTemp(buckets)));
        TestRunner->TestEqual(TEXT("Both styles are retained"), widget->get_buckets().Num(), 2);

        FVectors2f replacement;
        replacement.add(5.0f, 6.0f);
        replacement.add(7.0f, 8.0f);
        TestRunner->TestTrue(TEXT("One bucket can be replaced"),
                             widget->set_positions(0, replacement));
        replacement.xs[0] = 100.0f;

        auto const owned{widget->get_buckets()};
        TestRunner->TestEqual(TEXT("Lvalue updates are copied"), owned[0].positions.xs[0], 5.0f);
        TestRunner->TestEqual(TEXT("Other buckets are unchanged"), owned[1].positions.xs[0], 3.0f);
    }

    TEST_METHOD(MovesUpdatesAndClearsData)
    {
        auto widget{SNew(SRadar2D)};
        auto const style_index{widget->add_style(FRadar2DContactStyle{})};
        TestRunner->TestEqual(TEXT("The first style uses index zero"), style_index, 0);

        FVectors2f positions;
        positions.add(1.0f, 2.0f);
        positions.add(3.0f, 4.0f);
        TestRunner->TestTrue(TEXT("Rvalue updates are accepted"),
                             widget->set_positions(style_index, MoveTemp(positions)));
        TestRunner->TestTrue(TEXT("Rvalue storage is moved from the caller"), positions.is_empty());
        TestRunner->TestEqual(TEXT("Moved contacts are owned by the widget"),
                              widget->get_buckets()[0].positions.num(),
                              2);

        TestRunner->TestTrue(TEXT("An individual bucket can be cleared"),
                             widget->clear_positions(style_index));
        TestRunner->TestTrue(TEXT("The cleared bucket is empty"),
                             widget->get_buckets()[0].positions.is_empty());

        FVectors2f one_position;
        one_position.add(5.0f, 6.0f);
        TestRunner->TestTrue(TEXT("Data can be restored"),
                             widget->set_positions(style_index, MoveTemp(one_position)));
        widget->clear_positions();
        TestRunner->TestTrue(TEXT("All positions can be cleared"),
                             widget->get_buckets()[0].positions.is_empty());

        widget->clear_styles();
        TestRunner->TestTrue(TEXT("All styles can be cleared"), widget->get_buckets().IsEmpty());
    }

    TEST_METHOD(SupportsEmptyDataAndRejectsInvalidUpdates)
    {
        auto widget{SNew(SRadar2D)};
        TestRunner->TestTrue(TEXT("A new radar has no styles"), widget->get_buckets().IsEmpty());
        TestRunner->TestFalse(TEXT("Invalid ranges are rejected"), widget->set_range(0.0f));
        TestRunner->TestEqual(
            TEXT("Rejected ranges preserve the old value"), widget->get_range(), 1.0f);
        TestRunner->TestFalse(TEXT("Invalid style indices are rejected"),
                              widget->set_positions(0, {}));

        FRadar2DContactStyle invalid_style;
        invalid_style.rendered_size = {0.0f, 6.0f};
        TestRunner->TestEqual(TEXT("Invalid styles are rejected"),
                              widget->add_style(MoveTemp(invalid_style)),
                              INDEX_NONE);

        auto const style_index{widget->add_style(FRadar2DContactStyle{})};
        FVectors2f mismatched_positions;
        mismatched_positions.xs.Add(1.0f);
        TestRunner->TestFalse(TEXT("Mismatched SOA arrays are rejected"),
                              widget->set_positions(style_index, MoveTemp(mismatched_positions)));
        TestRunner->TestTrue(TEXT("Rejected data does not alter the bucket"),
                             widget->get_buckets()[style_index].positions.is_empty());
    }
};
