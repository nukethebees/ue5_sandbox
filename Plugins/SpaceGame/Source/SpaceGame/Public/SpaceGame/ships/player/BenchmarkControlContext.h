#pragma once

#include <CoreMinimal.h>
#include <UObject/WeakObjectPtr.h>

#include "BenchmarkControlContext.generated.h"

class ASpaceGamePlayerController;
class UEnhancedInputComponent;
class IEnhancedInputSubsystemInterface;
class UInputMappingContext;
class UInputAction;

USTRUCT(BlueprintType)
struct SPACEGAME_API FBenchmarkControlInputs {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Benchmark")
    TObjectPtr<UInputMappingContext> mapping_context{nullptr};

    UPROPERTY(EditAnywhere, Category = "Benchmark")
    TObjectPtr<UInputAction> exit{nullptr};

    [[nodiscard]] auto is_valid() const -> bool { return mapping_context && exit; }
};

struct SPACEGAME_API FBenchmarkControlContext {
    FBenchmarkControlContext() = default;
    FBenchmarkControlContext(FBenchmarkControlContext const&) = delete;
    auto operator=(FBenchmarkControlContext const&) -> FBenchmarkControlContext& = delete;

    auto initialise(ASpaceGamePlayerController& owner,
                    UEnhancedInputComponent& component,
                    IEnhancedInputSubsystemInterface& subsystem,
                    FBenchmarkControlInputs const& input) -> bool;
    auto can_bind() const -> bool;
    auto bind() -> bool;
    void unbind();
    void shutdown();
    auto is_bound() const -> bool { return bound_; }
  private:
    inline static constexpr int32 mapping_priority_{100};
    TWeakObjectPtr<ASpaceGamePlayerController> owner_;
    TWeakObjectPtr<UEnhancedInputComponent> component_;
    TWeakObjectPtr<UObject> subsystem_object_;
    IEnhancedInputSubsystemInterface* subsystem_{nullptr};
    FBenchmarkControlInputs const* input_{nullptr};
    TWeakObjectPtr<UInputMappingContext> registered_mapping_;
    uint32 binding_handle_{0};
    bool bound_{false};
};
