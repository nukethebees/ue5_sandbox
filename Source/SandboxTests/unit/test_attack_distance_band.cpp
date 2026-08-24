#include <SpaceGame/combat/lasers/AttackDistanceBand.h>

#include <CQTest.h>

TEST_CLASS(AttackDistanceBand, "Sandbox.UnitTests")
{
    TEST_METHOD(ValuesAreValid)
    {
        FAttackDistanceBand band;
        TestRunner->TestTrue(TEXT("Default attack distance band is valid"),
                             band.values_are_valid());

        band.minimum_ratio = 0.f;
        band.desired_ratio = 0.f;
        band.maximum_ratio = 1.f;
        TestRunner->TestTrue(TEXT("Equal values and range boundaries are valid"),
                             band.values_are_valid());

        band = FAttackDistanceBand{};
        band.minimum_ratio = 0.6f;
        TestRunner->TestFalse(TEXT("Minimum cannot exceed desired"), band.values_are_valid());

        band = FAttackDistanceBand{};
        band.maximum_ratio = 0.45f;
        TestRunner->TestFalse(TEXT("Desired cannot exceed maximum"), band.values_are_valid());

        band = FAttackDistanceBand{};
        band.minimum_ratio = -0.1f;
        TestRunner->TestFalse(TEXT("Minimum cannot be negative"), band.values_are_valid());

        band = FAttackDistanceBand{};
        band.maximum_ratio = 1.1f;
        TestRunner->TestFalse(TEXT("Maximum cannot exceed one"), band.values_are_valid());
    }
};
