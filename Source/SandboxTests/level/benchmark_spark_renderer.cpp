#include <SpaceGameRendering/SparkEffects.h>
#include <SpaceGameRendering/SparkRendererComponent.h>

#include <AssetCompilingManager.h>
#include <Components/MapTestSpawner.h>
#include <Components/SceneCaptureComponent2D.h>
#include <CoreGlobals.h>
#include <Engine/TextureRenderTarget2D.h>
#include <Engine/World.h>
#include <GameFramework/Actor.h>
#include <HAL/FileManager.h>
#include <HAL/IConsoleManager.h>
#include <HAL/PlatformTime.h>
#include <Misc/CommandLine.h>
#include <Misc/DateTime.h>
#include <Misc/FileHelper.h>
#include <Misc/Parse.h>
#include <Misc/Paths.h>
#include <ProfilingDebugging/MiscTrace.h>
#include <ProfilingDebugging/TraceAuxiliary.h>
#include <RenderingThread.h>
#include <RHI.h>
#include <ShaderCompiler.h>

#include <CQTest.h>

namespace SparkBenchmark {
struct FSummary {
    double minimum{0.0};
    double median{0.0};
    double percentile_95{0.0};
    double maximum{0.0};
};

auto summarize(TArray<double> samples) -> FSummary {
    check(!samples.IsEmpty());
    samples.Sort();
    auto const percentile{[&samples](double const fraction) {
        auto const index{
            FMath::Clamp(FMath::CeilToInt(fraction * samples.Num()) - 1, 0, samples.Num() - 1)};
        return samples[index];
    }};
    return {
        .minimum = samples[0],
        .median = percentile(0.5),
        .percentile_95 = percentile(0.95),
        .maximum = samples.Last(),
    };
}

void append_summary(FString& csv,
                    TCHAR const* const metric,
                    TCHAR const* const unit,
                    int32 const capacity,
                    int32 const sparks_per_hit,
                    int32 const impacts_per_frame,
                    int32 const admitted_particles_per_frame,
                    int32 const render_width,
                    int32 const render_height,
                    TArray<double> const& samples) {
    if (samples.IsEmpty()) {
        return;
    }
    auto const summary{summarize(samples)};
    csv += FString::Printf(TEXT("%d,%d,%d,%d,%d,%d,%s,%s,%d,%.6f,%.6f,%.6f,%.6f\n"),
                           capacity,
                           sparks_per_hit,
                           impacts_per_frame,
                           admitted_particles_per_frame,
                           render_width,
                           render_height,
                           metric,
                           unit,
                           samples.Num(),
                           summary.minimum,
                           summary.median,
                           summary.percentile_95,
                           summary.maximum);
}
} // namespace SparkBenchmark

TEST_CLASS(SparkRendererBenchmark, "Sandbox.SparkBenchmark")
{
    TUniquePtr<FMapTestSpawner> spawner_{nullptr};
    USparkRendererComponent* renderer_{nullptr};
    USceneCaptureComponent2D* capture_{nullptr};
    UTextureRenderTarget2D* target_{nullptr};
    TUniquePtr<FSparkEffects> effects_;
    TArray<double> frame_ms_;
    TArray<double> game_thread_ms_;
    TArray<double> render_thread_ms_;
    TArray<double> gpu_ms_;
    TArray<double> submit_ms_;
    int32 capacity_{50000};
    int32 sparks_per_hit_{96};
    int32 impacts_per_frame_{100};
    int32 warmup_frames_{60};
    int32 frame_index_{0};
    float duration_seconds_{10.0f};
    double measurement_start_seconds_{0.0};
    double previous_frame_seconds_{0.0};
    FString output_base_name_;
    bool owns_insights_trace_{false};
    bool frame_rate_limits_disabled_{false};
    int32 previous_vsync_{0};
    int32 previous_editor_vsync_{0};
    float previous_max_fps_{0.0f};

    static constexpr int32 render_width{1920};
    static constexpr int32 render_height{1080};

    BEFORE_EACH()
    {
        spawner_ = FMapTestSpawner::CreateFromTempLevel(TestCommandBuilder);
        spawner_->AddWaitUntilLoadedCommand(TestRunner);
    }

    AFTER_EACH()
    {
        stop_insights_trace();
        restore_frame_rate_limits();
        effects_.Reset();
        spawner_.Reset();
    }

    void parse_command_line() {
        FParse::Value(
            FCommandLine::Get(), TEXT("SandboxSparkBenchmarkSeconds="), duration_seconds_);
        FParse::Value(FCommandLine::Get(), TEXT("SandboxSparkBenchmarkCapacity="), capacity_);
        FParse::Value(
            FCommandLine::Get(), TEXT("SandboxSparkBenchmarkSparksPerHit="), sparks_per_hit_);
        FParse::Value(
            FCommandLine::Get(), TEXT("SandboxSparkBenchmarkImpactsPerFrame="), impacts_per_frame_);
        FParse::Value(
            FCommandLine::Get(), TEXT("SandboxSparkBenchmarkWarmupFrames="), warmup_frames_);
        duration_seconds_ = FMath::Max(duration_seconds_, 0.1f);
        capacity_ = FMath::Clamp(capacity_, 1, 1000000);
        sparks_per_hit_ = FMath::Clamp(sparks_per_hit_, 1, capacity_);
        impacts_per_frame_ = FMath::Clamp(impacts_per_frame_, 0, capacity_);
        warmup_frames_ = FMath::Max(warmup_frames_, 0);
    }

    void setup() {
        parse_command_line();
        auto& world{spawner_->GetWorld()};
        auto* const actor{world.SpawnActor<AActor>()};
        renderer_ = NewObject<USparkRendererComponent>(actor);
        actor->SetRootComponent(renderer_);
        actor->AddInstanceComponent(renderer_);
        FSparkRendererSettings settings;
        settings.capacity = capacity_;
        settings.maximum_draw_distance = 1000000.0f;
        settings.acceleration = FVector3f::ZeroVector;
        renderer_->initialise(settings);
        prefill_particle_pool();

        FAssetCompilingManager::Get().FinishAllCompilation();
        if (GShaderCompilingManager != nullptr) {
            GShaderCompilingManager->FinishAllCompilation();
        }
        renderer_->RegisterComponent();
        effects_ = MakeUnique<FSparkEffects>(*renderer_);

        target_ = NewObject<UTextureRenderTarget2D>(actor);
        target_->RenderTargetFormat = RTF_RGBA16f;
        target_->ClearColor = FLinearColor::Black;
        target_->InitAutoFormat(render_width, render_height);
        target_->UpdateResourceImmediate(true);

        capture_ = NewObject<USceneCaptureComponent2D>(actor);
        actor->AddInstanceComponent(capture_);
        capture_->TextureTarget = target_;
        capture_->bCaptureEveryFrame = false;
        capture_->bCaptureOnMovement = false;
        capture_->ProjectionType = ECameraProjectionMode::Orthographic;
        capture_->OrthoWidth = 20000.0f;
        capture_->CaptureSource = SCS_SceneColorHDR;
        capture_->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
        capture_->ShowOnlyComponent(renderer_);
        capture_->ShowFlags.SetPostProcessing(false);
        capture_->ShowFlags.SetAtmosphere(false);
        capture_->ShowFlags.SetFog(false);
        capture_->ShowFlags.SetAntiAliasing(false);
        capture_->SetWorldLocation(FVector{-20000.0, 0.0, 0.0});
        capture_->SetWorldRotation(FRotator::ZeroRotator);
        capture_->RegisterComponent();

        world.SendAllEndOfFrameUpdates();
        FlushRenderingCommands();
        TestRunner->TestNotNull(TEXT("The spark benchmark scene proxy is created"),
                                renderer_->GetSceneProxy());

        output_base_name_ = FString::Printf(TEXT("Spark_%d_%d_%d_%s"),
                                            capacity_,
                                            sparks_per_hit_,
                                            impacts_per_frame_,
                                            *FDateTime::Now().ToString(TEXT("%Y-%m-%d_%H-%M-%S")));
        disable_frame_rate_limits();
        start_insights_trace();
        TRACE_BOOKMARK(TEXT("Spark benchmark start: capacity=%d sparks_per_hit=%d "
                            "impacts_per_frame=%d warmup_frames=%d"),
                       capacity_,
                       sparks_per_hit_,
                       impacts_per_frame_,
                       warmup_frames_);
        if (warmup_frames_ == 0) {
            measurement_start_seconds_ = FPlatformTime::Seconds();
            TRACE_BOOKMARK(TEXT("Spark benchmark measured frames begin"));
        }
    }

    void prefill_particle_pool() {
        TArray<FSparkParticleRecord> particles;
        particles.SetNumUninitialized(capacity_);
        auto const columns{FMath::Max(
            FMath::CeilToInt(FMath::Sqrt(static_cast<float>(capacity_) * 16.0f / 9.0f)), 1)};
        auto const rows{FMath::Max(FMath::DivideAndRoundUp(capacity_, columns), 1)};
        for (int32 index{0}; index < capacity_; ++index) {
            auto const column{index % columns};
            auto const row{index / columns};
            auto const column_alpha{columns > 1 ? static_cast<float>(column) / (columns - 1)
                                                : 0.5f};
            auto const row_alpha{rows > 1 ? static_cast<float>(row) / (rows - 1) : 0.5f};
            auto const velocity{(index & 1) == 0 ? FVector3f{0.0f, 4000.0f, 1000.0f}
                                                 : FVector3f{0.0f, -1000.0f, 4000.0f}};
            auto& particle{particles[index]};
            particle.initial_position_spawn_time =
                FVector4f{0.0f,
                          FMath::Lerp(-9500.0f, 9500.0f, column_alpha),
                          FMath::Lerp(-5000.0f, 5000.0f, row_alpha),
                          0.0f};
            particle.initial_velocity_lifetime =
                FVector4f{velocity.X, velocity.Y, velocity.Z, 1000.0f};
            particle.emissive_colour_size = (index & 1) == 0 ? FVector4f{40.0f, 4.0f, 0.8f, 20.0f}
                                                             : FVector4f{2.0f, 8.0f, 40.0f, 20.0f};
            particle.streak_time_reserved = FVector4f{0.025f, 0.0f, 0.0f, 0.0f};
        }
        renderer_->submit_particles(particles, 0.0f);
    }

    auto make_burst(int32 const impact_index) const -> FSparkBurst {
        auto const grid_index{frame_index_ * impacts_per_frame_ + impact_index};
        auto const column{grid_index % 20};
        auto const row{(grid_index / 20) % 10};
        return {
            .location = {0.0f,
                         -14250.0f + static_cast<float>(column) * 1500.0f,
                         -6750.0f + static_cast<float>(row) * 1500.0f},
            .direction = FVector3f::UpVector,
            .colour = (grid_index & 1) == 0 ? FLinearColor{1.0f, 0.1f, 0.02f}
                                            : FLinearColor{0.05f, 0.2f, 1.0f},
            .speed = {2000.0f, 6000.0f},
            .lifetime = {0.6f, 1.0f},
            .size = {20.0f, 50.0f},
            .intensity = 40.0f,
            .spread_angle_degrees = 90.0f,
            .streak_time = 0.025f,
            .count = sparks_per_hit_,
            .seed = 0x5a17b00bu ^ static_cast<uint32>(grid_index),
        };
    }

    void submit_frame() {
        auto const start_cycles{FPlatformTime::Cycles64()};
        if (impacts_per_frame_ > 0) {
            for (int32 impact_index{0}; impact_index < impacts_per_frame_; ++impact_index) {
                effects_->queue_burst(make_burst(impact_index));
            }
            constexpr float fixed_dt{1.0f / 60.0f};
            effects_->commit(fixed_dt);
        } else {
            renderer_->submit_particles({}, 0.0f);
        }
        auto const submit_ms{
            FPlatformTime::ToMilliseconds64(FPlatformTime::Cycles64() - start_cycles)};
        spawner_->GetWorld().SendAllEndOfFrameUpdates();
        capture_->CaptureScene();

        auto const now{FPlatformTime::Seconds()};
        if (frame_index_ >= warmup_frames_) {
            if (previous_frame_seconds_ > 0.0) {
                frame_ms_.Add((now - previous_frame_seconds_) * 1000.0);
            }
            game_thread_ms_.Add(FPlatformTime::ToMilliseconds(GGameThreadTime));
            render_thread_ms_.Add(FPlatformTime::ToMilliseconds(GRenderThreadTime));
            auto const gpu_ms{FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles())};
            if (gpu_ms > 0.0) {
                gpu_ms_.Add(gpu_ms);
            }
            submit_ms_.Add(submit_ms);
        }
        previous_frame_seconds_ = now;
        ++frame_index_;
        if (frame_index_ == warmup_frames_) {
            measurement_start_seconds_ = now;
            previous_frame_seconds_ = 0.0;
            TRACE_BOOKMARK(TEXT("Spark benchmark measured frames begin"));
        }
    }

    auto benchmark_finished() -> bool {
        submit_frame();
        if (frame_index_ <= warmup_frames_ || measurement_start_seconds_ <= 0.0) {
            return false;
        }
        return FPlatformTime::Seconds() - measurement_start_seconds_ >= duration_seconds_;
    }

    void start_insights_trace() {
#if UE_TRACE_ENABLED
        if (FTraceAuxiliary::IsConnected()) {
            return;
        }
        FTraceAuxiliary::FOptions options;
        options.bExcludeTail = true;
        constexpr auto* channels{
            TEXT("cpu,gpu,frame,bookmark,counters,stats,rendercommands,rhicommands")};
        owns_insights_trace_ = FTraceAuxiliary::Start(
            FTraceAuxiliary::EConnectionType::File, *output_base_name_, channels, &options);
#endif
    }

    void stop_insights_trace() {
#if UE_TRACE_ENABLED
        if (owns_insights_trace_) {
            FTraceAuxiliary::Stop();
            owns_insights_trace_ = false;
        }
#endif
    }

    void disable_frame_rate_limits() {
        auto* const vsync{IConsoleManager::Get().FindConsoleVariable(TEXT("r.VSync"))};
        auto* const editor_vsync{IConsoleManager::Get().FindConsoleVariable(TEXT("r.VSyncEditor"))};
        auto* const max_fps{IConsoleManager::Get().FindConsoleVariable(TEXT("t.MaxFPS"))};
        if (vsync == nullptr || editor_vsync == nullptr || max_fps == nullptr) {
            return;
        }
        previous_vsync_ = vsync->GetInt();
        previous_editor_vsync_ = editor_vsync->GetInt();
        previous_max_fps_ = max_fps->GetFloat();
        vsync->Set(0, ECVF_SetByCode);
        editor_vsync->Set(0, ECVF_SetByCode);
        max_fps->Set(0.0f, ECVF_SetByCode);
        frame_rate_limits_disabled_ = true;
    }

    void restore_frame_rate_limits() {
        if (!frame_rate_limits_disabled_) {
            return;
        }
        if (auto* const vsync{IConsoleManager::Get().FindConsoleVariable(TEXT("r.VSync"))}) {
            vsync->Set(previous_vsync_, ECVF_SetByCode);
        }
        if (auto* const editor_vsync{
                IConsoleManager::Get().FindConsoleVariable(TEXT("r.VSyncEditor"))}) {
            editor_vsync->Set(previous_editor_vsync_, ECVF_SetByCode);
        }
        if (auto* const max_fps{IConsoleManager::Get().FindConsoleVariable(TEXT("t.MaxFPS"))}) {
            max_fps->Set(previous_max_fps_, ECVF_SetByCode);
        }
        frame_rate_limits_disabled_ = false;
    }

    void save_report() {
        FlushRenderingCommands();
        TRACE_BOOKMARK(TEXT("Spark benchmark stop: measured_frames=%d"), submit_ms_.Num());
        FString csv{TEXT("capacity,sparks_per_hit,impacts_per_frame,admitted_particles_per_frame,"
                         "render_width,render_height,metric,unit,samples,min,median,p95,max\n")};
        auto const requested_particles{static_cast<int64>(sparks_per_hit_) * impacts_per_frame_};
        auto const admitted_particles{
            static_cast<int32>(FMath::Min<int64>(capacity_, requested_particles))};
        auto const append{
            [&](TCHAR const* const metric, TCHAR const* const unit, TArray<double> const& samples) {
                SparkBenchmark::append_summary(csv,
                                               metric,
                                               unit,
                                               capacity_,
                                               sparks_per_hit_,
                                               impacts_per_frame_,
                                               admitted_particles,
                                               render_width,
                                               render_height,
                                               samples);
            }};
        append(TEXT("frame"), TEXT("ms"), frame_ms_);
        append(TEXT("game_thread"), TEXT("ms"), game_thread_ms_);
        append(TEXT("render_thread"), TEXT("ms"), render_thread_ms_);
        append(TEXT("gpu"), TEXT("ms"), gpu_ms_);
        append(TEXT("queue_expand_submit"), TEXT("ms"), submit_ms_);
        append(TEXT("uploaded"),
               TEXT("bytes/frame"),
               {static_cast<double>(admitted_particles) * sizeof(FSparkParticleRecord)});

        auto const directory{
            FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Benchmarks"), TEXT("Sparks"))};
        IFileManager::Get().MakeDirectory(*directory, true);
        auto const path{FPaths::Combine(directory, output_base_name_ + TEXT(".csv"))};
        if (FFileHelper::SaveStringToFile(csv, *path)) {
            TestRunner->AddInfo(FString::Printf(TEXT("Spark benchmark CSV saved to %s"), *path));
        } else {
            TestRunner->AddError(
                FString::Printf(TEXT("Could not save spark benchmark CSV to %s"), *path));
        }
        stop_insights_trace();
        TestRunner->TestTrue(TEXT("The benchmark recorded measured frames"), !submit_ms_.IsEmpty());
    }

    TEST_METHOD(SustainedImpacts)
    {
        TestCommandBuilder.Do([this] { setup(); })
            .Until([this] { return benchmark_finished(); }, FTimespan::FromSeconds(1200))
            .Then([this] { save_report(); });
    }
};
