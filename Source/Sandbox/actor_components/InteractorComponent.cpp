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
    auto* const world{GetWorld()};
    auto* const owner{GetOwner()};
    if (!world) {
        return;
    }

    auto const forward{owner->GetActorForwardVector()};
    auto const start{owner->GetActorLocation() + forward * forward_offset +
                     FVector(0.0f, 0.0f, height_offset)};
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
    DrawDebugCapsule(world,
                     (start + end) * 0.5f, // center point of the sweep
                     capsule_half_height,
                     capsule_radius,
                     FRotationMatrix::MakeFromZ(end - start).ToQuat(),
                     FColor::Green,
                     false,
                     2.0f // duration
    );
#endif

    if (!hit) {
        return;
    }

    // Only trigger the first valid one
    for (auto& hit_result : hit_results) {
        auto* const actor{hit_result.GetActor()};
        if ((actor == owner) || (actor == nullptr)) {
            continue;
        }

        if (auto* interactable_component{actor->FindComponentByClass<UInteractableComponent>()}) {
            if (interactable_component->try_interact(owner) != EInteractTriggered::none_triggered) {
                break;
            }
        }

        if (IInteractable::try_interact(actor, owner)) {
            break;
        }
    }
}
