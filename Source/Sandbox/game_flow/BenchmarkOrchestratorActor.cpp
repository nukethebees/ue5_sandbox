#include "BenchmarkOrchestratorActor.h"

#include <SpaceGame/support/logging/SandboxLogCategories.h>

#include "Camera/CameraActor.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/DateTime.h"
#include "ProfilingDebugging/TraceAuxiliary.h"
#include "TimerManager.h"

/*
    This will not work if the session browser is open as it will run a trace on top of this one.
    I need to find out why this happens.
*/

ABenchmarkOrchestratorActor::ABenchmarkOrchestratorActor() {
    PrimaryActorTick.bCanEverTick = false;
}

auto ABenchmarkOrchestratorActor::BeginPlay() -> void {
    Super::BeginPlay();

    if (!enabled) {
        return;
    }

    if (benchmark_camera == nullptr) {
        UE_LOG(LogSandbox, Fatal, TEXT("benchmark_camera is nullptr"));
        return;
    }

    auto* player_controller{UGameplayStatics::GetPlayerController(this, 0)};
    if (player_controller == nullptr) {
        UE_LOG(LogSandbox, Fatal, TEXT("player_controller is nullptr"));
        return;
    }
    player_controller->SetViewTarget(benchmark_camera);

    if (!run_benchmark) {
        return;
    }

    constexpr float minimum_print_time{5.0f};

    if ((benchmark_print_update_seconds > 0.0f) &&
        (benchmark_print_update_seconds < minimum_print_time)) {
        log_warning(TEXT("Benchmark print time below minimum of %f seconds. Increasing it."),
                    benchmark_print_update_seconds);
        benchmark_print_update_seconds = minimum_print_time;
    }

    benchmark_running_ = true;
    start_trace();
    auto& timer_manager{GetWorldTimerManager()};
    timer_manager.SetTimer(trace_timer_handle,
                           this,
                           &ABenchmarkOrchestratorActor::stop_trace,
                           benchmark_duration_seconds,
                           false);

    if (benchmark_print_update_seconds > 0.0f) {
        timer_manager.SetTimer(log_timer_handle,
                               this,
                               &ABenchmarkOrchestratorActor::log_time,
                               benchmark_print_update_seconds,
                               true);
    }
}
auto ABenchmarkOrchestratorActor::EndPlay(EEndPlayReason::Type const end_play_reason) -> void {
    Super::EndPlay(end_play_reason);

    stop_trace();
}

auto ABenchmarkOrchestratorActor::start_trace() -> void {
#if UE_TRACE_ENABLED
    auto const timestamp{FDateTime::Now().ToString(TEXT("%Y-%m-%d_%H-%M-%S"))};

    FString trace_filename{};

    if (benchmark_name.IsNone()) {
        trace_filename = FString::Printf(TEXT("benchmark_%s"), *timestamp);
    } else {
        constexpr int32 min_time_decimal_places{0};
        auto duration{FString::SanitizeFloat(benchmark_duration_seconds, min_time_decimal_places)};
        duration.ReplaceCharInline(TEXT('.'), TEXT('p'));

        trace_filename = FString::Printf(
            TEXT("benchmark_%s_%s_%ss"), *timestamp, *benchmark_name.ToString(), *duration);
    }

    FTraceAuxiliary::FOptions tracing_options;
    tracing_options.bExcludeTail = true;

    auto const channels{
        trace_none ? FString{}
                   : FString::JoinBy(trace_channels, TEXT(","), [](FString const& channel) {
                         return channel.ToLower();
                     })};

    FTraceAuxiliary::Start(
        FTraceAuxiliary::EConnectionType::File, *trace_filename, *channels, &tracing_options);

    UE_LOG(LogSandbox,
           Display,
           TEXT("Benchmark trace started: %s.utrace (channels: %s)"),
           *trace_filename,
           *channels);
#endif
}

auto ABenchmarkOrchestratorActor::stop_trace() -> void {
    if (!benchmark_running_) {
        return;
    }
    benchmark_running_ = false;

    auto& timer_manager{GetWorldTimerManager()};
    timer_manager.ClearTimer(trace_timer_handle);
    timer_manager.ClearTimer(log_timer_handle);

#if UE_TRACE_ENABLED
    FTraceAuxiliary::Stop();
    UE_LOG(LogSandbox, Display, TEXT("Benchmark trace stopped. File saved to Saved/Profiling/"));
#endif

#if WITH_EDITOR
    if (benchmark_end == EBenchmarkEndState::Quit) {
        log_display(TEXT("Quitting game after benchmark."));
        UKismetSystemLibrary::QuitGame(this, nullptr, EQuitPreference::Quit, true);
    } else if (benchmark_end == EBenchmarkEndState::Pause) {
        log_display(TEXT("Pausing game after benchmark."));
        GetWorld()->bDebugPauseExecution = true;
    }
#else
    log_display(TEXT("Quitting game after benchmark."));
    UKismetSystemLibrary::QuitGame(this, nullptr, EQuitPreference::Quit, true);
#endif
}
auto ABenchmarkOrchestratorActor::log_time() -> void {
    time_elapsed += benchmark_print_update_seconds;
    log_display(
        TEXT("Benchmark time: %.2f / %.2f seconds."), time_elapsed, benchmark_duration_seconds);
}
