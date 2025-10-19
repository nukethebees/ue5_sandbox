#include "Sandbox/actors/TiledWallActor.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"

#include "Sandbox/macros/null_checks.hpp"

FName const ATiledWallActor::face_material_name{TEXT("Face")};
FName const ATiledWallActor::edge_material_name{TEXT("Edge")};

ATiledWallActor::ATiledWallActor() {
    PrimaryActorTick.bCanEverTick = false;

    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootScene"));

    wall_mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WallMesh"));
    wall_mesh->SetupAttachment(RootComponent);
    wall_mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

    mesh_configs.Add(EWallMeshType::Floor,
                     FWallMeshConfig{nullptr, EWorldDimension::X, EWorldDimension::Y});
    mesh_configs.Add(EWallMeshType::Ceiling,
                     FWallMeshConfig{nullptr, EWorldDimension::X, EWorldDimension::Y});
    mesh_configs.Add(EWallMeshType::Wall_PosX,
                     FWallMeshConfig{nullptr, EWorldDimension::Y, EWorldDimension::Z});
    mesh_configs.Add(EWallMeshType::Wall_NegX,
                     FWallMeshConfig{nullptr, EWorldDimension::Y, EWorldDimension::Z});
    mesh_configs.Add(EWallMeshType::Wall_PosY,
                     FWallMeshConfig{nullptr, EWorldDimension::X, EWorldDimension::Z});
    mesh_configs.Add(EWallMeshType::Wall_NegY,
                     FWallMeshConfig{nullptr, EWorldDimension::X, EWorldDimension::Z});
}

void ATiledWallActor::OnConstruction(FTransform const& Transform) {
    Super::OnConstruction(Transform);
    constexpr auto LOGGER{NestedLogger<"OnConstruction">()};

    RETURN_IF_NULLPTR(wall_mesh);

    auto const* mesh_config{mesh_configs.Find(mesh_type)};
    RETURN_IF_NULLPTR(mesh_config);
    RETURN_IF_NULLPTR(mesh_config->mesh);

    wall_mesh->SetStaticMesh(mesh_config->mesh);

    auto const scale_x{get_scale_for_dimension(EWorldDimension::X)};
    auto const scale_y{get_scale_for_dimension(EWorldDimension::Y)};
    auto const scale_z{get_scale_for_dimension(EWorldDimension::Z)};

    FVector const scale{scale_x, scale_y, scale_z};
    wall_mesh->SetWorldScale3D(scale);

    RETURN_IF_NULLPTR(material.face_material);
    RETURN_IF_NULLPTR(material.edge_material);

    wall_mesh->SetMaterialByName(edge_material_name, material.edge_material);

    if (!face_material_inst) {
        face_material_inst = UMaterialInstanceDynamic::Create(material.face_material, this);
    }

    face_material_inst->SetScalarParameterValue(Constants::tiling_u(), scale_u);
    face_material_inst->SetScalarParameterValue(Constants::tiling_v(), scale_v);
    face_material_inst->SetTextureParameterValue(Constants::face_texture(), material.face_texture);

    face_material_inst->SetScalarParameterValue(Constants::metallic(), material.metallic);
    face_material_inst->SetScalarParameterValue(Constants::specular(), material.specular);
    face_material_inst->SetScalarParameterValue(Constants::roughness(), material.roughness);

    face_material_inst->SetVectorParameterValue(Constants::base_colour(), material.base_colour);
    face_material_inst->SetVectorParameterValue(Constants::emissive_colour(),
                                                material.emissive_colour);

    wall_mesh->SetMaterialByName(face_material_name, face_material_inst);
}
