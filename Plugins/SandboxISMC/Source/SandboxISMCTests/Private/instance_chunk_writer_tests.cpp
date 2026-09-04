#include "SandboxISMCInstanceChunkWriter.h"

#include <CQTest.h>

namespace {
void test_vector(FAutomationTestBase& test,
                 TCHAR const* message,
                 FVector3f const& actual,
                 FVector3f const& expected) {
    test.TestTrue(message, actual.Equals(expected, UE_KINDA_SMALL_NUMBER));
}
}

TEST_CLASS(SandboxISMCInstanceChunkWriter, "SandboxISMC.UnitTests")
{
    TEST_METHOD(PacksTransformsAndTracksItsSourceRange)
    {
        TArray<FSandboxISMCRenderInstance> packed;
        packed.SetNumUninitialized(2);
        FSandboxISMCInstanceChunkWriter writer{packed, 1024, FVector3f::ZeroVector, 0.0f, false};

        auto const positions{TArray<FVector3f>{{4.0f, 5.0f, 6.0f}, {7.0f, 8.0f, 9.0f}}};
        auto const rotations{
            TArray<FQuat4f>{FQuat4f::Identity, FQuat4f{FVector3f::UpVector, UE_HALF_PI}}};
        auto const scales{TArray<FVector3f>{{2.0f, 3.0f, 4.0f}, {1.0f, 2.0f, 3.0f}}};
        for (auto index = 0; index < packed.Num(); ++index) {
            writer.set_transform(index, positions[index], rotations[index], scales[index]);
        }

        TestRunner->TestEqual(
            TEXT("The writer exposes its source offset"), writer.first_index(), 1024);
        TestRunner->TestEqual(TEXT("The writer exposes its range length"), writer.num(), 2);
        TestRunner->TestEqual(TEXT("Render rows remain 64 bytes"),
                              static_cast<int32>(sizeof(FSandboxISMCRenderInstance)),
                              64);

        for (auto index = 0; index < packed.Num(); ++index) {
            auto const matrix{FTransform3f{rotations[index], positions[index], scales[index]}
                                  .ToMatrixWithScale()};
            TestRunner->TestTrue(TEXT("Packed origins preserve position"),
                                 packed[index].origin.Equals(FVector4f{positions[index], 0.0f}));
            TestRunner->TestTrue(TEXT("First packed rows match the transform"),
                                 packed[index].transform_row_0.Equals(FVector4f{
                                     matrix.M[0][0], matrix.M[0][1], matrix.M[0][2], 0.0f}));
            TestRunner->TestTrue(TEXT("Second packed rows match the transform"),
                                 packed[index].transform_row_1.Equals(FVector4f{
                                     matrix.M[1][0], matrix.M[1][1], matrix.M[1][2], 0.0f}));
            TestRunner->TestTrue(TEXT("Third packed rows match the transform"),
                                 packed[index].transform_row_2.Equals(FVector4f{
                                     matrix.M[2][0], matrix.M[2][1], matrix.M[2][2], 0.0f}));
        }
    }

    TEST_METHOD(AccumulatesConservativeMeshBounds)
    {
        TArray<FSandboxISMCRenderInstance> packed;
        packed.SetNumUninitialized(1);
        FSandboxISMCInstanceChunkWriter writer{packed, 0, {1.0f, 0.0f, 0.0f}, 2.0f, true};
        writer.set_transform(
            0, {10.0f, 0.0f, 0.0f}, FQuat4f{FVector3f::UpVector, UE_HALF_PI}, {-2.0f, 1.0f, 1.0f});

        auto const& bounds{writer.bounds()};
        test_vector(*TestRunner,
                    TEXT("The scaled mesh origin is rotated and translated"),
                    bounds.GetCenter(),
                    {10.0f, -2.0f, 0.0f});
        test_vector(*TestRunner,
                    TEXT("The largest absolute scale controls conservative extent"),
                    bounds.GetExtent(),
                    {4.0f, 4.0f, 4.0f});
    }
};
