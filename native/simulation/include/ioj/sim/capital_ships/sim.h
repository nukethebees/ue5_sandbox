#pragma once
#include <ioj/levels/team.h>
#include <ioj/settings/simulation_constraints.h>
#include <ioj/sim/capital_ships/capital_entity_data.h>
#include <ioj/sim/capital_ships/phase_inputs.h>
#include <ioj/sim/entities/entity_death_info.h>
#include <ioj/sim/entities/entity_frame_change.h>
#include <ioj/sim/entities/entity_id.h>
#include <ioj/sim/entities/entity_tables.h>
#include <ioj/sim/entities/entity_type.h>
#include <ioj/sim/levels/level_capital_spawn_events.h>

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace ioj::sim {
struct LevelSim;
struct LevelSimTestAccess;
class EntityLedger;
class LevelSpawnManager;
}

namespace ioj::sim::capital_ships {
class PhaseInterface;

struct Sim {
    using EntityStorage = CapitalEntityData;

    Sim(EntityLedger& ledger, EntityTables& entity_tables);
    Sim(Sim const&) = delete;
    Sim(Sim&&) = delete;
    auto operator=(Sim const&) -> Sim& = delete;
    auto operator=(Sim&&) -> Sim& = delete;

    /* **************************************** */
    // Configuration
    /* **************************************** */
    auto get_entities() const -> EntityStorage::ConstView { return entities.get_const_view(); }
    auto get_healths() const -> HealthConstView {
        return entity_tables_.health.get_const_view<EntityType::CapitalShip>();
    }
    auto get_frame_changes() const -> std::span<EntityFrameChange const> { return frame_changes_; }
    void reset_frame_output() { frame_changes_.clear(); }

    /* **************************************** */
    // Accessors
    /* **************************************** */
    auto get_num_instances() const noexcept -> std::uint32_t;
    auto is_valid(EntityId id) const noexcept -> bool;
    auto get_id(std::uint32_t index) const -> EntityId {
        return entities.get_const_view().entity_ids()[index];
    }
    auto get_team(std::uint32_t index) const noexcept -> Team {
        return entities.get_const_view().teams()[index];
    }
    auto get_team(EntityId id) const noexcept -> Team;
    auto get_health(EntityId id) const noexcept -> Health;
    auto find_first_index_on_team(Team team) const noexcept -> std::optional<std::uint32_t>;
    auto find_first_id_on_team(Team team) const noexcept -> std::optional<EntityId>;

  private:
    /* **************************************** */
    // Sim phases
    /* **************************************** */
    void initialise(settings::PositiveHealth max_health);
    void update_entity_lookup_table();
    void prepare_tick();
    void resolve_damage_events(ResolutionInputs const inputs);
    void publish_deaths(ResolutionInputs const inputs);
    void remove_components();
    void remove_entities();
    void finish_action();

    /* **************************************** */
    // Ship spawning
    /* **************************************** */
    auto register_ships(LevelCapitalSpawnEvents::ConstView spawn_data) -> std::vector<EntityId>;
    void spawn_ships(LevelCapitalSpawnEvents::ConstView spawn_data);

    /* **************************************** */
    // Death handling
    /* **************************************** */
    void handle_dead_entities();

    /* **************************************** */
    // Misc
    /* **************************************** */
    void clear_tick_buffers();

    friend class PhaseInterface;
    friend struct sim::LevelSim;
    friend struct sim::LevelSimTestAccess;

    friend class sim::LevelSpawnManager;

    settings::PositiveHealth max_health_{Health{5000}};
    EntityLedger& ledger_;
    EntityTables& entity_tables_;

    EntityStorage entities{};
    std::vector<std::uint32_t> local_indices_to_remove;
    EntityDeathInfo entity_death_info;
    std::vector<EntityFrameChange> frame_changes_;
};
} // namespace ioj::sim::capital_ships
