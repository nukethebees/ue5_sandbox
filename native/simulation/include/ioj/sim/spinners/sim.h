#pragma once
#include <ioj/column_math.h>
#include <ioj/frame_memory_resource.h>
#include <ioj/sim/entities/entity_tables.h>
#include <ioj/sim/profiling.h>
#include <ioj/sim/spinners/phase_inputs.h>
#include <ioj/sim/spinners/spinner_entity_data.h>
#include <ioj/sim/spinners/spinner_sim_config.h>
#include <ioj/vectors3f.h>

#include <cassert>
#include <cstdint>
#include <span>
#include <vector>

namespace ioj::sim {
class LevelSpawnManager;
struct SpawnContext;
struct SimClock;
}

namespace ioj::sim::spinners {
class PhaseInterface;
class SpawnInterface;

struct Sim {
    using EntityStorage = SpinnerEntityData;

    explicit Sim(EntityTables& entity_tables) noexcept;
    Sim(Sim const&) = delete;
    Sim(Sim&&) = delete;
    auto operator=(Sim const&) -> Sim& = delete;
    auto operator=(Sim&&) -> Sim& = delete;

    /* **************************************** */
    // Configuration
    /* **************************************** */
    auto get_entities() const -> EntityStorage::ConstView { return entities.const_view(); }

    /* **************************************** */
    // Accessors
    /* **************************************** */
    auto num_instances() const noexcept -> std::uint32_t;

  private:
    /* **************************************** */
    // Sim phases
    /* **************************************** */
    void initialise(SpinnerSimConfig const& new_config, SimClock const& clock);
    void update_entity_lookup_table();
    void prepare_tick(float dt);
    void think(float dt);
    void apply_movement(ActionInputs const inputs);
    void generate_fire_commands(ThinkingInputs const inputs);
    void materialize_fire_commands(ActionInputs const inputs);

    /* **************************************** */
    // Spawning
    /* **************************************** */
    auto spawn_instances(vectors3f::ResolvedConstView const new_locations,
                         std::span<float const> const new_yaws,
                         std::span<std::int32_t const> const new_fire_point_indices,
                         SpawnContext context) -> std::span<EntityId const>;

    /* **************************************** */
    // Firing
    /* **************************************** */
    void fire_lasers(ThinkingInputs const inputs);

    friend class PhaseInterface;
    friend class SpawnInterface;
    friend struct SpinnerSpawnTestAccess;

    float planned_yaw_delta_{};
    SpinnerSimConfig config{};
    NonNegative<std::int16_t> cooldown_restart_ticks_{};
    std::int16_t cooldown_cleaner_{};
    EntityTables& entity_tables_;
    EntityStorage entities{};
};
} // namespace ioj::sim::spinners
