#include "SandboxISMCBenchmarkActor.h"

#include "Camera/CameraComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/StaticMesh.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Kismet/GameplayStatics.h"
#include "MaterialDomain.h"
#include "Materials/Material.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "ProfilingDebugging/CountersTrace.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "ProfilingDebugging/MiscTrace.h"
#include "ProfilingDebugging/TraceAuxiliary.h"
#include "SandboxISMCComponent.h"
#include "UObject/ConstructorHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SandboxISMCBenchmarkActor)

DEFINE_LOG_CATEGORY_STATIC(LogSandboxISMCBenchmark, Log, All);

namespace {
TRACE_DECLARE_INT_COUNTER(BenchmarkRunning, TEXT("SandboxISMCBenchmark/Running"));
TRACE_DECLARE_INT_COUNTER(BenchmarkInstanceCount, TEXT("SandboxISMCBenchmark/InstanceCount"));
TRACE_DECLARE_FLOAT_COUNTER(BenchmarkFrameMs, TEXT("SandboxISMCBenchmark/FrameMs"));
TRACE_DECLARE_FLOAT_COUNTER(BenchmarkCustomTotalMs,
                            TEXT("SandboxISMCBenchmark/Custom/TotalUpdateMs"));
TRACE_DECLARE_FLOAT_COUNTER(BenchmarkCustomPrepareMs,
                            TEXT("SandboxISMCBenchmark/Custom/PrepareTransformsMs"));
TRACE_DECLARE_FLOAT_COUNTER(BenchmarkCustomApiMs, TEXT("SandboxISMCBenchmark/Custom/CommitApiMs"));
TRACE_DECLARE_FLOAT_COUNTER(BenchmarkCustomPackBoundsMs,
                            TEXT("SandboxISMCBenchmark/Custom/PackAndBoundsMs"));
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
                    int32 const instance_count,
                    TArray<double> const& samples) {
    if (samples.IsEmpty()) {
        return;
    }

    auto const summary{summarize(samples)};
    output += FString::Printf(TEXT("%s,%d,%s,%d,%.6f,%.6f,%.6f,%.6f\n"),
                              renderer,
                              instance_count,
                              metric,
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

    configure_components();
    output_base_name_ = FString::Printf(TEXT("SandboxISMC_Paired_%s"),
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
    TRACE_BOOKMARK(TEXT("SandboxISMC paired continuous benchmark start: %d instances per renderer"),
                   base_positions_.Num());
    UE_LOG(
        LogSandboxISMCBenchmark,
        Display,
        TEXT("Continuous paired benchmark started: custom grid on the left, engine ISMC grid on "
             "the right, %d instances updated by each renderer every frame. Stop PIE to finish."),
        base_positions_.Num());
}

void ASandboxISMCBenchmarkActor::EndPlay(EEndPlayReason::Type const end_play_reason) {
    if (running_) {
        running_ = false;
        TRACE_COUNTER_SET(BenchmarkRunning, 0);
        TRACE_BOOKMARK(TEXT("SandboxISMC paired continuous benchmark stop: %d frames"),
                       frame_ms_.Num());
        if (save_csv_) {
            save_report();
        }
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

    TRACE_CPUPROFILER_EVENT_SCOPE(SandboxISMCBenchmark_PairedFrameUpdates);
    animation_elapsed_seconds_ += delta_seconds;
    auto const vertical_phase{
        FMath::DegreesToRadians(360.0f * movement_frequency_hz_ * animation_elapsed_seconds_)};
    auto const vertical_offset{vertical_movement_amplitude_ * FMath::Sin(vertical_phase)};
    auto const angle{FMath::DegreesToRadians(rotation_speed_degrees_ * animation_elapsed_seconds_)};

    auto const custom_timing{update_custom(vertical_offset, angle)};
    auto const engine_timing{update_engine_ismc(vertical_offset, angle)};
    record_samples(custom_samples_, custom_timing);
    record_samples(engine_samples_, engine_timing);
    frame_ms_.Add(static_cast<double>(delta_seconds) * 1000.0);

    TRACE_COUNTER_SET_ALWAYS(BenchmarkFrameMs, static_cast<double>(delta_seconds) * 1000.0);
    TRACE_COUNTER_SET_ALWAYS(BenchmarkCustomTotalMs, custom_timing.total_ms);
    TRACE_COUNTER_SET_ALWAYS(BenchmarkCustomPrepareMs, custom_timing.prepare_ms);
    TRACE_COUNTER_SET_ALWAYS(BenchmarkCustomApiMs, custom_timing.api_ms);
    TRACE_COUNTER_SET_ALWAYS(BenchmarkCustomPackBoundsMs, custom_timing.pack_bounds_ms);
    TRACE_COUNTER_SET_ALWAYS(BenchmarkEngineTotalMs, engine_timing.total_ms);
    TRACE_COUNTER_SET_ALWAYS(BenchmarkEnginePrepareMs, engine_timing.prepare_ms);
    TRACE_COUNTER_SET_ALWAYS(BenchmarkEngineApiMs, engine_timing.api_ms);
}

void ASandboxISMCBenchmarkActor::configure_components() {
    custom_ismc_->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    custom_ismc_->SetGenerateOverlapEvents(false);
    custom_ismc_->SetCanEverAffectNavigation(false);
    custom_ismc_->CanCharacterStepUpOn = ECB_No;
    custom_ismc_->bVisibleInRayTracing = false;
    custom_ismc_->SetMobility(EComponentMobility::Movable);

    engine_ismc_->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    engine_ismc_->SetGenerateOverlapEvents(false);
    engine_ismc_->SetCanEverAffectNavigation(false);
    engine_ismc_->CanCharacterStepUpOn = ECB_No;
    engine_ismc_->bVisibleInRayTracing = false;
    engine_ismc_->SetMobility(EComponentMobility::Movable);
    engine_ismc_->SetCullDistances(0, 0);
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

    base_positions_.SetNumUninitialized(count);
    engine_update_transforms_.SetNumUninitialized(count);
    for (auto instance_index = 0; instance_index < count; ++instance_index) {
        auto const x{instance_index % width};
        auto const y{instance_index / width};
        auto const position{origin + FVector3f{static_cast<float>(x) * grid_spacing_,
                                               static_cast<float>(y) * grid_spacing_,
                                               0.0f}};
        base_positions_[instance_index] = position;
        engine_update_transforms_[instance_index] =
            FTransform{FQuat::Identity, FVector{position}, FVector::OneVector};
    }

    auto const separation{grid_width + grid_gap_};
    custom_ismc_->SetRelativeLocation(FVector{-separation * 0.5f, 0.0, 0.0});
    engine_ismc_->SetRelativeLocation(FVector{separation * 0.5f, 0.0, 0.0});
    auto const view_extent{FMath::Max(grid_width * 2.0f + grid_gap_, grid_height)};
    auto const camera_location{FVector{0.0, -view_extent * 1.1, view_extent * 0.7}};
    camera_->SetRelativeLocation(camera_location);
    camera_->SetRelativeRotation((FVector::ZeroVector - camera_location).Rotation());

    auto* const material{UMaterial::GetDefaultMaterial(MD_Surface)};
    custom_ismc_->SetMaterial(0, material);
    engine_ismc_->SetMaterial(0, material);

    auto const custom_start{FPlatformTime::Cycles64()};
    {
        TRACE_CPUPROFILER_EVENT_SCOPE(SandboxISMCBenchmark_CustomCreateInstances);
        custom_ismc_->set_static_mesh(static_mesh_);
        custom_ismc_->clear_instances();
        custom_ismc_->reserve_instances(count);
        for (auto const position : base_positions_) {
            custom_ismc_->add_instance(position);
        }
        custom_ismc_->commit_instance_updates();
    }
    custom_creation_ms_ = FPlatformTime::ToMilliseconds64(FPlatformTime::Cycles64() - custom_start);

    auto const engine_start{FPlatformTime::Cycles64()};
    {
        TRACE_CPUPROFILER_EVENT_SCOPE(SandboxISMCBenchmark_EngineISMCCreateInstances);
        engine_ismc_->ClearInstances();
        engine_ismc_->SetStaticMesh(static_mesh_);
        engine_ismc_->AddInstances(engine_update_transforms_, false, false, false);
    }
    engine_creation_ms_ = FPlatformTime::ToMilliseconds64(FPlatformTime::Cycles64() - engine_start);

    custom_ismc_->SetVisibility(true);
    engine_ismc_->SetVisibility(true);
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
    auto const prepare_start{FPlatformTime::Cycles64()};
    {
        TRACE_CPUPROFILER_EVENT_SCOPE(SandboxISMCBenchmark_CustomPrepareTransforms);
        auto positions{custom_ismc_->positions()};
        auto rotations{custom_ismc_->rotations()};
        auto const rotation{FQuat4f{FVector3f::UpVector, angle_radians}};
        auto const count{base_positions_.Num()};
        for (auto instance_index = 0; instance_index < count; ++instance_index) {
            positions[instance_index] =
                base_positions_[instance_index] + FVector3f{0.0f, 0.0f, vertical_offset};
            rotations[instance_index] = rotation;
        }
    }
    auto const prepare_cycles{FPlatformTime::Cycles64() - prepare_start};

    auto const api_start{FPlatformTime::Cycles64()};
    {
        TRACE_CPUPROFILER_EVENT_SCOPE(SandboxISMCBenchmark_CustomCommit);
        custom_ismc_->commit_instance_updates();
    }
    auto const api_cycles{FPlatformTime::Cycles64() - api_start};
    auto const total_cycles{FPlatformTime::Cycles64() - total_start};
    auto const metrics{custom_ismc_->get_update_metrics()};
    return {
        .total_ms = FPlatformTime::ToMilliseconds64(total_cycles),
        .prepare_ms = FPlatformTime::ToMilliseconds64(prepare_cycles),
        .pack_bounds_ms = metrics.prepare_ms,
        .api_ms = FPlatformTime::ToMilliseconds64(api_cycles),
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
        auto const count{base_positions_.Num()};
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
        engine_ismc_->BatchUpdateInstancesTransforms(
            0, engine_update_transforms_, false, true, true);
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
    samples.prepare_ms.Add(timing.prepare_ms);
    if (timing.pack_bounds_ms >= 0.0) {
        samples.pack_bounds_ms.Add(timing.pack_bounds_ms);
    }
    samples.api_ms.Add(timing.api_ms);
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
    FString csv{TEXT("renderer,instances,metric,samples,min_ms,median_ms,p95_ms,max_ms\n")};
    csv += FString::Printf(TEXT("custom,%d,creation,1,%.6f,%.6f,%.6f,%.6f\n"),
                           base_positions_.Num(),
                           custom_creation_ms_,
                           custom_creation_ms_,
                           custom_creation_ms_,
                           custom_creation_ms_);
    csv += FString::Printf(TEXT("engine_ismc,%d,creation,1,%.6f,%.6f,%.6f,%.6f\n"),
                           base_positions_.Num(),
                           engine_creation_ms_,
                           engine_creation_ms_,
                           engine_creation_ms_,
                           engine_creation_ms_);
    append_summary(csv, TEXT("paired"), TEXT("frame"), base_positions_.Num(), frame_ms_);
    append_summary(csv,
                   TEXT("custom"),
                   TEXT("total_update"),
                   base_positions_.Num(),
                   custom_samples_.total_update_ms);
    append_summary(
        csv, TEXT("custom"), TEXT("prepare"), base_positions_.Num(), custom_samples_.prepare_ms);
    append_summary(csv,
                   TEXT("custom"),
                   TEXT("pack_bounds"),
                   base_positions_.Num(),
                   custom_samples_.pack_bounds_ms);
    append_summary(csv, TEXT("custom"), TEXT("api"), base_positions_.Num(), custom_samples_.api_ms);
    append_summary(csv,
                   TEXT("engine_ismc"),
                   TEXT("total_update"),
                   base_positions_.Num(),
                   engine_samples_.total_update_ms);
    append_summary(csv,
                   TEXT("engine_ismc"),
                   TEXT("prepare"),
                   base_positions_.Num(),
                   engine_samples_.prepare_ms);
    append_summary(
        csv, TEXT("engine_ismc"), TEXT("api"), base_positions_.Num(), engine_samples_.api_ms);

    auto const directory{FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Benchmarks"))};
    IFileManager::Get().MakeDirectory(*directory, true);
    auto const path{FPaths::Combine(directory, output_base_name_ + TEXT(".csv"))};
    if (FFileHelper::SaveStringToFile(csv, *path)) {
        UE_LOG(LogSandboxISMCBenchmark, Display, TEXT("Benchmark CSV saved to %s"), *path);
    } else {
        UE_LOG(LogSandboxISMCBenchmark, Error, TEXT("Could not save benchmark CSV to %s"), *path);
    }
}
