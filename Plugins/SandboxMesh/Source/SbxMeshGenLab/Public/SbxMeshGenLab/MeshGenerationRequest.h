#pragma once

#include "SbxMeshGenLab/BeveledBoxGenerator.h"
#include "SbxMeshGenLab/BoxGenerator.h"
#include "SbxMeshGenLab/ConeGenerator.h"
#include "SbxMeshGenLab/CylinderGenerator.h"
#include "SbxMeshGenLab/HexFrameGenerator.h"
#include "SbxMeshGenLab/HexTileGenerator.h"
#include "SbxMeshGenLab/HoneycombPanelGenerator.h"
#include "SbxMeshGenLab/MeshMaterialRole.h"
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

using FSbxMeshGenerationRequest = mesh_gen::GenerationRequest;

namespace SandboxMesh {

[[nodiscard]] SBXMESHGENLAB_API auto make_default_mesh_request(ESbxMeshShape shape)
    -> FSbxMeshGenerationRequest;
[[nodiscard]] SBXMESHGENLAB_API auto to_native(ESbxMeshShape shape) -> mesh_gen::Shape;
[[nodiscard]] SBXMESHGENLAB_API auto to_unreal(mesh_gen::Shape shape) -> ESbxMeshShape;
[[nodiscard]] SBXMESHGENLAB_API auto validate_mesh_request(FSbxMeshGenerationRequest const& request)
    -> FString;
[[nodiscard]] SBXMESHGENLAB_API auto describe_mesh_request(FSbxMeshGenerationRequest const& request)
    -> FString;

using mesh_gen::generate_mesh;

}
