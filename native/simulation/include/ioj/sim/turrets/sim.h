#pragma once
#include <cstdint>
#include <ioj/sim/system_read_views.h>
#include <ioj/sim/turrets/spawn_data.h>
#include <optional>
#include <span>
#include <vector>

#include <ioj/sim/sim_config.h>

#include <ioj/sim/entity_death_info.h>
#include <ioj/sim/lasers/sim.h>
#include <ioj/sim/registry_entity_data.h>
#include <ioj/sim/sim_clock.h>
#include <ioj/sim/turret_entity_data.h>

#include <memory_resource>

namespace ioj::sim {
struct LevelSim;
struct TurretSimConfig;
struct EntityRegistry;
class LevelSpawnManager;
struct SpatialQueryManager;
}

namespace ioj::sim::turrets {
class PhaseInterface;

struct Sim {
    using RegistryEntityData = ioj::sim::RegistryEntityData;
    using EntityData = ioj::sim::TurretEntityData;
    using SpawnData = ioj::sim::turrets::SpawnData;

    Sim(SimClock const& clock,
        EntityRegistry& entity_registry,
        SpatialQueryManager const& spatial_query_manager,
        ioj::sim::lasers::Sim& laser_simulation,
        std::pmr::memory_resource& frame_memory_resource) noexcept;
    Sim(Sim const&) = delete;
    Sim(Sim&&) = delete;
    auto operator=(Sim const&) -> Sim& = delete;
    auto operator=(Sim&&) -> Sim& = delete;

    /* **************************************** */
    // Configuration
    /* **************************************** */
    auto get_read_view() const -> TurretReadView {
        return {entities.get_const_view(), &entity_registry, frame_changes_, death_locations_};
    }
    void reset_frame_output() {
        frame_changes_.clear();
        death_locations_.clear();
    }
    void set_config(TurretSimConfig const& new_config) noexcept;

    /* **************************************** */
    // Accessors
    /* **************************************** */
    auto get_num_instances() const noexcept -> std::int32_t;
    auto get_target_handles() const -> std::span<RegistryEntityHandle const>;
    auto get_entity_registry() const -> EntityRegistry const& { return entity_registry; }
    auto get_laser_simulation() const -> ioj::sim::lasers::Sim const& { return laser_simulation; }

    /* **************************************** */
    // Checks
    /* **************************************** */
    void validate_array_sizes() const;
    void validate_entity_handles() const;

    std::int32_t search_slice_size{64};
  private:
    /* **************************************** */
    // Sim phases
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
    auto register_turrets(SpawnDataConstView spawn_data, ioj::sim::Rotators3fConstView rotations)
        -> std::vector<RegistryEntityHandle>;

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
    friend struct ::ioj::sim::LevelSim;

    friend class ::ioj::sim::LevelSpawnManager;

    TurretSimConfig config{};
    SimClock const& simulation_clock;
    EntityRegistry& entity_registry;
    SpatialQueryManager const& spatial_query_manager;
    ioj::sim::lasers::Sim& laser_simulation;
    std::pmr::memory_resource& frame_memory_resource;
    EntityData entities{};
    EntityDeathInfo entity_death_info;
    std::vector<EntityFrameChange> frame_changes_;
    std::vector<ioj::sim::Vector3f> death_locations_;
    RegistryEntityData entity_update_data;
    std::int32_t target_refresh_next_offset{0};
    std::int16_t cooldown_restart_ticks_{};
    std::int16_t cooldown_cleaner_{};

    std::vector<std::int32_t> local_indices_to_remove;
};
} // namespace ioj::sim::turrets
