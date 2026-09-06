#include <SpaceGameRendering/SparkRendererComponent.h>

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

TEST_CLASS(SparkRendererPixels, "Sandbox.SparkRenderTests")
{
    TUniquePtr<FMapTestSpawner> spawner{nullptr};
    USparkRendererComponent* renderer_{nullptr};
    USceneCaptureComponent2D* capture_{nullptr};
    UTextureRenderTarget2D* target_{nullptr};
    uint64 submitted_frame_{0};
    float initial_centroid_{0.0f};
    static constexpr int32 image_size{256};

    BEFORE_EACH()
    {
        spawner = FMapTestSpawner::CreateFromTempLevel(TestCommandBuilder);
        spawner->AddWaitUntilLoadedCommand(TestRunner);
    }

    AFTER_EACH()
    { spawner.Reset(); }

    void setup() {
        auto& world{spawner->GetWorld()};
        auto* const actor{world.SpawnActor<AActor>()};
        renderer_ = NewObject<USparkRendererComponent>(actor);
        actor->SetRootComponent(renderer_);
        actor->AddInstanceComponent(renderer_);
        FSparkRendererSettings settings;
        settings.capacity = 1;
        settings.acceleration = FVector3f::ZeroVector;
        settings.minimum_thickness_pixels = 0.0f;
        settings.maximum_thickness_pixels = 32.0f;
        settings.maximum_length_pixels = 128.0f;
        renderer_->initialise(settings);
        FAssetCompilingManager::Get().FinishAllCompilation();
        if (GShaderCompilingManager != nullptr) {
            GShaderCompilingManager->FinishAllCompilation();
        }
        renderer_->RegisterComponent();
        world.SendAllEndOfFrameUpdates();
        FlushRenderingCommands();
        TestRunner->TestNotNull(TEXT("The spark scene proxy is created"),
                                renderer_->GetSceneProxy());

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
        capture_->ShowOnlyComponent(renderer_);
        capture_->ShowFlags.SetPostProcessing(false);
        capture_->ShowFlags.SetAtmosphere(false);
        capture_->ShowFlags.SetFog(false);
        capture_->ShowFlags.SetAntiAliasing(false);
        capture_->SetWorldLocation(FVector{-500.0, 0.0, 0.0});
        capture_->SetWorldRotation(FRotator::ZeroRotator);
        capture_->RegisterComponent();
    }

    void set_time(float const effect_time, bool const spawn) {
        TArray<FSparkParticleRecord> particles;
        if (spawn) {
            FSparkParticleRecord& particle{particles.AddDefaulted_GetRef()};
            particle.initial_position_spawn_time = FVector4f{0.0f, 0.0f, 0.0f, 0.0f};
            particle.initial_velocity_lifetime = FVector4f{0.0f, 0.0f, 1000.0f, 0.2f};
            particle.emissive_colour_size = FVector4f{8.0f, 0.0f, 0.0f, 20.0f};
            particle.streak_time_reserved = FVector4f{0.04f, 0.0f, 0.0f, 0.0f};
        }
        renderer_->submit_particles(particles, effect_time);
        spawner->GetWorld().SendAllEndOfFrameUpdates();
        submitted_frame_ = GFrameCounter;
    }

    auto capture_brightness_centroid() -> TPair<float, float> {
        capture_->CaptureScene();
        FlushRenderingCommands();
        TArray<FLinearColor> pixels;
        if (!target_->GameThread_GetRenderTargetResource()->ReadLinearColorPixels(pixels) ||
            pixels.Num() != image_size * image_size) {
            TestRunner->AddError(TEXT("Spark render target pixels could not be read"));
            return {0.0f, 0.0f};
        }

        auto total_brightness{0.0f};
        auto weighted_row{0.0f};
        for (int32 row{0}; row < image_size; ++row) {
            for (int32 column{0}; column < image_size; ++column) {
                auto const brightness{FMath::Max(pixels[row * image_size + column].R, 0.0f)};
                total_brightness += brightness;
                weighted_row += brightness * static_cast<float>(row);
            }
        }
        return {total_brightness,
                total_brightness > UE_SMALL_NUMBER ? weighted_row / total_brightness : 0.0f};
    }

    TEST_METHOD(ProducesHdrMovesAlongVelocityAndExpires)
    {
        auto const next_frame{[this] { return GFrameCounter > submitted_frame_ + 1; }};
        TestCommandBuilder
            .Do([this] {
                setup();
                set_time(0.0f, true);
            })
            .Until(next_frame, FTimespan::FromSeconds(10))
            .Do([this] {
                auto const [brightness, centroid]{capture_brightness_centroid()};
                TestRunner->TestTrue(TEXT("Initial spark has HDR energy"), brightness > 1.0f);
                initial_centroid_ = centroid;
                renderer_->MarkRenderStateDirty();
                spawner->GetWorld().SendAllEndOfFrameUpdates();
                FlushRenderingCommands();
                TestRunner->TestNotNull(TEXT("The recreated spark scene proxy exists"),
                                        renderer_->GetSceneProxy());
                auto const [restored_brightness, restored_centroid]{capture_brightness_centroid()};
                TestRunner->TestTrue(TEXT("Proxy recreation restores live particle records"),
                                     restored_brightness > 1.0f);
                TestRunner->TestTrue(TEXT("Restored particle remains in the same position"),
                                     FMath::Abs(restored_centroid - initial_centroid_) < 1.0f);
                set_time(0.1f, false);
            })
            .Until(next_frame, FTimespan::FromSeconds(10))
            .Do([this] {
                auto const [brightness, centroid]{capture_brightness_centroid()};
                TestRunner->TestTrue(TEXT("Moving spark remains visible"), brightness > 0.1f);
                TestRunner->TestTrue(
                    *FString::Printf(TEXT("Spark moves upward with positive Z velocity "
                                          "(initial %.2f, moved %.2f)"),
                                     initial_centroid_,
                                     centroid),
                    centroid < initial_centroid_ - 20.0f);
                set_time(0.3f, false);
            })
            .Until(next_frame, FTimespan::FromSeconds(10))
            .Do([this] {
                auto const [brightness, centroid]{capture_brightness_centroid()};
                TestRunner->TestTrue(TEXT("Expired spark disappears"), brightness < 0.01f);
            });
    }
};
