#include "Sandbox/environment/structures/IsmGrid.h"

#include "Sandbox/logging/SandboxLogCategories.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/StaticMesh.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

AIsmGrid::AIsmGrid()
    : ismc{CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("ismc"))} {

    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

    ismc->SetupAttachment(RootComponent);
}

void AIsmGrid::OnConstruction(FTransform const& transform) {
    Super::OnConstruction(transform);
    update_isms(false);
}
void AIsmGrid::update_isms_button() {
    return update_isms(true);
}
void AIsmGrid::update_isms(bool warn_on_no_mesh) {
    RETURN_IF_NULLPTR(ismc);
    if (!mesh) {
        if (warn_on_no_mesh) {
            WARN_IS_FALSE(LogSandboxActor, mesh);
        }
        return;
    }
    RETURN_IF_NULLPTR(mesh);

    ismc->ClearInstances();
    ismc->SetStaticMesh(mesh);
    ismc->PreAllocateInstancesMemory(num());

    auto const origin{GetActorLocation() + fixed_offset};
    auto const rotation{[&]() -> FQuat {
        switch (rotation_mode) {
            case ERotationMode::parent: {
                return GetActorQuat();
            }
            case ERotationMode::from_centre: {
                UE_LOG(
                    LogSandboxActor, Warning, TEXT("ERotationMode::from_centre not implemented."));
                return GetActorQuat();
            }
            default: {
                break;
            }
        }

        UE_LOG(LogSandboxActor, Error, TEXT("Unhandled IsmGrid ERotationMode"));
        return GetActorQuat();
    }()};

    auto const scale{GetActorScale()};
    auto const mesh_bounds{mesh->GetBounds().BoxExtent * 2.0};
    auto const mesh_bounds_offset{offset_by_mesh_bounds ? mesh_bounds : FVector3d::ZeroVector};
    auto const full_element_offset{mesh_bounds_offset + element_offset};

#if WITH_EDITOR
    this->mesh_bounds_ = mesh_bounds;
    this->mesh_bounds_offset_ = mesh_bounds_offset;
    this->full_element_offset_ = full_element_offset;
#endif

    for (int32 x{-x_axis.negative}; x < x_axis.positive; x++) {
        for (int32 y{-y_axis.negative}; y < y_axis.positive; y++) {
            for (int32 z{-z_axis.negative}; z < z_axis.positive; z++) {
                FVector3d index{
                    static_cast<double>(x),
                    static_cast<double>(y),
                    static_cast<double>(z),
                };
                auto const element_position{origin + (index * full_element_offset)};

                FTransform const element_transform{rotation, element_position, scale};
                constexpr bool world_space{true};
                ismc->AddInstance(element_transform, world_space);
            }
        }
    }
}
