#include "SandboxUI/widgets/SHistogram.h"

#include <CQTest.h>

#include <limits>

namespace {
void test_float(FAutomationTestBase& test,
                TCHAR const* const message,
                float const actual,
                float const expected) {
    test.TestTrue(message, FMath::IsNearlyEqual(actual, expected));
}
}

TEST_CLASS(HistogramBins, "SandboxUI.UnitTests")
{
    TEST_METHOD(AssignsSamplesToFixedDomainBins)
    {
        TArray<float> const samples{0.0f, 0.99f, 1.0f, 2.0f, 3.0f, 4.0f};

        auto const bins{build_histogram_bins(samples, 0.0f, 4.0f, 4)};

        TestRunner->TestEqual(TEXT("The requested number of bins is produced"), bins.Num(), 4);
        TestRunner->TestEqual(TEXT("The lower edge is included in the first bin"), bins[0], 2);
        TestRunner->TestEqual(TEXT("Internal edges start the following bin"), bins[1], 1);
        TestRunner->TestEqual(TEXT("Later internal edges use the same rule"), bins[2], 1);
        TestRunner->TestEqual(TEXT("The domain maximum is included in the last bin"), bins[3], 2);
    }

    TEST_METHOD(IgnoresOutOfDomainAndNonFiniteSamples)
    {
        TArray<float> const samples{-0.1f,
                                    0.25f,
                                    0.75f,
                                    1.1f,
                                    std::numeric_limits<float>::quiet_NaN(),
                                    std::numeric_limits<float>::infinity(),
                                    -std::numeric_limits<float>::infinity()};

        auto const bins{build_histogram_bins(samples, 0.0f, 1.0f, 2)};

        TestRunner->TestEqual(TEXT("Only the in-domain lower sample is counted"), bins[0], 1);
        TestRunner->TestEqual(TEXT("Only the in-domain upper sample is counted"), bins[1], 1);
    }

    TEST_METHOD(HandlesEmptyAndInvalidConfigurations)
    {
        auto const empty{build_histogram_bins({}, 0.0f, 1.0f, 4)};
        auto const reversed{build_histogram_bins({}, 1.0f, 0.0f, 4)};
        auto const flat{build_histogram_bins({}, 1.0f, 1.0f, 4)};
        auto const no_bins{build_histogram_bins({}, 0.0f, 1.0f, 0)};
        auto const non_finite{
            build_histogram_bins({}, 0.0f, std::numeric_limits<float>::infinity(), 4)};

        TestRunner->TestEqual(TEXT("Empty samples retain the configured bins"), empty.Num(), 4);
        TestRunner->TestTrue(TEXT("Empty samples produce zero counts"),
                             maximum_histogram_bin_count(empty) == 0);
        TestRunner->TestTrue(TEXT("Reversed domains produce no bins"), reversed.IsEmpty());
        TestRunner->TestTrue(TEXT("Zero-width domains produce no bins"), flat.IsEmpty());
        TestRunner->TestTrue(TEXT("Non-positive bin counts produce no bins"), no_bins.IsEmpty());
        TestRunner->TestTrue(TEXT("Non-finite domains produce no bins"), non_finite.IsEmpty());
    }
};

TEST_CLASS(HistogramGeometry, "SandboxUI.UnitTests")
{
    TEST_METHOD(BuildsProportionalBarsFromZeroBaseline)
    {
        TArray<int32> const bins{2, 4, 0};

        auto const geometry{build_histogram_geometry(bins, {120.0f, 100.0f}, 4.0f)};

        TestRunner->TestEqual(
            TEXT("The largest bin sets the shared scale"), geometry.maximum_count, 4);
        test_float(*TestRunner, TEXT("Bins receive equal slots"), geometry.slot_width, 40.0f);
        test_float(
            *TestRunner, TEXT("The configured gap reduces bar width"), geometry.bar_width, 36.0f);
        TestRunner->TestEqual(TEXT("Only non-empty bins emit bars"), geometry.bars.Num(), 2);

        auto const& first{geometry.bars[0]};
        TestRunner->TestEqual(TEXT("Bar geometry retains its bin index"), first.bin_index, 0);
        TestRunner->TestEqual(TEXT("Bar geometry retains its count"), first.count, 2);
        test_float(*TestRunner, TEXT("Half the maximum produces half height"), first.size.Y, 50.0f);
        test_float(
            *TestRunner, TEXT("Short bars start above the baseline"), first.position.Y, 50.0f);

        auto const& second{geometry.bars[1]};
        test_float(*TestRunner, TEXT("The maximum bin fills the plot"), second.size.Y, 100.0f);
        test_float(*TestRunner, TEXT("The maximum bin reaches the top"), second.position.Y, 0.0f);
    }

    TEST_METHOD(HandlesEmptyAndZeroCounts)
    {
        auto const empty{build_histogram_geometry({}, {100.0f, 100.0f}, 2.0f)};
        TArray<int32> const bins{0, 0};
        auto const zero{build_histogram_geometry(bins, {100.0f, 100.0f}, 2.0f)};

        TestRunner->TestEqual(TEXT("Empty bins have a zero maximum"), empty.maximum_count, 0);
        TestRunner->TestTrue(TEXT("Empty bins emit no bars"), empty.bars.IsEmpty());
        TestRunner->TestEqual(TEXT("Zero bins have a zero maximum"), zero.maximum_count, 0);
        TestRunner->TestTrue(TEXT("Zero bins emit no bars"), zero.bars.IsEmpty());
        test_float(*TestRunner, TEXT("Zero bins retain stable slots"), zero.slot_width, 50.0f);
    }
};

TEST_CLASS(HistogramData, "SandboxUI.UnitTests")
{
    TEST_METHOD(OwnsReplacesRebinsAndClearsSamples)
    {
        auto widget{SNew(SHistogram).DomainMinimum(0.0f).DomainMaximum(4.0f).BinCount(4)};
        TArray<float> samples{0.5f, 1.5f};
        widget->set_samples(samples);
        samples[0] = 3.5f;

        test_float(
            *TestRunner, TEXT("Lvalue snapshots are copied"), widget->get_samples()[0], 0.5f);
        TestRunner->TestEqual(TEXT("Owned samples are binned"), widget->get_bins()[0], 1);

        TArray<float> replacement{2.5f, 2.75f};
        widget->set_samples(MoveTemp(replacement));
        TestRunner->TestTrue(TEXT("Rvalue snapshots move from the caller"), replacement.IsEmpty());
        TestRunner->TestEqual(
            TEXT("Replacement samples replace prior bin counts"), widget->get_bins()[2], 2);

        widget->set_bin_configuration(2.0f, 3.0f, 2);
        TestRunner->TestEqual(
            TEXT("Configuration changes rebuild the bins"), widget->get_bins().Num(), 2);
        TestRunner->TestEqual(
            TEXT("Samples accumulate in the rebuilt upper bin"), widget->get_bins()[1], 2);

        widget->clear_samples();
        TestRunner->TestTrue(TEXT("Owned samples can be cleared"), widget->get_samples().IsEmpty());
        TestRunner->TestEqual(
            TEXT("Clearing retains the configured bin count"), widget->get_bins().Num(), 2);
        TestRunner->TestEqual(TEXT("Clearing resets accumulated counts"),
                              maximum_histogram_bin_count(widget->get_bins()),
                              0);
    }
};
