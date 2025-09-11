#include "Sandbox/actor_components/InteractorComponent.h"

#include "Sandbox/actor_components/InteractableComponent.h"

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
                        ETriggerEvent::Triggered,
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

    // Only trigger the first valid one
    for (auto& hit_result : hit_results) {
        auto* const actor{hit_result.GetActor()};

        log_verbose(TEXT("Checking hit actor %s."), *actor->GetName());

        if ((actor == owner) || (actor == nullptr)) {
            continue;
        }

        if (auto* interactable_component{actor->FindComponentByClass<UInteractableComponent>()}) {
            log_verbose(TEXT("Found UInteractableComponent."));
            if (interactable_component->try_interact(owner) != EInteractTriggered::none_triggered) {
                break;
            }
        }

        if (IInteractable::try_interact(actor, owner)) {
            break;
        }
    }

    cooling_down = true;
    constexpr bool loop_timer{false};
    world->GetTimerManager().SetTimer(
        cooldown_handle, [this]() { cooling_down = false; }, interaction_cooldown, loop_timer);
}
