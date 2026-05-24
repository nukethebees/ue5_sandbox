#include "SandboxCore/Public/interpolation.h"

#include "Misc/AutomationTest.h"

BEGIN_DEFINE_SPEC(FLerp1DViewSpec,
                  "SandboxCore.Math.Lerp1D.View",
                  EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

END_DEFINE_SPEC(FLerp1DViewSpec)

void FLerp1DViewSpec::Define() {
    It("returns true and writes interpolated values when array sizes match", [this]() {
        TArray<float> const From{0.0f, 10.0f, -10.0f, 100.0f};
        TArray<float> const To{10.0f, 20.0f, 10.0f, 200.0f};
        TArray<float> const Alpha{0.0f, 0.5f, 1.0f, 0.25f};
        TArray<float> Out;
        Out.Init(-1.0f, From.Num());

        bool const bResult{ml::lerp_1d<float>(From, To, Alpha, Out)};

        TestTrue(TEXT("Result"), bResult);
        TestEqual(TEXT("Out[0]"), Out[0], 0.0f);
        TestEqual(TEXT("Out[1]"), Out[1], 15.0f);
        TestEqual(TEXT("Out[2]"), Out[2], 10.0f);
        TestEqual(TEXT("Out[3]"), Out[3], 125.0f);
    });

    It("returns false and leaves output unchanged when To has the wrong size", [this]() {
        TArray<float> const From{0.0f, 10.0f};
        TArray<float> const To{10.0f};
        TArray<float> const Alpha{0.5f, 0.5f};
        TArray<float> Out;
        Out.Init(-1.0f, From.Num());

        bool const bResult{ml::lerp_1d<float>(From, To, Alpha, Out)};

        TestFalse(TEXT("Result"), bResult);
        TestEqual(TEXT("Out[0] unchanged"), Out[0], -1.0f);
        TestEqual(TEXT("Out[1] unchanged"), Out[1], -1.0f);
    });

    It("returns false and leaves output unchanged when Alpha has the wrong size", [this]() {
        TArray<float> const From{0.0f, 10.0f};
        TArray<float> const To{10.0f, 20.0f};
        TArray<float> const Alpha{0.5f};
        TArray<float> Out;
        Out.Init(-1.0f, From.Num());

        bool const bResult{ml::lerp_1d<float>(From, To, Alpha, Out)};

        TestFalse(TEXT("Result"), bResult);
        TestEqual(TEXT("Out[0] unchanged"), Out[0], -1.0f);
        TestEqual(TEXT("Out[1] unchanged"), Out[1], -1.0f);
    });

    It("returns false and leaves output unchanged when Out has the wrong size", [this]() {
        TArray<float> const From{0.0f, 10.0f};
        TArray<float> const To{10.0f, 20.0f};
        TArray<float> const Alpha{0.5f, 0.5f};
        TArray<float> Out;
        Out.Init(-1.0f, 1);

        bool const bResult{ml::lerp_1d<float>(From, To, Alpha, Out)};

        TestFalse(TEXT("Result"), bResult);
        TestEqual(TEXT("Out[0] unchanged"), Out[0], -1.0f);
    });
}
