#include "SbxMeshGenLab/MeshMaterialRole.h"

namespace SandboxMesh {

auto get_mesh_material_slot_name(ESbxMeshMaterialRole const role) -> FName {
    switch (role) {
        case ESbxMeshMaterialRole::Structure:
            return TEXT("Structure");
        case ESbxMeshMaterialRole::Armor:
            return TEXT("Armor");
        case ESbxMeshMaterialRole::Glass:
            return TEXT("Glass");
        case ESbxMeshMaterialRole::Emissive:
            return TEXT("Emissive");
    }

    checkNoEntry();
    return NAME_None;
}

auto get_mesh_material_color(ESbxMeshMaterialRole const role) -> FLinearColor {
    switch (role) {
        case ESbxMeshMaterialRole::Structure:
            return FLinearColor{0.025f, 0.03f, 0.04f};
        case ESbxMeshMaterialRole::Armor:
            return FLinearColor{0.95f, 0.48f, 0.025f};
        case ESbxMeshMaterialRole::Glass:
            return FLinearColor{0.025f, 0.3f, 0.5f};
        case ESbxMeshMaterialRole::Emissive:
            return FLinearColor{1.0f, 0.12f, 0.005f};
    }

    checkNoEntry();
    return FLinearColor{1.0f, 0.0f, 1.0f};
}

auto to_native(ESbxMeshMaterialRole const role) -> mesh_gen::MaterialRole {
    return static_cast<mesh_gen::MaterialRole>(role);
}

auto to_unreal(mesh_gen::MaterialRole const role) -> ESbxMeshMaterialRole {
    return static_cast<ESbxMeshMaterialRole>(role);
}

}
