#pragma once

#include "CoreMinimal.h"

#include "MeshMaterialRole.generated.h"

UENUM()
enum class ESbxMeshMaterialRole : uint8 {
    Structure UMETA(DisplayName = "Structure (Dark)"),
    Armor UMETA(DisplayName = "Armor (Yellow)"),
    Glass UMETA(DisplayName = "Glass (Blue)"),
    Emissive UMETA(DisplayName = "Emissive (Orange)"),
};

namespace SandboxMesh {

inline constexpr int32 mesh_material_role_count{4};

[[nodiscard]] SBXMESHGENLAB_API auto get_mesh_material_slot_name(ESbxMeshMaterialRole role)
    -> FName;
[[nodiscard]] SBXMESHGENLAB_API auto get_mesh_material_color(ESbxMeshMaterialRole role)
    -> FLinearColor;

}
