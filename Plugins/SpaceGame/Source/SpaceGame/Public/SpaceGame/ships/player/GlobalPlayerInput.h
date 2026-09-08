#pragma once

#include <CoreMinimal.h>
#include <UObject/WeakObjectPtr.h>

class ASpaceGamePlayerController;
class UEnhancedInputComponent;
class IEnhancedInputSubsystemInterface;
class UInputMappingContext;
struct FGlobalControlInputs;

struct SPACEGAME_API FGlobalPlayerInput {
    FGlobalPlayerInput() = default;
    FGlobalPlayerInput(FGlobalPlayerInput const&) = delete;
    auto operator=(FGlobalPlayerInput const&) -> FGlobalPlayerInput& = delete;

    auto initialise(ASpaceGamePlayerController& owner,
                    UEnhancedInputComponent& component,
                    IEnhancedInputSubsystemInterface& subsystem,
                    FGlobalControlInputs const& input) -> bool;
    void set_mapping_enabled(bool enabled);
    void shutdown();
    auto is_mapping_enabled() const -> bool { return mapping_enabled_; }
  private:
    inline static constexpr int32 mapping_priority_{100};
    TWeakObjectPtr<UEnhancedInputComponent> component_;
    TWeakObjectPtr<UObject> subsystem_object_;
    IEnhancedInputSubsystemInterface* subsystem_{nullptr};
    TWeakObjectPtr<UInputMappingContext> mapping_;
    uint32 binding_handle_{0};
    bool bound_{false};
    bool mapping_enabled_{false};
};
