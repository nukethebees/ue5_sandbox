#pragma once

#include <Sandbox/batch_game/SimulationClockInterface.h>
#include <Sandbox/batch_game/SpatialQueryHit.h>
#include <Sandbox/batch_game/test_entity_registry/EntityDeathInfo.h>
#include <Sandbox/batch_game/test_entity_registry/TestEntityRegistryData.h>
#include <Sandbox/batch_game/TestLasers.h>
#include <Sandbox/batch_game/TestStaticTurretsSoA.h>
#include <Sandbox/batch_game/TestTeam.h>
#include <Sandbox/logging/ActorLoggingConfig.h>
#include <Sandbox/utilities/DrawDebugConfig.h>
#include <SandboxNative/RegistryEntityHandle.h>

#include <SandboxCore/soa_array_mixin.h>
#include <SandboxCore/soa_rotators.h>
#include <SandboxCore/soa_vectors.h>
#include <SandboxCore/tick_countdown.h>

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include <utility>

#include "TestStaticTurrets.generated.h"

class UInstancedStaticMeshComponent;
class UPrimitiveComponent;

class ATestStaticTurretsProxy;
class UTestStaticTurretsConfig;
class ATestLasers;
struct FTestEntityRegistry;
class ATestBatchOrchestrator;
struct FTestStaticTurretsSpatialQueryAccess;

namespace ml {
struct FSpatialQueryManager;
}

namespace ml::test_static_turrets {
class PhaseInterface;
}

UCLASS()
class SANDBOX_API ATestStaticTurrets : public AActor {
    GENERATED_BODY()
    friend class ml::test_static_turrets::PhaseInterface;
  public:
    using Proxy = ATestStaticTurretsProxy;
    using RegistryEntityData = ml::entity_registry::EntityData;
    using EntityData = ml::test_static_turrets::EntityData;

    static constexpr bool is_world_space{false};
    static constexpr int32 n_custom_ismc_floats{3}; // RGB[3]

    ATestStaticTurrets();

    void bind_simulation_clock(ATestBatchOrchestrator const& orchestrator) noexcept;
    // Accessors
    auto get_num_instances() const noexcept -> int32;

    void set_actor_config(UTestStaticTurretsConfig* const new_config) noexcept {
        actor_config = new_config;
    }

    auto get_target_handles() const -> TConstArrayView<FRegistryEntityHandle>;

    auto get_entity_registry() const -> FTestEntityRegistry const* { return entity_registry; }
    void set_entity_registry(FTestEntityRegistry& reg) { entity_registry = &reg; }
    void set_spatial_query_manager(ml::FSpatialQueryManager const& manager) {
        spatial_query_manager = &manager;
    }

    auto get_laser_actor() const -> ATestLasers const* { return laser_actor; }
    void set_laser_actor(ATestLasers& new_ref) { laser_actor = &new_ref; }

    // Checks
    void validate_array_sizes() const;
    void validate_proxy_handles() const;
  protected:
    // Spawning
    void register_all_proxies_in_level();

    // Visuals
    void configure_ismc();

    // Entity data
    void prepare_entity_update_data();

    // Searchng
    void perform_search();
    void perform_search_on_slice(int32 slice_index,
                                 int32 n_turrets,
                                 int32 min_turrets_per_slice,
                                 float radius);

    // Attacking
    void fire_at_enemies();
    auto get_disengage_radius() const -> float;

    // Death handling
    void handle_dead_entities();
    void trigger_death_effects();

    // Misc
    void clear_tick_buffers();

    // Debugging
    void draw_debugging_shapes() const;

    UPROPERTY(EditAnywhere, Category = "Sandbox")
    TObjectPtr<UTestStaticTurretsConfig> actor_config{nullptr};
    ml::test_batch_orchestrator::SimulationClockInterface simulation_clock;

    // Entity Data
    FTestEntityRegistry* entity_registry{nullptr};
    ml::FSpatialQueryManager const* spatial_query_manager{nullptr};

    EntityData entities{};
    EntityDeathInfo entity_death_info;
    RegistryEntityData entity_update_data;

    // Visuals
    UPROPERTY()
    TObjectPtr<UInstancedStaticMeshComponent> instances;
    TArray<FTransform> ismc_transforms;

    // Searching
    UPROPERTY(EditAnywhere, Category = "Performance", meta = (ClampMin = "1", UIMin = "1"))
    int32 search_slice_size{64};

    int32 target_refresh_next_offset{0};

    // Firing
    UPROPERTY(EditAnywhere, Category = "Sandbox")
    TObjectPtr<ATestLasers> laser_actor{nullptr};

    TArray<int32> scratch_int_buffer;
    FVectors3f line_of_sight_start_locations;
    FVectors3f line_of_sight_end_locations;
    TArray<FRegistryEntityHandle> line_of_sight_hit_entity_handles;
    ml::test_lasers::SpawnRequests new_lasers;

    // Despawning
    UPROPERTY()
    TArray<int32> local_indices_to_remove;

    // Debugging / logging
    UPROPERTY(EditAnywhere)
    FDrawDebugConfig debug_drawer;
    UPROPERTY(EditAnywhere, Category = "Sandbox")
    bool draw_target_arrows_enabled{false};
    UPROPERTY(EditAnywhere, Category = "Sandbox")
    bool draw_debug_entity_info_enabled{false};
  private:
    void clear_runtime_state();
    void begin_play();
    void begin_tick();
    void update_timers(float const dt);
    void make_decisions();
    void queue_commands();
    void resolve_damage_events();
    void update_entity_registry();
    void sync_from_registry();
    void update_visual_data();
    void commit_visual_data();
    void end_tick();

    auto get_spatial_query_component() const -> UPrimitiveComponent const*;
    void resolve_hits(TConstArrayView<ml::FSpatialQueryHit> hits,
                      TArrayView<FRegistryEntityHandle> out_entity_handles) const;

    friend struct FTestStaticTurretsSpatialQueryAccess;
};

struct FTestStaticTurretsSpatialQueryAccess {
    ATestStaticTurrets const* actor{nullptr};

    auto get_spatial_query_component() const -> UPrimitiveComponent const* {
        return actor->get_spatial_query_component();
    }

    void resolve_hits(TConstArrayView<ml::FSpatialQueryHit> const hits,
                      TArrayView<FRegistryEntityHandle> const out_entity_handles) const {
        actor->resolve_hits(hits, out_entity_handles);
    }
};
