#pragma once

#include <mesh_gen/MeshGeneration.h>

#include "CoreMinimal.h"

#include "MeshMaterialRole.generated.h"

UENUM()
enum class ESbxMeshMaterialRole : uint8 {
    Structure UMETA(DisplayName = "Structure (Dark)"),
    Armor UMETA(DisplayName = "Armor (Yellow)"),
    Glass UMETA(DisplayName = "Glass (Blue)"),
    Emissive UMETA(DisplayName = "Emissive Slot (Orange Preview)"),
};

namespace SandboxMesh {

inline constexpr int32 mesh_material_role_count{4};

[[nodiscard]] SBXMESHGENLAB_API auto get_mesh_material_slot_name(ESbxMeshMaterialRole role)
    -> FName;
[[nodiscard]] SBXMESHGENLAB_API auto get_mesh_material_color(ESbxMeshMaterialRole role)
    -> FLinearColor;
[[nodiscard]] SBXMESHGENLAB_API auto to_native(ESbxMeshMaterialRole role) -> mesh_gen::MaterialRole;
[[nodiscard]] SBXMESHGENLAB_API auto to_unreal(mesh_gen::MaterialRole role) -> ESbxMeshMaterialRole;

}
