#include <ioj/presentation/sparks.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numbers>

namespace spark_tests {
using namespace ioj;
using namespace ioj::presentation;

auto make_burst() -> SparkBurst {
    return {.emission = {.location = {10.f, 20.f, 30.f},
                         .direction = {1.f, 0.f, 0.f},
                         .colour = {1.f, 0.5f, 0.25f},
                         .seed = 12345},
            .style = {.count = 32,
                      .speed = {100.f, 200.f},
                      .lifetime = {1.f, 2.f},
                      .size = {3.f, 5.f},
                      .intensity = 10.f,
                      .spread_angle_degrees = 45.f,
                      .streak_time = 0.1f}};
}
auto velocity(SparkParticleRecord const& particle) -> Vector3f {
    auto const& value{particle.initial_velocity_lifetime};
    return {value.x(), value.y(), value.z()};
}
auto record(float const spawn_time, float const lifetime) -> SparkParticleRecord {
    SparkParticleRecord result;
    result.initial_position_spawn_time.w() = spawn_time;
    result.initial_velocity_lifetime.w() = lifetime;
    return result;
}

TEST(NativeSparks, ExpansionIsDeterministicAndWithinRanges) {
    auto const burst{make_burst()};
    std::vector<SparkParticleRecord> first;
    std::vector<SparkParticleRecord> second;
    expand_spark_bursts(std::span{&burst, 1}, burst.style.count, 7.f, first);
    expand_spark_bursts(std::span{&burst, 1}, burst.style.count, 7.f, second);
    ASSERT_EQ(first.size(), 32u);
    EXPECT_EQ(first, second);
    for (auto const& particle : first) {
        auto const speed{size(velocity(particle))};
        EXPECT_GE(speed, 100.f);
        EXPECT_LE(speed, 200.f);
        EXPECT_GE(particle.initial_velocity_lifetime.w(), 1.f);
        EXPECT_LE(particle.initial_velocity_lifetime.w(), 2.f);
        EXPECT_GE(particle.emissive_colour_size.w(), 3.f);
        EXPECT_LE(particle.emissive_colour_size.w(), 5.f);
        EXPECT_GE(velocity(particle).x() / speed,
                  std::cos(std::numbers::pi_v<float> / 4.f) - 1.e-4f);
        EXPECT_EQ(particle.initial_position_spawn_time, (Vector<float, 4>{10.f, 20.f, 30.f, 7.f}));
    }
}
TEST(NativeSparks, ZeroAndIsotropicSpreadAreHandled) {
    auto burst{make_burst()};
    burst.style.speed = {1.f, 1.f};
    burst.style.spread_angle_degrees = 0.f;
    std::vector<SparkParticleRecord> particles;
    expand_spark_bursts(std::span{&burst, 1}, burst.style.count, 0.f, particles);
    ASSERT_EQ(particles.size(), 32u);
    for (auto const& particle : particles) {
        EXPECT_TRUE(almost_equal(velocity(particle), Vector3f{1.f, 0.f, 0.f}, 1.e-4f));
    }
    burst.emission.direction = {};
    expand_spark_bursts(std::span{&burst, 1}, burst.style.count, 0.f, particles);
    for (auto const& particle : particles) {
        EXPECT_TRUE(almost_equal(velocity(particle), Vector3f{0.f, 0.f, 1.f}, 1.e-4f));
    }
    burst.emission.direction = {1.f, 0.f, 0.f};
    burst.style.spread_angle_degrees = 180.f;
    expand_spark_bursts(std::span{&burst, 1}, burst.style.count, 0.f, particles);
    EXPECT_TRUE(std::ranges::any_of(
        particles, [](auto const& particle) { return velocity(particle).x() > 0.f; }));
    EXPECT_TRUE(std::ranges::any_of(
        particles, [](auto const& particle) { return velocity(particle).x() < 0.f; }));
}
TEST(NativeSparks, InvalidAndZeroCountBurstsAreIgnored) {
    auto zero{make_burst()};
    zero.style.count = 0;
    auto invalid{make_burst()};
    invalid.emission.location.x() = std::numeric_limits<float>::quiet_NaN();
    auto reversed{make_burst()};
    reversed.style.count = 2;
    reversed.style.lifetime = {2.f, 1.f};
    std::array const bursts{zero, invalid, reversed};
    std::vector<SparkParticleRecord> particles;
    EXPECT_EQ(expand_spark_bursts(bursts, 10, 0.f, particles), 2);
    ASSERT_EQ(particles.size(), 2u);
    for (auto const& particle : particles) {
        EXPECT_GE(particle.initial_velocity_lifetime.w(), 1.f);
        EXPECT_LE(particle.initial_velocity_lifetime.w(), 2.f);
    }
    EXPECT_EQ(expand_spark_bursts(bursts, 0, 0.f, particles), 2);
    EXPECT_TRUE(particles.empty());
}
TEST(NativeSparks, OverflowSamplesAcrossFlattenedBursts) {
    auto first{make_burst()};
    first.style.count = 100;
    first.emission.colour = {1.f, 0.f, 0.f};
    auto second{first};
    second.emission.colour = {0.f, 0.f, 1.f};
    std::array const bursts{first, second};
    std::vector<SparkParticleRecord> particles;
    EXPECT_EQ(expand_spark_bursts(bursts, 10, 0.f, particles), 200);
    ASSERT_EQ(particles.size(), 10u);
    EXPECT_EQ(std::ranges::count_if(
                  particles,
                  [](auto const& particle) { return particle.emissive_colour_size.x() > 0.f; }),
              5);
    EXPECT_EQ(std::ranges::count_if(
                  particles,
                  [](auto const& particle) { return particle.emissive_colour_size.z() > 0.f; }),
              5);
}
TEST(NativeSparks, RingWrapCopiesPayloadsAndReportsLiveOverwrites) {
    SparkStorage storage;
    storage.reset(3);
    std::array const first{record(0.f, 10.f), record(0.f, 11.f)};
    std::array const second{record(2.f, 12.f), record(2.f, 13.f)};
    EXPECT_EQ(storage.submit_particles(first, 1.f).overwritten, 0);
    auto const wrapped{storage.submit_particles(second, 2.f)};
    ASSERT_EQ(wrapped.range_count, 2);
    EXPECT_EQ(wrapped.ranges[0].destination, 2);
    EXPECT_EQ(wrapped.ranges[1].destination, 0);
    EXPECT_EQ(wrapped.overwritten, 1);
    EXPECT_EQ(storage.particles()[2], second[0]);
    EXPECT_EQ(storage.particles()[0], second[1]);
    EXPECT_EQ(storage.particles()[1], first[1]);
    EXPECT_EQ(storage.submit_particles(first, 20.f).overwritten, 0);
}
TEST(NativeSparks, GpuPlansContainSelectionIndicesAndBoundedAllocations) {
    SparkStorage storage;
    storage.reset(10);
    auto first{make_burst()};
    first.style.count = 100;
    auto second{first};
    ++second.emission.seed;
    std::array const bursts{first, second};
    SparkGpuBatch pending;
    auto const result{storage.append_bursts(bursts, 0.f, pending)};
    EXPECT_EQ(result.requested, 200);
    EXPECT_EQ(result.admitted, 10);
    EXPECT_EQ(result.replaced_slots, 0);
    ASSERT_EQ(pending.bursts.size(), 2u);
    EXPECT_EQ(pending.bursts[0].allocation, (Vector<std::uint32_t, 4>{12345, 0, 5, 0}));
    EXPECT_EQ(pending.bursts[1].allocation, (Vector<std::uint32_t, 4>{12346, 5, 5, 5}));
    EXPECT_EQ(pending.selection_indices,
              (std::vector<std::uint32_t>{10, 30, 50, 70, 90, 10, 30, 50, 70, 90}));
    EXPECT_EQ(pending.upload_bytes(), 232u);
    EXPECT_EQ(storage.append_bursts(bursts, 0.f, pending).replaced_slots, 10);
    EXPECT_EQ(pending.bursts[2].allocation.w(), 10u);
    storage.clear();
    pending.clear();
    EXPECT_EQ(storage.append_bursts(bursts, 0.f, pending).replaced_slots, 0);
}
TEST(NativeSparks, PendingQueueIsBoundedAndResetClearsTimeAndDrops) {
    SparkQueue queue{2};
    auto burst{make_burst()};
    queue.queue(burst);
    ++burst.emission.seed;
    queue.queue(burst);
    queue.queue(burst);
    ASSERT_EQ(queue.bursts().size(), 2u);
    EXPECT_EQ(queue.bursts()[0].emission.seed, 12345u);
    EXPECT_EQ(queue.bursts()[1].emission.seed, 12346u);
    EXPECT_EQ(queue.dropped_bursts(), 1);
    queue.advance(2.f);
    queue.advance(-1.f);
    queue.advance(std::numeric_limits<float>::quiet_NaN());
    EXPECT_EQ(queue.effect_time(), 2.f);
    queue.clear_pending();
    EXPECT_TRUE(queue.bursts().empty());
    EXPECT_EQ(queue.effect_time(), 2.f);
    queue.clear();
    EXPECT_EQ(queue.effect_time(), 0.f);
    EXPECT_EQ(queue.dropped_bursts(), 0);
}

TEST(NativeSparks, RendererSettingsClampLimitsAndRejectNonfiniteValues) {
    SparkRendererSettings settings{
        .capacity = -1,
        .maximum_draw_distance = -1.f,
        .acceleration = {std::numeric_limits<float>::quiet_NaN(), 0.f, 0.f},
        .minimum_thickness_pixels = 8.f,
        .maximum_thickness_pixels = 2.f,
        .maximum_length_pixels = std::numeric_limits<float>::infinity()};
    auto const normalized{normalize_spark_renderer_settings(settings)};
    EXPECT_EQ(normalized.capacity, 1);
    EXPECT_EQ(normalized.maximum_draw_distance, 0.f);
    EXPECT_TRUE(is_finite(normalized.acceleration));
    EXPECT_EQ(normalized.minimum_thickness_pixels, 8.f);
    EXPECT_EQ(normalized.maximum_thickness_pixels, 8.f);
    EXPECT_TRUE(std::isfinite(normalized.maximum_length_pixels));
}
}
