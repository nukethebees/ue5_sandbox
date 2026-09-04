#include "SandboxISMCComponent.h"

#include "AssetCompilingManager.h"
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

    auto submit(int32 const count, float const height, int32 const colour_shift) -> void {
        if (component_ == nullptr) {
            return;
        }
        int32 const visible_indices[]{0, count == 3 ? 1 : 1024, count == 3 ? 2 : 4096};
        component_->set_instances(
            count, ESandboxISMCParallelism::Auto, [&](FSandboxISMCInstanceChunkWriter& chunk) {
                auto const [offset, chunk_count]{chunk.range()};
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
                    chunk.set_transform(
                        local_index, position, FQuat4f::Identity, FVector3f::OneVector);
                }
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
};
