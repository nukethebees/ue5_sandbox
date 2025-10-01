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

    if (benchmark_camera) {
        if (auto* pc{UGameplayStatics::GetPlayerController(this, 0)}) {
            pc->SetViewTarget(benchmark_camera);
        } else {
            UE_LOG(LogTemp, Warning, TEXT("pc is nullptr"));
        }
    } else {
        UE_LOG(LogTemp, Warning, TEXT("benchmark_camera is nullptr"));
    }

    start_trace();
    GetWorldTimerManager().SetTimer(trace_timer_handle,
                                    this,
                                    &ABenchmarkOrchestratorActor::stop_trace,
                                    benchmark_duration_seconds,
                                    false);
}

void ABenchmarkOrchestratorActor::start_trace() {
#if UE_TRACE_ENABLED
    auto const timestamp{FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"))};
    auto const trace_filename{FString::Printf(TEXT("benchmark_%s"), *timestamp)};

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
    UKismetSystemLibrary::QuitGame(this, nullptr, EQuitPreference::Quit, true);
#endif
}
