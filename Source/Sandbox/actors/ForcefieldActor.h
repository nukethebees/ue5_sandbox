#pragma once

#include "Components/BoxComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/StaticMeshComponent.h"
#include "CoreMinimal.h"
#include "Engine/TimerHandle.h"
#include "GameFramework/Actor.h"
#include "Particles/ParticleSystemComponent.h"
#include "Sandbox/interfaces/Activatable.h"
#include "Sandbox/mixins/log_msg_mixin.hpp"

#include "ForcefieldActor.generated.h"

UENUM(BlueprintType)
enum class EForcefieldState : uint8 {
    Inactive UMETA(DisplayName = "Inactive"),
    Activating UMETA(DisplayName = "Activating"),
    Active UMETA(DisplayName = "Active"),
    Deactivating UMETA(DisplayName = "Deactivating"),
    Cooldown UMETA(DisplayName = "Cooldown")
};

namespace ml {
inline static constexpr wchar_t ForcefieldActorLogTag[]{TEXT("Forcefield")};
}

UCLASS()
class SANDBOX_API AForcefieldActor
    : public AActor
    , public IActivatable
    , public ml::LogMsgMixin<ml::ForcefieldActorLogTag> {
    GENERATED_BODY()
  public:
    AForcefieldActor();
  protected:
    virtual void BeginPlay() override;

    // IActivatable interface
    void trigger_activation(AActor* instigator = nullptr) override;
    bool can_activate(AActor const* instigator) const override;

    // Configuration properties
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forcefield Settings")
    float transition_duration{1.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forcefield Settings")
    float cooldown_duration{3.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forcefield Settings")
    float distortion_strength{0.5f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forcefield Settings")
    bool enable_distortion_effect{true};

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
  private:
    // Debug drawing constants
    static constexpr bool debug_persistent_lines{false};
    static constexpr float debug_lifetime{-1.0f};
    static constexpr uint8 debug_depth_priority{0};
    static constexpr float debug_line_thickness{2.0f};
    static constexpr float debug_update_interval{1.0f};

    // State management
    EForcefieldState current_state{EForcefieldState::Inactive};
    FTimerHandle state_timer_handle;
    FTimerHandle cooldown_timer_handle;
    FTimerHandle debug_timer_handle;

    // Material instances for dynamic effects
    UPROPERTY()
    class UMaterialInstanceDynamic* barrier_material_instance{nullptr};

    // State transition methods
    void start_activation();
    void complete_activation();
    void start_deactivation();
    void complete_deactivation();
    void start_cooldown();
    void complete_cooldown();

    // Visual management
    void update_visual_effects();
    void update_collision_blocking();

    // Debug visualization
    void draw_debug_info() const;

    // Validation helpers
    bool is_in_transition_state() const;
    bool can_change_state() const;
};
