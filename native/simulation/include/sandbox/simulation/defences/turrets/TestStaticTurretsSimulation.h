#pragma once
#include <cstdint>
#include <optional>
#include <sandbox/simulation/defences/turrets/TestStaticTurretsSpawnData.h>
#include <sandbox/simulation/simulation/SystemReadViews.h>
#include <span>
#include <vector>

#include <sandbox/simulation/simulation/LevelSimulationConfig.h>

#include <sandbox/simulation/combat/lasers/TestLasersSimulation.h>
#include <sandbox/simulation/entity_death_info.h>
#include <sandbox/simulation/registry_entity_data.h>
#include <sandbox/simulation/simulation/SimulationClock.h>
#include <sandbox/simulation/turret_entity_data.h>

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

struct Simulation {
    using RegistryEntityData = ml::simulation::RegistryEntityData;
    using EntityData = ml::simulation::TurretEntityData;
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
        frame_changes_.clear();
        death_locations_.clear();
    }
    void set_config(FTurretSimulationConfig const& new_config) noexcept;

    /* **************************************** */
    // Accessors
    /* **************************************** */
    auto get_num_instances() const noexcept -> std::int32_t;
    auto get_target_handles() const -> std::span<FRegistryEntityHandle const>;
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
    std::int32_t search_slice_size{64};
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
    auto register_turrets(SpawnDataConstView spawn_data,
                          ml::simulation::Rotators3fConstView rotations)
        -> std::vector<FRegistryEntityHandle>;

    /* **************************************** */
    // Entity data
    /* **************************************** */
    void prepare_entity_update_data();

    /* **************************************** */
    // Searching
    /* **************************************** */
    void perform_search();
    void perform_search_on_slice(std::int32_t job_index,
                                 std::int32_t n_turrets,
                                 std::int32_t turrets_per_job,
                                 float radius);

    /* **************************************** */
    // Attacking
    /* **************************************** */
    void fire_at_enemies();
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
    FSimulationClock const& simulation_clock;
    FTestEntityRegistry& entity_registry;
    FSpatialQueryManager const& spatial_query_manager;
    ml::test_lasers::Simulation& laser_simulation;
    std::pmr::memory_resource& frame_memory_resource;
    EntityData entities{};
    EntityDeathInfo entity_death_info;
    std::vector<FEntityFrameChange> frame_changes_;
    std::vector<ml::simulation::Vector3f> death_locations_;
    RegistryEntityData entity_update_data;
    std::int32_t target_refresh_next_offset{0};
    std::int16_t cooldown_restart_ticks_{};
    std::int16_t cooldown_cleaner_{};

    std::vector<std::int32_t> local_indices_to_remove;
};
} // namespace ml::test_static_turrets
