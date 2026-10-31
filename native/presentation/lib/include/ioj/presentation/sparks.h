#pragma once

#include <ioj/math_types.h>

#include <array>
#include <cstdint>
#include <limits>
#include <span>
#include <vector>

namespace ioj::presentation {
struct SparkScalarRange {
    float min{};
    float max{};
};

struct SparkBurstStyle {
    std::int32_t count{16};
    SparkScalarRange speed{3000.f, 10000.f};
    SparkScalarRange lifetime{0.08f, 0.25f};
    SparkScalarRange size{4.f, 12.f};
    float intensity{20.f};
    float spread_angle_degrees{90.f};
    float streak_time{0.025f};
};

struct SparkBurstEmission {
    Vector3f location{};
    Vector3f direction{0.f, 0.f, 1.f};
    Vector3f colour{1.f, 1.f, 1.f};
    std::uint32_t seed{};
};

struct SparkBurst {
    SparkBurstEmission emission;
    SparkBurstStyle style;
};

struct SparkRendererSettings {
    std::int32_t capacity{100000};
    float maximum_draw_distance{200000.f};
    Vector3f acceleration{0.f, 0.f, -980.f};
    float minimum_thickness_pixels{0.75f};
    float maximum_thickness_pixels{12.f};
    float maximum_length_pixels{128.f};
};

auto normalize_spark_renderer_settings(SparkRendererSettings settings) -> SparkRendererSettings;

struct alignas(16) SparkParticleRecord {
    auto operator==(SparkParticleRecord const&) const -> bool = default;

    Vector<float, 4> initial_position_spawn_time{};
    Vector<float, 4> initial_velocity_lifetime{};
    Vector<float, 4> emissive_colour_size{};
    Vector<float, 4> streak_time_reserved{};
};

struct alignas(16) GpuSparkBurst {
    inline static constexpr std::uint32_t direct_particle_indices{
        std::numeric_limits<std::uint32_t>::max()};

    Vector<float, 4> location_spawn_time{};
    Vector<float, 4> direction_cone_cosine{};
    Vector<float, 4> colour_intensity{};
    Vector<float, 4> speed_lifetime_ranges{};
    Vector<float, 4> size_streak_reserved{};
    Vector<std::uint32_t, 4> allocation{};
};

struct SparkGpuBatch {
    void clear();
    auto upload_bytes() const -> std::size_t;

    std::vector<GpuSparkBurst> bursts;
    std::vector<std::uint32_t> selection_indices;
};

struct SparkSubmissionResult {
    std::int64_t requested{};
    std::int32_t admitted{};
    std::int32_t replaced_slots{};
};

struct SparkUploadRange {
    std::int32_t destination{};
    std::int32_t source{};
    std::int32_t count{};
};

struct SparkParticleSubmission {
    std::array<SparkUploadRange, 2> ranges{};
    std::int32_t range_count{};
    std::int32_t overwritten{};
};

auto spark_particle_count(SparkBurst const& burst) -> std::int32_t;
auto expand_spark_bursts(std::span<SparkBurst const> bursts,
                         std::int32_t capacity,
                         float effect_time,
                         std::vector<SparkParticleRecord>& output) -> std::int64_t;

class SparkStorage {
  public:
    // Require a nonnegative capacity. Reset discards pending particle lifetimes.
    void reset(std::int32_t capacity);
    void clear();
    auto capacity() const -> std::int32_t;
    auto particles() const -> std::span<SparkParticleRecord const>;
    auto latest_expiry_time() const -> float;
    auto append_bursts(std::span<SparkBurst const> bursts,
                       float effect_time,
                       SparkGpuBatch& pending) -> SparkSubmissionResult;
    auto submit_particles(std::span<SparkParticleRecord const> particles, float effect_time)
        -> SparkParticleSubmission;
  private:
    std::vector<SparkParticleRecord> particles_;
    std::int32_t cursor_{};
    std::int32_t allocated_slots_{};
    float latest_expiry_time_{};
};

class SparkQueue {
  public:
    explicit SparkQueue(std::int32_t capacity, float effect_time = 0.f);
    void queue(SparkBurst const& burst);
    void advance(float delta_seconds);
    void clear_pending();
    void clear();
    auto bursts() -> std::span<SparkBurst>;
    auto effect_time() const -> float;
    auto dropped_bursts() const -> std::int64_t;
  private:
    std::vector<SparkBurst> bursts_;
    std::int32_t capacity_{};
    float effect_time_{};
    std::int64_t dropped_bursts_{};
};

static_assert(sizeof(SparkParticleRecord) == 64);
static_assert(sizeof(GpuSparkBurst) == 96);
}
