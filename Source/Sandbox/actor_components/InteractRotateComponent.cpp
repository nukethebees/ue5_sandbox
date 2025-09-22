#include "Sandbox/actor_components/InteractRotateComponent.h"

#include "Sandbox/data/trigger/TriggerCapabilities.h"
#include "Sandbox/subsystems/TriggerSubsystem.h"

UInteractRotateComponent::UInteractRotateComponent() {
    PrimaryComponentTick.bCanEverTick = false;
}

void UInteractRotateComponent::BeginPlay() {
    Super::BeginPlay();

    log_verbose(TEXT("BeginPlay()"));

    // Register with TriggerSubsystem
    if (auto * owner{GetOwner()}) {
        if (auto * const world{GetWorld()}) {
            if (auto * const trigger_subsystem{world->GetSubsystem<UTriggerSubsystem>()}) {
                if (auto const id{
                    trigger_subsystem->register_triggerable(*owner, rotation_payload)}) {
                    triggerable_id = *id;
                }

            } else {
                log_warning(TEXT("Couldn't get UTriggerSubsystem."));
            }
        } else {
            log_warning(TEXT("Couldn't get world."));
        }
    }
    
}

void UInteractRotateComponent::interact(AActor* interactor) {
    trigger_rotation(interactor);
}

void UInteractRotateComponent::trigger_rotation(AActor* instigator) {
    log_verbose(TEXT("Triggering rotation."));

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
        } else {
            log_warning(TEXT("Couldn't get UTriggerSubsystem."));
        }
    } else {
        log_warning(TEXT("Couldn't get world."));
    }
}
