#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "SandboxGameShared/logging/LogMsgMixin.hpp"

#include "BenchmarkOrchestratorActor.generated.h"

class ACameraActor;

UENUM(BlueprintType)
enum class EBenchmarkEndState : uint8 {
    Play UMETA(DisplayName = "Play"),
    Quit UMETA(DisplayName = "Quit"),
    Pause UMETA(DisplayName = "Pause")
};

UCLASS()
class SANDBOX_API ABenchmarkOrchestratorActor
    : public AActor
    , public ml::LogMsgMixin<"ABenchmarkOrchestratorActor"> {
    GENERATED_BODY()
  public:
    ABenchmarkOrchestratorActor();
  protected:
    auto BeginPlay() -> void override;
    auto EndPlay(EEndPlayReason::Type const end_play_reason) -> void override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Benchmark")
    FName benchmark_name{NAME_None};

    UPROPERTY(EditAnywhere, Category = "Benchmark")
    float benchmark_duration_seconds{10.0f};

    UPROPERTY(EditAnywhere, Category = "Benchmark")
    float benchmark_print_update_seconds{5.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Benchmark")
    TObjectPtr<ACameraActor> benchmark_camera{nullptr};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Benchmark")
    bool enabled{true};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Benchmark")
    bool run_benchmark{true};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Benchmark")
    bool trace_none{false};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Benchmark")
    EBenchmarkEndState benchmark_end{EBenchmarkEndState::Quit};

    UPROPERTY(EditAnywhere, Category = "Benchmark|Tracing")
    TArray<FString> trace_channels{TEXT("CPU"),
                                   TEXT("Callstack"),
                                   TEXT("Counter"),
                                   TEXT("File"),
                                   TEXT("Frame"),
                                   TEXT("GPU"),
                                   TEXT("Log"),
                                   TEXT("Memory"),
                                   TEXT("Niagara"),
                                   TEXT("Physics"),
                                   TEXT("RenderCommands"),
                                   TEXT("RHICommands"),
                                   TEXT("Stats"),
                                   TEXT("Task"),
                                   TEXT("Slate"),
                                   TEXT("SlateWidgets")};
  private:
    auto start_trace() -> void;
    auto stop_trace() -> void;
    auto log_time() -> void;

    FTimerHandle trace_timer_handle;
    FTimerHandle log_timer_handle;
    float time_elapsed{0.0f};
    bool benchmark_running_{false};
};
