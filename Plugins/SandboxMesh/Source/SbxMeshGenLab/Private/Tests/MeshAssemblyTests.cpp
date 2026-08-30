#include "SbxMeshGenLab/BoxGenerator.h"
#include "SbxMeshGenLab/MeshAssembly.h"

#include <CQTest.h>

namespace SandboxMesh {

TEST_CLASS(MeshAssembly, "SandboxMesh.UnitTests")
{
    TEST_METHOD(AppendsBuffersAndOffsetsIndices)
    {
        auto const box{generate_box()};
        FSbxMeshData assembly;

        append_transformed_mesh(assembly, box, {});
        append_transformed_mesh(assembly, box, FSbxMeshTransform{FVector3f{100.0f, 0.0f, 0.0f}});

        TestRunner->TestEqual(
            TEXT("Both vertex buffers are appended"), assembly.positions.Num(), 48);
        TestRunner->TestEqual(TEXT("Both index buffers are appended"), assembly.indices.Num(), 72);
        TestRunner->TestEqual(TEXT("Second mesh indices use the second vertex range"),
                              assembly.indices[36],
                              box.indices[0] + 24);
        TestRunner->TestTrue(
            TEXT("Second mesh is translated"),
            assembly.positions[24].Equals(box.positions[0] + FVector3f{100.0f, 0.0f, 0.0f}));
    }

    TEST_METHOD(TransformsPositionsAndNormals)
    {
        FSbxMeshData source;
        source.positions.Add(FVector3f{1.0f, 0.0f, 0.0f});
        source.normals.Add(FVector3f{1.0f, 1.0f, 0.0f}.GetSafeNormal());
        source.uvs.Add(FVector2f{0.25f, 0.75f});
        source.indices.Add(0);

        FSbxMeshTransform const transform{FVector3f{10.0f, 20.0f, 30.0f},
                                          FRotator3f{0.0f, 90.0f, 0.0f},
                                          FVector3f{2.0f, 1.0f, 1.0f}};
        FSbxMeshData destination;
        append_transformed_mesh(destination, source, transform);

        TestRunner->TestTrue(
            TEXT("Position uses scale, rotation, and translation"),
            destination.positions[0].Equals(FVector3f{10.0f, 22.0f, 30.0f}, 0.001f));
        TestRunner->TestTrue(
            TEXT("Normal uses inverse scale and rotation"),
            destination.normals[0].Equals(FVector3f{-0.894427f, 0.447214f, 0.0f}, 0.001f));
        TestRunner->TestTrue(TEXT("Transformed normal remains normalized"),
                             FMath::IsNearlyEqual(destination.normals[0].SizeSquared(), 1.0f));
        TestRunner->TestEqual(TEXT("UV is preserved"), destination.uvs[0], source.uvs[0]);
    }

    TEST_METHOD(GeneratesDeterministicAssembly)
    {
        TArray<FSbxMeshAssemblyPart> const parts{
            {make_default_mesh_request(ESbxMeshShape::Box), {}},
            {make_default_mesh_request(ESbxMeshShape::Cylinder),
             {FVector3f{75.0f, 0.0f, 0.0f},
              FRotator3f{0.0f, 45.0f, 0.0f},
              FVector3f{0.5f, 0.5f, 0.5f}}}};

        auto const first{generate_mesh_assembly(parts)};
        auto const second{generate_mesh_assembly(parts)};

        TestRunner->TestTrue(TEXT("Positions are deterministic"),
                             first.positions == second.positions);
        TestRunner->TestTrue(TEXT("Normals are deterministic"), first.normals == second.normals);
        TestRunner->TestTrue(TEXT("UVs are deterministic"), first.uvs == second.uvs);
        TestRunner->TestTrue(TEXT("Indices are deterministic"), first.indices == second.indices);
    }
};

}
