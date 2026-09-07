#include "SpaceGameRendering/SparkEffects.h"

#include "SpaceGameRendering/SparkRendererComponent.h"

#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "ProfilingDebugging/CountersTrace.h"
#include "UObject/UObjectIterator.h"

TRACE_DECLARE_INT_COUNTER(SandboxSparkRequested, TEXT("Sandbox/Sparks/Requested"));
TRACE_DECLARE_INT_COUNTER(SandboxSparkAdmitted, TEXT("Sandbox/Sparks/Admitted"));
TRACE_DECLARE_INT_COUNTER(SandboxSparkOverwritten, TEXT("Sandbox/Sparks/Overwritten"));
TRACE_DECLARE_INT_COUNTER(SandboxSparkDroppedBursts, TEXT("Sandbox/Sparks/DroppedBursts"));
TRACE_DECLARE_FLOAT_COUNTER(SandboxSparkExpansionMs, TEXT("Sandbox/Sparks/ExpansionMs"));
TRACE_DECLARE_FLOAT_COUNTER(SandboxSparkSubmissionMs, TEXT("Sandbox/Sparks/SubmissionMs"));

namespace SpaceGame::Sparks::Private {
TAutoConsoleVariable<int32> use_cpu_expansion{
    TEXT("sg.Sparks.CpuExpansion"),
    0,
    TEXT("Uses the legacy CPU spark burst expander when non-zero."),
    ECVF_Default};

auto hash(uint32 value) -> uint32 {
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    value ^= value >> 16;
    return value;
}

auto random_unit_float(uint32& state) -> float {
    state = hash(state + 0x9e3779b9u);
    return static_cast<float>(state >> 8) * (1.0f / 16777216.0f);
}

auto random_range(uint32& state, FSparkScalarRange const range) -> float {
    auto const minimum{FMath::Min(range.min, range.max)};
    auto const maximum{FMath::Max(range.min, range.max)};
    return FMath::Lerp(minimum, maximum, random_unit_float(state));
}

auto is_finite(FSparkScalarRange const range) -> bool {
    return FMath::IsFinite(range.min) && FMath::IsFinite(range.max);
}

auto particle_count(FSparkBurst const& burst) -> int32 {
    auto const& emission{burst.emission};
    auto const& style{burst.style};
    auto const colour_is_finite{!emission.colour.ContainsNaN()};
    auto const valid{
        style.count > 0 && !emission.location.ContainsNaN() && !emission.direction.ContainsNaN() &&
        colour_is_finite && is_finite(style.speed) && is_finite(style.lifetime) &&
        is_finite(style.size) && FMath::IsFinite(style.intensity) && style.intensity > 0.0f &&
        FMath::IsFinite(style.spread_angle_degrees) && FMath::IsFinite(style.streak_time) &&
        FMath::Max(style.lifetime.min, style.lifetime.max) > 0.0f};
    return valid ? style.count : 0;
}

auto sample_cone(FVector3f direction, float const spread_angle_degrees, uint32& state)
    -> FVector3f {
    direction = direction.GetSafeNormal(UE_SMALL_NUMBER, FVector3f::UpVector);
    auto const maximum_angle{
        FMath::DegreesToRadians(FMath::Clamp(spread_angle_degrees, 0.0f, 180.0f))};
    auto const cosine{FMath::Lerp(1.0f, FMath::Cos(maximum_angle), random_unit_float(state))};
    auto const sine{FMath::Sqrt(FMath::Max(0.0f, 1.0f - cosine * cosine))};
    auto const azimuth{2.0f * UE_PI * random_unit_float(state)};

    auto const helper{FMath::Abs(direction.Z) < 0.999f ? FVector3f::UpVector
                                                       : FVector3f::RightVector};
    auto const tangent{FVector3f::CrossProduct(helper, direction).GetSafeNormal()};
    auto const bitangent{FVector3f::CrossProduct(direction, tangent)};
    return direction * cosine + tangent * (sine * FMath::Cos(azimuth)) +
           bitangent * (sine * FMath::Sin(azimuth));
}

auto expand_particle(FSparkBurst const& burst,
                     int32 const original_particle_index,
                     float const time) -> FSparkParticleRecord {
    auto const& emission{burst.emission};
    auto const& style{burst.style};
    auto state{hash(emission.seed ^ static_cast<uint32>(original_particle_index))};
    auto const direction{sample_cone(emission.direction, style.spread_angle_degrees, state)};
    auto const speed{FMath::Max(0.0f, random_range(state, style.speed))};
    auto const lifetime{FMath::Max(0.0f, random_range(state, style.lifetime))};
    auto const size{FMath::Max(0.0f, random_range(state, style.size))};
    auto const brightness_variation{FMath::Lerp(0.85f, 1.15f, random_unit_float(state))};
    auto const intensity{FMath::Max(0.0f, style.intensity) * brightness_variation};
    return {
        .initial_position_spawn_time =
            FVector4f{emission.location.X, emission.location.Y, emission.location.Z, time},
        .initial_velocity_lifetime =
            FVector4f{direction.X * speed, direction.Y * speed, direction.Z * speed, lifetime},
        .emissive_colour_size = FVector4f{emission.colour.X * intensity,
                                          emission.colour.Y * intensity,
                                          emission.colour.Z * intensity,
                                          size},
        .streak_time_reserved = FVector4f{FMath::Max(0.0f, style.streak_time), 0.0f, 0.0f, 0.0f},
    };
}
} // namespace SpaceGame::Sparks::Private

auto expand_spark_bursts(TConstArrayView<FSparkBurst> const bursts,
                         int32 const capacity,
                         float const effect_time,
                         TArray<FSparkParticleRecord>& output) -> int64 {
    TArray<int32, TInlineAllocator<64>> burst_counts;
    burst_counts.SetNumUninitialized(bursts.Num());
    int64 requested_count{0};
    auto const burst_count{bursts.Num()};
    for (int32 index{0}; index < burst_count; ++index) {
        burst_counts[index] = SpaceGame::Sparks::Private::particle_count(bursts[index]);
        requested_count += burst_counts[index];
    }
    auto const admitted_count{
        static_cast<int32>(FMath::Min<int64>(requested_count, FMath::Max(capacity, 0)))};
    output.Reset(admitted_count);
    if (admitted_count <= 0) {
        return requested_count;
    }

    int32 burst_index{0};
    int64 burst_start{0};
    auto const quotient{requested_count / admitted_count};
    auto const remainder{requested_count % admitted_count};
    for (int32 admitted_index{0}; admitted_index < admitted_count; ++admitted_index) {
        auto const half_remainder_numerator{(quotient % 2) * admitted_count +
                                            (static_cast<int64>(admitted_index) * 2 + 1) *
                                                remainder};
        auto const flattened_index{static_cast<int64>(admitted_index) * quotient + quotient / 2 +
                                   half_remainder_numerator / (admitted_count * 2)};
        while (burst_index + 1 < burst_count &&
               flattened_index >= burst_start + burst_counts[burst_index]) {
            burst_start += burst_counts[burst_index];
            ++burst_index;
        }
        auto const original_particle_index{static_cast<int32>(flattened_index - burst_start)};
        output.Add(SpaceGame::Sparks::Private::expand_particle(
            bursts[burst_index], original_particle_index, effect_time));
    }
    return requested_count;
}

FSparkEffects::FSparkEffects(USparkRendererComponent& renderer)
    : renderer_{&renderer}
    , effect_time_{renderer.get_effect_time()} {}

void FSparkEffects::queue_burst(FSparkBurst const& burst) {
    check(IsInGameThread());
    if (SpaceGame::Sparks::Private::particle_count(burst) <= 0) {
        return;
    }
    if (queued_bursts_.Num() >= renderer_->get_capacity()) {
        ++dropped_bursts_;
        return;
    }
    queued_bursts_.Add(burst);
}

void FSparkEffects::commit(float const dt) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FSparkEffects::commit);
    if (FMath::IsFinite(dt)) {
        effect_time_ += FMath::Max(dt, 0.0f);
    }
    auto const capacity{renderer_->get_capacity()};
    for (auto& burst : queued_bursts_) {
        auto& emission{burst.emission};
        emission.location = FVector3f{renderer_->GetComponentTransform().InverseTransformPosition(
            FVector{emission.location})};
        emission.direction =
            FVector3f{renderer_->GetComponentTransform().InverseTransformVectorNoScale(
                FVector{emission.direction})};
    }
    int64 requested_count{0};
    int32 admitted_count{0};
    int32 overwritten{0};
    double expansion_milliseconds{0.0};
    auto const submission_start_cycles{FPlatformTime::Cycles64()};
    if (SpaceGame::Sparks::Private::use_cpu_expansion.GetValueOnGameThread() != 0) {
        auto const expansion_start_cycles{FPlatformTime::Cycles64()};
        requested_count =
            expand_spark_bursts(queued_bursts_, capacity, effect_time_, expanded_particles_);
        expansion_milliseconds =
            FPlatformTime::ToMilliseconds64(FPlatformTime::Cycles64() - expansion_start_cycles);
        admitted_count = expanded_particles_.Num();
        overwritten = renderer_->submit_particles(expanded_particles_, effect_time_);
    } else {
        expanded_particles_.Reset();
        auto const result{renderer_->submit_bursts(queued_bursts_, effect_time_)};
        requested_count = result.requested;
        admitted_count = result.admitted;
        overwritten = result.replaced_slots;
    }
    auto const submission_milliseconds{
        FPlatformTime::ToMilliseconds64(FPlatformTime::Cycles64() - submission_start_cycles) -
        expansion_milliseconds};
    TRACE_COUNTER_SET(SandboxSparkRequested, requested_count);
    TRACE_COUNTER_SET(SandboxSparkAdmitted, admitted_count);
    TRACE_COUNTER_SET(SandboxSparkOverwritten, overwritten);
    TRACE_COUNTER_SET(SandboxSparkDroppedBursts, dropped_bursts_);
    TRACE_COUNTER_SET(SandboxSparkExpansionMs, expansion_milliseconds);
    TRACE_COUNTER_SET(SandboxSparkSubmissionMs, submission_milliseconds);
    queued_bursts_.Reset();
}

void FSparkEffects::clear() {
    queued_bursts_.Reset();
    expanded_particles_.Reset();
    effect_time_ = 0.0f;
    dropped_bursts_ = 0;
    renderer_->clear_sparks();
}

namespace SpaceGame::Sparks::Private {
void emit_debug_burst(TArray<FString> const& arguments, UWorld* const world) {
    if (!IsValid(world)) {
        return;
    }
    for (TObjectIterator<USparkRendererComponent> iterator; iterator; ++iterator) {
        auto& renderer{**iterator};
        if (renderer.GetWorld() != world || !renderer.IsRegistered()) {
            continue;
        }
        auto const location{FVector3f{
            world->GetFirstPlayerController()
                ? world->GetFirstPlayerController()->PlayerCameraManager->GetCameraLocation() +
                      world->GetFirstPlayerController()
                              ->PlayerCameraManager->GetActorForwardVector() *
                          1000.0f
                : FVector::ZeroVector}};
        auto const capacity{renderer.get_capacity()};
        auto const particles_per_burst{
            arguments.IsValidIndex(0) ? FMath::Clamp(FCString::Atoi(*arguments[0]), 1, capacity)
                                      : 256};
        auto const burst_count{arguments.IsValidIndex(1)
                                   ? FMath::Clamp(FCString::Atoi(*arguments[1]), 1, capacity)
                                   : 1};
        auto const lifetime{arguments.IsValidIndex(2)
                                ? FMath::Max(FCString::Atof(*arguments[2]), UE_SMALL_NUMBER)
                                : 0.8f};
        FSparkEffects effects{renderer};
        for (int32 burst_index{0}; burst_index < burst_count; ++burst_index) {
            effects.queue_burst({
                .emission = {.location = location,
                             .direction = FVector3f::UpVector,
                             .colour = FVector3f{1.0f, 0.35f, 0.05f},
                             .seed = 0x51a7c0deu + static_cast<uint32>(burst_index)},
                .style = {.count = particles_per_burst,
                          .speed = {2000.0f, 8000.0f},
                          .lifetime = {lifetime, lifetime},
                          .size = {5.0f, 15.0f},
                          .intensity = 30.0f,
                          .spread_angle_degrees = 180.0f,
                          .streak_time = 0.03f},
            });
        }
        effects.commit(0.0f);
        return;
    }
}

FAutoConsoleCommandWithWorldAndArgs debug_burst_command{
    TEXT("sg.Sparks.DebugBurst"),
    TEXT("Emits fixed-seed sparks in front of the player camera. Arguments: "
         "[particles_per_burst=256] [burst_count=1] [lifetime=0.8]."),
    FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&emit_debug_burst)};
} // namespace SpaceGame::Sparks::Private
