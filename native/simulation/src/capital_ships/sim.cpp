#include "ioj/sim/capital_ships/sim.h"

#include <ioj/column_math.h>
#include <ioj/rotator_math.h>
#include <ioj/sim/batch_operations.h>
#include <ioj/sim/combat_events.h>
#include <ioj/sim/entities/entity_death_info.h>
#include <ioj/sim/entities/entity_ledger.h>
#include <ioj/sim/entities/health.h>
#include <ioj/sim/levels/spawn_context.h>
#include <ioj/sim/phase_checks.h>
#include <ioj/sim/profiling.h>
#include <ioj/sim/sim_clock.h>

#include <cassert>
#include <cstdint>
#include <vector>

namespace ioj::sim::capital_ships {

/* **************************************** */
// Configuration
/* **************************************** */
// Level initialisation
void Sim::initialise(settings::PositiveHealth const max_health) {
    max_health_ = max_health;
    profiling::plot("Sandbox/CapitalShipCount", 0);
}
Sim::Sim(EntityTables& entity_tables)
    : entity_tables_{entity_tables} {}

/* **************************************** */
// Sim phases
/* **************************************** */
// Preparation
void Sim::update_entity_lookup_table() {
    auto const rows{entities.const_view()};
    entity_tables_.publish<EntityType::CapitalShip>(rows.entity_ids(), rows.teams(), max_health_);
}
void Sim::prepare_tick() {
    SANDBOX_PROFILE_SCOPE("capital_ships::Sim::prepare_tick");
    update_entity_lookup_table();
}

// Resolution
void Sim::resolve_damage_events(ResolutionInputs const inputs) {
    auto* const scratch_resource{&inputs.scratch_resource};

    SANDBOX_PROFILE_SCOPE("capital_ships::Sim::resolve_damage_events");
    auto const entities{this->entities.view()};
    auto const healths{entity_tables_.health.view<EntityType::CapitalShip>()};
    sim::resolve_damage_events(inputs.combat_events.events_for(EntityType::CapitalShip),
                               entity_tables_.lookups.for_type(EntityType::CapitalShip),
                               entities.entity_ids(),
                               entities.view_locations().const_view().resolve(),
                               healths,
                               inputs.local_indices_to_remove,
                               inputs.entity_death_info,
                               inputs.ledger,
                               scratch_resource);
}
void Sim::publish_deaths(ResolutionInputs const inputs) {
    inputs.ledger.record_deaths(inputs.entity_death_info.const_view(), inputs.tick);
}

// ResolutionCommit
void Sim::remove_components(ResolutionCommitInputs const inputs) {
    assert(is_removal_allowed(entity_tables_.lookups.clock()));
    SANDBOX_PROFILE_SCOPE("capital_ships::Sim::remove_components");

    sort_and_deduplicate_removal_indices(inputs.local_indices_to_remove);
}
void Sim::remove_entities(ResolutionCommitInputs const inputs) {
    assert(is_removal_allowed(entity_tables_.lookups.clock()));
    SANDBOX_PROFILE_SCOPE("capital_ships::Sim::remove_entities");

    handle_dead_entities(inputs);
}
void Sim::finish_action() {
    SANDBOX_PROFILE_SCOPE("capital_ships::Sim::finish_action");
    profiling::plot("Sandbox/CapitalShipCount", num_instances());
}

/* **************************************** */
// Accessors
/* **************************************** */
auto Sim::num_instances() const noexcept -> std::uint32_t {
    return entities.num();
}

/* **************************************** */
// Ship spawning
/* **************************************** */
auto Sim::register_ships(LevelCapitalSpawnEvents::ConstView const spawn_data,
                         SpawnContext const context) -> std::vector<EntityId> {
    SANDBOX_PROFILE_SCOPE("capital_ships::Sim::register_ships");
    auto const n_to_add{spawn_data.num()};
    if (n_to_add == 0) {
        return {};
    }

    auto const first_new_index{entities.num()};
    spawn_ships(spawn_data);

    auto const spawn_teams{spawn_data.teams()};
    auto const spawn_healths{spawn_data.healths()};
    auto const new_entity_ids{entities.right(n_to_add).entity_ids()};
    context.ledger.record_spawns(
        EntityType::CapitalShip, spawn_teams, spawn_healths, new_entity_ids);
    std::vector<EntityId> new_ids{new_entity_ids.begin(), new_entity_ids.end()};
    entity_tables_.health.initialise_entities<EntityType::CapitalShip>(new_entity_ids,
                                                                       spawn_healths);
    auto const entities{this->entities.const_view()};
    auto const locations{entities.view_locations().resolve()};
    auto const rotations{entities.view_rotations().resolve()};
    auto const teams{entities.teams()};
    auto const entity_ids{entities.entity_ids()};
    for (std::uint32_t i{}; i < n_to_add; ++i) {
        auto const index{first_new_index + i};
        frame_changes_.push_back({.kind = EntityFrameChangeKind::Spawn,
                                  .index = index,
                                  .location = vector_at(locations, index),
                                  .rotation = rotation_at(rotations, index),
                                  .team = teams[index],
                                  .id = entity_ids[index]});
    }
    return new_ids;
}
void Sim::spawn_ships(LevelCapitalSpawnEvents::ConstView const spawn_data) {
    assert(is_preparation_mutation_allowed(entity_tables_.lookups.clock()));
    SANDBOX_PROFILE_SCOPE("capital_ships::Sim::spawn_ships");
    spawn_data.validate();
    auto const n_to_add{spawn_data.num()};

    entities.add_defaulted(n_to_add);
    auto const appended{entities.right(n_to_add)};
    auto const appended_locations{appended.view_locations().resolve()};
    auto const appended_rotations{appended.view_rotations().resolve()};
    auto const appended_teams{appended.teams()};

    auto const spawn_data_locations{spawn_data.view_locations().resolve()};
    auto const spawn_data_rotations{spawn_data.view_rotations().resolve()};
    auto const spawn_data_teams{spawn_data.teams()};

    for (std::uint32_t index{}; index < n_to_add; ++index) {
        set_vector(appended_locations, index, vector_at(spawn_data_locations, index));
        set_rotation(appended_rotations, index, rotation_at(spawn_data_rotations, index));
        appended_teams[index] = spawn_data_teams[index];
    }
}

/* **************************************** */
// Death handling
/* **************************************** */
void Sim::handle_dead_entities(ResolutionCommitInputs const inputs) {
    SANDBOX_PROFILE_SCOPE("capital_ships::Sim::handle_dead_entities");
    if (inputs.local_indices_to_remove.empty()) {
        return;
    }

    auto const entities{this->entities.const_view()};
    auto const entity_ids{entities.entity_ids()};

    for (auto const index : inputs.local_indices_to_remove) {
        frame_changes_.push_back(
            {.kind = EntityFrameChangeKind::RemoveSwap, .index = index, .id = entity_ids[index]});
    }

    entity_tables_.lookups.for_type(EntityType::CapitalShip)
        .retire_rows(entity_ids, inputs.local_indices_to_remove);
    this->entities.remove_at_swap(inputs.local_indices_to_remove);
}

} // namespace capital_ships
