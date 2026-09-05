#include "SandboxUI/EntityOverlay/EntityOverlayTypes.h"

#include <CQTest.h>

#include <limits>

TEST_CLASS(EntityOverlayCollector, "SandboxUI.UnitTests")
{
    TEST_METHOD(HandlesEmptyAndMismatchedSources)
    {
        FEntityOverlayCollector collector;
        TArray<FEntityOverlayInstance> output;
        collector.begin(FVector3f::ZeroVector, 100.0f, output);

        TestRunner->TestEqual(TEXT("Empty sources append no candidates"), collector.append({}), 0);

        TArray<FVector3f> positions{{1.0f, 0.0f, 0.0f}};
        TestRunner->TestEqual(TEXT("Mismatched sources append no partial candidates"),
                              collector.append({.positions = positions, .health_values = {}}),
                              0);
        TestRunner->TestTrue(TEXT("Output remains empty"), output.IsEmpty());
    }

    TEST_METHOD(FiltersRangeInclusivelyAndCompacts)
    {
        FEntityOverlayCollector collector;
        TArray<FEntityOverlayInstance> output;
        collector.begin(FVector3f::ZeroVector, 10.0f, output);

        TestRunner->TestTrue(TEXT("Inside range is accepted"),
                             collector.try_add({9.0f, 0.0f, 0.0f}, 1.0f));
        TestRunner->TestTrue(TEXT("Maximum range boundary is accepted"),
                             collector.try_add({10.0f, 0.0f, 0.0f}, 0.5f));
        TestRunner->TestFalse(TEXT("Outside range is rejected"),
                              collector.try_add({10.01f, 0.0f, 0.0f}, 1.0f));
        TestRunner->TestEqual(TEXT("Accepted candidates are compact"), output.Num(), 2);
    }

    TEST_METHOD(ClampsAndValidatesHealth)
    {
        FEntityOverlayCollector collector;
        TArray<FEntityOverlayInstance> output;
        collector.begin(FVector3f::ZeroVector, 10.0f, output);

        static_cast<void>(collector.try_add(FVector3f::ZeroVector, -0.5f));
        static_cast<void>(collector.try_add(FVector3f::ZeroVector, 1.5f));
        static_cast<void>(
            collector.try_add(FVector3f::ZeroVector, std::numeric_limits<float>::quiet_NaN()));

        TestRunner->TestEqual(TEXT("Low health clamps to zero"), output[0].health, 0.0f);
        TestRunner->TestEqual(TEXT("High health clamps to one"), output[1].health, 1.0f);
        TestRunner->TestEqual(TEXT("Non-finite health becomes zero"), output[2].health, 0.0f);
        TestRunner->TestEqual(
            TEXT("Invalid health is counted"), collector.invalid_health_count(), 1);
    }

    TEST_METHOD(AppendsMultipleSources)
    {
        FEntityOverlayCollector collector;
        TArray<FEntityOverlayInstance> output;
        collector.begin(FVector3f::ZeroVector, 100.0f, output);

        TArray<FVector3f> first_positions{{1.0f, 0.0f, 0.0f}, {2.0f, 0.0f, 0.0f}};
        TArray<float> first_health{0.25f, 0.5f};
        TArray<FVector3f> second_positions{{3.0f, 0.0f, 0.0f}};
        TArray<float> second_health{0.75f};

        TestRunner->TestEqual(
            TEXT("First source is appended"), collector.append({first_positions, first_health}), 2);
        TestRunner->TestEqual(TEXT("Second source is appended"),
                              collector.append({second_positions, second_health}),
                              1);
        TestRunner->TestEqual(TEXT("All sources share one compact output"), output.Num(), 3);
        TestRunner->TestEqual(TEXT("Source ordering is retained"), output[2].health, 0.75f);
    }
};
