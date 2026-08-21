#pragma once

#include <Sandbox/batch_game/ProxyEntityMap.h>
#include <Sandbox/batch_game/SpatialQueryHit.h>
#include <Sandbox/batch_game/test_entity_registry/EntityDeathInfo.h>
#include <Sandbox/batch_game/test_entity_registry/TestEntityRegistryData.h>
#include <Sandbox/batch_game/TestCapitalShipFighterOrderQueue.h>
#include <Sandbox/batch_game/TestCapitalShipFighters.h>
#include <Sandbox/batch_game/TestCapitalShipFightersCommandInterface.h>
#include <Sandbox/batch_game/TestCapitalShipFighterSpawnQueue.h>
#include <Sandbox/batch_game/TestCapitalShipsSoA.h>
#include <Sandbox/batch_game/TestTeam.h>
#include <Sandbox/utilities/DrawDebugConfig.h>
#include <Sandbox/utilities/IndexSpan.h>
#include <SandboxNative/RegistryEntityHandle.h>

#include <SandboxCore/countdown_timers.h>
#include <SandboxCore/generation_index.h>
#include <SandboxCore/multi_buffer.h>
#include <SandboxCore/soa_array_mixin.h>
#include <SandboxCore/soa_rotators.h>
#include <SandboxCore/soa_vectors.h>

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include <optional>

#include "TestCapitalShips.generated.h"

class UInstancedStaticMeshComponent;
class UBoxComponent;
class UPrimitiveComponent;

class UTestCapitalShipsConfig;
class ATestCapitalShipProxy;
struct FTestEntityRegistry;
class ADelayedNiagaraSpawner;
class UTestTeamVisualData;
struct FTestCapitalShipsSpatialQueryAccess;

namespace ml {
struct FSpatialQueryManager;
}

namespace ml::test_capital_ships {
class PhaseInterface;
}

UCLASS()
class SANDBOX_API ATestCapitalShips : public AActor {
    GENERATED_BODY()
    friend class ml::test_capital_ships::PhaseInterface;
  public:
    using RegistryEntityData = ml::entity_registry::EntityData;

    using SpawnData = ml::test_capital_ships::SpawnData;
    using EntityTickData = ml::test_capital_ships::EntityTickData;
    using EntityData = ml::test_capital_ships::EntityData;
    using FighterReassignment = ml::test_capital_ships::FighterReassignment;
    using EntityBuffers = ml::MultiBuffer<EntityTickData, 2>;
    using Proxy = ATestCapitalShipProxy;

    static constexpr bool is_world_space{false};
    static constexpr int32 n_custom_ismc_floats{3}; // RGB[3]

    ATestCapitalShips();

    // Accessors
    auto get_num_instances() const -> int32;
    auto is_valid(FRegistryEntityHandle const index) const -> bool;
    auto get_niagara_spawner() const -> ADelayedNiagaraSpawner const*;
    void set_niagara_spawner(ADelayedNiagaraSpawner& spawner);

    void set_actor_config(UTestCapitalShipsConfig* const new_config) noexcept {
        actor_config = new_config;
    }

    auto get_entity_registry() const -> FTestEntityRegistry const* { return entity_registry; }
    void set_entity_registry(FTestEntityRegistry& reg) { entity_registry = &reg; }
    void set_spatial_query_manager(ml::FSpatialQueryManager const& manager) {
        spatial_query_manager = &manager;
    }

    inline void bind_fighters(ATestCapitalShipFighters& fighters) {
        fighters_interface.bind(fighters);
    }

    auto get_handle(int32 i) const -> FRegistryEntityHandle { return entities.handles[i]; }

    auto get_fighter_spawn_slots() const noexcept -> int32;
    auto get_fighters_spawned() const noexcept -> int32 { return fighters_spawned; }
    auto get_fighter_handles() const noexcept -> TConstArrayView<FRegistryEntityHandle> {
        return fighter_handles;
    }
    auto get_capital_fighter_handle_spans() const noexcept -> auto const& {
        return entities.capital_fighter_handle_spans;
    }
    auto get_capital_fighter_handle_span(int32 const i) const noexcept -> FIndexSpan {
        return entities.capital_fighter_handle_spans[i];
    }
    auto get_fighter_handles(int32 const i) const noexcept
        -> TConstArrayView<FRegistryEntityHandle> {
        return get_fighter_handles(entities.capital_fighter_handle_spans[i]);
    }
    auto get_fighter_handles(FIndexSpan const span) const noexcept
        -> TConstArrayView<FRegistryEntityHandle> {
        return TConstArrayView<FRegistryEntityHandle>{fighter_handles}.Slice(span.offset,
                                                                             span.count);
    }
    auto get_target_handle(int32 const i) const noexcept -> FRegistryEntityHandle {
        return entities.target_handles[i];
    }
    auto get_target_handles() const noexcept -> TConstArrayView<FRegistryEntityHandle> {
        return entities.target_handles;
    }

    auto get_team(int32 const i) const noexcept -> ETestTeam { return entities.teams[i]; }
    auto get_team(FRegistryEntityHandle handle) const noexcept -> ETestTeam;

    auto get_health(FRegistryEntityHandle handle) const noexcept -> int32;

    auto find_first_index_on_team(ETestTeam team) const noexcept -> std::optional<int32>;
    auto find_first_handle_on_team(ETestTeam team) const noexcept
        -> std::optional<FRegistryEntityHandle> {
        auto const result{find_first_index_on_team(team)};
        if (result) {
            return entities.handles[*result];
        }
        return std::nullopt;
    }

    // Checks
    void validate_array_sizes() const;
    void validate_proxy_handles() const;
  private:
    void clear_runtime_state();
    void begin_play();
    void begin_tick();
    void update_timers(float const dt);
    void make_decisions();
    void resolve_damage_events();
    void update_entity_registry();
    void sync_from_registry();
    void update_visual_data();
    void commit_visual_data();
    void end_tick();

    // Ship spawning
    void register_all_proxies_in_level();
    void bind_proxy_entities(FProxyEntityMap const& proxy_entities);
    void spawn_ships(SpawnData const& spawn_data);

    // Entity data
    void prepare_entity_update_data();

    // Fighter spawning
    void queue_fighter_spawns();
    void refresh_fighter_handles();

    // Orders
    void queue_fighter_orders();

    // Visuals
    void configure_ismc();

    // Death handling
    void handle_dead_entities();
    void reassign_fighter_handles_of_dying_capital();
    void trigger_death_effects();

    // Debugging
    void draw_debugging_shapes() const;

    // Misc
    void clear_tick_buffers();

    auto get_spatial_query_component() const -> UPrimitiveComponent const*;
    void resolve_hits(TConstArrayView<ml::FSpatialQueryHit> hits,
                      TArrayView<FRegistryEntityHandle> out_entity_handles) const;
    void visual_log_state() const;

    // Config / context
    UPROPERTY(EditAnywhere, Category = "Sandbox", meta = (AllowPrivateAccess))
    TObjectPtr<UTestCapitalShipsConfig> actor_config{nullptr};
    FTestEntityRegistry* entity_registry{nullptr};
    ml::FSpatialQueryManager const* spatial_query_manager{nullptr};

    // Visuals
    UPROPERTY(EditDefaultsOnly, Category = "Sandbox", meta = (AllowPrivateAccess))
    TObjectPtr<UInstancedStaticMeshComponent> instances;
    UPROPERTY(EditAnywhere, Category = "Sandbox", meta = (AllowPrivateAccess))
    TObjectPtr<ADelayedNiagaraSpawner> niagara_spawner{nullptr};

    // Entity data
    EntityData entities{};
    EntityBuffers tick_buffers{};

    TArray<int32> local_indices_to_remove;
    EntityDeathInfo entity_death_info;
    RegistryEntityData entity_update_data;

    // Fighter spawning
    ml::test_capital_ship_fighters::CommandInterface fighters_interface;

    TArray<FRegistryEntityHandle> fighter_handles;
    TArray<FRegistryEntityHandle> fighter_handles_scratch;
    FighterReassignment fighter_reassignment_queue;

    int32 fighters_spawned{0};

    // Targets
    TArray<int32> indices_without_targets_buffer;

    // Fighter orders
    TestCapitalShipFighterOrderQueue fighter_order_queue{};

    // Debugging
    UPROPERTY(EditAnywhere, Category = "Sandbox", meta = (AllowPrivateAccess))
    FDrawDebugConfig debug_drawer;
    UPROPERTY(EditAnywhere, Category = "Sandbox", meta = (AllowPrivateAccess))
    bool debugging_shapes_enabled{false};

    friend struct FTestCapitalShipsSpatialQueryAccess;
};

struct FTestCapitalShipsSpatialQueryAccess {
    ATestCapitalShips const* actor{nullptr};

    auto get_spatial_query_component() const -> UPrimitiveComponent const* {
        return actor->get_spatial_query_component();
    }

    void resolve_hits(TConstArrayView<ml::FSpatialQueryHit> const hits,
                      TArrayView<FRegistryEntityHandle> const out_entity_handles) const {
        actor->resolve_hits(hits, out_entity_handles);
    }
};
