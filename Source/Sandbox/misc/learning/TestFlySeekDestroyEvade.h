#pragma once

#include "Sandbox/combat/BurstFire.h"
#include "Sandbox/logging/ActorLoggingConfig.h"
#include "Sandbox/misc/learning/TestMaterialConfig.h"
#include "Sandbox/players/VisionConfig.h"
#include "Sandbox/utilities/DrawDebugConfig.h"
#include "Sandbox/utilities/FloatBounds.h"
#include "TestFlyBase.h"

#include "CoreMinimal.h"

#include "TestFlySeekDestroyEvade.generated.h"

class UArrowComponent;

class ATestVolume;
class AShipLaser;

UENUM()
enum class ETestFlySeekDestroyEvadeState : uint8 {
    searching,
    chasing,
    attacking,
    attack_repositioning,
    evading,
};

USTRUCT()
struct FTestFlySeekDestroyEvadeStateVisualsConfig {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Fly")
    FDrawDebugConfig debug_drawer;
    UPROPERTY(EditAnywhere, Category = "Fly")
    FTestMaterialConfig material{};
};

USTRUCT()
struct FTestFlySeekDestroyEvadeMovementConfig {
    GENERATED_BODY()

    FTestFlySeekDestroyEvadeMovementConfig() noexcept = default;
    FTestFlySeekDestroyEvadeMovementConfig(float turn_speed_deg_per_s, float speed)
        : turn_speed_deg_per_s(turn_speed_deg_per_s)
        , speed(speed) {}

    UPROPERTY(EditAnywhere, Category = "Fly")
    float turn_speed_deg_per_s{60.f};
    UPROPERTY(EditAnywhere, Category = "Fly")
    float speed{3000.f};
};

USTRUCT()
struct FTestFlySeekDestroyEvadeSearchStateConfig {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere)
    FTestFlySeekDestroyEvadeStateVisualsConfig visuals_config{};

    UPROPERTY(EditAnywhere, Category = "Fly")
    FTestFlySeekDestroyEvadeMovementConfig movement;

    UPROPERTY(EditAnywhere)
    TObjectPtr<ATestVolume> search_volume{nullptr};
    UPROPERTY(EditAnywhere)
    float min_distance_to_new_point{5000.f};
    UPROPERTY(EditAnywhere)
    float acceptance_radius{500.f};
};

USTRUCT()
struct FTestFlySeekDestroyEvadeChaseStateConfig {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Fly")
    FTestFlySeekDestroyEvadeStateVisualsConfig visuals_config{};

    UPROPERTY(EditAnywhere, Category = "Fly")
    FTestFlySeekDestroyEvadeMovementConfig movement;
    UPROPERTY(EditAnywhere, Category = "Fly")
    float acceptance_radius{3000.f};
};

USTRUCT()
struct FTestFlySeekDestroyEvadeAttackStateConfig {
    GENERATED_BODY()

    // Visuals
    UPROPERTY(EditAnywhere, Category = "Fly")
    FTestFlySeekDestroyEvadeStateVisualsConfig visuals_config{};
    UPROPERTY(EditAnywhere, Category = "Fly")
    float target_sphere_radius{200.f};

    // Movement
    UPROPERTY(EditAnywhere, Category = "Fly")
    FTestFlySeekDestroyEvadeMovementConfig movement;
    UPROPERTY(EditAnywhere, Category = "Fly")
    float reposition_radius{1000.f};

    // Laser
    UPROPERTY(EditAnywhere, Category = "Fly")
    TSubclassOf<AShipLaser> laser_class;
    UPROPERTY(EditAnywhere, Category = "Fly")
    int32 laser_damage{10};

    // Aiming
    UPROPERTY(EditAnywhere, Category = "Fly")
    float max_fire_angle_degrees{3.f};
};

USTRUCT()
struct FTestFlySeekDestroyEvadeAttackRepositioningStateConfig {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere)
    FTestFlySeekDestroyEvadeStateVisualsConfig visuals_config{};

    UPROPERTY(EditAnywhere, Category = "Fly")
    FTestFlySeekDestroyEvadeMovementConfig movement;
    UPROPERTY(EditAnywhere, Category = "Fly")
    float acceptance_radius{1000.f};
    UPROPERTY(EditAnywhere, Category = "Fly")
    FFloatBounds new_position_bounds{1000.f, 1500.f};
    UPROPERTY(EditAnywhere, Category = "Fly")
    // Abandon the reposition if the target moves this far away
    float target_threshold_distance{2000.f};
};

USTRUCT()
struct FTestFlySeekDestroyEvadeEvadeStateConfig {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Fly")
    FTestFlySeekDestroyEvadeStateVisualsConfig visuals_config{};

    UPROPERTY(EditAnywhere, Category = "Fly")
    FTestFlySeekDestroyEvadeMovementConfig movement;
    UPROPERTY(EditAnywhere, Category = "Fly")
    FFloatBounds new_position_bounds{1000.f, 1500.f};
};

USTRUCT()
struct FTestFlySeekDestroyEvadeConfig {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Fly")
    FTestFlySeekDestroyEvadeSearchStateConfig search;
    UPROPERTY(EditAnywhere, Category = "Fly")
    FTestFlySeekDestroyEvadeChaseStateConfig chase;
    UPROPERTY(EditAnywhere, Category = "Fly")
    FTestFlySeekDestroyEvadeAttackStateConfig attack;
    UPROPERTY(EditAnywhere, Category = "Fly")
    FTestFlySeekDestroyEvadeAttackRepositioningStateConfig attack_reposition;
    UPROPERTY(EditAnywhere, Category = "Fly")
    FTestFlySeekDestroyEvadeEvadeStateConfig evade;

    UPROPERTY(EditAnywhere, Category = "Fly|Vision")
    FVisionConfig vision{5000.f, 25.f};

    UPROPERTY(EditAnywhere, Category = "Fly")
    bool randomise_at_begin_play{false};
};

UCLASS()
class ATestFlySeekDestroyEvade : public ATestFlyBase {
  public:
    GENERATED_BODY()

    ATestFlySeekDestroyEvade();

    void Tick(float dt) override;

    // Config
    void set_config(FTestFlySeekDestroyEvadeConfig const& new_config);

    // Movement
    auto within_radius(FVector const& point_a, FVector const& point_b, float const r) const -> bool;
    auto within_bounds(FVector const& point_a,
                       FVector const& point_b,
                       float const min_bound,
                       float const max_bound) const -> bool;
  protected:
    void OnConstruction(FTransform const& t) override;
    void BeginPlay() override;

    // State
    void set_state(ETestFlySeekDestroyEvadeState new_state);
    void transition_to_state(ETestFlySeekDestroyEvadeState const old_state);

    // Search
    void handle_search(float dt);
    auto scan_for_target() -> bool;

    // Chase
    void handle_chase(float dt);

    // Attack
    void handle_attack(float dt);
    auto get_attack_position(float dt) const -> FVector;
    void fire_laser();

    // Attack repositioning
    void handle_attack_repositioning(float dt);
    void set_reposition_destination();

    // Evade
    void handle_evade(float dt);

    // Movement
    auto get_random_position_in_volume(FVector const& reference, float min_dist = 0.f) const
        -> FVector;
    auto get_random_position(FVector const& reference, float min_dist, float max_dist) const
        -> FVector;
    void move_to_location(float dt, FVector const& location);
    void set_movement(FTestFlySeekDestroyEvadeMovementConfig const& new_config);

    // IDamageableShip
    auto apply_damage(ShipDamageContext context) -> FShipDamageResult override;

    // Debugging
    void draw_debug_shapes();

    // Target
    auto validate_target() -> bool;
    void log_target_not_valid();

    UPROPERTY(EditAnywhere, Category = "Fly")
    TObjectPtr<UArrowComponent> fire_point{nullptr};

    UPROPERTY(EditAnywhere, Category = "Fly")
    FTestFlySeekDestroyEvadeConfig config;

    UPROPERTY(EditAnywhere, Category = "Fly")
    ETestFlySeekDestroyEvadeState state{ETestFlySeekDestroyEvadeState::searching};

    UPROPERTY(EditAnywhere, Category = "Fly")
    bool show_debug_shapes{false};

    // Movement
    UPROPERTY(EditAnywhere, Category = "Fly|Movement")
    float turn_speed_deg_per_s{60.f};
    UPROPERTY(VisibleAnywhere, Category = "Fly|Movement")
    float speed{0.f};

    // Objective
    UPROPERTY(VisibleAnywhere, Category = "Fly")
    FVector destination;
    UPROPERTY(VisibleAnywhere, Category = "Fly")
    TWeakObjectPtr<AActor> target{nullptr};

    UPROPERTY(VisibleAnywhere, Category = "Fly")
    FVector target_previous_position_0;
    UPROPERTY(VisibleAnywhere, Category = "Fly")
    FVector target_previous_position_1;

    // Attacking
    UPROPERTY(EditAnywhere, Category = "Fly|Attack")
    FBurstFire burst{};
    UPROPERTY(EditAnywhere, Category = "Fly|Attack")
    FCooldown stale_attack_timeout{5.f};

    // Logging
    UPROPERTY(EditAnywhere, Category = "Fly")
    FActorLoggingConfig log_config{1.f};
};
