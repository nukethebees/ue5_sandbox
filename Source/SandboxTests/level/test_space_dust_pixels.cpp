#include <SandboxShaders/SpaceDust/SpaceDustComponent.h>

#include <AssetCompilingManager.h>
#include <Components/MapTestSpawner.h>
#include <Components/SceneCaptureComponent2D.h>
#include <Engine/TextureRenderTarget2D.h>
#include <Engine/World.h>
#include <GameFramework/Actor.h>
#include <RenderingThread.h>
#include <ShaderCompiler.h>
#include <TextureResource.h>

#include <CQTest.h>

TEST_CLASS(SpaceDustPixels, "Sandbox.SpaceDustRenderTests")
{
    TUniquePtr<FMapTestSpawner> spawner_{nullptr};
    USpaceDustComponent* dust_{nullptr};
    USceneCaptureComponent2D* capture_{nullptr};
    UTextureRenderTarget2D* target_{nullptr};
    static constexpr int32 image_size{256};

    BEFORE_EACH()
    {
        spawner_ = FMapTestSpawner::CreateFromTempLevel(TestCommandBuilder);
        spawner_->AddWaitUntilLoadedCommand(TestRunner);
    }

    AFTER_EACH()
    { spawner_.Reset(); }

    void setup() {
        auto& world{spawner_->GetWorld()};
        auto* const actor{world.SpawnActor<AActor>()};
        dust_ = NewObject<USpaceDustComponent>(actor);
        dust_->bOnlyOwnerSee = false;
        dust_->bHiddenInSceneCapture = false;
        actor->SetRootComponent(dust_);
        actor->AddInstanceComponent(dust_);

        FSpaceDustSettings settings;
        settings.particle_count = 1024;
        settings.volume_dimensions = FVector{2000.0, 2000.0, 2000.0};
        settings.particle_size = 24.0f;
        settings.brightness = 2.0f;
        settings.minimum_visible_speed = 100.0f;
        settings.full_visible_speed = 200.0f;
        settings.maximum_streak_pixels = 32.0f;
        settings.volume_edge_fade_fraction = 0.0f;
        dust_->apply_settings(settings);

        FAssetCompilingManager::Get().FinishAllCompilation();
        if (GShaderCompilingManager != nullptr) {
            GShaderCompilingManager->FinishAllCompilation();
        }
        dust_->RegisterComponent();

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
        capture_->ProjectionType = ECameraProjectionMode::Perspective;
        capture_->FOVAngle = 90.0f;
        capture_->CaptureSource = SCS_SceneColorHDR;
        capture_->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
        capture_->ShowOnlyComponent(dust_);
        capture_->ShowFlags.SetPostProcessing(false);
        capture_->ShowFlags.SetAtmosphere(false);
        capture_->ShowFlags.SetFog(false);
        capture_->ShowFlags.SetAntiAliasing(false);
        capture_->SetWorldLocation(FVector::ZeroVector);
        capture_->SetWorldRotation(FRotator::ZeroRotator);
        capture_->RegisterComponent();

        world.SendAllEndOfFrameUpdates();
        FlushRenderingCommands();
        TestRunner->TestNotNull(TEXT("Space dust scene proxy is created"), dust_->GetSceneProxy());
    }

    auto capture_brightness() const -> float {
        capture_->CaptureScene();
        FlushRenderingCommands();

        TArray<FLinearColor> pixels;
        if (!target_->GameThread_GetRenderTargetResource()->ReadLinearColorPixels(pixels) ||
            pixels.Num() != image_size * image_size) {
            TestRunner->AddError(TEXT("Space dust render target pixels could not be read"));
            return 0.0f;
        }

        auto brightness{0.0f};
        for (FLinearColor const& pixel : pixels) {
            brightness += FMath::Max3(pixel.R, pixel.G, pixel.B);
        }
        return brightness;
    }

    TEST_METHOD(UsesWorldVelocityToGateProceduralDustVisibility)
    {
        TestCommandBuilder
            .Do([this] {
                setup();
                dust_->update_motion(FVector::ZeroVector);
                spawner_->GetWorld().SendAllEndOfFrameUpdates();
            })
            .Do([this] {
                auto const stopped_brightness{capture_brightness()};
                TestRunner->TestTrue(TEXT("Stopped dust is effectively invisible"),
                                     stopped_brightness < 0.01f);

                dust_->update_motion(FVector{0.0, 1000.0, 0.0});
                spawner_->GetWorld().SendAllEndOfFrameUpdates();
                FlushRenderingCommands();

                auto const moving_brightness{capture_brightness()};
                TestRunner->TestTrue(TEXT("Lateral world velocity makes dust visible"),
                                     moving_brightness > 1.0f);
            });
    }
};
