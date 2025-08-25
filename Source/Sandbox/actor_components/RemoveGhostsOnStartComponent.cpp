// Fill out your copyright notice in the Description page of Project Settings.

#include "Sandbox/actor_components/RemoveGhostsOnStartComponent.h"

#include "EngineUtils.h"
#include "GameFramework/Actor.h"

URemoveGhostsOnStartComponent::URemoveGhostsOnStartComponent() {
    PrimaryComponentTick.bCanEverTick = true;
}
void URemoveGhostsOnStartComponent::TickComponent(float DeltaTime,
                                                  ELevelTick TickType,
                                                  FActorComponentTickFunction* ThisTickFunction) {
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (auto* world{GetWorld()}) {
        for (auto* actor : TActorRange<AActor>(world)) {
            if (!actor) {
                continue;
            }

            auto components{actor->GetComponents()};
            for (auto* component : components) {
                if (component->ComponentHasTag(cleanup_tag)) {
#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
                    UE_LOG(LogTemp,
                           Display,
                           TEXT("URemoveGhostsOnStartComponent: Destroyed component: \"%s\" on "
                                "actor: \"%s\""),
                           *component->GetName(),
                           *actor->GetName());
#endif
                    component->DestroyComponent();
                }
            }
        }
    }

    // Clean itself up
    DestroyComponent();
}
