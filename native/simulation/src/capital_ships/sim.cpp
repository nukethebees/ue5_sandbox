#include "ioj/sim/capital_ships/sim.h"
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <ioj/sim/combat_events.h>
#include <ioj/sim/sim_config.h>
#include <optional>
#include <sandbox/core/countdown.h>
#include <span>
#include <vector>

#include <ioj/sim/batch_operations.h>
#include <ioj/sim/entity_ledger.h>
#include <ioj/sim/fighter_diagnostics.h>
#include <ioj/sim/fighter_frame_spawn_queue.h>
#include <ioj/sim/fighters/sim.h>
#include <ioj/sim/health.h>
#include <ioj/sim/profiling.h>
#include <ioj/sim/spatial_query_manager.h>

#include <sandbox/core/array_math.h>
#include <sandbox/core/frame_array.h>

namespace ioj::sim::capital_ships {

/* **************************************** */
// Configuration
/* **************************************** */
void Sim::set_config(CapitalShipSimConfig const& new_config) noexcept {
    config = new_config;
}
Sim::Sim(EntityLedger& ledger,
         CombatEvents const& combat_events,
         HealthTable& health_table,
         AgentAccessor const& agents,
         SpatialQueryManager const& in_spatial_query_manager,
         fighters::Sim& fighters)
    : ledger_{ledger}
    , combat_events_{combat_events}
    , health_table_{health_table}
    , agents_{agents}
    , spatial_query_manager{in_spatial_query_manager}
    , fighters_interface{fighters} {}

/* **************************************** */
// Sim phases
/* **************************************** */
void Sim::begin_play() {
    SANDBOX_PROFILE_SCOPE("capital_ships::Sim::begin_play");
    profiling::plot("Sandbox/CapitalShipCount", 0);
    assert(static_cast<std::size_t>(config.fighter_spawn_slots) ==
           config.fighter_spawn_slots_relative_transforms.size());
    validate_array_sizes();
}
void Sim::prepare_tick(float const dt) {
    SANDBOX_PROFILE_SCOPE("capital_ships::Sim::prepare_tick");
    clear_tick_buffers();
    auto const entities{this->entities.get_view().columns()};
    ml::tick_countdowns(entities.fighter_spawn_timers, dt);
}
void Sim::think(float const, ml::FrameScratch& scratch) {
    SANDBOX_PROFILE_SCOPE("capital_ships::Sim::think");

    auto const entities{this->entities.get_view().columns()};
    for (auto& target : entities.target_ids) {
        if (!agents_.read_alive(target)) {
            target = {};
        }
    }
    ml::FrameArray<std::int32_t> indices_without_targets{&scratch};
    auto const n_capitals{static_cast<std::int32_t>(entities.target_ids.size())};
    indices_without_targets.reserve(n_capitals);
    for (std::int32_t index{}; index < n_capitals; ++index) {
        if (!entities.target_ids[index].is_valid()) {
            indices_without_targets.add(index);
        }
    }
    for (auto const index : indices_without_targets) {
        entities.target_ids[index] = spatial_query_manager.get_any_non_team_entity(
            entities.teams[index], EntityType::CapitalShip);
    }
    queue_fighter_spawns(scratch);
    queue_fighter_orders();
}
void Sim::resolve_fighters_of_dying_capitals() {
    batch::sort_and_deduplicate_removal_indices(local_indices_to_remove);
    reassign_fighters_of_dying_capital();
}
void Sim::resolve_damage_events() {
    SANDBOX_PROFILE_SCOPE("capital_ships::Sim::resolve_damage_events");
    auto const entities{this->entities.get_view().columns()};
    auto const healths{health_table_.get_view(entities.health_indices)};
    batch::resolve_damage_events(combat_events_.events_for(EntityType::CapitalShip),
                                 agents_.indexes(),
                                 entities.entity_ids,
                                 healths,
                                 local_indices_to_remove,
                                 entity_death_info,
                                 ledger_);
    auto const batch_index{static_cast<std::int32_t>(deaths_.size())};
    for (auto const index : local_indices_to_remove) {
        deaths_.push_back({entities.locations[index], batch_index});
    }
}
void Sim::publish_deaths() {
    auto const deaths{entity_death_info.get_const_view()};
    for (std::int32_t i{}; i < deaths.num(); ++i) {
        ledger_.record_death(deaths.victims[i], deaths.killers[i], deaths.reasons[i]);
    }
}
void Sim::cleanup_entities() {
    agents_.indexes().assert_removal_allowed();
    SANDBOX_PROFILE_SCOPE("capital_ships::Sim::cleanup_entities");
    handle_dead_entities();
    local_indices_to_remove.clear();
    entity_death_info.reset();
}
void Sim::finish_action() {
    SANDBOX_PROFILE_SCOPE("capital_ships::Sim::finish_action");
    profiling::plot("Sandbox/CapitalShipCount", get_num_instances());
    validate_array_sizes();
}

/* **************************************** */
// Accessors
/* **************************************** */
auto Sim::get_num_instances() const noexcept -> std::int32_t {
    return entities.num();
}
auto Sim::is_valid(EntityUniqueId const id) const noexcept -> bool {
    return id.is_valid() && id.entity_type() == EntityType::CapitalShip &&
           agents_.indexes().find(id) >= 0;
}
auto Sim::get_fighter_ids(std::int32_t const index) const noexcept
    -> std::span<EntityUniqueId const> {
    auto const entities{this->entities.get_const_view().columns()};
    return get_fighter_ids(entities.fighter_id_spans[index]);
}
auto Sim::get_fighter_ids(IndexSpan const span) const noexcept -> std::span<EntityUniqueId const> {
    return get_fighter_ids().subspan(static_cast<std::size_t>(span.offset),
                                     static_cast<std::size_t>(span.count));
}
auto Sim::get_team(EntityUniqueId const id) const noexcept -> Team {
    auto const index{agents_.indexes().find(id)};
    assert(index >= 0);
    return entities.get_const_view().teams()[index];
}
auto Sim::get_health(EntityUniqueId const id) const noexcept -> Health {
    auto const index{agents_.indexes().find(id)};
    assert(index >= 0);
    auto const entity_data{entities.get_const_view().columns()};
    return health_table_.get_const_view(entity_data.health_indices).health(index);
}
auto Sim::find_first_index_on_team(Team const team) const noexcept -> std::optional<std::int32_t> {
    auto const entities{this->entities.get_const_view().columns()};
    auto const found{std::ranges::find(entities.teams, team)};
    if (found == entities.teams.end()) {
        return std::nullopt;
    }
    return static_cast<std::int32_t>(found - entities.teams.begin());
}
auto Sim::find_first_id_on_team(Team const team) const noexcept -> std::optional<EntityUniqueId> {
    auto const result{find_first_index_on_team(team)};
    return result ? std::optional<EntityUniqueId>{entities.get_const_view().entity_ids()[*result]}
                  : std::nullopt;
}

/* **************************************** */
// Ship spawning
/* **************************************** */
auto Sim::register_ships(CapitalSpawnDataConstView const spawn_data)
    -> std::vector<EntityUniqueId> {
    SANDBOX_PROFILE_SCOPE("capital_ships::Sim::register_ships");
    auto const n_to_add{spawn_data.num()};
    if (n_to_add == 0) {
        return {};
    }

    auto const first_new_index{entities.num()};
    spawn_ships(spawn_data);

    std::vector<EntityUniqueId> new_ids;
    new_ids.reserve(static_cast<std::size_t>(n_to_add));
    for (std::int32_t i{}; i < n_to_add; ++i) {
        auto const id{ledger_.record_spawn(
            EntityType::CapitalShip, spawn_data.teams[i], is_alive(spawn_data.healths[i]))};
        new_ids.push_back(id);
        this->entities.get_view().entity_ids()[first_new_index + i] = id;
    }
    auto const entity_data{entities.get_view().columns()};
    health_table_.add(
        std::span<EntityUniqueId const>{entity_data.entity_ids}.subspan(
            static_cast<std::size_t>(first_new_index), static_cast<std::size_t>(n_to_add)),
        spawn_data.healths,
        std::span<HealthIndex>{entity_data.health_indices}.subspan(
            static_cast<std::size_t>(first_new_index), static_cast<std::size_t>(n_to_add)));
    validate_array_sizes();
    auto const entities{this->entities.get_const_view().columns()};
    for (std::int32_t i{}; i < n_to_add; ++i) {
        auto const index{first_new_index + i};
        frame_changes_.push_back({.kind = EntityFrameChangeKind::Spawn,
                                  .index = index,
                                  .location = entities.locations[index],
                                  .rotation = entities.rotations[index],
                                  .team = entities.teams[index],
                                  .id = entities.entity_ids[index]});
    }
    agents_.indexes().bind(EntityType::CapitalShip, entities.entity_ids);
    return new_ids;
}
void Sim::spawn_ships(CapitalSpawnDataConstView const spawn_data) {
    agents_.indexes().assert_preparation_mutation_allowed();
    SANDBOX_PROFILE_SCOPE("capital_ships::Sim::spawn_ships");
    spawn_data.validate_array_sizes();
    auto const n_to_add{spawn_data.num()};

    entities.add_defaulted(n_to_add);
    auto const appended{entities.right(n_to_add).columns()};
    for (std::int32_t index{}; index < n_to_add; ++index) {
        appended.locations.set(index, spawn_data.locations[index]);
        appended.rotations.set(index, spawn_data.rotations[index]);
        appended.fighter_spawn_timers[index] = spawn_data.initial_spawn_delays[index];
        appended.fighter_spawn_cooldowns[index] = spawn_data.spawn_cooldowns[index];
        appended.teams[index] = spawn_data.teams[index];
        appended.target_ids[index] = spawn_data.target_ids[index];
    }
    validate_array_sizes();
}

/* **************************************** */
// Fighter spawning
/* **************************************** */
auto Sim::get_fighter_spawn_slots() const noexcept -> std::int32_t {
    return config.fighter_spawn_slots;
}
void Sim::queue_fighter_spawns(ml::FrameScratch& scratch) {
    SANDBOX_PROFILE_SCOPE("capital_ships::Sim::queue_fighter_spawns");
    if (!diagnostics_enabled_) {}

    auto const entities{this->entities.get_view().columns()};

    auto const n_capital_ships{get_num_instances()};
    ml::FrameArray<std::int32_t> ships_ready_to_spawn_fighters_indices{&scratch};
    ships_ready_to_spawn_fighters_indices.set_num(n_capital_ships);
    ships_ready_to_spawn_fighters_indices.set_num(
        ml::kernel::collect_indices_less_equal(entities.fighter_spawn_timers.data(),
                                               n_capital_ships,
                                               0.f,
                                               ships_ready_to_spawn_fighters_indices.data()));
    for (auto index{ships_ready_to_spawn_fighters_indices.num() - 1}; index >= 0; --index) {
        if (!entities.target_ids[ships_ready_to_spawn_fighters_indices[index]].is_valid()) {
            ships_ready_to_spawn_fighters_indices.remove_at_swap(index);
        }
    }
    if (ships_ready_to_spawn_fighters_indices.num() == 0) {
        return;
    }

    auto const& relative_transforms{config.fighter_spawn_slots_relative_transforms};
    fighters::FrameSpawnQueue fighter_spawn_wave{scratch};
    assert(std::in_range<std::int32_t>(relative_transforms.size()));
    fighter_spawn_wave.reserve(static_cast<std::int32_t>(relative_transforms.size()));
    for (auto const capital_index : ships_ready_to_spawn_fighters_indices) {
        fighter_spawn_wave.clear();
        auto const base_location{entities.locations[capital_index]};
        auto const base_rotation{entities.rotations[capital_index]};
        Transform3d const base_transform{
            to_quaternion(Rotator3d{base_rotation.pitch, base_rotation.yaw, base_rotation.roll}),
            {base_location.X, base_location.Y, base_location.Z},
            {1.0, 1.0, 1.0}};

        for (auto const& relative_transform : relative_transforms) {
            auto const new_transform{relative_transform * base_transform};
            fighter_spawn_wave.add(to_float(new_transform.location),
                                   to_float(new_transform.rotator()),
                                   entities.teams[capital_index],
                                   entities.entity_ids[capital_index],
                                   entities.target_ids[capital_index]);
        }
        auto const spawn_wave{fighter_spawn_wave.get_const_view()};
        fighters_interface.queue_spawns(spawn_wave);
        entities.fighter_spawn_timers[capital_index] =
            entities.fighter_spawn_cooldowns[capital_index];
    }
}
void Sim::refresh_fighter_ids(ml::FrameScratch& scratch) {
    auto const entities{this->entities.get_view().columns()};
    auto const ids{fighters_interface.get_entity_ids()};
    auto const parents{fighters_interface.get_parent_ids()};
    auto const healths{fighters_interface.get_healths()};
    auto const capital_count{entities.num()};
    auto const fighter_count{ids.size()};
    ml::FrameArray<std::int32_t> counts{&scratch};
    ml::FrameArray<std::int32_t> owners{&scratch};
    counts.set_num(capital_count);
    owners.set_num(static_cast<std::int32_t>(fighter_count));
    std::ranges::fill(owners, -1);
    for (std::size_t index{}; index < fighter_count; ++index) {
        if (is_dead(healths.health(static_cast<std::int32_t>(index)))) {
            continue;
        }
        auto const parent{parents[index]};
        if (!parent.is_valid() || parent.entity_type() != EntityType::CapitalShip) {
            continue;
        }
        auto const owner{agents_.indexes().find(parent)};
        if (owner >= 0) {
            owners[index] = owner;
            ++counts[owner];
        }
    }
    std::int32_t offset{};
    for (std::int32_t index{}; index < capital_count; ++index) {
        auto const count{counts[index]};
        entities.fighter_id_spans[index] = {offset, count};
        counts[index] = offset;
        offset += count;
    }
    fighter_ids.resize(static_cast<std::size_t>(offset));
    for (std::size_t index{}; index < fighter_count; ++index) {
        if (owners[index] >= 0) {
            fighter_ids[counts[owners[index]]++] = ids[index];
        }
    }
}
/* **************************************** */
// Orders
/* **************************************** */
void Sim::queue_fighter_orders() {
    SANDBOX_PROFILE_SCOPE("capital_ships::Sim::queue_fighter_orders");

    auto const n_capitals{get_num_instances()};
    auto const fighter_targets{fighters_interface.get_target_ids()};
    auto const entities{this->entities.get_const_view().columns()};
    fighter_order_queue.reset();
    for (std::int32_t capital_index{}; capital_index < n_capitals; ++capital_index) {
        auto const capital_target{entities.target_ids[capital_index]};
        auto const span{entities.fighter_id_spans[capital_index]};
        auto const end{span.end()};
        assert(span.offset >= 0 && span.count >= 0);
        assert(static_cast<std::size_t>(end) <= fighter_ids.size());

        for (auto index{span.start()}; index < end; ++index) {
            auto const fighter_id{fighter_ids[static_cast<std::size_t>(index)]};
            if (!capital_target.is_valid()) {
                fighter_order_queue.add(fighter_id,
                                        FighterOrder{.task = 1, .target = 1},
                                        FighterTask::Standby,
                                        capital_target);
                continue;
            }

            auto const fighter_index{agents_.indexes().find(fighter_id)};
            assert(fighter_index >= 0);
            auto const target{fighter_targets[fighter_index]};
            if (!agents_.read_alive(target)) {
                fighter_order_queue.add(
                    fighter_id, FighterOrder{.task = 0, .target = 1}, {}, capital_target);
            }
        }
    }

    if (fighter_order_queue.num() > 0) {
        fighters_interface.queue_orders(fighter_order_queue);
    }
}

/* **************************************** */
// Targets
/* **************************************** */
void Sim::set_target_id(EntityUniqueId const ship_id, EntityUniqueId const target_id) {
    assert(ledger_.is_valid_unique_id(target_id));
    auto const entity_index{agents_.indexes().find(ship_id)};
    auto const entities{this->entities.get_view().columns()};
    assert(entity_index >= 0 && entity_index < entities.num());
    assert(entities.entity_ids[entity_index] == ship_id);
    entities.target_ids[entity_index] = target_id;
}

/* **************************************** */
// Death handling
/* **************************************** */
void Sim::handle_dead_entities() {
    SANDBOX_PROFILE_SCOPE("capital_ships::Sim::handle_dead_entities");
    if (local_indices_to_remove.empty()) {
        return;
    }

    batch::sort_and_deduplicate_removal_indices(local_indices_to_remove);
    auto const entities{this->entities.get_const_view().columns()};
    health_table_.remove_rows(
        local_indices_to_remove, entities.health_indices, entities.entity_ids);

    for (auto const index : local_indices_to_remove) {
        frame_changes_.push_back({.kind = EntityFrameChangeKind::RemoveSwap,
                                  .index = index,
                                  .id = entities.entity_ids[index]});
    }

    for (auto const index : local_indices_to_remove) {
        agents_.indexes().retire(entities.entity_ids[index]);
        this->entities.remove_at_swap(index, 1);
    }
}
void Sim::reassign_fighters_of_dying_capital() {
    auto const entities{this->entities.get_const_view().columns()};
    auto const capital_healths{health_table_.get_const_view(entities.health_indices)};
    ml::EnumArray<Team, EntityUniqueId, static_cast<std::size_t>(Team::COUNT)> replacements{};
    auto const count{entities.num()};
    for (std::int32_t index{}; index < count; ++index) {
        auto& replacement{replacements[entities.teams[index]]};
        if (is_alive(capital_healths.health(index)) &&
            (!replacement.is_valid() || entities.entity_ids[index] < replacement)) {
            replacement = entities.entity_ids[index];
        }
    }
    auto const ids{fighters_interface.get_entity_ids()};
    auto const parents{fighters_interface.get_parent_ids()};
    auto const healths{fighters_interface.get_healths()};
    auto const fighter_count{ids.size()};
    for (auto const dying_index : local_indices_to_remove) {
        auto const parent{entities.entity_ids[dying_index]};
        auto const replacement{replacements[entities.teams[dying_index]]};
        for (std::size_t index{}; index < fighter_count; ++index) {
            if (parents[index] != parent ||
                is_dead(healths.health(static_cast<std::int32_t>(index)))) {
                continue;
            }
            fighters_interface.set_parent_id(ids[index], replacement);
        }
        fighters_interface.reassign_pending_spawns(parent, replacement);
    }
}

/* **************************************** */
// Misc
/* **************************************** */
void Sim::clear_tick_buffers() {
    local_indices_to_remove.clear();
    entity_death_info.reset();
}

/* **************************************** */
// Checks
/* **************************************** */
void Sim::validate_array_sizes() const {
    entities.get_const_view().columns().validate_array_sizes();
}
} // namespace capital_ships
