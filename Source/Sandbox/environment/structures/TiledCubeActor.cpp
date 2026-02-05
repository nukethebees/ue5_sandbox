#include "Sandbox/environment/structures/TiledCubeActor.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

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

    RETURN_IF_NULLPTR(cube_mesh);
    RETURN_IF_NULLPTR(cube_mesh->GetStaticMesh());

    auto const full_width{tile_width * n_tiles_width};
    auto const full_height{tile_height * n_tiles_height};
    auto const full_depth{tile_depth * n_tiles_depth};

    FVector const scale{full_width, full_depth, full_height};
    cube_mesh->SetWorldScale3D(scale);
    cube_mesh->SetMaterialByName(edge_material_name, material.edge_material);

    if (!face_material_inst) {
        face_material_inst = UMaterialInstanceDynamic::Create(material.face_material, this);
    }

    face_material_inst->SetScalarParameterValue(Constants::tiling_u(), get_n(u_direction));
    face_material_inst->SetScalarParameterValue(Constants::tiling_v(), get_n(v_direction));
    face_material_inst->SetTextureParameterValue(Constants::face_texture(), material.face_texture);

    face_material_inst->SetScalarParameterValue(Constants::metallic(), material.metallic);
    face_material_inst->SetScalarParameterValue(Constants::specular(), material.specular);
    face_material_inst->SetScalarParameterValue(Constants::roughness(), material.roughness);

    face_material_inst->SetVectorParameterValue(Constants::base_colour(), material.base_colour);
    face_material_inst->SetVectorParameterValue(Constants::emissive_colour(),
                                                material.emissive_colour);

    cube_mesh->SetMaterialByName(face_material_name, face_material_inst);
}
