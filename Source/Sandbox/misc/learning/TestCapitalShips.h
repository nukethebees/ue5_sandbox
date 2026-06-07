#pragma once

#include "Sandbox/misc/learning/TestEntityOwnerId.h"
#include "Sandbox/misc/learning/TestTeam.h"
#include "Sandbox/utilities/DrawDebugConfig.h"

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

    static constexpr bool is_world_space{false};

    using Proxy = ATestCapitalShipProxy;

    ATestCapitalShips();

    void clear_runtime_state();
    void begin_play();
    void tick(float const dt);
    void update_entity_registry();
    void resolve_damage_targets();
    void sync_from_registry();
    void update_visuals();
    void end_frame();

    // Getters
    auto get_num_instances() const -> int32;
    auto get_location(FGenerationIndex const index) const -> FVector;
    auto is_valid(FGenerationIndex const index) const -> bool;
    auto get_entity_from_hit_slot(int32 const hit_slot) const -> FGenerationIndex;

    // Setters
    void set_owner_id(TestEntityOwnerId const new_owner_id);
    auto get_owner_id() const -> TestEntityOwnerId;

    // Checks
    void validate_array_sizes() const;
  protected:
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

    // Debugging
    bool array_sizes_consistent() const;
    void draw_debugging_shapes() const;

    // Config / context
    UPROPERTY(EditAnywhere, Category = "Ship")
    TObjectPtr<UTestCapitalShipsConfig> actor_config{nullptr};
    UPROPERTY(EditAnywhere, Category = "Ship")
    TObjectPtr<ATestEntityRegistry> entity_registry{nullptr};

    UPROPERTY(EditDefaultsOnly)
    TObjectPtr<UInstancedStaticMeshComponent> instances;

    // Entity data
    UPROPERTY()
    TArray<FGenerationIndex> entity_indices;
    UPROPERTY()
    TArray<int32> local_indices_to_remove;
    TestEntityOwnerId owner_id{};

    UPROPERTY()
    TArray<FTransform> transforms;

    // Fighter spawning
    UPROPERTY(EditAnywhere, Category = "Ship")
    TObjectPtr<ATestCapitalShipFighters> fighters_actor{nullptr};
    UPROPERTY()
    FCountdownTimers spawn_timers;
    UPROPERTY()
    TArray<int32> ships_ready_to_spawn_fighters_buffer;

    // Teams
    UPROPERTY()
    TArray<ETestTeam> teams{};

    // Health
    UPROPERTY()
    TArray<int32> healths{};

    // Targets
    UPROPERTY()
    TArray<FGenerationIndex> target_entity_indices;

    // Debugging
    UPROPERTY(EditAnywhere)
    FDrawDebugConfig debug_drawer;
    UPROPERTY(EditAnywhere, Category = "Ship")
    bool debugging_shapes_enabled{false};
};
