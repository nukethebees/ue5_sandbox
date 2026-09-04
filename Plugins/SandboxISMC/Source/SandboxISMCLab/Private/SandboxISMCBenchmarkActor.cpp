#include "SandboxISMCBenchmarkActor.h"

#include "Camera/CameraComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Editor/UnrealEdEngine.h"
#include "Engine/StaticMesh.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Kismet/GameplayStatics.h"
#include "MaterialDomain.h"
#include "Materials/Material.h"
#include "Misc/CommandLine.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "ProfilingDebugging/CountersTrace.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "ProfilingDebugging/MiscTrace.h"
#include "ProfilingDebugging/TraceAuxiliary.h"
#include "RenderingThread.h"
#include "RenderTimer.h"
#include "RHI.h"
#include "SandboxISMCComponent.h"
#include "UnrealEdGlobals.h"
#include "UObject/ConstructorHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SandboxISMCBenchmarkActor)

DEFINE_LOG_CATEGORY_STATIC(LogSandboxISMCBenchmark, Log, All);

namespace {
TRACE_DECLARE_INT_COUNTER(BenchmarkRunning, TEXT("SandboxISMCBenchmark/Running"));
TRACE_DECLARE_INT_COUNTER(BenchmarkInstanceCount, TEXT("SandboxISMCBenchmark/InstanceCount"));
TRACE_DECLARE_INT_COUNTER(BenchmarkUpdatedInstanceCount,
                          TEXT("SandboxISMCBenchmark/UpdatedInstanceCount"));
TRACE_DECLARE_FLOAT_COUNTER(BenchmarkFrameMs, TEXT("SandboxISMCBenchmark/FrameMs"));
TRACE_DECLARE_FLOAT_COUNTER(BenchmarkGameThreadMs, TEXT("SandboxISMCBenchmark/GameThreadMs"));
TRACE_DECLARE_FLOAT_COUNTER(BenchmarkRenderThreadMs, TEXT("SandboxISMCBenchmark/RenderThreadMs"));
TRACE_DECLARE_FLOAT_COUNTER(BenchmarkGpuMs, TEXT("SandboxISMCBenchmark/GpuMs"));
TRACE_DECLARE_FLOAT_COUNTER(BenchmarkCustomTotalMs,
                            TEXT("SandboxISMCBenchmark/Custom/TotalUpdateMs"));
TRACE_DECLARE_FLOAT_COUNTER(BenchmarkCustomBuildMs,
                            TEXT("SandboxISMCBenchmark/Custom/BuildSnapshotMs"));
TRACE_DECLARE_FLOAT_COUNTER(BenchmarkCustomApiMs,
                            TEXT("SandboxISMCBenchmark/Custom/SetInstancesApiMs"));
TRACE_DECLARE_MEMORY_COUNTER(BenchmarkCustomUploadBytes,
                             TEXT("SandboxISMCBenchmark/Custom/UploadBytes"));
TRACE_DECLARE_FLOAT_COUNTER(BenchmarkEngineTotalMs,
                            TEXT("SandboxISMCBenchmark/EngineISMC/TotalUpdateMs"));
TRACE_DECLARE_FLOAT_COUNTER(BenchmarkEnginePrepareMs,
                            TEXT("SandboxISMCBenchmark/EngineISMC/PrepareTransformsMs"));
TRACE_DECLARE_FLOAT_COUNTER(BenchmarkEngineApiMs,
                            TEXT("SandboxISMCBenchmark/EngineISMC/BatchUpdateApiMs"));
TRACE_DECLARE_FLOAT_COUNTER(BenchmarkCustomCreationMs,
                            TEXT("SandboxISMCBenchmark/Custom/CreationMs"));
TRACE_DECLARE_FLOAT_COUNTER(BenchmarkEngineCreationMs,
                            TEXT("SandboxISMCBenchmark/EngineISMC/CreationMs"));

struct FSummary {
    double minimum_ms{0.0};
    double median_ms{0.0};
    double percentile_95_ms{0.0};
    double maximum_ms{0.0};
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
        .minimum_ms = samples[0],
        .median_ms = percentile(0.5),
        .percentile_95_ms = percentile(0.95),
        .maximum_ms = samples.Last(),
    };
}

void append_summary(FString& output,
                    TCHAR const* renderer,
                    TCHAR const* metric,
                    TCHAR const* unit,
                    TCHAR const* mode,
                    TCHAR const* visibility,
                    int32 const instance_count,
                    int32 const updated_instance_count,
                    double const update_percentage,
                    TArray<double> const& samples) {
    if (samples.IsEmpty()) {
        return;
    }

    auto const summary{summarize(samples)};
    output += FString::Printf(TEXT("%s,%s,%s,%d,%d,%.3f,%s,%s,%d,%.6f,%.6f,%.6f,%.6f\n"),
                              renderer,
                              mode,
                              visibility,
                              instance_count,
                              updated_instance_count,
                              update_percentage,
                              metric,
                              unit,
                              samples.Num(),
                              summary.minimum_ms,
                              summary.median_ms,
                              summary.percentile_95_ms,
                              summary.maximum_ms);
}
} // namespace

ASandboxISMCBenchmarkActor::ASandboxISMCBenchmarkActor() {
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;

    root_ = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(root_);

    camera_ = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
    camera_->SetupAttachment(root_);

    custom_ismc_ = CreateDefaultSubobject<USandboxISMCComponent>(TEXT("CustomISMC"));
    custom_ismc_->SetupAttachment(root_);

    engine_ismc_ = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("EngineISMC"));
    engine_ismc_->SetupAttachment(root_);

    ConstructorHelpers::FObjectFinder<UStaticMesh> const cube{
        TEXT("/Engine/BasicShapes/Cube.Cube")};
    if (cube.Succeeded()) {
        static_mesh_ = cube.Object;
    }

    configure_components();
}

void ASandboxISMCBenchmarkActor::BeginPlay() {
    Super::BeginPlay();

    if (auto* const player_controller{UGameplayStatics::GetPlayerController(this, 0)}) {
        player_controller->SetViewTarget(this);
    } else {
        UE_LOG(LogSandboxISMCBenchmark,
               Warning,
               TEXT("No player controller is available for the fixed benchmark camera"));
    }

    parse_command_line();
    configure_components();
    request_end_pie_on_completion_ =
        FParse::Param(FCommandLine::Get(), TEXT("SandboxISMCBenchmarkEndPIE"));
    output_base_name_ = FString::Printf(TEXT("SandboxISMC_%s_%dpct_%s_%s"),
                                        *get_mode_name(),
                                        FMath::RoundToInt(update_percentage_),
                                        *get_visibility_name(),
                                        *FDateTime::Now().ToString(TEXT("%Y-%m-%d_%H-%M-%S")));
    start_insights_trace();
    if (!create_instances()) {
        stop_insights_trace();
        SetActorTickEnabled(false);
        return;
    }

    disable_frame_rate_limits();
    running_ = true;
    TRACE_COUNTER_SET(BenchmarkRunning, 1);
    TRACE_COUNTER_SET(BenchmarkInstanceCount, base_positions_.Num());
    TRACE_COUNTER_SET(BenchmarkUpdatedInstanceCount, get_update_count());
    TRACE_BOOKMARK(
        TEXT("SandboxISMC benchmark start: mode=%s instances=%d updated=%d visibility=%s"),
        *get_mode_name(),
        base_positions_.Num(),
        get_update_count(),
        *get_visibility_name());
    UE_LOG(LogSandboxISMCBenchmark,
           Display,
           TEXT("Continuous benchmark started: mode=%s, instances=%d, updated=%d (%.1f%%), "
                "visibility=%s, shadows=%s"),
           *get_mode_name(),
           base_positions_.Num(),
           get_update_count(),
           update_percentage_,
           *get_visibility_name(),
           cast_shadows_ ? TEXT("on") : TEXT("off"));
}

void ASandboxISMCBenchmarkActor::EndPlay(EEndPlayReason::Type const end_play_reason) {
    if (running_) {
        finish_benchmark();
    }

    stop_insights_trace();
    restore_frame_rate_limits();
    Super::EndPlay(end_play_reason);
}

void ASandboxISMCBenchmarkActor::Tick(float const delta_seconds) {
    Super::Tick(delta_seconds);

    if (!running_) {
        return;
    }

    TRACE_CPUPROFILER_EVENT_SCOPE(SandboxISMCBenchmark_FrameUpdates);
    animation_elapsed_seconds_ += delta_seconds;
    auto const vertical_phase{
        FMath::DegreesToRadians(360.0f * movement_frequency_hz_ * animation_elapsed_seconds_)};
    auto const vertical_offset{vertical_movement_amplitude_ * FMath::Sin(vertical_phase)};
    auto const angle{FMath::DegreesToRadians(rotation_speed_degrees_ * animation_elapsed_seconds_)};

    FUpdateTiming custom_timing;
    FUpdateTiming engine_timing;
    if (runs_custom()) {
        custom_timing = update_custom(vertical_offset, angle);
        record_samples(custom_samples_, custom_timing);
    }
    if (runs_engine_ismc()) {
        engine_timing = update_engine_ismc(vertical_offset, angle);
        record_samples(engine_samples_, engine_timing);
    }
    frame_ms_.Add(static_cast<double>(delta_seconds) * 1000.0);
    auto const game_thread_ms{FPlatformTime::ToMilliseconds(GGameThreadTime)};
    auto const render_thread_ms{FPlatformTime::ToMilliseconds(GRenderThreadTime)};
    auto const gpu_ms{FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles())};
    game_thread_ms_.Add(game_thread_ms);
    render_thread_ms_.Add(render_thread_ms);
    if (gpu_ms > 0.0) {
        gpu_ms_.Add(gpu_ms);
    }

    TRACE_COUNTER_SET_ALWAYS(BenchmarkFrameMs, static_cast<double>(delta_seconds) * 1000.0);
    TRACE_COUNTER_SET_ALWAYS(BenchmarkGameThreadMs, game_thread_ms);
    TRACE_COUNTER_SET_ALWAYS(BenchmarkRenderThreadMs, render_thread_ms);
    TRACE_COUNTER_SET_ALWAYS(BenchmarkGpuMs, gpu_ms);
    TRACE_COUNTER_SET_ALWAYS(BenchmarkCustomTotalMs, custom_timing.total_ms);
    TRACE_COUNTER_SET_ALWAYS(BenchmarkCustomBuildMs, custom_timing.build_ms);
    TRACE_COUNTER_SET_ALWAYS(BenchmarkCustomApiMs, custom_timing.api_ms);
    TRACE_COUNTER_SET_ALWAYS(BenchmarkCustomUploadBytes,
                             static_cast<int64>(FMath::Max(custom_timing.uploaded_bytes, 0.0)));
    TRACE_COUNTER_SET_ALWAYS(BenchmarkEngineTotalMs, engine_timing.total_ms);
    TRACE_COUNTER_SET_ALWAYS(BenchmarkEnginePrepareMs, engine_timing.prepare_ms);
    TRACE_COUNTER_SET_ALWAYS(BenchmarkEngineApiMs, engine_timing.api_ms);

    if (automatic_stop_seconds_ > 0.0f && animation_elapsed_seconds_ >= automatic_stop_seconds_) {
        finish_benchmark();
        if (request_end_pie_on_completion_ && GUnrealEd != nullptr) {
            GUnrealEd->RequestEndPlayMap();
        }
    }
}

void ASandboxISMCBenchmarkActor::parse_command_line() {
    FParse::Value(
        FCommandLine::Get(), TEXT("SandboxISMCBenchmarkSeconds="), automatic_stop_seconds_);
    automatic_stop_seconds_ = FMath::Max(automatic_stop_seconds_, 0.0f);
    FParse::Value(FCommandLine::Get(), TEXT("SandboxISMCBenchmarkInstances="), instance_count_);
    instance_count_ = FMath::Max(instance_count_, 1);
    FParse::Value(
        FCommandLine::Get(), TEXT("SandboxISMCBenchmarkUpdatePercent="), update_percentage_);
    update_percentage_ = FMath::Clamp(update_percentage_, 0.0f, 100.0f);

    int32 shadows{cast_shadows_ ? 1 : 0};
    FParse::Value(FCommandLine::Get(), TEXT("SandboxISMCBenchmarkShadows="), shadows);
    cast_shadows_ = shadows != 0;

    FString mode;
    if (FParse::Value(FCommandLine::Get(), TEXT("SandboxISMCBenchmarkMode="), mode)) {
        if (mode.Equals(TEXT("paired"), ESearchCase::IgnoreCase)) {
            mode_ = ESandboxISMCBenchmarkMode::Paired;
        } else if (mode.Equals(TEXT("custom"), ESearchCase::IgnoreCase)) {
            mode_ = ESandboxISMCBenchmarkMode::CustomOnly;
        } else if (mode.Equals(TEXT("engine"), ESearchCase::IgnoreCase) ||
                   mode.Equals(TEXT("engine_ismc"), ESearchCase::IgnoreCase)) {
            mode_ = ESandboxISMCBenchmarkMode::EngineISMCOnly;
        } else {
            UE_LOG(LogSandboxISMCBenchmark,
                   Warning,
                   TEXT("Unknown SandboxISMC benchmark mode '%s'; using paired"),
                   *mode);
            mode_ = ESandboxISMCBenchmarkMode::Paired;
        }
    }

    FString visibility;
    if (FParse::Value(FCommandLine::Get(), TEXT("SandboxISMCBenchmarkVisibility="), visibility)) {
        if (visibility.Equals(TEXT("all"), ESearchCase::IgnoreCase)) {
            visibility_ = ESandboxISMCBenchmarkVisibility::All;
        } else if (visibility.Equals(TEXT("half"), ESearchCase::IgnoreCase)) {
            visibility_ = ESandboxISMCBenchmarkVisibility::Half;
        } else if (visibility.Equals(TEXT("none"), ESearchCase::IgnoreCase)) {
            visibility_ = ESandboxISMCBenchmarkVisibility::None;
        } else {
            UE_LOG(LogSandboxISMCBenchmark,
                   Warning,
                   TEXT("Unknown SandboxISMC benchmark visibility '%s'; using all"),
                   *visibility);
            visibility_ = ESandboxISMCBenchmarkVisibility::All;
        }
    }
}

void ASandboxISMCBenchmarkActor::configure_components() {
    custom_ismc_->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    custom_ismc_->SetGenerateOverlapEvents(false);
    custom_ismc_->SetCanEverAffectNavigation(false);
    custom_ismc_->CanCharacterStepUpOn = ECB_No;
    custom_ismc_->bVisibleInRayTracing = false;
    custom_ismc_->SetMobility(EComponentMobility::Movable);
    custom_ismc_->SetCastShadow(cast_shadows_);

    engine_ismc_->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    engine_ismc_->SetGenerateOverlapEvents(false);
    engine_ismc_->SetCanEverAffectNavigation(false);
    engine_ismc_->CanCharacterStepUpOn = ECB_No;
    engine_ismc_->bVisibleInRayTracing = false;
    engine_ismc_->SetMobility(EComponentMobility::Movable);
    engine_ismc_->SetCullDistances(0, 0);
    engine_ismc_->SetCastShadow(cast_shadows_);
}

bool ASandboxISMCBenchmarkActor::create_instances() {
    if (static_mesh_ == nullptr) {
        UE_LOG(LogSandboxISMCBenchmark, Error, TEXT("Benchmark static mesh is null"));
        return false;
    }

    TRACE_CPUPROFILER_EVENT_SCOPE(SandboxISMCBenchmark_CreateMatchingInstances);
    auto const count{FMath::Max(instance_count_, 1)};
    auto const width{FMath::CeilToInt(FMath::Sqrt(static_cast<float>(count)))};
    auto const height{FMath::DivideAndRoundUp(count, width)};
    auto const grid_width{static_cast<float>(width - 1) * grid_spacing_};
    auto const grid_height{static_cast<float>(height - 1) * grid_spacing_};
    auto const origin{FVector3f{-grid_width * 0.5f, -grid_height * 0.5f, 0.0f}};
    auto const paired_width{runs_custom() && runs_engine_ismc() ? grid_width * 2.0f + grid_gap_
                                                                : grid_width};
    auto const view_extent{FMath::Max(paired_width, grid_height)};
    auto const visible_count{visibility_ == ESandboxISMCBenchmarkVisibility::All ? count
                             : visibility_ == ESandboxISMCBenchmarkVisibility::Half
                                 ? FMath::DivideAndRoundUp(count, 2)
                                 : 0};
    auto const hidden_offset{FVector3f{0.0f, -view_extent * 4.0f, 0.0f}};

    base_positions_.SetNumUninitialized(count);
    engine_update_transforms_.SetNumUninitialized(count);
    for (auto instance_index = 0; instance_index < count; ++instance_index) {
        auto const x{instance_index % width};
        auto const y{instance_index / width};
        auto position{origin + FVector3f{static_cast<float>(x) * grid_spacing_,
                                         static_cast<float>(y) * grid_spacing_,
                                         0.0f}};
        if (instance_index >= visible_count) {
            position += hidden_offset;
        }
        base_positions_[instance_index] = position;
        engine_update_transforms_[instance_index] =
            FTransform{FQuat::Identity, FVector{position}, FVector::OneVector};
    }

    auto const separation{runs_custom() && runs_engine_ismc() ? grid_width + grid_gap_ : 0.0f};
    custom_ismc_->SetRelativeLocation(FVector{-separation * 0.5f, 0.0, 0.0});
    engine_ismc_->SetRelativeLocation(FVector{separation * 0.5f, 0.0, 0.0});
    auto const camera_location{FVector{0.0, -view_extent * 1.1, view_extent * 0.7}};
    camera_->SetRelativeLocation(camera_location);
    camera_->SetRelativeRotation((FVector::ZeroVector - camera_location).Rotation());

    auto* const material{UMaterial::GetDefaultMaterial(MD_Surface)};
    custom_ismc_->SetMaterial(0, material);
    engine_ismc_->SetMaterial(0, material);

    if (runs_custom()) {
        auto const custom_start{FPlatformTime::Cycles64()};
        {
            TRACE_CPUPROFILER_EVENT_SCOPE(SandboxISMCBenchmark_CustomCreateInstances);
            custom_ismc_->set_static_mesh(*static_mesh_);
            custom_ismc_->set_instances(
                count, ESandboxISMCParallelism::Auto, [&](FSandboxISMCInstanceChunkWriter& chunk) {
                    auto const chunk_count{chunk.num()};
                    auto const first_index{chunk.first_index()};
                    for (auto local_index = 0; local_index < chunk_count; ++local_index) {
                        auto const instance_index{first_index + local_index};
                        chunk.set_transform(local_index,
                                            base_positions_[instance_index],
                                            FQuat4f::Identity,
                                            FVector3f::OneVector);
                    }
                });
        }
        custom_creation_ms_ =
            FPlatformTime::ToMilliseconds64(FPlatformTime::Cycles64() - custom_start);
    }

    if (runs_engine_ismc()) {
        auto const engine_start{FPlatformTime::Cycles64()};
        {
            TRACE_CPUPROFILER_EVENT_SCOPE(SandboxISMCBenchmark_EngineISMCCreateInstances);
            engine_ismc_->ClearInstances();
            engine_ismc_->SetStaticMesh(static_mesh_);
            engine_ismc_->AddInstances(engine_update_transforms_, false, false, false);
        }
        engine_creation_ms_ =
            FPlatformTime::ToMilliseconds64(FPlatformTime::Cycles64() - engine_start);
    }

    custom_ismc_->SetVisibility(runs_custom());
    engine_ismc_->SetVisibility(runs_engine_ismc());
    TRACE_COUNTER_SET(BenchmarkCustomCreationMs, custom_creation_ms_);
    TRACE_COUNTER_SET(BenchmarkEngineCreationMs, engine_creation_ms_);
    UE_LOG(LogSandboxISMCBenchmark,
           Display,
           TEXT("Creation: custom %.3f ms, engine ISMC %.3f ms"),
           custom_creation_ms_,
           engine_creation_ms_);
    return true;
}

auto ASandboxISMCBenchmarkActor::update_custom(float const vertical_offset,
                                               float const angle_radians) -> FUpdateTiming {
    TRACE_CPUPROFILER_EVENT_SCOPE(SandboxISMCBenchmark_CustomUpdate);
    auto const total_start{FPlatformTime::Cycles64()};
    auto const api_start{FPlatformTime::Cycles64()};
    {
        TRACE_CPUPROFILER_EVENT_SCOPE(SandboxISMCBenchmark_CustomSetInstances);
        auto const count{base_positions_.Num()};
        auto const updated_count{get_update_count()};
        auto const rotation{FQuat4f{FVector3f::UpVector, angle_radians}};
        custom_ismc_->set_instances(
            count, ESandboxISMCParallelism::Auto, [&](FSandboxISMCInstanceChunkWriter& chunk) {
                auto const chunk_count{chunk.num()};
                auto const first_index{chunk.first_index()};
                for (auto local_index = 0; local_index < chunk_count; ++local_index) {
                    auto const instance_index{first_index + local_index};
                    auto const animated{instance_index < updated_count};
                    auto const position{base_positions_[instance_index] +
                                        FVector3f{0.0f, 0.0f, animated ? vertical_offset : 0.0f}};
                    chunk.set_transform(local_index,
                                        position,
                                        animated ? rotation : FQuat4f::Identity,
                                        FVector3f::OneVector);
                }
            });
    }
    auto const api_cycles{FPlatformTime::Cycles64() - api_start};
    auto const total_cycles{FPlatformTime::Cycles64() - total_start};
    auto const metrics{custom_ismc_->get_update_metrics()};
    return {
        .total_ms = FPlatformTime::ToMilliseconds64(total_cycles),
        .build_ms = metrics.build_ms,
        .api_ms = FPlatformTime::ToMilliseconds64(api_cycles),
        .uploaded_bytes = static_cast<double>(metrics.upload_bytes),
    };
}

auto ASandboxISMCBenchmarkActor::update_engine_ismc(float const vertical_offset,
                                                    float const angle_radians) -> FUpdateTiming {
    TRACE_CPUPROFILER_EVENT_SCOPE(SandboxISMCBenchmark_EngineISMCUpdate);
    auto const total_start{FPlatformTime::Cycles64()};
    auto const prepare_start{FPlatformTime::Cycles64()};
    {
        TRACE_CPUPROFILER_EVENT_SCOPE(SandboxISMCBenchmark_EngineISMCPrepareTransforms);
        auto const rotation{FQuat{FVector::UpVector, static_cast<double>(angle_radians)}};
        auto const count{get_update_count()};
        for (auto instance_index = 0; instance_index < count; ++instance_index) {
            auto const position{base_positions_[instance_index] +
                                FVector3f{0.0f, 0.0f, vertical_offset}};
            engine_update_transforms_[instance_index] =
                FTransform{rotation, FVector{position}, FVector::OneVector};
        }
    }
    auto const prepare_cycles{FPlatformTime::Cycles64() - prepare_start};

    auto const api_start{FPlatformTime::Cycles64()};
    {
        TRACE_CPUPROFILER_EVENT_SCOPE(SandboxISMCBenchmark_EngineISMCBatchUpdate);
        auto const update_count{get_update_count()};
        if (update_count > 0) {
            engine_ismc_->BatchUpdateInstancesTransforms(
                0, MakeArrayView(engine_update_transforms_).Left(update_count), false, true, true);
        }
    }
    auto const api_cycles{FPlatformTime::Cycles64() - api_start};
    auto const total_cycles{FPlatformTime::Cycles64() - total_start};
    return {
        .total_ms = FPlatformTime::ToMilliseconds64(total_cycles),
        .prepare_ms = FPlatformTime::ToMilliseconds64(prepare_cycles),
        .api_ms = FPlatformTime::ToMilliseconds64(api_cycles),
    };
}

void ASandboxISMCBenchmarkActor::record_samples(FRendererSamples& samples,
                                                FUpdateTiming const& timing) {
    samples.total_update_ms.Add(timing.total_ms);
    if (timing.prepare_ms >= 0.0) {
        samples.prepare_ms.Add(timing.prepare_ms);
    }
    if (timing.build_ms >= 0.0) {
        samples.build_ms.Add(timing.build_ms);
    }
    samples.api_ms.Add(timing.api_ms);
    if (timing.uploaded_bytes >= 0.0) {
        samples.uploaded_bytes.Add(timing.uploaded_bytes);
    }
}

bool ASandboxISMCBenchmarkActor::runs_custom() const {
    return mode_ != ESandboxISMCBenchmarkMode::EngineISMCOnly;
}

bool ASandboxISMCBenchmarkActor::runs_engine_ismc() const {
    return mode_ != ESandboxISMCBenchmarkMode::CustomOnly;
}

int32 ASandboxISMCBenchmarkActor::get_update_count() const {
    return FMath::Clamp(
        FMath::CeilToInt(static_cast<double>(base_positions_.Num()) * update_percentage_ / 100.0),
        0,
        base_positions_.Num());
}

FString ASandboxISMCBenchmarkActor::get_mode_name() const {
    switch (mode_) {
        case ESandboxISMCBenchmarkMode::Paired:
            return TEXT("paired");
        case ESandboxISMCBenchmarkMode::CustomOnly:
            return TEXT("custom");
        case ESandboxISMCBenchmarkMode::EngineISMCOnly:
            return TEXT("engine_ismc");
    }
    return TEXT("unknown");
}

FString ASandboxISMCBenchmarkActor::get_visibility_name() const {
    switch (visibility_) {
        case ESandboxISMCBenchmarkVisibility::All:
            return TEXT("all_visible");
        case ESandboxISMCBenchmarkVisibility::Half:
            return TEXT("half_visible");
        case ESandboxISMCBenchmarkVisibility::None:
            return TEXT("none_visible");
    }
    return TEXT("unknown");
}

void ASandboxISMCBenchmarkActor::finish_benchmark() {
    if (!running_) {
        return;
    }

    running_ = false;
    TRACE_COUNTER_SET(BenchmarkRunning, 0);
    TRACE_BOOKMARK(TEXT("SandboxISMC continuous benchmark stop: %d frames"), frame_ms_.Num());
    FlushRenderingCommands();
    if (save_csv_) {
        save_report();
    }
    stop_insights_trace();
    restore_frame_rate_limits();

    UE_LOG(LogSandboxISMCBenchmark,
           Display,
           TEXT("Continuous benchmark complete after %.2f seconds and %d frames"),
           animation_elapsed_seconds_,
           frame_ms_.Num());
}

void ASandboxISMCBenchmarkActor::start_insights_trace() {
#if UE_TRACE_ENABLED
    if (!capture_insights_trace_) {
        return;
    }
    if (FTraceAuxiliary::IsConnected()) {
        UE_LOG(LogSandboxISMCBenchmark,
               Display,
               TEXT("Using the Unreal Insights trace already connected to this process"));
        return;
    }

    FTraceAuxiliary::FOptions options;
    options.bExcludeTail = true;
    constexpr auto* channels{
        TEXT("cpu,gpu,frame,bookmark,counters,stats,rendercommands,rhicommands")};
    owns_insights_trace_ = FTraceAuxiliary::Start(
        FTraceAuxiliary::EConnectionType::File, *output_base_name_, channels, &options);
    if (owns_insights_trace_) {
        UE_LOG(LogSandboxISMCBenchmark,
               Display,
               TEXT("Unreal Insights trace started: %s.utrace"),
               *output_base_name_);
    } else {
        UE_LOG(LogSandboxISMCBenchmark, Warning, TEXT("Could not start Unreal Insights trace"));
    }
#endif
}

void ASandboxISMCBenchmarkActor::stop_insights_trace() {
#if UE_TRACE_ENABLED
    if (owns_insights_trace_) {
        FTraceAuxiliary::Stop();
        owns_insights_trace_ = false;
        UE_LOG(LogSandboxISMCBenchmark,
               Display,
               TEXT("Unreal Insights trace saved under Saved/Profiling"));
    }
#endif
}

void ASandboxISMCBenchmarkActor::disable_frame_rate_limits() {
    if (!disable_frame_rate_limits_) {
        return;
    }

    auto* const vsync{IConsoleManager::Get().FindConsoleVariable(TEXT("r.VSync"))};
    auto* const editor_vsync{IConsoleManager::Get().FindConsoleVariable(TEXT("r.VSyncEditor"))};
    auto* const max_fps{IConsoleManager::Get().FindConsoleVariable(TEXT("t.MaxFPS"))};
    if (vsync == nullptr || editor_vsync == nullptr || max_fps == nullptr) {
        UE_LOG(
            LogSandboxISMCBenchmark,
            Warning,
            TEXT("Could not find r.VSync, r.VSyncEditor, or t.MaxFPS; frame-rate limits were not "
                 "changed"));
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

void ASandboxISMCBenchmarkActor::restore_frame_rate_limits() {
    if (!frame_rate_limits_disabled_) {
        return;
    }

    auto* const vsync{IConsoleManager::Get().FindConsoleVariable(TEXT("r.VSync"))};
    auto* const editor_vsync{IConsoleManager::Get().FindConsoleVariable(TEXT("r.VSyncEditor"))};
    auto* const max_fps{IConsoleManager::Get().FindConsoleVariable(TEXT("t.MaxFPS"))};
    if (vsync != nullptr) {
        vsync->Set(previous_vsync_, ECVF_SetByCode);
    }
    if (editor_vsync != nullptr) {
        editor_vsync->Set(previous_editor_vsync_, ECVF_SetByCode);
    }
    if (max_fps != nullptr) {
        max_fps->Set(previous_max_fps_, ECVF_SetByCode);
    }
    frame_rate_limits_disabled_ = false;
}

void ASandboxISMCBenchmarkActor::save_report() const {
    FString csv{TEXT("renderer,mode,visibility,instances,updated_instances,update_percent,metric,"
                     "unit,samples,min,median,p95,max\n")};
    auto const mode_name{get_mode_name()};
    auto const visibility_name{get_visibility_name()};
    auto const instance_count{base_positions_.Num()};
    auto const updated_instance_count{get_update_count()};
    auto const append{[&](TCHAR const* renderer,
                          TCHAR const* metric,
                          TCHAR const* unit,
                          TArray<double> const& samples) {
        append_summary(csv,
                       renderer,
                       metric,
                       unit,
                       *mode_name,
                       *visibility_name,
                       instance_count,
                       updated_instance_count,
                       update_percentage_,
                       samples);
    }};

    append(TEXT("benchmark"), TEXT("frame"), TEXT("ms"), frame_ms_);
    append(TEXT("benchmark"), TEXT("game_thread"), TEXT("ms"), game_thread_ms_);
    append(TEXT("benchmark"), TEXT("render_thread"), TEXT("ms"), render_thread_ms_);
    append(TEXT("benchmark"), TEXT("gpu"), TEXT("ms"), gpu_ms_);
    if (runs_custom()) {
        append(TEXT("custom"), TEXT("creation"), TEXT("ms"), {custom_creation_ms_});
        append(TEXT("custom"), TEXT("total_update"), TEXT("ms"), custom_samples_.total_update_ms);
        append(TEXT("custom"), TEXT("build"), TEXT("ms"), custom_samples_.build_ms);
        append(TEXT("custom"), TEXT("api"), TEXT("ms"), custom_samples_.api_ms);
        append(TEXT("custom"), TEXT("uploaded"), TEXT("bytes"), custom_samples_.uploaded_bytes);
    }
    if (runs_engine_ismc()) {
        append(TEXT("engine_ismc"), TEXT("creation"), TEXT("ms"), {engine_creation_ms_});
        append(
            TEXT("engine_ismc"), TEXT("total_update"), TEXT("ms"), engine_samples_.total_update_ms);
        append(TEXT("engine_ismc"), TEXT("prepare"), TEXT("ms"), engine_samples_.prepare_ms);
        append(TEXT("engine_ismc"), TEXT("api"), TEXT("ms"), engine_samples_.api_ms);
    }

    auto const directory{FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Benchmarks"))};
    IFileManager::Get().MakeDirectory(*directory, true);
    auto const path{FPaths::Combine(directory, output_base_name_ + TEXT(".csv"))};
    if (FFileHelper::SaveStringToFile(csv, *path)) {
        UE_LOG(LogSandboxISMCBenchmark, Display, TEXT("Benchmark CSV saved to %s"), *path);
    } else {
        UE_LOG(LogSandboxISMCBenchmark, Error, TEXT("Could not save benchmark CSV to %s"), *path);
    }
}
