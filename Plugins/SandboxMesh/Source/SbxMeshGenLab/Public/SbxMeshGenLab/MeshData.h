#pragma once

#include "CoreMinimal.h"
#include "SbxMeshGenLab/MeshMaterialRole.h"

struct FSbxMeshData {
    TArray<FVector3f> positions;
    TArray<FVector3f> normals;
    TArray<FVector2f> uvs;
    TArray<uint32> indices;
    TArray<ESbxMeshMaterialRole> triangle_material_roles;
};
