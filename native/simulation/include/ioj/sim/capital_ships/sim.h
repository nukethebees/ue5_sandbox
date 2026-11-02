#pragma once
#include <ioj/levels/team.h>
#include <ioj/settings/simulation_constraints.h>
#include <ioj/sim/capital_ships/capital_entity_data.h>
#include <ioj/sim/capital_ships/phase_inputs.h>
#include <ioj/sim/entities/entity_frame_change.h>
#include <ioj/sim/entities/entity_id.h>
#include <ioj/sim/entities/entity_tables.h>
#include <ioj/sim/entities/entity_type.h>
#include <ioj/sim/levels/level_capital_spawn_events.h>

#include <cstdint>
#include <span>
#include <vector>

namespace ioj::sim {
struct SpawnContext;
class LevelSpawnManager;
}

namespace ioj::sim::capital_ships {
class PhaseInterface;

struct Sim {
    using EntityStorage = CapitalEntityData;

    explicit Sim(EntityTables& entity_tables);
    Sim(Sim const&) = delete;
    Sim(Sim&&) = delete;
    auto operator=(Sim const&) -> Sim& = delete;
    auto operator=(Sim&&) -> Sim& = delete;

    /* **************************************** */
    // Configuration
    /* **************************************** */
    auto get_entities() const -> EntityStorage::ConstView { return entities.const_view(); }
    auto healths() const -> HealthConstView {
        return entity_tables_.health.const_view<EntityType::CapitalShip>();
    }
    auto frame_changes() const -> std::span<EntityFrameChange const> { return frame_changes_; }
    void reset_frame_output() { frame_changes_.clear(); }

    /* **************************************** */
    // Accessors
    /* **************************************** */
    auto num_instances() const noexcept -> std::uint32_t;

  private:
    /* **************************************** */
    // Sim phases
    /* **************************************** */
    void initialise(settings::PositiveHealth max_health);
    void update_entity_lookup_table();
    void prepare_tick();
    void resolve_damage_events(ResolutionInputs const inputs);
    void publish_deaths(ResolutionInputs const inputs);
    void remove_components(ResolutionCommitInputs const inputs);
    void remove_entities(ResolutionCommitInputs const inputs);
    void finish_action();

    /* **************************************** */
    // Ship spawning
    /* **************************************** */
    auto register_ships(LevelCapitalSpawnEvents::ConstView spawn_data, SpawnContext context)
        -> std::vector<EntityId>;
    void spawn_ships(LevelCapitalSpawnEvents::ConstView spawn_data);

    /* **************************************** */
    // Death handling
    /* **************************************** */
    void handle_dead_entities(ResolutionCommitInputs const inputs);

    friend class PhaseInterface;

    friend class sim::LevelSpawnManager;

    settings::PositiveHealth max_health_{Health{5000}};
    EntityTables& entity_tables_;

    EntityStorage entities{};
    std::vector<EntityFrameChange> frame_changes_;
};
} // namespace ioj::sim::capital_ships
