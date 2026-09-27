#include "SandboxISMCInstanceChunkWriter.h"

#include <CQTest.h>

namespace {
void test_vector(FAutomationTestBase& test,
                 TCHAR const* message,
                 FVector3f const& actual,
                 FVector3f const& expected) {
    test.TestTrue(message, actual.Equals(expected, 0.001f));
}
}

TEST_CLASS(SandboxISMCInstanceChunkWriter, "SandboxISMC.UnitTests")
{
    TEST_METHOD(PacksTransformsAndTracksItsSourceRange)
    {
        TArray<FSandboxISMCRenderInstance> packed;
        packed.SetNumUninitialized(2);
        FSandboxISMCInstanceChunkWriter writer{packed,
                                               {},
                                               0,
                                               1024,
                                               FBox3f{FVector3f{0, 0, 0}, FVector3f{192, 192, 192}},
                                               FVector3f{96, 96, 96},
                                               FVector3f::ZeroVector,
                                               FVector3f::ZeroVector};

        auto const positions{TArray<FVector3f>{{64.0f, 80.0f, 96.0f}, {112.0f, 128.0f, 144.0f}}};
        auto const rotations{
            TArray<FQuat4f>{FQuat4f::Identity, FQuat4f{FVector3f::UpVector, UE_HALF_PI}}};
        writer.set_transforms<ESandboxISMCBoundsMode::Supplied>(positions, rotations);

        auto const [offset, count]{writer.range()};
        TestRunner->TestEqual(TEXT("The writer exposes its source offset"), offset, 1024);
        TestRunner->TestEqual(TEXT("The writer exposes its range length"), count, 2);
        TestRunner->TestEqual(TEXT("Packed transforms occupy 12 bytes"),
                              static_cast<int32>(sizeof(FSandboxISMCRenderInstance)),
                              12);

        for (auto index = 0; index < packed.Num(); ++index) {
            auto const& value{packed[index]};
            auto const decoded{ml::sandbox_ismc::unpack_quat32(value.rotation)};
            FQuat4f const orientation{decoded.X, decoded.Y, decoded.Z, decoded.W};
            TestRunner->TestTrue(TEXT("Quaternion error stays below 0.3 degrees"),
                                 orientation.AngularDistance(rotations[index]) <
                                     FMath::DegreesToRadians(0.3f));
            for (int32 axis{0}; axis < 3; ++axis) {
                TestRunner->TestEqual(TEXT("Position decodes relative to snapshot root"),
                                      96.0f +
                                          value.position[axis] * ml::sandbox_ismc::position_quantum,
                                      positions[index][axis]);
            }
        }
    }

    TEST_METHOD(MatchesTransformedLocalBoxesWithRotation)
    {
        TArray<FSandboxISMCRenderInstance> packed;
        packed.SetNumUninitialized(1);
        FVector3f const center{1.0f, -2.0f, 3.0f};
        FVector3f const extent{2.0f, 5.0f, 0.5f};
        FBox3f const local_box{center - extent, center + extent};
        for (auto const rotation : {FQuat4f::Identity,
                                    FQuat4f{FVector3f::UpVector, UE_HALF_PI},
                                    FRotator3f{27.0f, 63.0f, -18.0f}.Quaternion()}) {
            FSandboxISMCInstanceChunkWriter writer{
                packed,
                {},
                0,
                0,
                FBox3f{FVector3f{-100, -100, -100}, FVector3f{100, 100, 100}},
                FVector3f::ZeroVector,
                center,
                extent};
            FVector3f const position{10.0f, -20.0f, 30.0f};
            {
                FVector3f const transform_positions[]{position};
                FQuat4f const transform_rotations[]{rotation};
                writer.set_transforms<ESandboxISMCBoundsMode::Calculate>(transform_positions,
                                                                         transform_rotations);
            }
            auto const expected{
                local_box.TransformBy(FTransform3f{rotation, position}.ToMatrixWithScale())};
            test_vector(*TestRunner,
                        TEXT("Minimum matches Unreal transformed AABB"),
                        writer.bounds().Min,
                        expected.Min);
            test_vector(*TestRunner,
                        TEXT("Maximum matches Unreal transformed AABB"),
                        writer.bounds().Max,
                        expected.Max);
        }
    }

    TEST_METHOD(BoundsContainDecodedCornersAfterPositionAndRotationRounding)
    {
        TArray<FSandboxISMCRenderInstance> packed;
        packed.SetNumUninitialized(1);
        FVector3f const origin{13, -27, 9};
        FVector3f const extent{70, 30, 12};
        FBox3f const domain{FVector3f{-327000}, FVector3f{327000}};
        FRandomStream random{613};
        for (int32 sample{0}; sample < 100; ++sample) {
            FSandboxISMCInstanceChunkWriter writer{
                packed, {}, 0, 0, domain, FVector3f::ZeroVector, origin, extent};
            FVector3f const position{static_cast<float>(random.FRandRange(-300000.0f, 300000.0f)),
                                     static_cast<float>(random.FRandRange(-300000.0f, 300000.0f)),
                                     static_cast<float>(random.FRandRange(-300000.0f, 300000.0f))};
            auto const rotation{FRotator3f{static_cast<float>(random.FRandRange(-180.0f, 180.0f)),
                                           static_cast<float>(random.FRandRange(-180.0f, 180.0f)),
                                           static_cast<float>(random.FRandRange(-180.0f, 180.0f))}
                                    .Quaternion()};
            {
                FVector3f const transform_positions[]{position};
                FQuat4f const transform_rotations[]{rotation};
                writer.set_transforms<ESandboxISMCBoundsMode::Calculate>(transform_positions,
                                                                         transform_rotations);
            }
            auto const& value{packed[0]};
            auto const q{ml::sandbox_ismc::unpack_quat32(value.rotation)};
            FVector3f const decoded_position{FVector3f{static_cast<float>(value.position[0]),
                                                       static_cast<float>(value.position[1]),
                                                       static_cast<float>(value.position[2])} *
                                             ml::sandbox_ismc::position_quantum};
            FTransform3f const transform{FQuat4f{q.X, q.Y, q.Z, q.W}, decoded_position};
            for (int32 corner{0}; corner < 8; ++corner) {
                auto const point{origin + FVector3f{(corner & 1) ? extent.X : -extent.X,
                                                    (corner & 2) ? extent.Y : -extent.Y,
                                                    (corner & 4) ? extent.Z : -extent.Z}};
                TestRunner->TestTrue(TEXT("Decoded geometry remains inside automatic bounds"),
                                     FSandboxISMCInstanceChunkWriter::expand_render_bounds(
                                         writer.bounds(), origin, extent)
                                         .IsInsideOrOn(transform.TransformPosition(point)));
            }
        }
    }

    TEST_METHOD(ExposesPerInstanceCustomDataSlices)
    {
        TArray<FSandboxISMCRenderInstance> packed;
        packed.SetNumUninitialized(2);
        TArray<float> custom_data;
        custom_data.SetNumUninitialized(6);
        FSandboxISMCInstanceChunkWriter writer{packed,
                                               custom_data,
                                               3,
                                               50,
                                               FBox3f{FVector3f::ZeroVector, FVector3f::ZeroVector},
                                               FVector3f::ZeroVector,
                                               FVector3f::ZeroVector,
                                               FVector3f::ZeroVector};

        auto first{writer.custom_data(0)};
        first[0] = 0.1f;
        first[1] = 0.2f;
        first[2] = 0.3f;
        auto second{writer.custom_data(1)};
        second[0] = 0.4f;
        second[1] = 0.5f;
        second[2] = 0.6f;

        TestRunner->TestEqual(
            TEXT("The writer exposes the custom-data stride"), writer.num_custom_data_floats(), 3);
        TestRunner->TestEqual(
            TEXT("The first row begins at the first float"), custom_data[0], 0.1f);
        TestRunner->TestEqual(
            TEXT("The second row follows the first row contiguously"), custom_data[3], 0.4f);
        TestRunner->TestEqual(TEXT("The final channel is retained"), custom_data[5], 0.6f);
    }
};
