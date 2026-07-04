#pragma once

#include <Sandbox/batch_game/test_entity_registry/EntityDeathInfo.h>
#include <Sandbox/batch_game/test_entity_registry/RegistryEntityHandle.h>
#include <Sandbox/batch_game/test_entity_registry/TestEntityOwnerId.h>
#include <Sandbox/batch_game/test_entity_registry/TestEntityRegistryData.h>
#include <Sandbox/batch_game/TestTeam.h>
#include <Sandbox/utilities/DrawDebugConfig.h>

#include <SandboxCore/countdown_timers.h>
#include <SandboxCore/generation_index.h>
#include <SandboxCore/soa_rotators.h>
#include <SandboxCore/soa_vectors.h>

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "TestCapitalShips.generated.h"

class UInstancedStaticMeshComponent;
class UBoxComponent;

class UTestCapitalShipsConfig;
class ATestCapitalShipProxy;
class ATestCapitalShipFighters;
class ATestEntityRegistry;
class ADelayedNiagaraSpawner;
class UTestTeamVisualData;

UCLASS()
class ATestCapitalShips : public AActor {
  public:
    GENERATED_BODY()

    static constexpr bool is_world_space{false};
    static constexpr int32 n_custom_ismc_floats{3}; // RGB[3]

    using Proxy = ATestCapitalShipProxy;

    ATestCapitalShips();

    void clear_runtime_state();
    void begin_play();

    void begin_tick();
    void commit_spawns();
    void tick(float const dt);
    void resolve_hit_events();
    void update_entity_registry();
    void sync_from_registry();
    void update_visuals();
    void end_tick();

    // Accessors
    auto get_num_instances() const -> int32;
    auto is_valid(FRegistryEntityHandle const index) const -> bool;
    auto get_entity_from_hit_slot(int32 const hit_slot) const -> FRegistryEntityHandle;

    void set_owner_id(TestEntityOwnerId const new_owner_id);
    auto get_owner_id() const -> TestEntityOwnerId;

    auto get_niagara_spawner() const -> ADelayedNiagaraSpawner const*;
    void set_niagara_spawner(ADelayedNiagaraSpawner& spawner);

    auto get_entity_registry() const -> ATestEntityRegistry const* { return entity_registry; }
    void set_entity_registry(ATestEntityRegistry& reg) { entity_registry = &reg; }

    // Checks
    void validate_array_sizes() const;
  protected:
    // Ship spawning
    void register_all_proxies_in_level();
    void spawn_ships(TConstArrayView<FRegistryEntityHandle> const new_indices,
                     FVectors3f::ConstView const new_locations,
                     FRotatorsf::ConstView const new_rotations,
                     TConstArrayView<ETestTeam> const new_teams,
                     TConstArrayView<FRegistryEntityHandle> const new_target_indices);

    // Entity data
    void prepare_entity_update_data();

    // Fighter spawning
    void handle_fighter_spawning();

    // Visuals
    void configure_ismc();

    // Death handling
    void handle_dead_entities();
    void trigger_death_effects();

    // Debugging
    void draw_debugging_shapes() const;

    // Misc
    void clear_tick_buffers();

    // Config / context
    UPROPERTY(EditAnywhere, Category = "Sandbox")
    TObjectPtr<UTestCapitalShipsConfig> actor_config{nullptr};
    UPROPERTY(EditAnywhere, Category = "Sandbox")
    TObjectPtr<ATestEntityRegistry> entity_registry{nullptr};

    // Visuals
    UPROPERTY(EditDefaultsOnly, Category = "Sandbox")
    TObjectPtr<UInstancedStaticMeshComponent> instances;
    UPROPERTY(EditAnywhere, Category = "Sandbox")
    TObjectPtr<ADelayedNiagaraSpawner> niagara_spawner{nullptr};

    // Entity data
    TestEntityOwnerId owner_id{};
    TArray<FRegistryEntityHandle> entity_handles;
    UPROPERTY()
    TArray<int32> local_indices_to_remove;
    EntityDeathInfo entity_death_info;
    FTestEntityRegistryEntityData entity_update_data;

    // Transform
    UPROPERTY()
    FVectors3f locations;
    UPROPERTY()
    FRotatorsf rotations;

    // Fighter spawning
    UPROPERTY(EditAnywhere, Category = "Sandbox")
    TObjectPtr<ATestCapitalShipFighters> fighters_actor{nullptr};
    UPROPERTY()
    FCountdownTimers spawn_timers;
    UPROPERTY()
    TArray<int32> ships_ready_to_spawn_fighters_buffer;

    FVectors3f new_fighter_locations;
    FRotatorsf new_fighter_rotations;
    TArray<ETestTeam> new_fighter_teams;
    TArray<FRegistryEntityHandle> new_fighter_targets;

    // Teams
    UPROPERTY()
    TArray<ETestTeam> teams{};

    // Health
    UPROPERTY()
    TArray<int32> healths{};

    // Targets
    TArray<FRegistryEntityHandle> target_entity_indices;

    // Debugging
    UPROPERTY(EditAnywhere, Category = "Sandbox")
    FDrawDebugConfig debug_drawer;
    UPROPERTY(EditAnywhere, Category = "Sandbox")
    bool debugging_shapes_enabled{false};
};
