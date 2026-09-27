#include "SandboxISMCComponent.h"

#include "AssetCompilingManager.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/MapTestSpawner.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInterface.h"
#include "RenderingThread.h"
#include "ShaderCompiler.h"
#include "TextureResource.h"
#include <CQTest.h>

TEST_CLASS(SandboxISMCRenderPixels, "SandboxISMC.RenderTests")
{
    TUniquePtr<FMapTestSpawner> spawner{nullptr};
    USandboxISMCComponent* component_{nullptr};
    USceneCaptureComponent2D* capture_{nullptr};
    UTextureRenderTarget2D* target_{nullptr};
    uint64 submitted_frame_{0};
    static constexpr int32 image_size{256};

    BEFORE_EACH()
    {
        spawner = FMapTestSpawner::CreateFromTempLevel(TestCommandBuilder);
        spawner->AddWaitUntilLoadedCommand(TestRunner);
    }

    AFTER_EACH()
    { spawner.Reset(); }

    auto setup() -> void {
        auto& world{spawner->GetWorld()};
        auto* const mesh{LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"))};
        auto* const material{LoadObject<UMaterialInterface>(
            nullptr, TEXT("/SandboxISMC/Lab/M_SandboxISMCCustomData.M_SandboxISMCCustomData"))};
        TestRunner->TestNotNull(TEXT("The cube mesh loads"), mesh);
        TestRunner->TestNotNull(TEXT("The RGB custom-data material loads"), material);
        if (mesh == nullptr || material == nullptr) {
            return;
        }
        material->EnsureIsComplete();
        FAssetCompilingManager::Get().FinishAllCompilation();
        if (GShaderCompilingManager != nullptr) {
            GShaderCompilingManager->FinishAllCompilation();
        }

        auto* const actor{world.SpawnActor<AActor>()};
        component_ = NewObject<USandboxISMCComponent>(actor);
        actor->SetRootComponent(component_);
        actor->AddInstanceComponent(component_);
        component_->set_static_mesh(*mesh);
        component_->set_num_custom_data_floats(3);
        component_->SetMaterial(0, material);
        component_->RegisterComponent();

        target_ = NewObject<UTextureRenderTarget2D>(actor);
        target_->RenderTargetFormat = RTF_RGBA16f;
        target_->ClearColor = FLinearColor::Black;
        target_->InitAutoFormat(image_size, image_size);
        target_->UpdateResourceImmediate(true);

        capture_ = NewObject<USceneCaptureComponent2D>(actor);
        actor->AddInstanceComponent(capture_);
        capture_->TextureTarget = target_;
        capture_->bCaptureEveryFrame = false;
        capture_->bCaptureOnMovement = false;
        capture_->ProjectionType = ECameraProjectionMode::Orthographic;
        capture_->OrthoWidth = 512.0f;
        capture_->CaptureSource = SCS_SceneColorHDR;
        capture_->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
        capture_->ShowOnlyComponent(component_);
        capture_->ShowFlags.SetPostProcessing(false);
        capture_->ShowFlags.SetAtmosphere(false);
        capture_->ShowFlags.SetFog(false);
        capture_->ShowFlags.SetAntiAliasing(false);
        capture_->SetWorldLocation(FVector{-500.0, 0.0, 0.0});
        capture_->SetWorldRotation(FRotator::ZeroRotator);
        capture_->RegisterComponent();
    }

    auto submit(int32 const count,
                float const height,
                int32 const colour_shift,
                FQuat4f rotation = FQuat4f::Identity) -> void {
        if (component_ == nullptr) {
            return;
        }
        int32 const visible_indices[]{0, count == 3 ? 1 : 1024, count == 3 ? 2 : 4096};
        component_->set_instances(
            count,
            FBox3f{FVector3f{0, -128, FMath::Min(0.0f, height)},
                   FVector3f{0, 10000, FMath::Max(0.0f, height)}},
            ESandboxISMCParallelism::Auto,
            [&](FSandboxISMCInstanceChunkWriter& chunk) {
                auto const [offset, chunk_count]{chunk.range()};
                TArray<FVector3f, TInlineAllocator<1024>> transform_positions;
                transform_positions.SetNumUninitialized(chunk.num());
                TArray<FQuat4f, TInlineAllocator<1024>> transform_rotations;
                transform_rotations.SetNumUninitialized(chunk.num());
                for (auto local_index = 0; local_index < chunk_count; ++local_index) {
                    auto const index{offset + local_index};
                    auto position{FVector3f{0.0f, 10000.0f, 0.0f}};
                    auto data{chunk.custom_data(local_index)};
                    data[0] = data[1] = data[2] = 0.0f;
                    for (auto slot = 0; slot < 3; ++slot) {
                        if (index == visible_indices[slot]) {
                            position = {0.0f, static_cast<float>((slot - 1) * 128), height};
                            data[(slot + colour_shift) % 3] = 1.0f;
                        }
                    }
                    transform_positions[local_index] = position;
                    transform_rotations[local_index] = rotation;
                }
                chunk.set_transforms<ESandboxISMCBoundsMode::Calculate>(transform_positions,
                                                                        transform_rotations);
            });
        submitted_frame_ = GFrameCounter;
    }

    auto check_image(
        TCHAR const* stage, int32 const row, int32 const shift, bool const empty = false) -> void {
        if (capture_ == nullptr) {
            return;
        }
        capture_->CaptureScene();
        FlushRenderingCommands();
        TArray<FLinearColor> pixels;
        auto const read{
            target_->GameThread_GetRenderTargetResource()->ReadLinearColorPixels(pixels)};
        TestRunner->TestTrue(FString::Printf(TEXT("%s: pixels can be read"), stage), read);
        if (!read || pixels.Num() != image_size * image_size) {
            TestRunner->AddError(
                FString::Printf(TEXT("%s: incomplete render target readback"), stage));
            return;
        }
        for (auto slot = 0; slot < 3; ++slot) {
            auto const column{64 + slot * 64};
            auto matches{true};
            for (auto dy = -1; dy <= 1; ++dy) {
                for (auto dx = -1; dx <= 1; ++dx) {
                    auto const pixel{pixels[(row + dy) * image_size + column + dx]};
                    float const channels[]{pixel.R, pixel.G, pixel.B};
                    for (auto channel = 0; channel < 3; ++channel) {
                        auto const expected{!empty && channel == (slot + shift) % 3};
                        matches &= expected ? channels[channel] > 0.5f
                                            : FMath::Abs(channels[channel]) < 0.1f;
                    }
                }
            }
            auto const centre{pixels[row * image_size + column]};
            TestRunner->TestTrue(
                FString::Printf(TEXT("%s: slot %d centre patch (%d,%d), RGB=(%.3f,%.3f,%.3f)"),
                                stage,
                                slot,
                                column,
                                row,
                                centre.R,
                                centre.G,
                                centre.B),
                matches);
        }
    }

    TEST_METHOD(RendersUpdatedTransformsAndCustomDataAfterGrowthAndProxyRecreation)
    {
        auto const next_frame{[this] { return GFrameCounter > submitted_frame_ + 1; }};
        TestCommandBuilder
            .Do([this] {
                setup();
                submit(3, -96.0f, 0);
                spawner->GetWorld().SendAllEndOfFrameUpdates();
                FlushRenderingCommands();
                if (component_ != nullptr) {
                    TestRunner->TestNotNull(
                        TEXT("The initial scene proxy is created before capture"),
                        component_->GetSceneProxy());
                }
            })
            .Until(next_frame, FTimespan::FromSeconds(10))
            .Do([this] {
                check_image(TEXT("Initial RGB"), 176, 0);
                submit(3, 96.0f, 1);
            })
            .Until(next_frame, FTimespan::FromSeconds(10))
            .Do([this] {
                check_image(TEXT("Updated transforms and colours"), 80, 1);
                check_image(TEXT("Old positions are empty"), 176, 0, true);
                submit(4097, -96.0f, 2);
            })
            .Until(next_frame, FTimespan::FromSeconds(10))
            .Do([this] {
                check_image(TEXT("Grown buffers and distant chunk indices"), 176, 2);
                if (component_ != nullptr) {
                    component_->MarkRenderStateDirty();
                }
                submitted_frame_ = GFrameCounter;
            })
            .Until(next_frame, FTimespan::FromSeconds(10))
            .Do([this] {
                check_image(TEXT("Recreated proxy"), 176, 2);
                submit(3, 96.0f, 0);
            })
            .Until(next_frame, FTimespan::FromSeconds(10))
            .Do([this] {
                check_image(TEXT("Shrunk snapshot"), 80, 0);
                if (component_ != nullptr) {
                    component_->clear_instances();
                }
                submitted_frame_ = GFrameCounter;
            })
            .Until(next_frame, FTimespan::FromSeconds(10))
            .Do([this] { check_image(TEXT("Cleared snapshot"), 80, 0, true); });
    }

    TEST_METHOD(AutomaticBoundsFollowInstancesAcrossTheViewFrustum)
    {
        auto const next_frame{[this] { return GFrameCounter > submitted_frame_ + 1; }};
        TestCommandBuilder
            .Do([this] {
                setup();
                submit(3, 10000.0f, 0);
            })
            .Until(next_frame, FTimespan::FromSeconds(10))
            .Do([this] {
                check_image(TEXT("Outside the frustum"), 128, 0, true);
                submit(3, 0.0f, 0, FQuat4f{FVector3f::ForwardVector, 0.4f});
            })
            .Until(next_frame, FTimespan::FromSeconds(10))
            .Do([this] {
                check_image(TEXT("Rotated instances enter the frustum"), 128, 0);
                if (component_ != nullptr) {
                    TestRunner->TestTrue(TEXT("Registered bounds track the submitted location"),
                                         FMath::Abs(component_->Bounds.Origin.Z) < 1.0);
                }
                submit(3, 10000.0f, 0);
            })
            .Until(next_frame, FTimespan::FromSeconds(10))
            .Do([this] { check_image(TEXT("Instances leave the frustum again"), 128, 0, true); });
    }

    TEST_METHOD(PackedGpuSilhouettesMatchCpuDecodedTransformsAcrossRootChanges)
    {
        TestCommandBuilder.Do([this] {
            setup();
            if (component_ == nullptr || capture_ == nullptr) {
                return;
            }
            auto* reference{NewObject<UInstancedStaticMeshComponent>(component_->GetOwner())};
            component_->GetOwner()->AddInstanceComponent(reference);
            reference->SetMobility(EComponentMobility::Movable);
            reference->SetStaticMesh(component_->get_static_mesh());
            reference->SetMaterial(0, component_->GetMaterial(0));
            reference->SetNumCustomDataFloats(3);
            reference->RegisterComponent();

            // Includes a nonzero far-away root, returning to an earlier root, and proxy recreation.
            for (auto const root : {FVector3f{100000, -80000, 50000},
                                    FVector3f{-70000, 90000, -60000},
                                    FVector3f{100000, -80000, 50000}}) {
                auto const domain{FBox3f{root - FVector3f{512}, root + FVector3f{512}}};
                TArray<FVector3f> const positions{root + FVector3f{-8.0f, -136.1f, -40.0f},
                                                  root + FVector3f{8.0f, 7.9f, 40.1f},
                                                  root + FVector3f{7.9f, 136.0f, -39.9f}};
                TArray<FQuat4f> const rotations{FRotator3f{27, 63, -18}.Quaternion(),
                                                FRotator3f{-80, 172, 91}.Quaternion(),
                                                FRotator3f{178, -34, 43}.Quaternion()};
                TArray<FSandboxISMCRenderInstance> packed;
                packed.SetNumUninitialized(3);
                FSandboxISMCInstanceChunkWriter cpu{
                    packed, {}, 0, 0, domain, root, FVector3f::ZeroVector, FVector3f::ZeroVector};
                reference->ClearInstances();
                cpu.set_transforms<ESandboxISMCBoundsMode::Supplied>(positions, rotations);
                for (int32 index{0}; index < 3; ++index) {
                    auto const& value{packed[index]};
                    auto const q{ml::sandbox_ismc::unpack_quat32(value.rotation)};
                    FVector const location{FVector{root} +
                                           FVector{static_cast<double>(value.position[0]),
                                                   static_cast<double>(value.position[1]),
                                                   static_cast<double>(value.position[2])} *
                                               ml::sandbox_ismc::position_quantum};
                    reference->AddInstance(FTransform{FQuat{q.X, q.Y, q.Z, q.W}, location});
                    for (int32 channel{0}; channel < 3; ++channel) {
                        reference->SetCustomDataValue(
                            index, channel, channel == index ? 1.0f : 0.0f, true);
                    }
                }
                component_->set_instances(
                    3,
                    domain,
                    ESandboxISMCParallelism::Sequential,
                    [&](FSandboxISMCInstanceChunkWriter& chunk) {
                        TArray<FVector3f, TInlineAllocator<1024>> transform_positions;
                        transform_positions.SetNumUninitialized(chunk.num());
                        TArray<FQuat4f, TInlineAllocator<1024>> transform_rotations;
                        transform_rotations.SetNumUninitialized(chunk.num());
                        for (int32 index{0}; index < 3; ++index) {
                            transform_positions[index] = positions[index];
                            transform_rotations[index] = rotations[index];
                            auto data{chunk.custom_data(index)};
                            for (int32 channel{0}; channel < 3; ++channel) {
                                data[channel] = channel == index ? 1.0f : 0.0f;
                            }
                        }
                        chunk.set_transforms<ESandboxISMCBoundsMode::Calculate>(
                            transform_positions, transform_rotations);
                    });
                spawner->GetWorld().SendAllEndOfFrameUpdates();
                FlushRenderingCommands();
                capture_->SetWorldLocation(FVector{root} + FVector{-500, 0, 0});
                auto const capture_pixels{[&](UPrimitiveComponent* primitive) {
                    capture_->ClearShowOnlyComponents();
                    capture_->ShowOnlyComponent(primitive);
                    capture_->CaptureScene();
                    FlushRenderingCommands();
                    TArray<FLinearColor> pixels;
                    TestRunner->TestTrue(
                        TEXT("Silhouette readback succeeds"),
                        target_->GameThread_GetRenderTargetResource()->ReadLinearColorPixels(
                            pixels));
                    return pixels;
                }};
                auto const expected{capture_pixels(reference)};
                auto const verify_pixels{[&] {
                    auto const actual{capture_pixels(component_)};
                    if (!TestRunner->TestEqual(
                            TEXT("Readbacks have matching sizes"), actual.Num(), expected.Num())) {
                        return;
                    }
                    int32 differences{0};
                    int32 coloured{0};
                    auto const pixel_count{expected.Num()};
                    for (int32 index{0}; index < pixel_count; ++index) {
                        auto const a{actual[index]};
                        auto const e{expected[index]};
                        differences += (a.R > 0.5f) != (e.R > 0.5f) ||
                                       (a.G > 0.5f) != (e.G > 0.5f) || (a.B > 0.5f) != (e.B > 0.5f);
                        coloured += e.R > 0.5f || e.G > 0.5f || e.B > 0.5f;
                    }
                    TestRunner->TestTrue(TEXT("Reference geometry is visible"), coloured > 500);
                    TestRunner->TestTrue(
                        FString::Printf(TEXT("CPU/GPU silhouettes agree (edge differences %d)"),
                                        differences),
                        differences <= 12);
                }};
                verify_pixels();
                component_->MarkRenderStateDirty();
                spawner->GetWorld().SendAllEndOfFrameUpdates();
                FlushRenderingCommands();
                verify_pixels();
            }
        });
    }
};
