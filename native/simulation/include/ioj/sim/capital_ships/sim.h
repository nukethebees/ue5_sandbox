#pragma once
#include <ioj/levels/team.h>
#include <ioj/settings/simulation_constraints.h>
#include <ioj/sim/capital_ships/capital_entity_data.h>
#include <ioj/sim/capital_ships/phase_inputs.h>
#include <ioj/sim/entities/entity_frame_changes.h>
#include <ioj/sim/entities/entity_id.h>
#include <ioj/sim/entities/entity_tables.h>
#include <ioj/sim/entities/entity_type.h>
#include <ioj/sim/levels/level_spawn_tables.h>

#include <cstdint>
#include <span>
#include <vector>

namespace ioj::sim {
struct SpawnContext;
class LevelSpawnManager;
}

namespace ioj::sim::capital_ships {
class InitialisationInterface;
class PreparationInterface;
class ResolutionInterface;
class ResolutionCommitInterface;
class SpawnInterface;

// Simulate capital ship lifecycles.
struct Sim {
    using EntityStorage = CapitalEntityData;

    Sim() = default;
    Sim(Sim const&) = delete;
    Sim(Sim&&) = delete;
    auto operator=(Sim const&) -> Sim& = delete;
    auto operator=(Sim&&) -> Sim& = delete;

    /* **************************************** */
    // Configuration
    /* **************************************** */
    auto get_entities() const -> EntityStorage::CompactConstView { return entities_.const_view(); }

    /* **************************************** */
    // Accessors
    /* **************************************** */
    auto num_instances() const noexcept -> std::uint32_t;

  private:
    /* **************************************** */
    // Sim phases
    /* **************************************** */
    void initialise(settings::PositiveHealth max_health);
    void update_entity_lookup_table(EntityTables& tables, SimClock const& clock);
    void prepare_tick(EntityTables& tables, SimClock const& clock);
    void resolve_damage_events(ResolutionInputs const inputs);
    void remove_entities(ResolutionCommitInputs const inputs);
    void finish_action();

    /* **************************************** */
    // Ship spawning
    /* **************************************** */
    auto register_ships(LevelCapitalSpawnEvents::CompactConstView spawn_data,
                        SpawnContext context,
                        EntityFrameChanges& changes) -> std::vector<EntityId>;
    void spawn_ships(LevelCapitalSpawnEvents::CompactConstView spawn_data);

    /* **************************************** */
    // Death handling
    /* **************************************** */
    void handle_dead_entities(ResolutionCommitInputs const inputs);

    friend class InitialisationInterface;
    friend class PreparationInterface;
    friend class ResolutionInterface;
    friend class ResolutionCommitInterface;
    friend class SpawnInterface;

    settings::PositiveHealth max_health_{Health{5000}};

    EntityStorage entities_{};
};
} // namespace ioj::sim::capital_ships
