// Fill out your copyright notice in the Description page of Project Settings.

#include "Sandbox/actor_components/MassBulletVisualizationComponent.h"

UMassBulletVisualizationComponent::UMassBulletVisualizationComponent() {
    PrimaryComponentTick.bCanEverTick = false;
}

void UMassBulletVisualizationComponent::BeginPlay() {
    Super::BeginPlay();

    if (auto* owner{GetOwner()}) {
        ismc = NewObject<UInstancedStaticMeshComponent>(owner, TEXT("BulletISMC"));
        ismc->RegisterComponent();
        ismc->AttachToComponent(owner->GetRootComponent(),
                                FAttachmentTransformRules::KeepRelativeTransform);
    }
}

int32 UMassBulletVisualizationComponent::add_instance(FTransform const& transform) {
    if (!ismc) {
        return -1;
    }

    if (free_indices.Num() > 0) {
        auto const index{free_indices.Pop()};
        ismc->UpdateInstanceTransform(index, transform, true, true);
        return index;
    }

    return ismc->AddInstance(transform, true);
}

void UMassBulletVisualizationComponent::update_instance(int32 instance_index,
                                                        FTransform const& transform) {
    if (ismc && instance_index >= 0) {
        ismc->UpdateInstanceTransform(instance_index, transform, true, true);
    }
}

void UMassBulletVisualizationComponent::remove_instance(int32 instance_index) {
    if (ismc && instance_index >= 0) {
        free_indices.Add(instance_index);
        ismc->UpdateInstanceTransform(
            instance_index, FTransform{FVector{0.0f, 0.0f, -10000.0f}}, true, true);
    }
}
