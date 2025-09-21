#include "Sandbox/actor_components/InteractRotateComponent.h"

#include "Sandbox/data/trigger/TriggerCapabilities.h"
#include "Sandbox/subsystems/TriggerSubsystem.h"

UInteractRotateComponent::UInteractRotateComponent() {
    PrimaryComponentTick.bCanEverTick = false;
}

void UInteractRotateComponent::BeginPlay() {
    Super::BeginPlay();

    // Register with TriggerSubsystem
    if (auto* const world{GetWorld()}) {
        if (auto* const trigger_subsystem{world->GetSubsystem<UTriggerSubsystem>()}) {
            triggerable_id = trigger_subsystem->register_triggerable(GetOwner(), rotation_payload);
        }
    }
}

void UInteractRotateComponent::interact(AActor* interactor) {
    trigger_rotation(interactor);
}

void UInteractRotateComponent::trigger_rotation(AActor* instigator) {
    if (auto* const world{GetWorld()}) {
        if (auto* const trigger_subsystem{world->GetSubsystem<UTriggerSubsystem>()}) {
            FTriggeringSource source{.type = ETriggerForm::PlayerInteraction,
                                     .capabilities = {},
                                     .instigator = instigator,
                                     .source_location = instigator ? instigator->GetActorLocation()
                                                                   : GetOwner()->GetActorLocation(),
                                     .source_triggerable = std::nullopt};

            if (instigator) {
                source.capabilities.add_capability(ETriggerCapability::Humanoid);
            }

            trigger_subsystem->trigger(triggerable_id, source);
        }
    }
}
