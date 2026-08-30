#pragma once

#include "SbxMeshGenLab/BoxGenerator.h"
#include "SbxMeshGenLab/ConeGenerator.h"
#include "SbxMeshGenLab/CylinderGenerator.h"
#include "SbxMeshGenLab/HexFrameGenerator.h"
#include "SbxMeshGenLab/HexTileGenerator.h"
#include "SbxMeshGenLab/HoneycombPanelGenerator.h"
#include "SbxMeshGenLab/SphereGenerator.h"

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
};

struct FSbxMeshGenerationRequest {
    ESbxMeshShape shape{ESbxMeshShape::Box};
    FName asset_name{TEXT("SM_GeneratedBox")};
    FSbxBoxParameters box;
    FSbxCylinderParameters cylinder;
    FSbxSphereParameters sphere;
    FSbxConeParameters cone;
    FSbxHexTileParameters hex_tile;
    FSbxHexFrameParameters hex_frame;
    FSbxHoneycombPanelParameters honeycomb_panel;
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
