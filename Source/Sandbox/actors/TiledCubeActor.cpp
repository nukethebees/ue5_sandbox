#include "Sandbox/actors/TiledCubeActor.h"

#include "Materials/MaterialInstanceDynamic.h"
#include "Components/StaticMeshComponent.h"

FName const ATiledCubeActor::face_material_name{TEXT("Face")};
FName const ATiledCubeActor::edge_material_name{TEXT("Edge")};

ATiledCubeActor::ATiledCubeActor() {
    PrimaryActorTick.bCanEverTick = false;

    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootScene"));

    cube_mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CubeMesh"));
    cube_mesh->SetupAttachment(RootComponent);
    cube_mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ATiledCubeActor::OnConstruction(FTransform const& Transform) {
    Super::OnConstruction(Transform);
    constexpr auto LOGGER{NestedLogger<"OnConstruction">()};

    if (!cube_mesh) {
        LOGGER.log_warning(TEXT("cube_mesh is nullptr."));
        return;
    }
    if (!cube_mesh->GetStaticMesh()) {
        LOGGER.log_warning(TEXT("cube_mesh has no static mesh."));
        return;
    }

    auto const full_width{width * tile_width};
    auto const full_height{height * tile_height};

    FVector const scale{full_width, depth, full_height};
    cube_mesh->SetWorldScale3D(scale);
    cube_mesh->SetMaterial(1, edge_material);

    if (!face_material_inst) {
        face_material_inst = UMaterialInstanceDynamic::Create(face_material, this);
    }

    if (!face_material_inst) {
        LOGGER.log_warning(TEXT("face_material_inst is nullptr."));
        return;
    }

    static FName tiling_u{TEXT("tiling_u")};
    static FName tiling_v{TEXT("tiling_v")};
    face_material_inst->SetScalarParameterValue(tiling_u, tile_width);
    face_material_inst->SetScalarParameterValue(tiling_v, tile_height);
    cube_mesh->SetMaterial(0, face_material_inst);
}
