#include "Sandbox/actor_components/RotatingActorComponent.h"

#include "Sandbox/subsystems/RotationManagerSubsystem.h"
#include "Sandbox/subsystems/DestructionManagerSubsystem.h"

URotatingActorComponent::URotatingActorComponent() {
    PrimaryComponentTick.bCanEverTick = false;
}

void URotatingActorComponent::BeginPlay() {
    Super::BeginPlay();

    auto* world{GetWorld()};
    if (!world) {
        UE_LOGFMT(LogTemp, Warning, "Couldn't get world in in BeginPlay.");
        return;
    }

    if (auto* rotation_manager{world->GetSubsystem<URotationManagerSubsystem>()}) {
        if (auto* owner{GetOwner()}) {
            if (auto* root{owner->GetRootComponent()}) {
                if (rotation_type == ERotationType::STATIC) {
                    rotation_manager->add(*root, speed);
                } else {
                    rotation_manager->add(*root, *this);
                }
            }
        }

    } else {
        UE_LOGFMT(LogTemp, Warning, "Couldn't get URotationManagerSubsystem.");
    }

    if (destroy_component_after_static_registration) {
        if (auto* destruction_manager{world->GetSubsystem<UDestructionManagerSubsystem>()}) {
            destruction_manager->queue_destruction(this);
        } else {
            UE_LOGFMT(LogTemp, Warning, "Couldn't get UDestructionManagerSubsystem in BeginPlay.");
        }
    }
}

void URotatingActorComponent::EndPlay(EEndPlayReason::Type EndPlayReason) {
    if (!destroy_component_after_static_registration) {
        unregister_from_subsystem();
    }

    Super::EndPlay(EndPlayReason);
}

void URotatingActorComponent::unregister_from_subsystem() {
    if (auto* world{GetWorld()}) {
        if (auto* rotation_manager{world->GetSubsystem<URotationManagerSubsystem>()}) {
            rotation_manager->remove(*this);
        } else {
            UE_LOGFMT(LogTemp, Warning, "Couldn't get UDestructionManagerSubsystem in EndPlay.");
        }
    } else {
        UE_LOGFMT(LogTemp, Warning, "Couldn't get world in in BeginPlay.");
    }
}
