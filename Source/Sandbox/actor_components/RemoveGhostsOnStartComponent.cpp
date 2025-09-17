// Fill out your copyright notice in the Description page of Project Settings.

#include "Sandbox/actor_components/RemoveGhostsOnStartComponent.h"

#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "Sandbox/subsystems/DestructionManagerSubsystem.h"

URemoveGhostsOnStartComponent::URemoveGhostsOnStartComponent() {
    PrimaryComponentTick.bCanEverTick = true;
}
void URemoveGhostsOnStartComponent::TickComponent(float DeltaTime,
                                                  ELevelTick TickType,
                                                  FActorComponentTickFunction* ThisTickFunction) {
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (auto* world{GetWorld()}) {
        auto* destruction_manager{world->GetSubsystem<UDestructionManagerSubsystem>()};
        if (!destruction_manager) {
            return;
        }

        for (auto* actor : TActorRange<AActor>(world)) {
            if (!actor) {
                continue;
            }

            auto components{actor->GetComponents()};
            for (auto* component : components) {
                if (component->ComponentHasTag(cleanup_tag)) {
                    destruction_manager->queue_component_destruction(component);
                }
            }
        }

        // Clean itself up
        destruction_manager->queue_component_destruction(this);
    }
}
