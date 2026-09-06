#include <SpaceGameRendering/SparkRendererComponent.h>

#include <CQTest.h>

#include <limits>

namespace SparkEffectsTests {
auto make_burst() -> FSparkBurst {
    return {
        .location = {10.0f, 20.0f, 30.0f},
        .direction = FVector3f::ForwardVector,
        .colour = FLinearColor{1.0f, 0.5f, 0.25f},
        .speed = {100.0f, 200.0f},
        .lifetime = {1.0f, 2.0f},
        .size = {3.0f, 5.0f},
        .intensity = 10.0f,
        .spread_angle_degrees = 45.0f,
        .streak_time = 0.1f,
        .count = 32,
        .seed = 12345,
    };
}

auto make_record(float const spawn_time, float const lifetime) -> FSparkParticleRecord {
    FSparkParticleRecord record;
    record.initial_position_spawn_time.W = spawn_time;
    record.initial_velocity_lifetime.W = lifetime;
    return record;
}

auto velocity(FSparkParticleRecord const& particle) -> FVector3f {
    auto const& value{particle.initial_velocity_lifetime};
    return {value.X, value.Y, value.Z};
}
} // namespace SparkEffectsTests

TEST_CLASS(SparkEffects, "Sandbox.UnitTests")
{
    TEST_METHOD(ExpansionIsDeterministicAndWithinRanges)
    {
        auto burst{SparkEffectsTests::make_burst()};
        TArray<FSparkParticleRecord> first;
        TArray<FSparkParticleRecord> second;
        expand_spark_bursts(MakeArrayView(&burst, 1), burst.count, 7.0f, first);
        expand_spark_bursts(MakeArrayView(&burst, 1), burst.count, 7.0f, second);

        TestRunner->TestEqual(TEXT("Expansion count is exact"), first.Num(), burst.count);
        TestRunner->TestTrue(TEXT("Fixed seed produces identical records"),
                             FMemory::Memcmp(first.GetData(),
                                             second.GetData(),
                                             first.Num() * sizeof(FSparkParticleRecord)) == 0);
        auto const minimum_cosine{FMath::Cos(FMath::DegreesToRadians(45.0f))};
        for (auto const& particle : first) {
            auto const velocity{SparkEffectsTests::velocity(particle)};
            auto const speed{velocity.Length()};
            TestRunner->TestTrue(TEXT("Speed is within the authored range"),
                                 speed >= 100.0f && speed <= 200.0f);
            TestRunner->TestTrue(TEXT("Lifetime is within the authored range"),
                                 particle.initial_velocity_lifetime.W >= 1.0f &&
                                     particle.initial_velocity_lifetime.W <= 2.0f);
            TestRunner->TestTrue(TEXT("Size is within the authored range"),
                                 particle.emissive_colour_size.W >= 3.0f &&
                                     particle.emissive_colour_size.W <= 5.0f);
            TestRunner->TestTrue(
                TEXT("Direction remains inside the cone"),
                FVector3f::DotProduct(velocity / speed, FVector3f::ForwardVector) >=
                    minimum_cosine - UE_KINDA_SMALL_NUMBER);
        }
    }

    TEST_METHOD(ZeroAndIsotropicSpreadAreHandled)
    {
        auto burst{SparkEffectsTests::make_burst()};
        burst.speed = {1.0f, 1.0f};
        burst.spread_angle_degrees = 0.0f;
        TArray<FSparkParticleRecord> particles;
        expand_spark_bursts(MakeArrayView(&burst, 1), burst.count, 0.0f, particles);
        for (auto const& particle : particles) {
            TestRunner->TestTrue(TEXT("Zero spread follows the burst direction"),
                                 SparkEffectsTests::velocity(particle).Equals(
                                     FVector3f::ForwardVector, UE_KINDA_SMALL_NUMBER));
        }

        burst.spread_angle_degrees = 180.0f;
        expand_spark_bursts(MakeArrayView(&burst, 1), burst.count, 0.0f, particles);
        auto has_forward{false};
        auto has_backward{false};
        for (auto const& particle : particles) {
            auto const dot{FVector3f::DotProduct(SparkEffectsTests::velocity(particle),
                                                 FVector3f::ForwardVector)};
            has_forward |= dot > 0.0f;
            has_backward |= dot < 0.0f;
        }
        TestRunner->TestTrue(TEXT("180 degree spread covers both hemispheres"),
                             has_forward && has_backward);
    }

    TEST_METHOD(InvalidAndZeroCountBurstsAreIgnored)
    {
        auto zero_count{SparkEffectsTests::make_burst()};
        zero_count.count = 0;
        auto invalid{SparkEffectsTests::make_burst()};
        invalid.location.X = std::numeric_limits<float>::quiet_NaN();
        auto reversed_lifetime{SparkEffectsTests::make_burst()};
        reversed_lifetime.count = 2;
        reversed_lifetime.lifetime = {2.0f, 1.0f};
        FSparkBurst const bursts[]{zero_count, invalid, reversed_lifetime};
        TArray<FSparkParticleRecord> particles;

        auto const requested{expand_spark_bursts(bursts, 10, 0.0f, particles)};

        TestRunner->TestEqual(TEXT("Only valid particles are requested"), requested, int64{2});
        TestRunner->TestEqual(TEXT("Only valid particles are expanded"), particles.Num(), 2);
    }

    TEST_METHOD(OverflowSamplesAcrossFlattenedBursts)
    {
        auto first{SparkEffectsTests::make_burst()};
        auto second{first};
        first.count = 100;
        first.colour = FLinearColor::Red;
        second.count = 100;
        second.colour = FLinearColor::Blue;
        FSparkBurst const bursts[]{first, second};
        TArray<FSparkParticleRecord> particles;
        auto const requested{expand_spark_bursts(bursts, 10, 0.0f, particles)};

        TestRunner->TestEqual(TEXT("All requested particles are counted"), requested, int64{200});
        TestRunner->TestEqual(TEXT("Admission is bounded by capacity"), particles.Num(), 10);
        auto red_count{0};
        auto blue_count{0};
        for (auto const& particle : particles) {
            red_count += particle.emissive_colour_size.X > 0.0f ? 1 : 0;
            blue_count += particle.emissive_colour_size.Z > 0.0f ? 1 : 0;
        }
        TestRunner->TestEqual(TEXT("Sampling represents the first burst"), red_count, 5);
        TestRunner->TestEqual(TEXT("Sampling represents the second burst"), blue_count, 5);
    }

    TEST_METHOD(RingWrapReportsLiveOverwrites)
    {
        auto* const renderer{NewObject<USparkRendererComponent>()};
        FSparkRendererSettings settings;
        settings.capacity = 3;
        renderer->initialise(settings);

        FSparkParticleRecord const first[]{SparkEffectsTests::make_record(0.0f, 10.0f),
                                           SparkEffectsTests::make_record(0.0f, 10.0f)};
        FSparkParticleRecord const second[]{SparkEffectsTests::make_record(0.0f, 10.0f),
                                            SparkEffectsTests::make_record(0.0f, 10.0f)};
        TestRunner->TestEqual(TEXT("Initial allocation overwrites nothing"),
                              renderer->submit_particles(first, 1.0f),
                              0);
        TestRunner->TestEqual(TEXT("Wrapped allocation overwrites one live slot"),
                              renderer->submit_particles(second, 1.0f),
                              1);
        TestRunner->TestEqual(TEXT("Expired slots are not counted as overwritten"),
                              renderer->submit_particles(first, 20.0f),
                              0);
    }

    TEST_METHOD(QueuedBurstsAccumulateUntilCommitAndClearResetsTheRing)
    {
        auto* const renderer{NewObject<USparkRendererComponent>()};
        FSparkRendererSettings settings;
        settings.capacity = 2;
        renderer->initialise(settings);
        FSparkEffects effects{*renderer};
        auto burst{SparkEffectsTests::make_burst()};
        burst.count = 1;
        burst.lifetime = {10.0f, 10.0f};

        effects.queue_burst(burst);
        burst.seed += 1;
        effects.queue_burst(burst);
        effects.commit(0.0f);

        FSparkParticleRecord const replacement[]{SparkEffectsTests::make_record(0.0f, 10.0f)};
        TestRunner->TestEqual(TEXT("Both pre-commit bursts occupy the ring"),
                              renderer->submit_particles(replacement, 0.0f),
                              1);
        effects.clear();
        TestRunner->TestEqual(TEXT("Clear removes live ring entries"),
                              renderer->submit_particles(replacement, 0.0f),
                              0);
        TestRunner->TestEqual(
            TEXT("Clear resets presentation time"), renderer->get_effect_time(), 0.0f);
    }
};
