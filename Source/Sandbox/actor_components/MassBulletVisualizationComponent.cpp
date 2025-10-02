#include "Sandbox/actor_components/MassBulletVisualizationComponent.h"

#include "Sandbox/actors/BulletActor.h"

#include "Sandbox/macros/null_checks.hpp"

UMassBulletVisualizationComponent::UMassBulletVisualizationComponent() {
    PrimaryComponentTick.bCanEverTick = false;
}

void UMassBulletVisualizationComponent::BeginPlay() {
    Super::BeginPlay();

    RETURN_IF_NULLPTR(bullet_class);
    TRY_INIT_PTR(owner, GetOwner());
    TRY_INIT_PTR(bullet_cdo, bullet_class->GetDefaultObject<ABulletActor>());
    TRY_INIT_PTR(mesh_component, bullet_cdo->FindComponentByClass<UStaticMeshComponent>());

    ismc = NewObject<UInstancedStaticMeshComponent>(owner, TEXT("BulletISMC"));
    ismc->SetStaticMesh(mesh_component->GetStaticMesh());
    ismc->SetMaterial(0, mesh_component->GetMaterial(0));
    ismc->RegisterComponent();
    ismc->AttachToComponent(owner->GetRootComponent(),
                            FAttachmentTransformRules::KeepRelativeTransform);
}

std::optional<int32> UMassBulletVisualizationComponent::add_instance(FTransform const& transform) {
    if (!ismc) {
        return std::nullopt;
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
    if (ismc) {
        ismc->UpdateInstanceTransform(instance_index, transform, true, true);
    }
}

void UMassBulletVisualizationComponent::remove_instance(int32 instance_index) {
    if (ismc) {
        free_indices.Add(instance_index);
        ismc->UpdateInstanceTransform(
            instance_index, FTransform{FVector{0.0f, 0.0f, -10000.0f}}, true, true);
    }
}
