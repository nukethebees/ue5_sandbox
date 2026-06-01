#pragma once

#include "TestTeam.h"

#include "SandboxCore/countdown_timers.h"
#include "SandboxCore/generation_index.h"

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "TestCapitalShips.generated.h"

class UInstancedStaticMeshComponent;
class UBoxComponent;

class UTestCapitalShipsConfig;
class ATestCapitalShipProxy;
class ATestCapitalShipFighters;
class ATestEntityRegistry;

UCLASS()
class ATestCapitalShips : public AActor {
  public:
    GENERATED_BODY()

    ATestCapitalShips();

    void PostInitializeComponents() override;
    void Tick(float dt) override;

    // Getters
    auto get_num_instances() const -> int32;
    auto get_location(FGenerationIndex const index) const -> FVector;
    auto is_valid(FGenerationIndex const index) const -> bool;
  protected:
    void BeginPlay() override;

    // Ship spawning
    void register_all_proxies_in_level();
    void spawn_ships(TConstArrayView<FGenerationIndex> const new_indices,
                     TConstArrayView<FTransform> const new_transforms,
                     TConstArrayView<ETestTeam> const new_teams,
                     TConstArrayView<FGenerationIndex> const new_target_indices);

    // Fighter spawning
    void handle_fighter_spawning();

    // Visuals
    void configure_ismc();

    // Misc
    void clear_runtime_state();

    // Debugging
    bool array_sizes_consistent() const;
    void draw_debugging_shapes() const;

    // Config / context
    UPROPERTY(EditAnywhere, Category = "Ship")
    TObjectPtr<UTestCapitalShipsConfig> ship_config{nullptr};
    UPROPERTY(EditAnywhere, Category = "Ship")
    TObjectPtr<ATestEntityRegistry> entity_registry{nullptr};

    UPROPERTY()
    TObjectPtr<UInstancedStaticMeshComponent> instances;
    UPROPERTY()
    TArray<TObjectPtr<UBoxComponent>> collision_boxes;

    // Entity data
    UPROPERTY()
    TArray<FGenerationIndex> entity_indices;

    UPROPERTY()
    TArray<FTransform> transforms;

    // Fighter spawning
    UPROPERTY(EditAnywhere, Category = "Ship")
    TObjectPtr<ATestCapitalShipFighters> fighters_actor{nullptr};
    UPROPERTY()
    FCountdownTimers spawn_timers;
    UPROPERTY()
    TArray<int32> ships_ready_to_spawn_fighters;

    // Teams
    UPROPERTY()
    TArray<ETestTeam> teams{};

    // Health
    UPROPERTY()
    TArray<int32> healths{};

    // Targets
    UPROPERTY()
    TArray<FGenerationIndex> target_entity_indices;

    UPROPERTY(EditAnywhere, Category = "Ship")
    bool debugging_shapes_enabled{false};
};
