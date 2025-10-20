#include "Sandbox/actor_components/InteractRotateComponent.h"

#include "Sandbox/data/trigger/TriggerCapabilities.h"
#include "Sandbox/subsystems/world/TriggerSubsystem.h"

UInteractRotateComponent::UInteractRotateComponent() {
    PrimaryComponentTick.bCanEverTick = false;
}

void UInteractRotateComponent::BeginPlay() {
    Super::BeginPlay();

    log_verbose(TEXT("BeginPlay()"));

    if (auto* owner{GetOwner()}) {
        if (auto* const world{GetWorld()}) {
            if (auto* const trigger_subsystem{world->GetSubsystem<UTriggerSubsystem>()}) {
                trigger_subsystem->register_triggerable(*owner, rotation_payload);
            } else {
                log_warning(TEXT("Couldn't get UTriggerSubsystem."));
            }
        } else {
            log_warning(TEXT("Couldn't get world."));
        }
    }
}
