#include <ioj/presentation/sparks.h>

#include <ioj/presentation/spark_queue.h>
#include <ioj/presentation/spark_renderer_settings.h>
#include <ioj/presentation/spark_storage.h>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <numbers>

namespace ioj::presentation {
auto normalize_spark_renderer_settings(SparkRendererSettings settings) -> SparkRendererSettings {
    SparkRendererSettings const defaults;
    settings.capacity = std::clamp(settings.capacity, 1, 1000000);
    settings.maximum_draw_distance = std::isfinite(settings.maximum_draw_distance)
                                       ? std::max(0.f, settings.maximum_draw_distance)
                                       : defaults.maximum_draw_distance;
    settings.acceleration =
        is_finite(settings.acceleration) ? settings.acceleration : defaults.acceleration;
    settings.minimum_thickness_pixels = std::isfinite(settings.minimum_thickness_pixels)
                                          ? std::max(0.f, settings.minimum_thickness_pixels)
                                          : defaults.minimum_thickness_pixels;
    settings.maximum_thickness_pixels = std::isfinite(settings.maximum_thickness_pixels)
                                          ? settings.maximum_thickness_pixels
                                          : defaults.maximum_thickness_pixels;
    settings.maximum_thickness_pixels =
        std::max(settings.minimum_thickness_pixels, settings.maximum_thickness_pixels);
    settings.maximum_length_pixels = std::isfinite(settings.maximum_length_pixels)
                                       ? std::max(0.f, settings.maximum_length_pixels)
                                       : defaults.maximum_length_pixels;
    return settings;
}
namespace spark_detail {
auto hash(std::uint32_t value) -> std::uint32_t {
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    value ^= value >> 16;
    return value;
}
auto random_unit_float(std::uint32_t& state) -> float {
    state = hash(state + 0x9e3779b9u);
    return static_cast<float>(state >> 8) * (1.f / 16777216.f);
}
auto random_range(std::uint32_t& state, SparkScalarRange const range) -> float {
    return std::lerp(
        std::min(range.min, range.max), std::max(range.min, range.max), random_unit_float(state));
}
auto is_finite(SparkScalarRange const range) -> bool {
    return std::isfinite(range.min) && std::isfinite(range.max);
}
auto cone_cosine(float const spread_angle_degrees) -> float {
    return std::cos(std::clamp(spread_angle_degrees, 0.f, 180.f) *
                    (std::numbers::pi_v<float> / 180.f));
}
auto direction(SparkBurst const& burst) -> Vector3f {
    auto const value{burst.emission.direction};
    return size_squared(value) < 1.e-8f ? Vector3f{0.f, 0.f, 1.f} : safe_normal(value);
}
auto sample_cone(SparkBurst const& burst, std::uint32_t& state) -> Vector3f {
    auto const axis{direction(burst)};
    auto const cosine{
        std::lerp(1.f, cone_cosine(burst.style.spread_angle_degrees), random_unit_float(state))};
    auto const sine{std::sqrt(std::max(0.f, 1.f - cosine * cosine))};
    auto const azimuth{2.f * std::numbers::pi_v<float> * random_unit_float(state)};
    auto const helper{std::abs(axis.z()) < 0.999f ? Vector3f{0.f, 0.f, 1.f}
                                                  : Vector3f{0.f, 1.f, 0.f}};
    auto const tangent{safe_normal(cross(helper, axis))};
    auto const bitangent{cross(axis, tangent)};
    return axis * cosine + tangent * (sine * std::cos(azimuth)) +
           bitangent * (sine * std::sin(azimuth));
}
auto expand_particle(SparkBurst const& burst, std::int32_t const index, float const time)
    -> SparkParticleRecord {
    auto const& emission{burst.emission};
    auto const& style{burst.style};
    auto state{hash(emission.seed ^ static_cast<std::uint32_t>(index))};
    auto const direction{sample_cone(burst, state)};
    auto const speed{std::max(0.f, random_range(state, style.speed))};
    auto const velocity{direction * speed};
    auto const lifetime{std::max(0.f, random_range(state, style.lifetime))};
    auto const size{std::max(0.f, random_range(state, style.size))};
    auto const intensity{std::max(0.f, style.intensity) *
                         std::lerp(0.85f, 1.15f, random_unit_float(state))};
    return {{emission.location.x(), emission.location.y(), emission.location.z(), time},
            {velocity.x(), velocity.y(), velocity.z(), lifetime},
            {emission.colour.x() * intensity,
             emission.colour.y() * intensity,
             emission.colour.z() * intensity,
             size},
            {std::max(0.f, style.streak_time), 0.f, 0.f, 0.f}};
}
auto gpu_burst(SparkBurst const& burst,
               float const time,
               std::int32_t const first,
               std::int32_t const count,
               std::uint32_t const selection) -> GpuSparkBurst {
    auto const& emission{burst.emission};
    auto const& style{burst.style};
    auto const axis{direction(burst)};
    return {{emission.location.x(), emission.location.y(), emission.location.z(), time},
            {axis.x(), axis.y(), axis.z(), cone_cosine(style.spread_angle_degrees)},
            {emission.colour.x(), emission.colour.y(), emission.colour.z(), style.intensity},
            {style.speed.min, style.speed.max, style.lifetime.min, style.lifetime.max},
            {style.size.min, style.size.max, style.streak_time, 0.f},
            {emission.seed,
             static_cast<std::uint32_t>(first),
             static_cast<std::uint32_t>(count),
             selection}};
}
struct Selection {
    std::vector<std::int32_t> counts;
    std::int64_t requested{};
    std::int32_t admitted{};
};
auto select(std::span<SparkBurst const> const bursts, std::int32_t const capacity) -> Selection {
    Selection result;
    result.counts.reserve(bursts.size());
    for (auto const& burst : bursts) {
        auto const count{spark_particle_count(burst)};
        result.counts.push_back(count);
        result.requested += count;
    }
    result.admitted =
        static_cast<std::int32_t>(std::min<std::int64_t>(result.requested, std::max(0, capacity)));
    return result;
}
auto flattened_index(Selection const& selection, std::int32_t const admitted_index)
    -> std::int64_t {
    auto const quotient{selection.requested / selection.admitted};
    auto const remainder{selection.requested % selection.admitted};
    auto const numerator{(quotient % 2) * selection.admitted +
                         (static_cast<std::int64_t>(admitted_index) * 2 + 1) * remainder};
    return static_cast<std::int64_t>(admitted_index) * quotient + quotient / 2 +
           numerator / (static_cast<std::int64_t>(selection.admitted) * 2);
}
}

auto spark_particle_count(SparkBurst const& burst) -> std::int32_t {
    auto const& emission{burst.emission};
    auto const& style{burst.style};
    auto const valid{
        style.count > 0 && is_finite(emission.location) && is_finite(emission.direction) &&
        is_finite(emission.colour) && spark_detail::is_finite(style.speed) &&
        spark_detail::is_finite(style.lifetime) && spark_detail::is_finite(style.size) &&
        std::isfinite(style.intensity) && style.intensity > 0.f &&
        std::isfinite(style.spread_angle_degrees) && std::isfinite(style.streak_time) &&
        std::max(style.lifetime.min, style.lifetime.max) > 0.f};
    return valid ? style.count : 0;
}
auto expand_spark_bursts(std::span<SparkBurst const> const bursts,
                         std::int32_t const capacity,
                         float const effect_time,
                         std::vector<SparkParticleRecord>& output) -> std::int64_t {
    auto const selection{spark_detail::select(bursts, capacity)};
    output.clear();
    output.reserve(selection.admitted);
    std::size_t burst_index{};
    std::int64_t burst_start{};
    for (std::int32_t index{}; index < selection.admitted; ++index) {
        auto const flattened{spark_detail::flattened_index(selection, index)};
        while (burst_index + 1 < bursts.size() &&
               flattened >= burst_start + selection.counts[burst_index]) {
            burst_start += selection.counts[burst_index++];
        }
        output.push_back(spark_detail::expand_particle(
            bursts[burst_index], static_cast<std::int32_t>(flattened - burst_start), effect_time));
    }
    return selection.requested;
}
void SparkGpuBatch::clear() {
    bursts.clear();
    selection_indices.clear();
}
auto SparkGpuBatch::upload_bytes() const -> std::size_t {
    return bursts.size() * sizeof(GpuSparkBurst) + selection_indices.size() * sizeof(std::uint32_t);
}
void SparkStorage::reset(std::int32_t const capacity) {
    assert(capacity >= 0);
    particles_.resize(static_cast<std::size_t>(capacity));
    clear();
}
void SparkStorage::clear() {
    std::ranges::fill(particles_, SparkParticleRecord{});
    cursor_ = 0;
    allocated_slots_ = 0;
    latest_expiry_time_ = 0.f;
}
auto SparkStorage::capacity() const -> std::int32_t {
    return static_cast<std::int32_t>(particles_.size());
}
auto SparkStorage::particles() const -> std::span<SparkParticleRecord const> {
    return particles_;
}
auto SparkStorage::latest_expiry_time() const -> float {
    return latest_expiry_time_;
}
auto SparkStorage::append_bursts(std::span<SparkBurst const> const bursts,
                                 float const effect_time,
                                 SparkGpuBatch& pending) -> SparkSubmissionResult {
    auto const selection{spark_detail::select(bursts, capacity())};
    auto const count{selection.admitted};
    if (count == 0) {
        return {.requested = selection.requested};
    }
    auto const replaced{std::max(count - (capacity() - allocated_slots_), 0)};
    allocated_slots_ = std::min(capacity(), allocated_slots_ + count);
    auto append = [&](std::size_t const index,
                      std::int32_t const particle_count,
                      std::uint32_t const selection_first) {
        pending.bursts.push_back(spark_detail::gpu_burst(
            bursts[index], effect_time, cursor_, particle_count, selection_first));
        latest_expiry_time_ = std::max(latest_expiry_time_,
                                       effect_time + std::max(bursts[index].style.lifetime.min,
                                                              bursts[index].style.lifetime.max));
    };
    if (selection.requested <= capacity()) {
        auto const burst_count{bursts.size()};
        for (std::size_t index{}; index < burst_count; ++index) {
            auto const particle_count{selection.counts[index]};
            if (particle_count == 0) {
                continue;
            }
            append(index, particle_count, GpuSparkBurst::direct_particle_indices);
            cursor_ = (cursor_ + particle_count) % capacity();
        }
    } else {
        std::size_t burst_index{};
        std::int64_t burst_start{};
        bool has_burst{};
        for (std::int32_t index{}; index < count; ++index) {
            auto const flattened{spark_detail::flattened_index(selection, index)};
            auto const previous{burst_index};
            while (burst_index + 1 < bursts.size() &&
                   flattened >= burst_start + selection.counts[burst_index]) {
                burst_start += selection.counts[burst_index++];
            }
            if (!has_burst || previous != burst_index) {
                append(
                    burst_index, 0, static_cast<std::uint32_t>(pending.selection_indices.size()));
                has_burst = true;
            }
            pending.selection_indices.push_back(
                static_cast<std::uint32_t>(flattened - burst_start));
            ++pending.bursts.back().allocation.z();
            cursor_ = (cursor_ + 1) % capacity();
        }
    }
    return {selection.requested, count, replaced};
}
auto SparkStorage::submit_particles(std::span<SparkParticleRecord const> const particles,
                                    float const effect_time) -> SparkParticleSubmission {
    SparkParticleSubmission result;
    auto const count{static_cast<std::int32_t>(std::min(particles.size(), particles_.size()))};
    if (count == 0) {
        return result;
    }
    auto const first_count{std::min(count, capacity() - cursor_)};
    result.ranges[result.range_count++] = {cursor_, 0, first_count};
    if (count > first_count) {
        result.ranges[result.range_count++] = {0, first_count, count - first_count};
    }
    for (std::int32_t index{}; index < count; ++index) {
        auto& destination{particles_[(cursor_ + index) % capacity()]};
        if (destination.initial_velocity_lifetime.w() > 0.f &&
            destination.initial_position_spawn_time.w() +
                    destination.initial_velocity_lifetime.w() >
                effect_time) {
            ++result.overwritten;
        }
        destination = particles[index];
        latest_expiry_time_ = std::max(latest_expiry_time_,
                                       destination.initial_position_spawn_time.w() +
                                           destination.initial_velocity_lifetime.w());
    }
    cursor_ = (cursor_ + count) % capacity();
    allocated_slots_ = std::min(capacity(), allocated_slots_ + count);
    return result;
}
SparkQueue::SparkQueue(std::int32_t const capacity, float const effect_time)
    : capacity_{capacity}
    , effect_time_{effect_time} {
    assert(capacity >= 0);
}
void SparkQueue::queue(SparkBurst const& burst) {
    if (spark_particle_count(burst) == 0) {
        return;
    }
    if (bursts_.size() >= static_cast<std::size_t>(capacity_)) {
        ++dropped_bursts_;
        return;
    }
    bursts_.push_back(burst);
}
void SparkQueue::advance(float const delta_seconds) {
    if (std::isfinite(delta_seconds)) {
        effect_time_ += std::max(0.f, delta_seconds);
    }
}
void SparkQueue::clear_pending() {
    bursts_.clear();
}
void SparkQueue::clear() {
    clear_pending();
    effect_time_ = 0.f;
    dropped_bursts_ = 0;
}
auto SparkQueue::bursts() -> std::span<SparkBurst> {
    return bursts_;
}
auto SparkQueue::effect_time() const -> float {
    return effect_time_;
}
auto SparkQueue::dropped_bursts() const -> std::int64_t {
    return dropped_bursts_;
}
}
