#include "Sandbox/actors/TiledCubeActor.h"

#include "Materials/MaterialInstanceDynamic.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Texture2D.h"

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

    auto const full_width{tile_width * n_tiles_width};
    auto const full_height{tile_height * n_tiles_height};

    FVector const scale{full_width, tile_depth, full_height};
    cube_mesh->SetWorldScale3D(scale);
    cube_mesh->SetMaterialByName(edge_material_name, material.edge_material);

    if (!face_material_inst) {
        face_material_inst = UMaterialInstanceDynamic::Create(material.face_material, this);
    }

    face_material_inst->SetScalarParameterValue(Constants::tiling_u(), n_tiles_width);
    face_material_inst->SetScalarParameterValue(Constants::tiling_v(), n_tiles_height);
    face_material_inst->SetTextureParameterValue(Constants::face_texture(), material.face_texture);

    face_material_inst->SetScalarParameterValue(Constants::metallic(), material.metallic);
    face_material_inst->SetScalarParameterValue(Constants::specular(), material.specular);
    face_material_inst->SetScalarParameterValue(Constants::roughness(), material.roughness);

    face_material_inst->SetVectorParameterValue(Constants::base_colour(), material.base_colour);
    face_material_inst->SetVectorParameterValue(Constants::emissive_colour(),
                                                material.emissive_colour);

    cube_mesh->SetMaterialByName(face_material_name, face_material_inst);
}
