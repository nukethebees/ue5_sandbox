#pragma once

#include <CoreMinimal.h>

#include "ObserverControlContext.generated.h"

class ACameraActor;
class ASpaceGamePlayerController;
class IEnhancedInputSubsystemInterface;
class UEnhancedInputComponent;
class UInputAction;
class UInputMappingContext;
struct FInputActionValue;

USTRUCT(BlueprintType)
struct SPACEGAME_API FObserverControlInputs {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Observer")
    TObjectPtr<UInputMappingContext> mapping_context{nullptr};

    UPROPERTY(EditAnywhere, Category = "Observer")
    TObjectPtr<UInputAction> move{nullptr};

    UPROPERTY(EditAnywhere, Category = "Observer")
    TObjectPtr<UInputAction> vertical_move{nullptr};

    UPROPERTY(EditAnywhere, Category = "Observer")
    TObjectPtr<UInputAction> look{nullptr};

    UPROPERTY(EditAnywhere, Category = "Observer")
    TObjectPtr<UInputAction> engage_look{nullptr};

    UPROPERTY(EditAnywhere, Category = "Observer")
    TObjectPtr<UInputAction> adjust_speed{nullptr};

    UPROPERTY(EditAnywhere, Category = "Observer")
    TObjectPtr<UInputAction> boost{nullptr};

    [[nodiscard]] auto is_valid() const -> bool {
        return mapping_context && move && vertical_move && look && engage_look && adjust_speed &&
               boost;
    }
};

struct SPACEGAME_API FObserverControlContext {
    FObserverControlContext() = default;
    FObserverControlContext(FObserverControlContext const&) = delete;
    FObserverControlContext(FObserverControlContext&&) = delete;
    auto operator=(FObserverControlContext const&) -> FObserverControlContext& = delete;
    auto operator=(FObserverControlContext&&) -> FObserverControlContext& = delete;

    auto initialise(ASpaceGamePlayerController& owner,
                    UEnhancedInputComponent& input_component,
                    IEnhancedInputSubsystemInterface& input_subsystem,
                    FObserverControlInputs const& input) -> bool;
    auto bind() -> bool;
    void unbind();
    void shutdown();
    void set_camera(ACameraActor* camera);

    [[nodiscard]] auto can_bind() const -> bool;
    [[nodiscard]] auto is_bound() const noexcept -> bool { return bound_; }
    [[nodiscard]] auto get_movement_speed() const noexcept -> float;
  private:
    void bind_actions();
    void remove_action_bindings();
    void toggle_look();
    void end_look();
    void move(FInputActionValue const& value);
    void move_vertical(FInputActionValue const& value);
    void look(FInputActionValue const& value);
    void adjust_speed(FInputActionValue const& value);
    void begin_boost();
    void end_boost();
    void publish_speed() const;
    auto get_camera() const -> ACameraActor*;

    inline static constexpr float movement_speeds_[]{5000.f, 15000.f, 50000.f, 150000.f, 500000.f};
    inline static constexpr int32 initial_speed_index_{2};
    inline static constexpr float boost_multiplier_{4.f};
    inline static constexpr float look_sensitivity_{0.1f};

    TWeakObjectPtr<ASpaceGamePlayerController> owner_;
    TWeakObjectPtr<UEnhancedInputComponent> input_component_;
    TWeakObjectPtr<UObject> input_subsystem_object_;
    TWeakObjectPtr<ACameraActor> camera_;
    IEnhancedInputSubsystemInterface* input_subsystem_{nullptr};
    FObserverControlInputs const* input_{nullptr};
    TArray<uint32> binding_handles_;
    TWeakObjectPtr<UInputMappingContext> registered_mapping_;
    int32 speed_index_{initial_speed_index_};
    bool initialised_{false};
    bool bound_{false};
    bool looking_{false};
    bool boosting_{false};
};
