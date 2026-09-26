#pragma once
#include <ioj/sim/column_math.h>
#include <ioj/sim/entity_ledger.h>
#include <ioj/sim/lasers/sim.h>
#include <ioj/sim/profiling.h>
#include <ioj/sim/sim_clock.h>
#include <ioj/sim/sim_config.h>
#include <ioj/sim/spinner_entity_data.h>
#include <ioj/sim/system_read_views.h>

#include <sandbox/core/frame_memory_resource.h>

#include <cassert>
#include <cstdint>
#include <span>
#include <vector>

namespace ioj::sim {
struct LevelSim;
class LevelSpawnManager;
class EntityLedger;
}

namespace ioj::sim::spinners {
class PhaseInterface;

struct Sim {
    using EntityStorage = SingleAllocationSpinnerEntityData;

    Sim(SimClock const& clock, EntityLedger& ledger, lasers::Sim& laser_simulation) noexcept;
    Sim(Sim const&) = delete;
    Sim(Sim&&) = delete;
    auto operator=(Sim const&) -> Sim& = delete;
    auto operator=(Sim&&) -> Sim& = delete;

    /* **************************************** */
    // Configuration
    /* **************************************** */
    auto get_read_view() const -> SpinnerReadView { return {entities.get_const_view()}; }
    void set_config(SpinnerSimConfig const& new_config) noexcept;

    /* **************************************** */
    // Accessors
    /* **************************************** */
    auto get_num_instances() const noexcept -> std::int32_t;
    auto get_laser_simulation() const -> lasers::Sim const& { return laser_simulation; }

    /* **************************************** */
    // Checks
    /* **************************************** */
  private:
    /* **************************************** */
    // Sim phases
    /* **************************************** */
    void begin_play();
    void prepare_tick(float dt);
    void think(float dt);
    void apply_movement(ml::FrameScratch& scratch);
    void generate_fire_commands();
    void materialize_fire_commands(ml::FrameScratch& scratch);
    std::vector<std::int32_t> pending_fire_indices_;
    void finish_action();

    /* **************************************** */
    // Spawning
    /* **************************************** */
    template <VectorColumns Locations>
    auto spawn_instances(Locations const new_locations,
                         std::span<float const> const new_yaws,
                         std::span<std::int32_t const> const new_fire_point_indices)
        -> std::span<EntityUniqueId const> {
        SANDBOX_PROFILE_SCOPE("spinners::Sim::spawn_instances");
        assert(simulation_clock.permits_preparation_mutation());

        auto const n{new_locations.num()};

        assert(new_yaws.size() == static_cast<std::size_t>(n));
        assert(new_fire_point_indices.size() == static_cast<std::size_t>(n));

        entities.add_uninitialised(n);
        auto const appended{entities.right(n)};
        auto const locations{appended.view_locations()};
        auto const xs{locations.xs()};
        auto const ys{locations.ys()};
        auto const zs{locations.zs()};
        auto const new_xs{new_locations.xs()};
        auto const new_ys{new_locations.ys()};
        auto const new_zs{new_locations.zs()};
        auto const yaws{appended.yaws()};
        auto const laser_cooldowns{appended.laser_cooldowns()};
        auto const next_fire_point_indices{appended.next_fire_point_indices()};
        auto const entity_ids{appended.entity_ids()};

        for (std::int32_t i{}; i < n; ++i) {
            xs[i] = new_xs[i];
            ys[i] = new_ys[i];
            zs[i] = new_zs[i];
            yaws[i] = new_yaws[i];
            laser_cooldowns[i] = 0;
            next_fire_point_indices[i] = new_fire_point_indices[i];
        }

        entities.get_const_view().validate();

        for (std::int32_t i{0}; i < n; ++i) {
            entity_ids[i] = ledger_.record_spawn(EntityType::TubeSpinner, Team::White, true);
        }

        return entity_ids;
    }

    /* **************************************** */
    // Movement
    /* **************************************** */

    /* **************************************** */
    // Firing
    /* **************************************** */
    void fire_lasers();

    friend class PhaseInterface;
    friend struct sim::LevelSim;
    friend class sim::LevelSpawnManager;

    friend struct SpinnerSpawnTestAccess;

    float planned_yaw_delta_{};
    SpinnerSimConfig config{};
    std::int16_t cooldown_restart_ticks_{};
    std::int16_t cooldown_cleaner_{};
    SimClock const& simulation_clock;
    EntityLedger& ledger_;
    lasers::Sim& laser_simulation;
    EntityStorage entities{};
};
} // namespace ioj::sim::spinners
