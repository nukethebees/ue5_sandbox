#pragma once

#include "Components/BoxComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TimelineComponent.h"
#include "CoreMinimal.h"
#include "Curves/CurveFloat.h"
#include "Engine/TimerHandle.h"
#include "GameFramework/Actor.h"
#include "Particles/ParticleSystemComponent.h"

#include "Sandbox/enums/ForcefieldState.h"
#include "Sandbox/data/trigger/ForcefieldPayload.h"
#include "Sandbox/data/trigger/TriggerableId.h"
#include "Sandbox/mixins/log_msg_mixin.hpp"

#include "ForcefieldActor.generated.h"

UCLASS()
class SANDBOX_API AForcefieldActor
    : public AActor
    , public ml::LogMsgMixin<"Forcefield"> {
    GENERATED_BODY()
  public:
    AForcefieldActor();

    struct Constants {
        static auto const& EmissiveStrength() {
            static auto const tag{FName(TEXT("EmissiveStrength"))};
            return tag;
        }

        static auto const& Opacity() {
            static auto const tag{FName(TEXT("Opacity"))};
            return tag;
        }

        static auto const& NoiseSpeed() {
            static auto const tag{FName(TEXT("NoiseSpeed"))};
            return tag;
        }

        static auto const& NoiseIntensity() {
            static auto const tag{FName(TEXT("NoiseIntensity"))};
            return tag;
        }

        static auto const& ForwardVector() {
            static auto const tag{FName(TEXT("ForwardVector"))};
            return tag;
        }
    };
  protected:
    virtual void BeginPlay() override;
    virtual void OnConstruction(FTransform const& Transform) override;
  public:
    void trigger_activation();
    bool can_activate() const;
  protected:
    // Configuration properties
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forcefield Settings")
    float cooldown_duration{3.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forcefield Settings")
    UTimelineComponent* transition_pulse_timeline;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forcefield Settings")
    UCurveFloat* transition_pulse_curve;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forcefield Settings")
    float distortion_strength{0.5f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forcefield Settings")
    float noise_animation_speed{1.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forcefield Settings")
    float noise_intensity{0.3f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Forcefield Settings",
              meta = (ClampMin = "-1.0", ClampMax = "1.0"))
    float peak_opacity{-1.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forcefield Settings")
    bool show_debug_visualization{false};

    // Getter for current state
    UFUNCTION(BlueprintCallable, Category = "Forcefield")
    EForcefieldState get_current_state() const { return current_state; }

    UFUNCTION(BlueprintCallable, Category = "Forcefield")
    bool is_blocking() const { return current_state == EForcefieldState::Active; }
  protected:
    // Components
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UBoxComponent* collision_volume;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UStaticMeshComponent* barrier_mesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UParticleSystemComponent* sparkle_particles;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UPostProcessComponent* distortion_effect;

    // Trigger system integration
    UPROPERTY(EditAnywhere, Category = "Trigger System")
    FForcefieldPayload forcefield_payload{};

    TriggerableId triggerable_id{TriggerableId::invalid()};
  private:
    // Debug drawing constants
    static constexpr bool debug_persistent_lines{false};
    static constexpr float debug_lifetime{-1.0f};
    static constexpr uint8 debug_depth_priority{0};
    static constexpr float debug_line_thickness{2.0f};
    static constexpr float debug_update_interval{1.0f};

    // Default opacity constants
    static constexpr float default_opacity{0.3f};

    // State management
    EForcefieldState current_state{EForcefieldState::Inactive};
    FTimerHandle state_timer_handle;
    FTimerHandle cooldown_timer_handle;
    FTimerHandle debug_timer_handle;

    // Material instances for dynamic effects
    UPROPERTY()
    class UMaterialInstanceDynamic* barrier_material_instance{nullptr};

    // Resolved opacity value (from material or override)
    float resolved_peak_opacity{default_opacity};
  public:
    // State transition methods (made public for FForcefieldPayload)
    void start_activation();
    void start_deactivation();
    void start_cooldown();
    void complete_cooldown();
    void change_state(EForcefieldState state);
  private:
    // Visual management
    void update_visual_effects();
    void update_collision_blocking();
    UFUNCTION()
    void on_pulse_update(float value);
    UFUNCTION()
    void on_pulse_end();
    UFUNCTION()
    void initialise_barrier_material();

    // Debug visualization
    void draw_debug_info() const;

    // Validation helpers
    bool is_visible() const;
    bool is_in_transition_state() const;
    bool can_change_state() const;
    void set_emissive_strength(float es);
    void set_opacity(float opacity);

    // Timeline curve helpers
    float get_curve_start_value() const;
    float get_curve_end_value() const;
};
