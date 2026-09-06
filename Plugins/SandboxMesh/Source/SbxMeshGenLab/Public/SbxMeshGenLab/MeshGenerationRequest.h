#pragma once

#include "SbxMeshGenLab/BeveledBoxGenerator.h"
#include "SbxMeshGenLab/BoxGenerator.h"
#include "SbxMeshGenLab/ConeGenerator.h"
#include "SbxMeshGenLab/CylinderGenerator.h"
#include "SbxMeshGenLab/HexFrameGenerator.h"
#include "SbxMeshGenLab/HexTileGenerator.h"
#include "SbxMeshGenLab/HoneycombPanelGenerator.h"
#include "SbxMeshGenLab/SphereGenerator.h"
#include "SbxMeshGenLab/WedgeGenerator.h"

#include "MeshGenerationRequest.generated.h"

UENUM()
enum class ESbxMeshShape : uint8 {
    Box UMETA(DisplayName = "Box"),
    Cylinder UMETA(DisplayName = "Cylinder"),
    Sphere UMETA(DisplayName = "Sphere"),
    Cone UMETA(DisplayName = "Cone"),
    HexTile UMETA(DisplayName = "Hex Tile"),
    HexFrame UMETA(DisplayName = "Hex Frame"),
    HoneycombPanel UMETA(DisplayName = "Honeycomb Panel"),
    BeveledBox UMETA(DisplayName = "Beveled Box"),
    Wedge UMETA(DisplayName = "Wedge"),
};

struct FSbxMeshGenerationRequest {
    ESbxMeshShape shape{ESbxMeshShape::Box};
    FName asset_name{TEXT("SM_GeneratedBox")};
    ESbxMeshMaterialRole material_role{ESbxMeshMaterialRole::Structure};
    FSbxBoxParameters box;
    FSbxCylinderParameters cylinder;
    FSbxSphereParameters sphere;
    FSbxConeParameters cone;
    FSbxHexTileParameters hex_tile;
    FSbxHexFrameParameters hex_frame;
    FSbxHoneycombPanelParameters honeycomb_panel;
    FSbxBeveledBoxParameters beveled_box;
    FSbxWedgeParameters wedge;
};

namespace SandboxMesh {

[[nodiscard]] SBXMESHGENLAB_API auto make_default_mesh_request(ESbxMeshShape shape)
    -> FSbxMeshGenerationRequest;
[[nodiscard]] SBXMESHGENLAB_API auto validate_mesh_request(FSbxMeshGenerationRequest const& request)
    -> FString;
[[nodiscard]] SBXMESHGENLAB_API auto generate_mesh(FSbxMeshGenerationRequest const& request)
    -> FSbxMeshData;
[[nodiscard]] SBXMESHGENLAB_API auto describe_mesh_request(FSbxMeshGenerationRequest const& request)
    -> FString;

}
