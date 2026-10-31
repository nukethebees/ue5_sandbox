#include "SpaceGameRendering/SparkEffects.h"

#include "SpaceGameRendering/SparkRendererComponent.h"
#include <ioj/presentation/sparks.h>

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
TAutoConsoleVariable<int32> use_cpu_expansion{TEXT("sg.Sparks.CpuExpansion"),
                                              0,
                                              TEXT("Expands sparks on the CPU when non-zero."),
                                              ECVF_Default};
}

FSparkEffects::FSparkEffects(USparkRendererComponent& renderer)
    : renderer_{&renderer}
    , queue_{renderer.get_capacity(), renderer.get_effect_time()} {}

void FSparkEffects::queue_burst(ioj::presentation::SparkBurst const& burst) {
    check(IsInGameThread());
    queue_.queue(burst);
}

void FSparkEffects::commit(float const delta_seconds) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FSparkEffects::commit);
    queue_.advance(delta_seconds);
    auto const transform{renderer_->GetComponentTransform()};
    for (auto& burst : queue_.bursts()) {
        auto& emission{burst.emission};
        emission.location = ioj::Vector3f{transform.InverseTransformPosition(
            FVector{emission.location.x(), emission.location.y(), emission.location.z()})};
        emission.direction = ioj::Vector3f{transform.InverseTransformVectorNoScale(
            FVector{emission.direction.x(), emission.direction.y(), emission.direction.z()})};
    }
    int64 requested_count{};
    int32 admitted_count{};
    int32 overwritten{};
    double expansion_milliseconds{};
    auto const submission_start_cycles{FPlatformTime::Cycles64()};
    if (SpaceGame::Sparks::Private::use_cpu_expansion.GetValueOnGameThread() != 0) {
        auto const expansion_start_cycles{FPlatformTime::Cycles64()};
        requested_count = ioj::presentation::expand_spark_bursts(
            queue_.bursts(), renderer_->get_capacity(), queue_.effect_time(), expanded_particles_);
        expansion_milliseconds =
            FPlatformTime::ToMilliseconds64(FPlatformTime::Cycles64() - expansion_start_cycles);
        admitted_count = static_cast<int32>(expanded_particles_.size());
        overwritten = renderer_->submit_particles(expanded_particles_, queue_.effect_time());
    } else {
        expanded_particles_.clear();
        auto const result{renderer_->submit_bursts(queue_.bursts(), queue_.effect_time())};
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
    TRACE_COUNTER_SET(SandboxSparkDroppedBursts, queue_.dropped_bursts());
    TRACE_COUNTER_SET(SandboxSparkExpansionMs, expansion_milliseconds);
    TRACE_COUNTER_SET(SandboxSparkSubmissionMs, submission_milliseconds);
    queue_.clear_pending();
}

void FSparkEffects::clear() {
    queue_.clear();
    expanded_particles_.clear();
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
                .emission = {.location = ioj::Vector3f{location},
                             .direction = {0.f, 0.f, 1.f},
                             .colour = {1.f, 0.35f, 0.05f},
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
