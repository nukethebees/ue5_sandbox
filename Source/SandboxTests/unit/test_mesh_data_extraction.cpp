#include <SGCollision/mesh_data_extraction.h>

#include <CQTest.h>
#include <Engine/StaticMesh.h>
#include <Math/Transform.h>
#include <PhysicsEngine/AggregateGeom.h>
#include <PhysicsEngine/BodySetup.h>

TEST_CLASS(MeshDataExtraction, "Sandbox.UnitTests")
{
    TEST_METHOD(CombinesCollisionBoxAabbs)
    {
        auto* const mesh{NewObject<UStaticMesh>()};
        auto* const body_setup{NewObject<UBodySetup>(mesh)};
        mesh->SetBodySetup(body_setup);

        FKBoxElem first_box{4.0f, 2.0f, 6.0f};
        first_box.Center = FVector{10.0, 0.0, 0.0};

        FKBoxElem second_box{2.0f, 6.0f, 4.0f};
        second_box.Center = FVector{-5.0, 4.0, 0.0};
        second_box.Rotation = FRotator{0.0, 90.0, 0.0};

        body_setup->AggGeom.BoxElems.Add(first_box);
        body_setup->AggGeom.BoxElems.Add(second_box);

        auto const aabb{ml::get_aabb(*mesh)};

        TestRunner->TestTrue(TEXT("Combined AABB is valid"), aabb.IsValid != 0);
        TestRunner->TestTrue(TEXT("Combined AABB has the expected minimum"),
                             aabb.Min.Equals(FVector{-8.0, -1.0, -3.0}));
        TestRunner->TestTrue(TEXT("Combined AABB has the expected maximum"),
                             aabb.Max.Equals(FVector{12.0, 5.0, 3.0}));
    }

    TEST_METHOD(NoCollisionBoxesReturnsInvalidAabb)
    {
        auto* const mesh{NewObject<UStaticMesh>()};
        auto* const body_setup{NewObject<UBodySetup>(mesh)};
        mesh->SetBodySetup(body_setup);

        auto const aabb{ml::get_aabb(*mesh)};

        TestRunner->TestFalse(TEXT("AABB is invalid"), aabb.IsValid != 0);
    }

    TEST_METHOD(TransformsNonUniformlyScaledCollisionBox)
    {
        FKAggregateGeom geometry;
        FKBoxElem box{2.0f, 4.0f, 6.0f};
        box.Center = FVector{1.0, 0.0, 0.0};
        geometry.BoxElems.Add(box);

        FTransform const local_to_world{
            FRotator{0.0, 90.0, 0.0}, FVector{10.0, 20.0, 30.0}, FVector{2.0, 3.0, 4.0}};
        auto const aabb{ml::get_aabb(geometry, local_to_world)};

        TestRunner->TestTrue(TEXT("Transformed AABB is valid"), aabb.IsValid != 0);
        TestRunner->TestTrue(TEXT("Transformed AABB has the expected minimum"),
                             aabb.Min.Equals(FVector{4.0, 20.0, 18.0}));
        TestRunner->TestTrue(TEXT("Transformed AABB has the expected maximum"),
                             aabb.Max.Equals(FVector{16.0, 24.0, 42.0}));
    }
};
