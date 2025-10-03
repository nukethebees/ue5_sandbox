// Fill out your copyright notice in the Description page of Project Settings.

#include "BenchmarkOrchestratorActor.h"

#include "Camera/CameraActor.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/DateTime.h"
#include "ProfilingDebugging/TraceAuxiliary.h"
#include "TimerManager.h"

ABenchmarkOrchestratorActor::ABenchmarkOrchestratorActor() {
    PrimaryActorTick.bCanEverTick = false;
}

void ABenchmarkOrchestratorActor::BeginPlay() {
    Super::BeginPlay();

    if (!run_benchmark) {
        return;
    }

    if (benchmark_camera) {
        if (auto* pc{UGameplayStatics::GetPlayerController(this, 0)}) {
            pc->SetViewTarget(benchmark_camera);
        } else {
            UE_LOG(LogTemp, Warning, TEXT("pc is nullptr"));
        }
    } else {
        UE_LOG(LogTemp, Warning, TEXT("benchmark_camera is nullptr"));
    }

    constexpr float minimum_print_time{5.0f};

    if ((benchmark_print_update_seconds > 0.0f) &&
        (benchmark_print_update_seconds < minimum_print_time)) {
        log_warning(TEXT("Benchmark print time below minimum of %f seconds. Increasing it."),
                    benchmark_print_update_seconds);
        benchmark_print_update_seconds = minimum_print_time;
    }

    start_trace();
    GetWorldTimerManager().SetTimer(trace_timer_handle,
                                    this,
                                    &ABenchmarkOrchestratorActor::stop_trace,
                                    benchmark_duration_seconds,
                                    false);

    if (benchmark_print_update_seconds > 0.0f) {
        GetWorldTimerManager().SetTimer(log_timer_handle,
                                        this,
                                        &ABenchmarkOrchestratorActor::log_time,
                                        benchmark_print_update_seconds,
                                        true);
    }
}
void ABenchmarkOrchestratorActor::EndPlay(EEndPlayReason::Type const EndPlayReason) {
    Super::EndPlay(EndPlayReason);

    if (!run_benchmark) {
        return;
    }

    if (trace_timer_handle.IsValid()) {
        GetWorldTimerManager().ClearTimer(trace_timer_handle);
        GetWorldTimerManager().ClearTimer(log_timer_handle);
    }
    stop_trace();
}

void ABenchmarkOrchestratorActor::start_trace() {
#if UE_TRACE_ENABLED
    auto const timestamp{FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"))};

    FString trace_filename{};

    if (benchmark_name.IsNone()) {
        trace_filename = FString::Printf(TEXT("benchmark_%s"), *timestamp);
    } else {
        trace_filename =
            FString::Printf(TEXT("benchmark_%s_%s"), *timestamp, *benchmark_name.ToString());
    }

    FTraceAuxiliary::FOptions tracing_options;
    tracing_options.bExcludeTail = true;

    FTraceAuxiliary::Start(
        FTraceAuxiliary::EConnectionType::File, *trace_filename, nullptr, &tracing_options);

    UE_LOG(LogTemp, Display, TEXT("Benchmark trace started: %s.utrace"), *trace_filename);
#endif
}

void ABenchmarkOrchestratorActor::stop_trace() {
#if UE_TRACE_ENABLED
    FTraceAuxiliary::Stop();
    UE_LOG(LogTemp, Display, TEXT("Benchmark trace stopped. File saved to Saved/Profiling/"));
#endif

#if WITH_EDITOR
    switch (benchmark_end) {
        using enum EBenchmarkEndState;
        case Quit: {
            log_display(TEXT("Quitting game after benchmark."));
            UKismetSystemLibrary::QuitGame(this, nullptr, EQuitPreference::Quit, true);
            break;
        }
        case Pause: {
            log_display(TEXT("Pausing game after benchmark."));
            GetWorld()->bDebugPauseExecution = true;
            break;
        }
        case Play:
            [[fallthrough]];
        default: {
            break;
        }
    }

#endif
}
void ABenchmarkOrchestratorActor::log_time() {
    time_elapsed += benchmark_print_update_seconds;
    log_display(
        TEXT("Benchmark time: %.2f / %.2f seconds."), time_elapsed, benchmark_duration_seconds);
}
