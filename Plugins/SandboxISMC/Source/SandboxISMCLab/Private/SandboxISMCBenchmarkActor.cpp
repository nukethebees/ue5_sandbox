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
#include "Materials/MaterialInterface.h"
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
TRACE_DECLARE_INT_COUNTER(BenchmarkRemovedCount, TEXT("SandboxISMCBenchmark/RemovedCount"));
TRACE_DECLARE_INT_COUNTER(BenchmarkAddedCount, TEXT("SandboxISMCBenchmark/AddedCount"));
TRACE_DECLARE_INT_COUNTER(BenchmarkStagingCapacityChanges,
                          TEXT("SandboxISMCBenchmark/Custom/StagingCapacityChanges"));
TRACE_DECLARE_INT_COUNTER(BenchmarkGpuBufferAllocations,
                          TEXT("SandboxISMCBenchmark/Custom/GpuBufferAllocations"));
TRACE_DECLARE_INT_COUNTER(BenchmarkStagingWaits, TEXT("SandboxISMCBenchmark/Custom/StagingWaits"));
TRACE_DECLARE_FLOAT_COUNTER(BenchmarkStagingWaitMs,
                            TEXT("SandboxISMCBenchmark/Custom/StagingWaitMs"));
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
TRACE_DECLARE_MEMORY_COUNTER(BenchmarkCustomTransformUploadBytes,
                             TEXT("SandboxISMCBenchmark/Custom/TransformUploadBytes"));
TRACE_DECLARE_MEMORY_COUNTER(BenchmarkCustomDataUploadBytes,
                             TEXT("SandboxISMCBenchmark/Custom/CustomDataUploadBytes"));
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
                    TCHAR const* bounds,
                    TCHAR const* custom_data,
                    TCHAR const* workload,
                    int32 const instance_count,
                    int32 const updated_instance_count,
                    double const update_percentage,
                    TArray<double> const& samples) {
    if (samples.IsEmpty()) {
        return;
    }

    auto const summary{summarize(samples)};
    output += FString::Printf(TEXT("%s,%s,%s,%s,%s,%s,%d,%d,%.3f,%s,%s,%d,%.6f,%.6f,%.6f,%.6f\n"),
                              renderer,
                              mode,
                              visibility,
                              bounds,
                              custom_data,
                              workload,
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

    ConstructorHelpers::FObjectFinderOptional<UMaterialInterface> custom_data_material{
        TEXT("/SandboxISMC/Lab/M_SandboxISMCCustomData.M_SandboxISMCCustomData")};
    if (custom_data_material.Succeeded()) {
        custom_data_material_ = custom_data_material.Get();
    }

    configure_components();
}

void ASandboxISMCBenchmarkActor::BeginPlay() {
    TRACE_CPUPROFILER_EVENT_SCOPE(ASandboxISMCBenchmarkActor::BeginPlay);
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
    output_base_name_ = FString::Printf(TEXT("SandboxISMC_%s_%s_bounds_%s_%dpct_%s_%s"),
                                        *get_mode_name(),
                                        *get_bounds_name(),
                                        *get_custom_data_name(),
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
    previous_metrics_ = custom_ismc_->get_update_metrics();
    UE_LOG(LogSandboxISMCBenchmark,
           Display,
           TEXT("Workload: churn=%s, min=%d, max=%d, half-cycle=%d updates, "
                "replacements=%.1f%%/update, warmup=%d updates"),
           churn_enabled_ ? TEXT("on") : TEXT("off"),
           minimum_live_count_,
           instance_count_,
           churn_half_cycle_updates_,
           replacement_percentage_,
           warmup_updates_);
    running_ = true;
    TRACE_COUNTER_SET(BenchmarkRunning, 1);
    TRACE_COUNTER_SET(BenchmarkInstanceCount, base_positions_.Num());
    TRACE_COUNTER_SET(BenchmarkUpdatedInstanceCount, get_update_count());
    TRACE_BOOKMARK(
        TEXT("SandboxISMC benchmark start: mode=%s instances=%d updated=%d visibility=%s "
             "bounds=%s custom_data=%s"),
        *get_mode_name(),
        base_positions_.Num(),
        get_update_count(),
        *get_visibility_name(),
        *get_bounds_name(),
        *get_custom_data_name());
    UE_LOG(LogSandboxISMCBenchmark,
           Display,
           TEXT("Continuous benchmark started: mode=%s, instances=%d, updated=%d (%.1f%%), "
                "visibility=%s, bounds=%s, custom_data=%s, shadows=%s"),
           *get_mode_name(),
           base_positions_.Num(),
           get_update_count(),
           update_percentage_,
           *get_visibility_name(),
           *get_bounds_name(),
           *get_custom_data_name(),
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

    TRACE_CPUPROFILER_EVENT_SCOPE(ASandboxISMCBenchmarkActor::Tick);
    advance_churn();
    if (update_index_ == static_cast<int64>(warmup_updates_) + 1) {
        TRACE_BOOKMARK(TEXT("SandboxISMC benchmark measured updates begin"));
    }
    animation_elapsed_seconds_ += delta_seconds;
    auto const vertical_phase{
        FMath::DegreesToRadians(360.0f * movement_frequency_hz_ * animation_elapsed_seconds_)};
    auto const vertical_offset{vertical_movement_amplitude_ * FMath::Sin(vertical_phase)};
    auto const angle{FMath::DegreesToRadians(rotation_speed_degrees_ * animation_elapsed_seconds_)};
    auto const colour_alpha{0.5f +
                            0.5f * FMath::Sin(animation_elapsed_seconds_ * UE_TWO_PI * 0.25f)};

    FUpdateTiming custom_timing;
    FUpdateTiming engine_timing;
    if (runs_custom()) {
        custom_timing = update_custom(vertical_offset, angle, colour_alpha);
        record_samples(custom_samples_, custom_timing);
    }
    if (runs_engine_ismc()) {
        engine_timing = update_engine_ismc(vertical_offset, angle, colour_alpha);
        record_samples(engine_samples_, engine_timing);
    }
    auto const game_thread_ms{FPlatformTime::ToMilliseconds(GGameThreadTime)};
    auto const render_thread_ms{FPlatformTime::ToMilliseconds(GRenderThreadTime)};
    auto const gpu_ms{FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles())};
    auto const metrics{custom_ismc_->get_update_metrics()};
    auto const wait_ms{metrics.staging_wait_ms - previous_metrics_.staging_wait_ms};
    if (update_index_ > warmup_updates_) {
        frame_ms_.Add(static_cast<double>(delta_seconds) * 1000.0);
        game_thread_ms_.Add(game_thread_ms);
        render_thread_ms_.Add(render_thread_ms);
        if (gpu_ms > 0.0) {
            gpu_ms_.Add(gpu_ms);
        }
        live_counts_.Add(live_count_);
        removed_counts_.Add(previous_live_count_ - retained_count_);
        added_counts_.Add(live_count_ - retained_count_);
        if (runs_custom()) {
            staging_capacity_changes_.Add(static_cast<double>(
                metrics.staging_capacity_changes - previous_metrics_.staging_capacity_changes));
            gpu_buffer_allocations_.Add(static_cast<double>(
                metrics.gpu_buffer_allocations - previous_metrics_.gpu_buffer_allocations));
            staging_waits_.Add(
                static_cast<double>(metrics.staging_waits - previous_metrics_.staging_waits));
            staging_wait_ms_.Add(wait_ms);
        }
    }
    previous_metrics_ = metrics;

    TRACE_COUNTER_SET(BenchmarkInstanceCount, live_count_);
    TRACE_COUNTER_SET(BenchmarkUpdatedInstanceCount, get_update_count());
    TRACE_COUNTER_SET_ALWAYS(BenchmarkRemovedCount, previous_live_count_ - retained_count_);
    TRACE_COUNTER_SET_ALWAYS(BenchmarkAddedCount, live_count_ - retained_count_);
    TRACE_COUNTER_SET(BenchmarkStagingCapacityChanges, metrics.staging_capacity_changes);
    TRACE_COUNTER_SET(BenchmarkGpuBufferAllocations, metrics.gpu_buffer_allocations);
    TRACE_COUNTER_SET(BenchmarkStagingWaits, metrics.staging_waits);
    TRACE_COUNTER_SET_ALWAYS(BenchmarkStagingWaitMs, wait_ms);

    TRACE_COUNTER_SET_ALWAYS(BenchmarkFrameMs, static_cast<double>(delta_seconds) * 1000.0);
    TRACE_COUNTER_SET_ALWAYS(BenchmarkGameThreadMs, game_thread_ms);
    TRACE_COUNTER_SET_ALWAYS(BenchmarkRenderThreadMs, render_thread_ms);
    TRACE_COUNTER_SET_ALWAYS(BenchmarkGpuMs, gpu_ms);
    TRACE_COUNTER_SET_ALWAYS(BenchmarkCustomTotalMs, custom_timing.total_ms);
    TRACE_COUNTER_SET_ALWAYS(BenchmarkCustomBuildMs, custom_timing.build_ms);
    TRACE_COUNTER_SET_ALWAYS(BenchmarkCustomApiMs, custom_timing.api_ms);
    TRACE_COUNTER_SET_ALWAYS(BenchmarkCustomUploadBytes,
                             static_cast<int64>(FMath::Max(custom_timing.uploaded_bytes, 0.0)));
    TRACE_COUNTER_SET_ALWAYS(
        BenchmarkCustomTransformUploadBytes,
        static_cast<int64>(FMath::Max(custom_timing.transform_upload_bytes, 0.0)));
    TRACE_COUNTER_SET_ALWAYS(
        BenchmarkCustomDataUploadBytes,
        static_cast<int64>(FMath::Max(custom_timing.custom_data_upload_bytes, 0.0)));
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
    int32 churn{churn_enabled_ ? 1 : 0};
    FParse::Value(FCommandLine::Get(), TEXT("SandboxISMCBenchmarkChurn="), churn);
    churn_enabled_ = churn != 0;
    FParse::Value(
        FCommandLine::Get(), TEXT("SandboxISMCBenchmarkMinInstances="), minimum_live_count_);
    minimum_live_count_ = FMath::Clamp(minimum_live_count_, 0, instance_count_);
    FParse::Value(FCommandLine::Get(),
                  TEXT("SandboxISMCBenchmarkHalfCycleUpdates="),
                  churn_half_cycle_updates_);
    churn_half_cycle_updates_ = FMath::Max(churn_half_cycle_updates_, 1);
    FParse::Value(FCommandLine::Get(),
                  TEXT("SandboxISMCBenchmarkReplacementPercent="),
                  replacement_percentage_);
    replacement_percentage_ = FMath::Clamp(replacement_percentage_, 0.0f, 100.0f);
    FParse::Value(FCommandLine::Get(), TEXT("SandboxISMCBenchmarkWarmupUpdates="), warmup_updates_);
    warmup_updates_ = FMath::Max(warmup_updates_, 0);
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

    FString custom_data;
    if (FParse::Value(FCommandLine::Get(), TEXT("SandboxISMCBenchmarkCustomData="), custom_data)) {
        if (custom_data.Equals(TEXT("none"), ESearchCase::IgnoreCase)) {
            custom_data_ = ESandboxISMCBenchmarkCustomData::None;
        } else if (custom_data.Equals(TEXT("static"), ESearchCase::IgnoreCase) ||
                   custom_data.Equals(TEXT("static_rgb"), ESearchCase::IgnoreCase)) {
            custom_data_ = ESandboxISMCBenchmarkCustomData::StaticRgb;
        } else if (custom_data.Equals(TEXT("animated"), ESearchCase::IgnoreCase) ||
                   custom_data.Equals(TEXT("animated_rgb"), ESearchCase::IgnoreCase)) {
            custom_data_ = ESandboxISMCBenchmarkCustomData::AnimatedRgb;
        } else {
            UE_LOG(LogSandboxISMCBenchmark,
                   Warning,
                   TEXT("Unknown SandboxISMC custom-data mode '%s'; using none"),
                   *custom_data);
            custom_data_ = ESandboxISMCBenchmarkCustomData::None;
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
    if (uses_custom_data() && custom_data_material_ == nullptr) {
        UE_LOG(LogSandboxISMCBenchmark, Error, TEXT("Custom-data benchmark material is null"));
        return false;
    }

    TRACE_CPUPROFILER_EVENT_SCOPE(ASandboxISMCBenchmarkActor::create_instances);
    auto const count{FMath::Max(instance_count_, 1)};
    live_count_ = count;
    previous_live_count_ = count;
    retained_count_ = count;
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
    base_colours_.SetNumUninitialized(count);
    engine_update_transforms_.SetNumUninitialized(count);
    engine_custom_data_.SetNumUninitialized(uses_custom_data() ? count * 3 : 0);
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
        auto const red{width > 1 ? static_cast<float>(x) / static_cast<float>(width - 1) : 0.5f};
        auto const green{height > 1 ? static_cast<float>(y) / static_cast<float>(height - 1)
                                    : 0.5f};
        auto const colour{FVector3f{red, green, 1.0f - red * 0.75f}};
        base_colours_[instance_index] = colour;
        engine_update_transforms_[instance_index] =
            FTransform{FQuat::Identity, FVector{position}, FVector::OneVector};
        if (uses_custom_data()) {
            auto const custom_data_offset{instance_index * 3};
            engine_custom_data_[custom_data_offset] = colour.X;
            engine_custom_data_[custom_data_offset + 1] = colour.Y;
            engine_custom_data_[custom_data_offset + 2] = colour.Z;
        }
    }

    supplied_local_bounds_ = FBox3f{ForceInit};
    for (auto const& position : base_positions_) {
        supplied_local_bounds_ += position;
    }
    auto const mesh_bounds{static_mesh_->GetBounds()};
    auto const mesh_radius{
        static_cast<float>(mesh_bounds.SphereRadius + mesh_bounds.Origin.Size())};
    supplied_local_bounds_ = supplied_local_bounds_.ExpandBy(FVector3f{
        mesh_radius, mesh_radius, mesh_radius + FMath::Abs(vertical_movement_amplitude_)});

    auto const separation{runs_custom() && runs_engine_ismc() ? grid_width + grid_gap_ : 0.0f};
    custom_ismc_->SetRelativeLocation(FVector{-separation * 0.5f, 0.0, 0.0});
    engine_ismc_->SetRelativeLocation(FVector{separation * 0.5f, 0.0, 0.0});
    auto const camera_location{FVector{0.0, -view_extent * 1.1, view_extent * 0.7}};
    camera_->SetRelativeLocation(camera_location);
    camera_->SetRelativeRotation((FVector::ZeroVector - camera_location).Rotation());

    auto* const material{uses_custom_data() ? custom_data_material_.Get()
                                            : UMaterial::GetDefaultMaterial(MD_Surface)};
    custom_ismc_->SetMaterial(0, material);
    engine_ismc_->SetMaterial(0, material);

    if (runs_custom()) {
        auto const custom_start{FPlatformTime::Cycles64()};
        {
            custom_ismc_->set_num_custom_data_floats(uses_custom_data() ? 3 : 0);
            custom_ismc_->set_static_mesh(*static_mesh_);
            auto const fill{[&](FSandboxISMCInstanceChunkWriter& chunk) {
                auto const [first_index, chunk_count]{chunk.range()};
                for (auto local_index = 0; local_index < chunk_count; ++local_index) {
                    auto const instance_index{first_index + local_index};
                    chunk.set_transform(local_index,
                                        base_positions_[instance_index],
                                        FQuat4f::Identity,
                                        FVector3f::OneVector);
                    if (uses_custom_data()) {
                        auto custom_data{chunk.custom_data(local_index)};
                        custom_data[0] = base_colours_[instance_index].X;
                        custom_data[1] = base_colours_[instance_index].Y;
                        custom_data[2] = base_colours_[instance_index].Z;
                    }
                }
            }};
            if (use_supplied_bounds_) {
                custom_ismc_->set_instances(
                    count, supplied_local_bounds_, ESandboxISMCParallelism::Auto, fill);
            } else {
                custom_ismc_->set_instances(count, ESandboxISMCParallelism::Auto, fill);
            }
        }
        custom_creation_ms_ =
            FPlatformTime::ToMilliseconds64(FPlatformTime::Cycles64() - custom_start);
    }

    if (runs_engine_ismc()) {
        auto const engine_start{FPlatformTime::Cycles64()};
        {
            engine_ismc_->ClearInstances();
            engine_ismc_->SetNumCustomDataFloats(uses_custom_data() ? 3 : 0);
            engine_ismc_->SetStaticMesh(static_mesh_);
            engine_ismc_->AddInstances(engine_update_transforms_, false, false, false);
            if (uses_custom_data()) {
                engine_ismc_->SetCustomData(0, count - 1, engine_custom_data_, true);
            }
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

auto ASandboxISMCBenchmarkActor::advance_churn() -> void {
    ++update_index_;
    previous_live_count_ = live_count_;
    retained_count_ = live_count_;
    if (!churn_enabled_) {
        return;
    }

    TRACE_CPUPROFILER_EVENT_SCOPE(ASandboxISMCBenchmarkActor::advance_churn);
    auto const half_cycle{static_cast<int64>(churn_half_cycle_updates_)};
    auto const phase{update_index_ % (2 * half_cycle)};
    auto const step{phase <= half_cycle ? phase : 2 * half_cycle - phase};
    live_count_ = instance_count_ -
                  static_cast<int32>((instance_count_ - minimum_live_count_) * step / half_cycle);
    auto const survivors{FMath::Min(previous_live_count_, live_count_)};
    replacement_remainder_ += static_cast<double>(survivors) * replacement_percentage_ / 100.0;
    auto const replaced_count{FMath::FloorToInt(replacement_remainder_)};
    replacement_remainder_ -= replaced_count;
    retained_count_ = survivors - replaced_count;

    // Replace a contiguous tail; survivors retain their indices in both renderers.
    for (auto index = retained_count_; index < live_count_; ++index) {
        auto const colour{base_colours_[index]};
        base_colours_[index] = FVector3f{colour.Y, colour.Z, colour.X};
    }
}

auto ASandboxISMCBenchmarkActor::update_custom(float const vertical_offset,
                                               float const angle_radians,
                                               float const colour_alpha) -> FUpdateTiming {
    TRACE_CPUPROFILER_EVENT_SCOPE(ASandboxISMCBenchmarkActor::update_custom);
    auto const total_start{FPlatformTime::Cycles64()};
    auto const api_start{FPlatformTime::Cycles64()};
    {
        auto const count{live_count_};
        auto const updated_count{get_update_count()};
        auto const rotation{FQuat4f{FVector3f::UpVector, angle_radians}};
        auto const fill{[&](FSandboxISMCInstanceChunkWriter& chunk) {
            auto const [first_index, chunk_count]{chunk.range()};
            {
                TRACE_CPUPROFILER_EVENT_SCOPE(
                    ASandboxISMCBenchmarkActor::update_custom::FillTransforms);
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
            }
            if (uses_custom_data()) {
                TRACE_CPUPROFILER_EVENT_SCOPE(
                    ASandboxISMCBenchmarkActor::update_custom::FillCustomData);
                for (auto local_index = 0; local_index < chunk_count; ++local_index) {
                    auto const instance_index{first_index + local_index};
                    auto colour{base_colours_[instance_index]};
                    if (instance_index < updated_count && animates_custom_data()) {
                        auto const rotated_colour{FVector3f{colour.Y, colour.Z, colour.X}};
                        colour = FMath::Lerp(colour, rotated_colour, colour_alpha);
                    }
                    auto custom_data{chunk.custom_data(local_index)};
                    custom_data[0] = colour.X;
                    custom_data[1] = colour.Y;
                    custom_data[2] = colour.Z;
                }
            }
        }};
        if (use_supplied_bounds_) {
            custom_ismc_->set_instances(
                count, supplied_local_bounds_, ESandboxISMCParallelism::Auto, fill);
        } else {
            custom_ismc_->set_instances(count, ESandboxISMCParallelism::Auto, fill);
        }
    }
    auto const api_cycles{FPlatformTime::Cycles64() - api_start};
    auto const total_cycles{FPlatformTime::Cycles64() - total_start};
    auto const metrics{custom_ismc_->get_update_metrics()};
    return {
        .total_ms = FPlatformTime::ToMilliseconds64(total_cycles),
        .build_ms = metrics.build_ms,
        .api_ms = FPlatformTime::ToMilliseconds64(api_cycles),
        .transform_upload_bytes = static_cast<double>(metrics.transform_upload_bytes),
        .custom_data_upload_bytes = static_cast<double>(metrics.custom_data_upload_bytes),
        .uploaded_bytes = static_cast<double>(metrics.upload_bytes),
    };
}

auto ASandboxISMCBenchmarkActor::update_engine_ismc(float const vertical_offset,
                                                    float const angle_radians,
                                                    float const colour_alpha) -> FUpdateTiming {
    TRACE_CPUPROFILER_EVENT_SCOPE(ASandboxISMCBenchmarkActor::update_engine_ismc);
    auto const total_start{FPlatformTime::Cycles64()};
    auto const prepare_start{FPlatformTime::Cycles64()};
    {
        auto const rotation{FQuat{FVector::UpVector, static_cast<double>(angle_radians)}};
        auto const updated_count{get_update_count()};
        auto const count{churn_enabled_ ? live_count_ : updated_count};
        {
            TRACE_CPUPROFILER_EVENT_SCOPE(
                ASandboxISMCBenchmarkActor::update_engine_ismc::PrepareTransforms);
            for (auto instance_index = 0; instance_index < count; ++instance_index) {
                auto const animated{instance_index < updated_count};
                auto const position{base_positions_[instance_index] +
                                    FVector3f{0.0f, 0.0f, animated ? vertical_offset : 0.0f}};
                engine_update_transforms_[instance_index] = FTransform{
                    animated ? rotation : FQuat::Identity, FVector{position}, FVector::OneVector};
            }
        }
        if (animates_custom_data() || (churn_enabled_ && uses_custom_data())) {
            TRACE_CPUPROFILER_EVENT_SCOPE(
                ASandboxISMCBenchmarkActor::update_engine_ismc::PrepareCustomData);
            for (auto instance_index = 0; instance_index < count; ++instance_index) {
                auto const base_colour{base_colours_[instance_index]};
                auto const rotated_colour{FVector3f{base_colour.Y, base_colour.Z, base_colour.X}};
                auto const colour{animates_custom_data() && instance_index < updated_count
                                      ? FMath::Lerp(base_colour, rotated_colour, colour_alpha)
                                      : base_colour};
                auto const custom_data_offset{instance_index * 3};
                engine_custom_data_[custom_data_offset] = colour.X;
                engine_custom_data_[custom_data_offset + 1] = colour.Y;
                engine_custom_data_[custom_data_offset + 2] = colour.Z;
            }
        }
        engine_removals_.Reset();
        engine_additions_.Reset();
        if (churn_enabled_) {
            engine_removals_.Reserve(previous_live_count_ - retained_count_);
            for (auto index = previous_live_count_; index > retained_count_; --index) {
                engine_removals_.Add(index - 1);
            }
            engine_additions_.Append(engine_update_transforms_.GetData() + retained_count_,
                                     live_count_ - retained_count_);
        }
    }
    auto const prepare_cycles{FPlatformTime::Cycles64() - prepare_start};

    auto const api_start{FPlatformTime::Cycles64()};
    {
        if (churn_enabled_) {
            TRACE_CPUPROFILER_EVENT_SCOPE(ASandboxISMCBenchmarkActor::update_engine_ismc::Churn);
            if (!engine_removals_.IsEmpty()) {
                engine_ismc_->RemoveInstances(engine_removals_, true);
            }
            if (!engine_additions_.IsEmpty()) {
                engine_ismc_->AddInstances(engine_additions_, false, false, false);
            }
        }
        auto const update_count{churn_enabled_ ? retained_count_ : get_update_count()};
        if (update_count > 0) {
            {
                TRACE_CPUPROFILER_EVENT_SCOPE(
                    ASandboxISMCBenchmarkActor::update_engine_ismc::SubmitTransforms);
                engine_ismc_->BatchUpdateInstancesTransforms(
                    0,
                    MakeArrayView(engine_update_transforms_).Left(update_count),
                    false,
                    !animates_custom_data(),
                    true);
            }
            if (animates_custom_data()) {
                TRACE_CPUPROFILER_EVENT_SCOPE(
                    ASandboxISMCBenchmarkActor::update_engine_ismc::SubmitCustomData);
                engine_ismc_->SetCustomData(
                    0,
                    update_count - 1,
                    MakeArrayView(engine_custom_data_).Left(update_count * 3),
                    true);
            }
        }
        if (churn_enabled_ && uses_custom_data() && live_count_ > retained_count_) {
            TRACE_CPUPROFILER_EVENT_SCOPE(
                ASandboxISMCBenchmarkActor::update_engine_ismc::NewInstanceCustomData);
            engine_ismc_->SetCustomData(
                retained_count_,
                live_count_ - 1,
                MakeArrayView(engine_custom_data_)
                    .Slice(retained_count_ * 3, (live_count_ - retained_count_) * 3),
                true);
        }
        check(engine_ismc_->GetInstanceCount() == live_count_);
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
    if (update_index_ <= warmup_updates_) {
        return;
    }
    auto& population_samples{live_count_ > previous_live_count_   ? samples.growing_update_ms
                             : live_count_ < previous_live_count_ ? samples.shrinking_update_ms
                                                                  : samples.steady_update_ms};
    population_samples.Add(timing.total_ms);
    samples.total_update_ms.Add(timing.total_ms);
    if (timing.prepare_ms >= 0.0) {
        samples.prepare_ms.Add(timing.prepare_ms);
    }
    if (timing.build_ms >= 0.0) {
        samples.build_ms.Add(timing.build_ms);
    }
    samples.api_ms.Add(timing.api_ms);
    if (timing.transform_upload_bytes >= 0.0) {
        samples.transform_upload_bytes.Add(timing.transform_upload_bytes);
    }
    if (timing.custom_data_upload_bytes >= 0.0) {
        samples.custom_data_upload_bytes.Add(timing.custom_data_upload_bytes);
    }
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
        FMath::CeilToInt(static_cast<double>(live_count_) * update_percentage_ / 100.0),
        0,
        live_count_);
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

FString ASandboxISMCBenchmarkActor::get_bounds_name() const {
    return use_supplied_bounds_ ? TEXT("supplied") : TEXT("calculated");
}

FString ASandboxISMCBenchmarkActor::get_custom_data_name() const {
    switch (custom_data_) {
        case ESandboxISMCBenchmarkCustomData::None:
            return TEXT("no_custom_data");
        case ESandboxISMCBenchmarkCustomData::StaticRgb:
            return TEXT("static_rgb");
        case ESandboxISMCBenchmarkCustomData::AnimatedRgb:
            return TEXT("animated_rgb");
    }
    return TEXT("unknown");
}

bool ASandboxISMCBenchmarkActor::uses_custom_data() const {
    return custom_data_ != ESandboxISMCBenchmarkCustomData::None;
}

bool ASandboxISMCBenchmarkActor::animates_custom_data() const {
    return custom_data_ == ESandboxISMCBenchmarkCustomData::AnimatedRgb;
}

void ASandboxISMCBenchmarkActor::finish_benchmark() {
    TRACE_CPUPROFILER_EVENT_SCOPE(ASandboxISMCBenchmarkActor::finish_benchmark);
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
    TRACE_CPUPROFILER_EVENT_SCOPE(ASandboxISMCBenchmarkActor::save_report);
    if (frame_ms_.IsEmpty()) {
        UE_LOG(LogSandboxISMCBenchmark,
               Warning,
               TEXT("No measured updates: increase the run duration or reduce Warmup Updates"));
    }
    FString csv{TEXT("renderer,mode,visibility,bounds,custom_data,churn,min_instances,half_cycle_"
                     "updates,replacement_percent,warmup_updates,instances,updated_instances,"
                     "update_percent,metric,unit,samples,min,median,p95,max\n")};
    auto const mode_name{get_mode_name()};
    auto const visibility_name{get_visibility_name()};
    auto const bounds_name{get_bounds_name()};
    auto const custom_data_name{get_custom_data_name()};
    auto const instance_count{base_positions_.Num()};
    auto const updated_instance_count{churn_enabled_ ? -1 : get_update_count()};
    auto const workload{FString::Printf(TEXT("%d,%d,%d,%.3f,%d"),
                                        churn_enabled_ ? 1 : 0,
                                        minimum_live_count_,
                                        churn_half_cycle_updates_,
                                        replacement_percentage_,
                                        warmup_updates_)};
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
                       *bounds_name,
                       *custom_data_name,
                       *workload,
                       instance_count,
                       updated_instance_count,
                       update_percentage_,
                       samples);
    }};

    append(TEXT("benchmark"), TEXT("frame"), TEXT("ms"), frame_ms_);
    append(TEXT("benchmark"), TEXT("game_thread"), TEXT("ms"), game_thread_ms_);
    append(TEXT("benchmark"), TEXT("render_thread"), TEXT("ms"), render_thread_ms_);
    append(TEXT("benchmark"), TEXT("gpu"), TEXT("ms"), gpu_ms_);
    append(TEXT("benchmark"), TEXT("live_instances"), TEXT("count"), live_counts_);
    append(TEXT("benchmark"), TEXT("removed_instances"), TEXT("count"), removed_counts_);
    append(TEXT("benchmark"), TEXT("added_instances"), TEXT("count"), added_counts_);
    auto const append_population_timings{
        [&](TCHAR const* renderer, FRendererSamples const& samples) {
            append(renderer, TEXT("growing_update"), TEXT("ms"), samples.growing_update_ms);
            append(renderer, TEXT("shrinking_update"), TEXT("ms"), samples.shrinking_update_ms);
            append(renderer, TEXT("steady_update"), TEXT("ms"), samples.steady_update_ms);
        }};
    if (runs_custom()) {
        append_population_timings(TEXT("custom"), custom_samples_);
        append(TEXT("custom"),
               TEXT("staging_capacity_changes"),
               TEXT("count/update"),
               staging_capacity_changes_);
        append(TEXT("custom"),
               TEXT("gpu_buffer_allocations"),
               TEXT("count/update"),
               gpu_buffer_allocations_);
        append(TEXT("custom"), TEXT("staging_waits"), TEXT("count/update"), staging_waits_);
        append(TEXT("custom"), TEXT("staging_wait"), TEXT("ms"), staging_wait_ms_);
        append(TEXT("custom"), TEXT("creation"), TEXT("ms"), {custom_creation_ms_});
        append(TEXT("custom"), TEXT("total_update"), TEXT("ms"), custom_samples_.total_update_ms);
        append(TEXT("custom"), TEXT("build"), TEXT("ms"), custom_samples_.build_ms);
        append(TEXT("custom"), TEXT("api"), TEXT("ms"), custom_samples_.api_ms);
        append(TEXT("custom"),
               TEXT("transform_uploaded"),
               TEXT("bytes"),
               custom_samples_.transform_upload_bytes);
        append(TEXT("custom"),
               TEXT("custom_data_uploaded"),
               TEXT("bytes"),
               custom_samples_.custom_data_upload_bytes);
        append(TEXT("custom"), TEXT("uploaded"), TEXT("bytes"), custom_samples_.uploaded_bytes);
    }
    if (runs_engine_ismc()) {
        append_population_timings(TEXT("engine_ismc"), engine_samples_);
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
