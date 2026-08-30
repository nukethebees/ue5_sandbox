#include "SbxShadersExperiments/GpuStarfield/GpuStarfieldExperimentActor.h"

#include "Async/Async.h"
#include "Camera/CameraActor.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "EngineUtils.h"
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

    if (!benchmark_active_ || benchmark_capture_pending_) {
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
    if (star_counts.IsEmpty()) {
        UE_LOG(LogGpuStarfieldBenchmark,
               Error,
               TEXT("GpuStarfieldBenchmarkCounts did not contain a valid star count."));
        FPlatformMisc::RequestExitWithStatus(true, 1);
        return;
    }

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
            auto const enabled_first{repeat_index % 2 == 1};
            benchmark_phases_.Add({star_count, repeat_index, enabled_first});
            benchmark_phases_.Add({star_count, repeat_index, !enabled_first});
        }
    }

    auto* const world{GetWorld()};
    if (world == nullptr) {
        UE_LOG(LogGpuStarfieldBenchmark, Error, TEXT("Benchmark world is unavailable."));
        FPlatformMisc::RequestExitWithStatus(true, 1);
        return;
    }

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
    FlushRenderingCommands();

    benchmark_frames_remaining_ = benchmark_warmup_frames_;
    benchmark_capture_pending_ = false;

    UE_LOG(LogGpuStarfieldBenchmark,
           Display,
           TEXT("Warming phase %d/%d: %d stars, %s, repeat %d."),
           benchmark_phase_index_ + 1,
           benchmark_phases_.Num(),
           phase.star_count,
           phase.enabled ? TEXT("enabled") : TEXT("disabled"),
           phase.repeat_index + 1);
}

void AGpuStarfieldExperimentActor::begin_csv_capture() {
    auto const& phase{benchmark_phases_[benchmark_phase_index_]};
    auto const filename{FString::Printf(TEXT("gpu_starfield_%d_%s_r%d.csv"),
                                        phase.star_count,
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
