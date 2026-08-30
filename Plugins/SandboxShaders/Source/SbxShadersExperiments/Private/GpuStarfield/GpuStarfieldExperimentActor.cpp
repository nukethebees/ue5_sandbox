#include "SbxShadersExperiments/GpuStarfield/GpuStarfieldExperimentActor.h"

#include "Async/Async.h"
#include "Camera/CameraActor.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "RenderingThread.h"

DEFINE_LOG_CATEGORY_STATIC(LogGpuStarfieldBenchmark, Log, All);

namespace {
constexpr int32 default_warmup_frames{60};
constexpr int32 default_capture_frames{180};
constexpr int32 default_repeat_count{3};
constexpr double benchmark_camera_distance{1000000000.0};
constexpr double benchmark_camera_lateral_distance{50000000.0};

auto parse_star_counts() -> TArray<int32> {
    FString values;
    if (!FParse::Value(FCommandLine::Get(), TEXT("GpuStarfieldBenchmarkCounts="), values, false)) {
        return {10000, 100000, 1000000};
    }

    values.TrimQuotesInline();
    TArray<FString> entries;
    values.ParseIntoArray(entries, TEXT(","), true);

    TArray<int32> result;
    for (auto const& entry : entries) {
        auto const star_count{FCString::Atoi(*entry)};
        if (star_count > 0 && star_count <= 1000000) {
            result.AddUnique(star_count);
        }
    }
    return result;
}

auto parse_camera_motion_modes() -> TArray<bool> {
    FString values;
    if (!FParse::Value(
            FCommandLine::Get(), TEXT("GpuStarfieldBenchmarkCameraModes="), values, false)) {
        return {false, true};
    }

    values.TrimQuotesInline();
    TArray<FString> entries;
    values.ParseIntoArray(entries, TEXT(","), true);

    TArray<bool> result;
    for (auto entry : entries) {
        entry.TrimStartAndEndInline();
        if (entry.Equals(TEXT("stationary"), ESearchCase::IgnoreCase)) {
            result.AddUnique(false);
        } else if (entry.Equals(TEXT("moving"), ESearchCase::IgnoreCase)) {
            result.AddUnique(true);
        }
    }
    return result;
}
}

AGpuStarfieldExperimentActor::AGpuStarfieldExperimentActor() {
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;

    starfield_component_ = CreateDefaultSubobject<UGpuStarfieldComponent>(TEXT("GpuStarfield"));
    SetRootComponent(starfield_component_);
}

void AGpuStarfieldExperimentActor::BeginPlay() {
    Super::BeginPlay();

    if (FParse::Param(FCommandLine::Get(), TEXT("GpuStarfieldBenchmark"))) {
        start_benchmark();
    }
}

void AGpuStarfieldExperimentActor::EndPlay(EEndPlayReason::Type const end_play_reason) {
    if (csv_finished_delegate_.IsValid()) {
        FCsvProfiler::Get()->OnCSVProfileFinished().Remove(csv_finished_delegate_);
        csv_finished_delegate_.Reset();
    }

    Super::EndPlay(end_play_reason);
}

void AGpuStarfieldExperimentActor::Tick(float const delta_seconds) {
    Super::Tick(delta_seconds);

    if (!benchmark_active_) {
        return;
    }

    update_benchmark_camera();

    if (benchmark_capture_pending_) {
        return;
    }

    if (benchmark_frames_remaining_ > 0) {
        --benchmark_frames_remaining_;
        return;
    }

    begin_csv_capture();
}

void AGpuStarfieldExperimentActor::OnConstruction(FTransform const& transform) {
    Super::OnConstruction(transform);
    apply_settings();
}

void AGpuStarfieldExperimentActor::PostRegisterAllComponents() {
    Super::PostRegisterAllComponents();
    apply_settings();
}

void AGpuStarfieldExperimentActor::apply_settings() {
    if (IsValid(starfield_component_)) {
        starfield_component_->apply_settings(settings);
    }
}

void AGpuStarfieldExperimentActor::start_benchmark() {
    auto const star_counts{parse_star_counts()};
    auto const camera_motion_modes{parse_camera_motion_modes()};
    if (star_counts.IsEmpty() || camera_motion_modes.IsEmpty()) {
        UE_LOG(LogGpuStarfieldBenchmark,
               Error,
               TEXT("GPU starfield benchmark counts or camera modes were invalid."));
        FPlatformMisc::RequestExitWithStatus(true, 1);
        return;
    }

    auto star_size_multiplier{settings.star_size_multiplier};
    FParse::Value(FCommandLine::Get(),
                  TEXT("GpuStarfieldBenchmarkStarSizeMultiplier="),
                  star_size_multiplier);
    settings.star_size_multiplier = FMath::Max(star_size_multiplier, 0.0f);

    benchmark_warmup_frames_ = default_warmup_frames;
    benchmark_capture_frames_ = default_capture_frames;
    auto repeat_count{default_repeat_count};
    FParse::Value(
        FCommandLine::Get(), TEXT("GpuStarfieldBenchmarkWarmupFrames="), benchmark_warmup_frames_);
    FParse::Value(FCommandLine::Get(),
                  TEXT("GpuStarfieldBenchmarkCaptureFrames="),
                  benchmark_capture_frames_);
    FParse::Value(FCommandLine::Get(), TEXT("GpuStarfieldBenchmarkRepeats="), repeat_count);
    benchmark_warmup_frames_ = FMath::Max(benchmark_warmup_frames_, 0);
    benchmark_capture_frames_ = FMath::Max(benchmark_capture_frames_, 30);
    repeat_count = FMath::Clamp(repeat_count, 1, 10);

    if (!FParse::Value(FCommandLine::Get(),
                       TEXT("GpuStarfieldBenchmarkOutput="),
                       benchmark_output_directory_)) {
        benchmark_output_directory_ =
            FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Benchmarks/GpuStarfield/Raw"));
    } else if (FPaths::IsRelative(benchmark_output_directory_)) {
        benchmark_output_directory_ =
            FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), benchmark_output_directory_);
    }
    IFileManager::Get().MakeDirectory(*benchmark_output_directory_, true);

    for (auto const star_count : star_counts) {
        for (int32 repeat_index{0}; repeat_index < repeat_count; ++repeat_index) {
            for (auto const camera_motion : camera_motion_modes) {
                auto const enabled_first{(repeat_index + static_cast<int32>(camera_motion)) % 2 ==
                                         1};
                benchmark_phases_.Add({star_count, repeat_index, enabled_first, camera_motion});
                benchmark_phases_.Add({star_count, repeat_index, !enabled_first, camera_motion});
            }
        }
    }

    auto* const world{GetWorld()};
    if (world == nullptr) {
        UE_LOG(LogGpuStarfieldBenchmark, Error, TEXT("Benchmark world is unavailable."));
        FPlatformMisc::RequestExitWithStatus(true, 1);
        return;
    }

    for (TActorIterator<ACameraActor> camera_iterator{world}; camera_iterator; ++camera_iterator) {
        auto* const camera{*camera_iterator};
        if (camera->GetLevel() == GetLevel()) {
            benchmark_camera_ = camera;
            benchmark_camera_origin_ = camera->GetActorLocation();
            break;
        }
    }

    auto* const player_controller{world->GetFirstPlayerController()};
    if (!benchmark_camera_.IsValid() || player_controller == nullptr) {
        UE_LOG(LogGpuStarfieldBenchmark,
               Error,
               TEXT("Benchmark requires a showcase camera and player controller."));
        FPlatformMisc::RequestExitWithStatus(true, 1);
        return;
    }
    player_controller->SetViewTarget(benchmark_camera_.Get());

    for (TActorIterator<AActor> actor_iterator{world}; actor_iterator; ++actor_iterator) {
        auto* const actor{*actor_iterator};
        if (actor == this || actor->GetLevel() != GetLevel() || actor->IsA<ACameraActor>()) {
            continue;
        }
        actor->SetActorHiddenInGame(true);
        actor->SetActorTickEnabled(false);
    }

    benchmark_active_ = true;
    SetActorTickEnabled(true);
    begin_benchmark_phase();
}

void AGpuStarfieldExperimentActor::begin_benchmark_phase() {
    auto const& phase{benchmark_phases_[benchmark_phase_index_]};
    settings.star_count = phase.star_count;
    apply_settings();
    starfield_component_->SetVisibility(phase.enabled, true);
    benchmark_camera_->SetActorLocation(benchmark_camera_origin_);
    FlushRenderingCommands();

    benchmark_frames_remaining_ = benchmark_warmup_frames_;
    benchmark_motion_frame_ = 0;
    benchmark_max_camera_distance_ = 0.0;
    benchmark_capture_pending_ = false;

    UE_LOG(LogGpuStarfieldBenchmark,
           Display,
           TEXT("Warming phase %d/%d: %d stars, %s, %s camera, repeat %d."),
           benchmark_phase_index_ + 1,
           benchmark_phases_.Num(),
           phase.star_count,
           phase.enabled ? TEXT("enabled") : TEXT("disabled"),
           phase.camera_motion ? TEXT("moving") : TEXT("stationary"),
           phase.repeat_index + 1);
}

void AGpuStarfieldExperimentActor::begin_csv_capture() {
    auto const& phase{benchmark_phases_[benchmark_phase_index_]};
    auto const filename{FString::Printf(TEXT("gpu_starfield_%d_%s_%s_r%d.csv"),
                                        phase.star_count,
                                        phase.camera_motion ? TEXT("moving") : TEXT("stationary"),
                                        phase.enabled ? TEXT("enabled") : TEXT("disabled"),
                                        phase.repeat_index + 1)};

    benchmark_capture_pending_ = true;
    TWeakObjectPtr<AGpuStarfieldExperimentActor> const weak_this{this};
    csv_finished_delegate_ = FCsvProfiler::Get()->OnCSVProfileFinished().AddLambda(
        [weak_this](FString const& output_filename) {
            AsyncTask(ENamedThreads::GameThread, [weak_this, output_filename] {
                if (auto* const actor{weak_this.Get()}) {
                    actor->finish_benchmark_phase(output_filename);
                }
            });
        });
    FCsvProfiler::Get()->BeginCapture(
        benchmark_capture_frames_, benchmark_output_directory_, filename);

    UE_LOG(LogGpuStarfieldBenchmark, Display, TEXT("Capturing %s"), *filename);
}

void AGpuStarfieldExperimentActor::finish_benchmark_phase(FString const& filename) {
    FCsvProfiler::Get()->OnCSVProfileFinished().Remove(csv_finished_delegate_);
    csv_finished_delegate_.Reset();

    auto const& phase{benchmark_phases_[benchmark_phase_index_]};
    if (phase.camera_motion && benchmark_max_camera_distance_ < benchmark_camera_distance * 0.95) {
        UE_LOG(LogGpuStarfieldBenchmark,
               Error,
               TEXT("Moving benchmark reached only %.2f km of the required %.2f km."),
               benchmark_max_camera_distance_ / 100000.0,
               benchmark_camera_distance / 100000.0);
        benchmark_active_ = false;
        SetActorTickEnabled(false);
        FPlatformMisc::RequestExitWithStatus(true, 1);
        return;
    }

    UE_LOG(LogGpuStarfieldBenchmark, Display, TEXT("Finished %s"), *filename);
    ++benchmark_phase_index_;
    if (benchmark_phase_index_ >= benchmark_phases_.Num()) {
        benchmark_active_ = false;
        SetActorTickEnabled(false);
        FPlatformMisc::RequestExit(false);
        return;
    }

    begin_benchmark_phase();
}

void AGpuStarfieldExperimentActor::update_benchmark_camera() {
    auto* const camera{benchmark_camera_.Get()};
    if (camera == nullptr || !benchmark_phases_.IsValidIndex(benchmark_phase_index_)) {
        return;
    }

    auto const& phase{benchmark_phases_[benchmark_phase_index_]};
    if (!phase.camera_motion) {
        return;
    }

    auto const total_frames{FMath::Max(benchmark_warmup_frames_ + benchmark_capture_frames_, 2)};
    auto const alpha{FMath::Clamp(static_cast<double>(benchmark_motion_frame_) /
                                      static_cast<double>(total_frames - 1),
                                  0.0,
                                  1.0)};
    auto const offset{FVector{benchmark_camera_distance * alpha,
                              benchmark_camera_lateral_distance * FMath::Sin(2.0 * PI * alpha),
                              benchmark_camera_lateral_distance * 0.25 * FMath::Sin(PI * alpha)}};
    camera->SetActorLocation(benchmark_camera_origin_ + offset);
    benchmark_max_camera_distance_ =
        FMath::Max(benchmark_max_camera_distance_,
                   FVector::Distance(camera->GetActorLocation(), benchmark_camera_origin_));
    ++benchmark_motion_frame_;
}
