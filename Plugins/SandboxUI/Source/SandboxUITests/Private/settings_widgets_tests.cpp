#include "SandboxUI/widgets/SettingsWidgets.h"

#include <CQTest.h>

TEST_CLASS(SettingsSlider, "SandboxUI.UnitTests")
{
    TEST_METHOD(ConvertsAndClampsItsConfiguredRange)
    {
        TestRunner->TestTrue(TEXT("The range minimum normalizes to zero"),
                             FMath::IsNearlyZero(SSettingsSlider::normalize(50.0f, 50.0f, 100.0f)));
        TestRunner->TestTrue(
            TEXT("The range maximum normalizes to one"),
            FMath::IsNearlyEqual(SSettingsSlider::normalize(100.0f, 50.0f, 100.0f), 1.0f));
        TestRunner->TestTrue(TEXT("Values below the range are clamped"),
                             FMath::IsNearlyZero(SSettingsSlider::normalize(10.0f, 50.0f, 100.0f)));
        TestRunner->TestTrue(
            TEXT("Normalized values are converted and stepped"),
            FMath::IsNearlyEqual(SSettingsSlider::denormalize(0.506f, 0.0f, 1.0f, 0.01f), 0.51f));
        TestRunner->TestTrue(
            TEXT("Converted values cannot exceed the maximum"),
            FMath::IsNearlyEqual(SSettingsSlider::denormalize(2.0f, 50.0f, 100.0f, 1.0f), 100.0f));
    }
};
