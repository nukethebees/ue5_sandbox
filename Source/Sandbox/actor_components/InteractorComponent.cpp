#include "Sandbox/actor_components/InteractorComponent.h"

#include "Sandbox/actor_components/InteractableComponent.h"
#include "Sandbox/data/TriggerCapabilities.h"
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
    if (cooling_down) {
        log_verbose(TEXT("Cooling down. Cannot interact."));
        return;
    }

    auto* const world{GetWorld()};
    auto* const owner{GetOwner()};
    if (!world) {
        log_warning(TEXT("Could not get world pointer."));
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
        log_verbose(TEXT("No hit."));
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
        log_verbose(TEXT("No valid hit actors."));
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

        auto const results{subsystem->trigger(hit_actors, source)};

        if (results.any_triggered()) {
            log_verbose(TEXT("Triggered %d actors through new system"), results.n_triggered);

            cooling_down = true;
            constexpr bool loop_timer{false};
            world->GetTimerManager().SetTimer(
                cooldown_handle,
                [this]() { cooling_down = false; },
                interaction_cooldown,
                loop_timer);
            return;
        }

        // If no actors triggered through new system, try old system with non-triggered actors
        log_verbose(TEXT("No actors triggered through new system, trying old system"));
        auto nt{results.get_not_triggered()};
        hit_actors = TArray<AActor*>(nt.data(), nt.size());
    }

    // Fallback to old interaction system
    for (auto* const actor : hit_actors) {
        log_verbose(TEXT("Checking hit actor %s with old system."), *actor->GetName());

        if (auto* interactable_component{actor->FindComponentByClass<UInteractableComponent>()}) {
            log_verbose(TEXT("Found UInteractableComponent."));
            if (interactable_component->try_interact(owner) != EInteractTriggered::none_triggered) {
                log_verbose(TEXT("Component successfully interacted with."));
                break;
            } else {
                log_verbose(TEXT("Try interact triggered none."));
            }
        }

        if (IInteractable::try_interact(actor, owner)) {
            log_verbose(TEXT("Interacted with the actor directly."));
            break;
        }
    }

    cooling_down = true;
    constexpr bool loop_timer{false};
    world->GetTimerManager().SetTimer(
        cooldown_handle, [this]() { cooling_down = false; }, interaction_cooldown, loop_timer);
}
