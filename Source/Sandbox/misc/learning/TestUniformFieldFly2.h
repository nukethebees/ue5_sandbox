#pragma once

#include "Sandbox/core/Cooldown.h"
#include "Sandbox/logging/ActorLoggingConfig.h"
#include "Sandbox/utilities/FloatBounds.h"

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"

#include "TestUniformFieldFly2.generated.h"

class UStaticMeshComponent;

class ATestUniformField;
class AShipLaser;

// This version can track an enemy

UENUM()
enum class ETestUniformFieldFly2State : uint8 {
    exploring,
    tracking,
};

UCLASS()
class ATestUniformFieldFly2 : public APawn {
    GENERATED_BODY()
  public:
    ATestUniformFieldFly2();

    void Tick(float dt) override;
  protected:
    // Life cycle
    void BeginPlay() override;
    void OnConstruction(FTransform const& transform) override;
    void EndPlay(EEndPlayReason::Type const reason) override;

    // State
    void set_state(ETestUniformFieldFly2State new_state);
    void explore(float dt);
    void track(float dt);

    // Navigation
    void set_new_destination();
    void move_to_destination(float dt);
    void display_destination();
    bool at_target() const;
    bool is_oob() const;

    // Vision
    void display_vision_cone();

    // Field
    bool assert_field_exists();

    // Targets
    bool try_find_target();

    // Logging
    bool can_log(EActorLoggingVerbosity msg_verbosity) const;
    bool can_tick_log(EActorLoggingVerbosity msg_verbosity) const;

    // State
    UPROPERTY(VisibleAnywhere, Category = "Ship")
    ETestUniformFieldFly2State state{ETestUniformFieldFly2State::exploring};

    // Visuals
    UPROPERTY(EditAnywhere, Category = "Ship")
    TObjectPtr<UStaticMeshComponent> mesh{nullptr};

    // Navigation
    UPROPERTY(EditAnywhere, Category = "Ship")
    FVector destination{FVector::ZeroVector};
    UPROPERTY(EditAnywhere, Category = "Ship")
    bool show_destination{true};
    UPROPERTY(EditAnywhere, Category = "Ship")
    float acceptance_radius{1000.f};
    UPROPERTY(EditAnywhere, Category = "Ship")
    FFloatBounds dist_bounds{1000.f, 5000.f};

    // Movement
    UPROPERTY(EditAnywhere, Category = "Ship")
    float speed{1000.f};

    // Vision
    UPROPERTY(EditAnywhere, Category = "Ship")
    float vision_radius{1000.f};
    UPROPERTY(EditAnywhere, Category = "Ship")
    float vision_angle_deg{40.f};
    UPROPERTY(EditAnywhere, Category = "Ship")
    bool show_vision_cone{true};

    // Field
    UPROPERTY(EditInstanceOnly, Category = "Ship")
    TObjectPtr<ATestUniformField> field{nullptr};

    // Target
    UPROPERTY(EditInstanceOnly, Category = "Ship")
    TArray<TSubclassOf<AActor>> target_classes;
    UPROPERTY(VisibleInstanceOnly, Category = "Ship")
    TWeakObjectPtr<AActor> target{nullptr};

    // Combat
    UPROPERTY(EditAnywhere, Category = "Ship")
    TSubclassOf<AShipLaser> laser_class;
    UPROPERTY(EditAnywhere, Category = "Ship")
    FCooldown fire_cooldown{0.3f};
    UPROPERTY(EditAnywhere, Category = "Ship")
    float fire_point_distance{100.f};

    // Logging
    UPROPERTY(EditAnywhere, Category = "Ship")
    FActorLoggingConfig log_config{2.f};
};
