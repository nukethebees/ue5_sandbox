#pragma once
#include <SpaceGameSimulation/simulation/SystemReadViews.h>

#include <SpaceGameSimulation/simulation/LevelSimulationConfig.h>

#include <SpaceGameSimulation/combat/lasers/TestLasersSimulation.h>
#include <SpaceGameSimulation/defences/turrets/TestStaticTurretsSoA.h>
#include <SpaceGameSimulation/entities/EntityDeathInfo.h>
#include <SpaceGameSimulation/entities/TestEntityRegistryData.h>
#include <SpaceGameSimulation/simulation/SimulationClockInterface.h>

#include <CoreMinimal.h>

#include <memory_resource>

struct FLevelSimulation;
struct FTurretSimulationConfig;
struct FTestEntityRegistry;

namespace ml {
class FLevelSpawnManager;
struct FSpatialQueryManager;
}

namespace ml::test_static_turrets {
class PhaseInterface;

#if WITH_DEV_AUTOMATION_TESTS
enum class EScratchAllocationMode : uint8 { Persistent, DirectRoot, LocalMonotonic };
#endif

struct SPACEGAMESIMULATION_API Simulation {
    using RegistryEntityData = ml::entity_registry::EntityData;
    using EntityData = ml::test_static_turrets::EntityData;
    using SpawnData = ml::test_static_turrets::SpawnData;

    Simulation(FSimulationClock const& clock,
               FTestEntityRegistry& entity_registry,
               FSpatialQueryManager const& spatial_query_manager,
               ml::test_lasers::Simulation& laser_simulation,
               std::pmr::memory_resource& frame_memory_resource) noexcept;
    Simulation(Simulation const&) = delete;
    Simulation(Simulation&&) = delete;
    auto operator=(Simulation const&) -> Simulation& = delete;
    auto operator=(Simulation&&) -> Simulation& = delete;

    /* **************************************** */
    // Configuration
    /* **************************************** */
    auto get_read_view() const -> FTurretReadView {
        return {entities.get_const_view(), &entity_registry, frame_changes_, death_locations_};
    }
    void reset_frame_output() {
        frame_changes_.Reset();
        death_locations_.Reset();
    }
    void set_config(FTurretSimulationConfig const& new_config) noexcept;
#if WITH_DEV_AUTOMATION_TESTS
    void set_scratch_allocation_mode(EScratchAllocationMode const mode) noexcept {
        scratch_allocation_mode_ = mode;
    }
    auto get_persistent_scratch_allocated_bytes() const noexcept -> SIZE_T {
        return scratch_int_buffer_.GetAllocatedSize() +
               line_of_sight_hit_entity_handles_.GetAllocatedSize();
    }
#endif

    /* **************************************** */
    // Accessors
    /* **************************************** */
    auto get_num_instances() const noexcept -> int32;
    auto get_target_handles() const -> TConstArrayView<FRegistryEntityHandle>;
    auto get_entity_registry() const -> FTestEntityRegistry const& { return entity_registry; }
    auto get_laser_simulation() const -> ml::test_lasers::Simulation const& {
        return laser_simulation;
    }

    /* **************************************** */
    // Checks
    /* **************************************** */
    void validate_array_sizes() const;
    void validate_entity_handles() const;

    float entity_radius{0.f};
    int32 search_slice_size{64};
  private:
    /* **************************************** */
    // Simulation phases
    /* **************************************** */
    void begin_play();
    void begin_tick();
    void update_timers(float dt);
    void make_decisions();
    void queue_commands();
    void resolve_damage_events();
    void update_entity_registry();
    void sync_from_registry();
    void end_tick();

    /* **************************************** */
    // Spawning
    /* **************************************** */
    auto register_turrets(SpawnDataConstView spawn_data, FRotatorsf::ConstView rotations)
        -> TArray<FRegistryEntityHandle>;

    /* **************************************** */
    // Entity data
    /* **************************************** */
    void prepare_entity_update_data();

    /* **************************************** */
    // Searching
    /* **************************************** */
    void perform_search();
    void perform_search_on_slice(int32 job_index,
                                 int32 n_turrets,
                                 int32 turrets_per_job,
                                 float radius);

    /* **************************************** */
    // Attacking
    /* **************************************** */
    void fire_at_enemies();
    template <typename CandidateIndices, typename HitEntityHandles>
    void fire_at_enemies_with_scratch(CandidateIndices& candidate_indices,
                                      HitEntityHandles& hit_entity_handles);
    auto get_disengage_radius() const -> float;

    /* **************************************** */
    // Death handling
    /* **************************************** */
    void handle_dead_entities();

    /* **************************************** */
    // Misc
    /* **************************************** */
    void clear_tick_buffers();

    friend class PhaseInterface;
    friend struct ::FLevelSimulation;

    friend class ::ml::FLevelSpawnManager;

    FTurretSimulationConfig config{};
    ml::test_batch_orchestrator::SimulationClockInterface simulation_clock;
    FTestEntityRegistry& entity_registry;
    FSpatialQueryManager const& spatial_query_manager;
    ml::test_lasers::Simulation& laser_simulation;
    std::pmr::memory_resource& frame_memory_resource;
    EntityData entities{};
    EntityDeathInfo entity_death_info;
    TArray<FEntityFrameChange> frame_changes_;
    TArray<FVector3f> death_locations_;
    RegistryEntityData entity_update_data;
    int32 target_refresh_next_offset{0};

    FVectors3f line_of_sight_start_locations;
    FVectors3f line_of_sight_end_locations;
#if WITH_DEV_AUTOMATION_TESTS
    EScratchAllocationMode scratch_allocation_mode_{EScratchAllocationMode::DirectRoot};
    TArray<int32> scratch_int_buffer_;
    TArray<FRegistryEntityHandle> line_of_sight_hit_entity_handles_;
#endif
    ml::test_lasers::SpawnRequests new_lasers;
    TArray<int32> local_indices_to_remove;
};
} // namespace ml::test_static_turrets
