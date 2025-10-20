#include "Sandbox/actor_components/InteractorComponent.h"

#include "Engine/CollisionProfile.h"

#include "Sandbox/data/trigger/TriggerCapabilities.h"
#include "Sandbox/subsystems/world/TriggerSubsystem.h"
#include "Sandbox/utilities/actor_utils.h"

#include "Sandbox/macros/null_checks.hpp"

UInteractorComponent::UInteractorComponent() {
    PrimaryComponentTick.bCanEverTick = false;
}

void UInteractorComponent::try_interact(FVector sweep_start, FVector sweep_end) {
    static constexpr auto LOG{NestedLogger<"try_interact">()};

    if (cooling_down) {
        LOG.log_verbose(TEXT("Cooling down. Cannot interact."));
        return;
    }

    TRY_INIT_PTR(world, GetWorld());
    TRY_INIT_PTR(owner, GetOwner());
    TArray<FHitResult> hit_results;

    auto channel_name{
        UCollisionProfile::Get()->ReturnChannelNameFromContainerIndex(collision_channel)};
    LOG.log_verbose(TEXT("Doing interaction trace on channel: %s."), *channel_name.ToString());

    bool const hit{
        world->LineTraceMultiByChannel(hit_results, sweep_start, sweep_end, collision_channel)};

#if WITH_EDITOR
    constexpr float debug_sweep_centre_point{0.5f};
    constexpr float debug_capsule_duration{2.0f};
    constexpr bool debug_capsule_persistent_lines{false};
    constexpr int32 depth_priority{0};
    constexpr float debug_line_thickness{2.5f};

    DrawDebugLine(world,
                  sweep_start,
                  sweep_end,
                  FColor::Green,
                  debug_capsule_persistent_lines,
                  debug_capsule_duration,
                  depth_priority,
                  debug_line_thickness);
#endif

    if (!hit) {
        LOG.log_verbose(TEXT("No hit."));
        return;
    }

    // Extract actors from hit results
    TArray<AActor*> hit_actors{};
    for (auto& hit_result : hit_results) {
        auto* const actor{hit_result.GetActor()};
        if (actor == owner) {
            continue;
        }

        CONTINUE_IF_FALSE(actor);
        hit_actors.Add(actor);
    }

    auto const n_hit_actors{hit_actors.Num()};
    LOG.log_verbose(TEXT("%d valid hit actors."), n_hit_actors);
    if (!n_hit_actors) {
        return;
    }

    // Try new trigger system first
    TRY_INIT_PTR(subsystem, world->GetSubsystem<UTriggerSubsystem>());
    FTriggeringSource source{.type = ETriggerSourceType::PlayerInteraction,
                             .capabilities = {},
                             .instigator = owner,
                             .source_location = owner->GetActorLocation(),
                             .source_triggerable = std::nullopt};

    source.capabilities.add_capability(ETriggerSourceCapability::Humanoid);

    bool any_triggered{false};
    for (auto* actor : hit_actors) {
        any_triggered |= subsystem->trigger(*actor, source) == ETriggerOccurred::yes;
    }

    if (any_triggered) {
        LOG.log_verbose(TEXT("At least one actor was triggered."));

        cooling_down = true;
        constexpr bool loop_timer{false};
        world->GetTimerManager().SetTimer(
            cooldown_handle, [this]() { cooling_down = false; }, interaction_cooldown, loop_timer);
    }
}
void UInteractorComponent::try_interact(FVector sweep_origin, FRotator sweep_direction) {
    TRY_INIT_PTR(owner, GetOwner());
    auto const forward{sweep_direction.Vector()};
    FVector const vertical_offset{0.0f, 0.0f, height_offset};
    auto const start{sweep_origin + forward * forward_offset + vertical_offset};
    auto const end{start + forward * interaction_range};

    try_interact(start, end);
}
void UInteractorComponent::try_interact() {
    TRY_INIT_PTR(owner, GetOwner());
    try_interact(owner->GetActorLocation(), owner->GetActorForwardVector());
}
