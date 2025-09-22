#include "Sandbox/actor_components/InteractorComponent.h"

#include "Sandbox/data/trigger/TriggerCapabilities.h"
#include "Sandbox/subsystems/TriggerSubsystem.h"

UInteractorComponent::UInteractorComponent() {
    PrimaryComponentTick.bCanEverTick = false;
}

void UInteractorComponent::BeginPlay() {
    Super::BeginPlay();

    auto const* const pawn{Cast<APawn>(GetOwner())};
    if (!pawn || !interact_action) {
        return;
    }

    if (auto* const eic{Cast<UEnhancedInputComponent>(pawn->InputComponent)}) {
        eic->BindAction(this->interact_action,
                        ETriggerEvent::Started,
                        this,
                        &UInteractorComponent::try_interact);
    }
}

void UInteractorComponent::try_interact() {
    static constexpr auto LOG{NestedLogger<"try_interact">()};

    LOG.log_verbose(TEXT("Start."));

    if (cooling_down) {
        LOG.log_verbose(TEXT("Cooling down. Cannot interact."));
        return;
    }

    auto* const world{GetWorld()};
    auto* const owner{GetOwner()};
    if (!world) {
        LOG.log_warning(TEXT("Could not get world pointer."));
        return;
    }

    auto const forward{owner->GetActorForwardVector()};
    FVector const vertical_offset{0.0f, 0.0f, height_offset};
    auto const start{owner->GetActorLocation() + forward * forward_offset + vertical_offset};
    auto const end{start + forward * interaction_range};

    TArray<FHitResult> hit_results;

    bool const hit{world->SweepMultiByChannel(
        hit_results,
        start,
        end,
        FQuat::Identity,
        collision_channel,
        FCollisionShape::MakeCapsule(capsule_radius, capsule_half_height))};

#if WITH_EDITOR
    constexpr float debug_sweep_centre_point{0.5f};
    constexpr float debug_capsule_duration{2.0f};
    constexpr bool debug_capsule_persistent_lines{false};
    DrawDebugCapsule(world,
                     (start + end) * debug_sweep_centre_point,
                     capsule_half_height,
                     capsule_radius,
                     FRotationMatrix::MakeFromZ(end - start).ToQuat(),
                     FColor::Green,
                     debug_capsule_persistent_lines,
                     debug_capsule_duration);
#endif

    if (!hit) {
        LOG.log_verbose(TEXT("No hit."));
        return;
    }

    // Extract actors from hit results
    TArray<AActor*> hit_actors{};
    for (auto& hit_result : hit_results) {
        auto* const actor{hit_result.GetActor()};
        if (actor && actor != owner) {
            hit_actors.Add(actor);
        }
    }

    if (hit_actors.IsEmpty()) {
        LOG.log_verbose(TEXT("No valid hit actors."));
        return;
    }

    // Try new trigger system first
    auto* subsystem{world->GetSubsystem<UTriggerSubsystem>()};
    if (subsystem) {
        FTriggeringSource source{.type = ETriggerForm::PlayerInteraction,
                                 .capabilities = {},
                                 .instigator = owner,
                                 .source_location = owner->GetActorLocation(),
                                 .source_triggerable = std::nullopt};

        source.capabilities.add_capability(ETriggerCapability::Humanoid);

        bool any_triggered{false};
        for (auto* actor : hit_actors) {
            any_triggered |= subsystem->trigger(*actor, source) == ETriggerOccurred::yes;
        }

        if (any_triggered) {
            LOG.log_verbose(TEXT("At least one actor was triggered."));

            cooling_down = true;
            constexpr bool loop_timer{false};
            world->GetTimerManager().SetTimer(
                cooldown_handle,
                [this]() { cooling_down = false; },
                interaction_cooldown,
                loop_timer);
            return;
        } else {
            LOG.log_verbose(TEXT("No actors triggered."));
        }

    } else {
        LOG.log_warning(TEXT("No UTriggerSubsystem found"));
    }
}
