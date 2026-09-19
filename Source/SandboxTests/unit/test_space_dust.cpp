#include <SandboxShaders/SpaceDust/SpaceDustComponent.h>

#include <CQTest.h>

TEST_CLASS(SpaceDust, "Sandbox.UnitTests")
{
    TEST_METHOD(NormalisesTuningWithoutParticleState)
    {
        FSpaceDustSettings settings;
        settings.particle_count = -7;
        settings.volume_dimensions = FVector{-2.0, 0.0, -30.0};
        settings.particle_size = -1.0f;
        settings.brightness = -2.0f;
        settings.colour = FLinearColor{-1.0f, -2.0f, -3.0f, 1.0f};
        settings.minimum_visible_speed = -100.0f;
        settings.full_visible_speed = -200.0f;
        settings.streak_seconds = -0.1f;
        settings.maximum_streak_pixels = -3.0f;
        settings.volume_edge_fade_fraction = 1.0f;

        auto const normalised{normalise_space_dust_settings(settings)};
        TestRunner->TestEqual(TEXT("Particle count clamps to zero"), normalised.particle_count, 0);
        TestRunner->TestEqual(TEXT("Volume X stays positive"), normalised.volume_dimensions.X, 1.0);
        TestRunner->TestEqual(TEXT("Volume Y stays positive"), normalised.volume_dimensions.Y, 1.0);
        TestRunner->TestEqual(TEXT("Volume Z stays positive"), normalised.volume_dimensions.Z, 1.0);
        TestRunner->TestEqual(TEXT("Particle size clamps"), normalised.particle_size, 0.0f);
        TestRunner->TestEqual(TEXT("Brightness clamps"), normalised.brightness, 0.0f);
        TestRunner->TestEqual(TEXT("Colour red clamps"), normalised.colour.R, 0.0f);
        TestRunner->TestEqual(TEXT("Minimum speed clamps"), normalised.minimum_visible_speed, 0.0f);
        TestRunner->TestTrue(TEXT("Full speed remains above minimum"),
                             normalised.full_visible_speed > normalised.minimum_visible_speed);
        TestRunner->TestEqual(TEXT("Streak seconds clamps"), normalised.streak_seconds, 0.0f);
        TestRunner->TestEqual(
            TEXT("Maximum streak pixels clamps"), normalised.maximum_streak_pixels, 0.0f);
        TestRunner->TestEqual(
            TEXT("Edge fade clamps"), normalised.volume_edge_fade_fraction, 0.49f);
    }

    TEST_METHOD(WrapsLargeAndNegativeCameraLocationsInDoublePrecision)
    {
        auto const phase{make_space_dust_translation_phase(
            FVector{16'000'000'125.0, -12'000'000'250.0, 8'000'000'999.0},
            FVector{16000.0, 12000.0, 8000.0})};
        TestRunner->TestEqual(TEXT("Large X retains its local phase"), phase.X, 125.0f);
        TestRunner->TestEqual(TEXT("Negative Y wraps into the local volume"), phase.Y, 11750.0f);
        TestRunner->TestEqual(TEXT("Large Z retains its local phase"), phase.Z, 999.0f);
    }
};
