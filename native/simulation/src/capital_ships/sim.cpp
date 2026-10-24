#include "ioj/sim/capital_ships/sim.h"

#include <ioj/array_math.h>
#include <ioj/column_math.h>
#include <ioj/countdown.h>
#include <ioj/frame_array.h>
#include <ioj/rotator_math.h>
#include <ioj/sim/batch_operations.h>
#include <ioj/sim/capital_ships/capital_ship_sim_config.h>
#include <ioj/sim/collision/spatial_query_manager.h>
#include <ioj/sim/combat_events.h>
#include <ioj/sim/entities/entity_ledger.h>
#include <ioj/sim/entities/health.h>
#include <ioj/sim/fighters/fighter_spawn_queue.h>
#include <ioj/sim/fighters/sim.h>
#include <ioj/sim/profiling.h>
#include <ioj/sim/sim_clock.h>

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <numeric>
#include <optional>
#include <span>
#include <vector>

namespace ioj::sim::capital_ships {

/* **************************************** */
// Configuration
/* **************************************** */
void Sim::set_config(CapitalShipSimConfig const& new_config) noexcept {
    config = new_config;
}
Sim::Sim(EntityLedger& ledger, EntityTables& entity_tables, fighters::Sim& fighters)
    : ledger_{ledger}
    , entity_tables_{entity_tables}
    , fighters_interface{fighters} {}

/* **************************************** */
// Sim phases
/* **************************************** */
void Sim::begin_play() {
    SANDBOX_PROFILE_SCOPE("capital_ships::Sim::begin_play");
    profiling::plot("Sandbox/CapitalShipCount", 0);
    assert(static_cast<std::size_t>(config.fighter_spawn_slots) ==
           config.fighter_spawn_slots_relative_transforms.size());
}
void Sim::update_entity_lookup_table() {
    auto const rows{entities.get_const_view()};
    entity_tables_.publish<EntityType::CapitalShip>(
        rows.entity_ids(), rows.teams(), config.max_health);
}
void Sim::prepare_tick(float const dt) {
    SANDBOX_PROFILE_SCOPE("capital_ships::Sim::prepare_tick");
    clear_tick_buffers();
    auto const entities{this->entities.get_view()};
    tick_countdowns(entities.fighter_spawn_timers(), dt);
    update_entity_lookup_table();
}
void Sim::think(ThinkingInputs const inputs) {
    auto* const scratch_resource{&inputs.scratch_resource};

    SANDBOX_PROFILE_SCOPE("capital_ships::Sim::think");

    auto const entities{this->entities.get_view()};
    auto const target_ids{entities.target_ids()};
    auto const teams{entities.teams()};

    auto const target_count{static_cast<std::uint32_t>(target_ids.size())};
    FrameArray<std::uint32_t> order{scratch_resource};
    FrameArray<EntityInstanceHandle> targets{scratch_resource};
    order.set_num(target_count);
    targets.set_num(target_count);
    entity_tables_.lookups.lookup_handles(target_ids, order, targets);

    for (std::uint32_t index{}; index < target_count; ++index) {
        if (!targets[index].is_valid()) {
            target_ids[index] = {};
        }
    }
    FrameArray<std::uint32_t> indices_without_targets{scratch_resource};
    auto const n_capitals{static_cast<std::uint32_t>(target_ids.size())};
    indices_without_targets.reserve(n_capitals);
    for (std::uint32_t index{}; index < n_capitals; ++index) {
        if (!target_ids[index].is_valid()) {
            indices_without_targets.add(index);
        }
    }
    for (auto const index : indices_without_targets) {
        target_ids[index] = inputs.spatial_query_manager.get_any_non_team_entity(
            teams[index], EntityType::CapitalShip);
    }
    queue_fighter_spawns(scratch_resource);
    queue_fighter_orders(scratch_resource);
}
void Sim::resolve_fighters_of_dying_capitals() {
    sort_and_deduplicate_removal_indices(local_indices_to_remove);
    reassign_fighters_of_dying_capital();
}
void Sim::resolve_damage_events(ResolutionInputs const inputs) {
    auto* const scratch_resource{&inputs.scratch_resource};

    SANDBOX_PROFILE_SCOPE("capital_ships::Sim::resolve_damage_events");
    auto const entities{this->entities.get_view()};
    auto const healths{entity_tables_.health.get_view<EntityType::CapitalShip>(entities.num())};
    sim::resolve_damage_events(inputs.combat_events.events_for(EntityType::CapitalShip),
                               entity_tables_.lookups.for_type(EntityType::CapitalShip),
                               entities.entity_ids(),
                               entities.view_locations().get_const_view().resolve(),
                               healths,
                               local_indices_to_remove,
                               entity_death_info,
                               ledger_,
                               scratch_resource);
}
void Sim::publish_deaths(ResolutionInputs const inputs) {
    ledger_.record_deaths(entity_death_info.get_const_view(), inputs.clock.get_completed_ticks());
}

void Sim::remove_components() {
    entity_tables_.lookups.assert_removal_allowed();
    SANDBOX_PROFILE_SCOPE("capital_ships::Sim::remove_components");

    sort_and_deduplicate_removal_indices(local_indices_to_remove);
    if (local_indices_to_remove.empty()) {
        return;
    }

    entity_tables_.health.remove_rows<EntityType::CapitalShip>(entities.num(),
                                                               local_indices_to_remove);
}
void Sim::remove_entities() {
    entity_tables_.lookups.assert_removal_allowed();
    SANDBOX_PROFILE_SCOPE("capital_ships::Sim::remove_entities");

    handle_dead_entities();
    local_indices_to_remove.clear();
    entity_death_info.reset();
}
void Sim::finish_action() {
    SANDBOX_PROFILE_SCOPE("capital_ships::Sim::finish_action");
    profiling::plot("Sandbox/CapitalShipCount", get_num_instances());
}

/* **************************************** */
// Accessors
/* **************************************** */
auto Sim::get_num_instances() const noexcept -> std::uint32_t {
    return entities.num();
}
auto Sim::is_valid(EntityId const id) const noexcept -> bool {
    return std::ranges::find(entities.get_const_view().entity_ids(), id) !=
           entities.get_const_view().entity_ids().end();
}
auto Sim::get_fighter_ids(std::uint32_t const index) const noexcept -> std::span<EntityId const> {
    auto const entities{this->entities.get_const_view()};
    return get_fighter_ids(entities.fighter_id_spans()[index]);
}
auto Sim::get_fighter_ids(IndexSpan const span) const noexcept -> std::span<EntityId const> {
    return get_fighter_ids().subspan(static_cast<std::size_t>(span.offset),
                                     static_cast<std::size_t>(span.count));
}
auto Sim::get_team(EntityId const id) const noexcept -> Team {
    auto const ids{entities.get_const_view().entity_ids()};
    auto const found{std::ranges::find(ids, id)};
    assert(found != ids.end());
    auto const index{static_cast<EntityFrameIndex>(found - ids.begin())};
    return entities.get_const_view().teams()[index];
}
auto Sim::get_health(EntityId const id) const noexcept -> Health {
    auto const ids{entities.get_const_view().entity_ids()};
    auto const found{std::ranges::find(ids, id)};
    assert(found != ids.end());
    auto const index{static_cast<EntityFrameIndex>(found - ids.begin())};
    return entity_tables_.health.get_const_view<EntityType::CapitalShip>(entities.num())
        .health(index);
}
auto Sim::find_first_index_on_team(Team const team) const noexcept -> std::optional<std::uint32_t> {
    auto const entities{this->entities.get_const_view()};
    auto const found{std::ranges::find(entities.teams(), team)};
    if (found == entities.teams().end()) {
        return std::nullopt;
    }
    return static_cast<std::uint32_t>(found - entities.teams().begin());
}
auto Sim::find_first_id_on_team(Team const team) const noexcept -> std::optional<EntityId> {
    auto const result{find_first_index_on_team(team)};
    return result ? std::optional<EntityId>{entities.get_const_view().entity_ids()[*result]}
                  : std::nullopt;
}

/* **************************************** */
// Ship spawning
/* **************************************** */
auto Sim::register_ships(LevelCapitalSpawnEvents::ConstView const spawn_data)
    -> std::vector<EntityId> {
    SANDBOX_PROFILE_SCOPE("capital_ships::Sim::register_ships");
    auto const n_to_add{spawn_data.num()};
    if (n_to_add == 0) {
        return {};
    }

    auto const first_new_index{entities.num()};
    spawn_ships(spawn_data);
    fighter_ids_current_ = false;

    auto const spawn_teams{spawn_data.teams()};
    auto const spawn_healths{spawn_data.healths()};
    auto const new_entity_ids{entities.right(n_to_add).entity_ids()};
    ledger_.record_spawns(EntityType::CapitalShip, spawn_teams, spawn_healths, new_entity_ids);
    std::vector<EntityId> new_ids{new_entity_ids.begin(), new_entity_ids.end()};
    entity_tables_.health.initialise_rows<EntityType::CapitalShip>(first_new_index, spawn_healths);
    auto const entities{this->entities.get_const_view()};
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
    entity_tables_.lookups.assert_preparation_mutation_allowed();
    SANDBOX_PROFILE_SCOPE("capital_ships::Sim::spawn_ships");
    spawn_data.validate();
    auto const n_to_add{spawn_data.num()};

    entities.add_defaulted(n_to_add);
    auto const appended{entities.right(n_to_add)};
    auto const appended_locations{appended.view_locations().resolve()};
    auto const appended_rotations{appended.view_rotations().resolve()};
    auto const appended_fighter_spawn_timers{appended.fighter_spawn_timers()};
    auto const appended_fighter_spawn_cooldowns{appended.fighter_spawn_cooldowns()};
    auto const appended_teams{appended.teams()};
    auto const appended_target_ids{appended.target_ids()};

    auto const spawn_data_locations{spawn_data.view_locations().resolve()};
    auto const spawn_data_rotations{spawn_data.view_rotations().resolve()};
    auto const spawn_data_initial_fighter_spawn_delays{spawn_data.initial_fighter_spawn_delays()};
    auto const spawn_data_fighter_spawn_cooldowns{spawn_data.fighter_spawn_cooldowns()};
    auto const spawn_data_teams{spawn_data.teams()};

    for (std::uint32_t index{}; index < n_to_add; ++index) {
        set_vector(appended_locations, index, vector_at(spawn_data_locations, index));
        set_rotation(appended_rotations, index, rotation_at(spawn_data_rotations, index));
        appended_fighter_spawn_timers[index] = spawn_data_initial_fighter_spawn_delays[index];
        appended_fighter_spawn_cooldowns[index] = spawn_data_fighter_spawn_cooldowns[index];
        appended_teams[index] = spawn_data_teams[index];
        appended_target_ids[index] = EntityId{};
    }
}

/* **************************************** */
// Fighter spawning
/* **************************************** */
auto Sim::get_fighter_spawn_slots() const noexcept -> std::uint32_t {
    return config.fighter_spawn_slots;
}
void Sim::queue_fighter_spawns(FrameMemoryResource* const scratch_resource) {
    SANDBOX_PROFILE_SCOPE("capital_ships::Sim::queue_fighter_spawns");

    auto const entities{this->entities.get_view()};

    auto const n_capital_ships{get_num_instances()};
    FrameArray<std::uint32_t> ships_ready_to_spawn_fighters_indices{scratch_resource};
    ships_ready_to_spawn_fighters_indices.set_num(n_capital_ships);
    ships_ready_to_spawn_fighters_indices.set_num(
        kernel::collect_indices_less_equal(entities.fighter_spawn_timers().data(),
                                           n_capital_ships,
                                           0.f,
                                           ships_ready_to_spawn_fighters_indices.data()));
    auto const target_ids{entities.target_ids()};

    for (auto remaining{ships_ready_to_spawn_fighters_indices.num()}; remaining > 0; --remaining) {
        auto const index{remaining - 1};
        if (!target_ids[ships_ready_to_spawn_fighters_indices[index]].is_valid()) {
            ships_ready_to_spawn_fighters_indices.remove_at_swap(index);
        }
    }
    if (ships_ready_to_spawn_fighters_indices.num() == 0) {
        return;
    }

    auto const& relative_transforms{config.fighter_spawn_slots_relative_transforms};
    FighterSpawnQueue fighter_spawn_wave{*scratch_resource};
    assert(std::in_range<std::uint32_t>(relative_transforms.size()));
    fighter_spawn_wave.reserve(static_cast<std::uint32_t>(relative_transforms.size()));
    auto const locations{entities.view_locations().resolve()};
    auto const teams{entities.teams()};
    auto const entity_ids{entities.entity_ids()};
    auto const spawn_timers{entities.fighter_spawn_timers()};
    auto const spawn_cooldowns{entities.fighter_spawn_cooldowns()};

    auto const rotations{entities.view_rotations().resolve()};

    for (auto const capital_index : ships_ready_to_spawn_fighters_indices) {
        fighter_spawn_wave.reset();
        auto const base_location{vector_at(locations, capital_index)};
        auto const base_rotation{rotation_at(rotations, capital_index)};
        Transform3d const base_transform{
            to_quaternion(Rotator3d{base_rotation.pitch, base_rotation.yaw, base_rotation.roll}),
            {base_location.x(), base_location.y(), base_location.z()},
            {1.0, 1.0, 1.0}};

        for (auto const& relative_transform : relative_transforms) {
            auto const new_transform{relative_transform * base_transform};
            auto const location{Vector3f{new_transform.location}};
            auto const rotation{Rotator3f{new_transform.rotator()}};
            fighter_spawn_wave.add(location.x(),
                                   location.y(),
                                   location.z(),
                                   rotation.pitch,
                                   rotation.yaw,
                                   rotation.roll,
                                   teams[capital_index],
                                   entity_ids[capital_index],
                                   target_ids[capital_index]);
        }
        fighters_interface.queue_spawns(fighter_spawn_wave.get_const_view());
        spawn_timers[capital_index] = spawn_cooldowns[capital_index];
    }
}
void Sim::refresh_fighter_ids(FrameMemoryResource* const scratch_resource) {
    auto const membership_revision{fighters_interface.get_membership_revision()};
    auto const layout_revision{fighters_interface.get_layout_revision()};
    if (fighter_ids_current_ && fighter_membership_revision_ == membership_revision &&
        fighter_layout_revision_ == layout_revision) {
        return;
    }

    auto const entities{this->entities.get_view()};
    auto const ids{fighters_interface.get_entity_ids()};
    auto const parents{fighters_interface.get_parent_ids()};
    auto const healths{fighters_interface.get_healths()};
    auto const capital_count{entities.num()};
    auto const fighter_count{static_cast<std::uint32_t>(ids.size())};
    FrameArray<std::uint32_t> counts{scratch_resource};
    FrameArray<std::uint32_t> owners{scratch_resource};
    counts.set_num(capital_count);
    owners.set_num(fighter_count);
    auto const capital_ids{entities.entity_ids()};
    FrameArray<std::uint32_t> capital_order{scratch_resource};
    capital_order.set_num(capital_count);
    std::iota(capital_order.begin(), capital_order.end(), 0u);
    std::ranges::sort(capital_order, {}, [&](auto const row) { return capital_ids[row]; });
    std::ranges::fill(owners, capital_count);
    for (std::uint32_t index{}; index < fighter_count; ++index) {
        if (is_dead(healths.health(index))) {
            continue;
        }
        auto const parent{parents[index]};
        if (!parent.is_valid() || parent.entity_type() != EntityType::CapitalShip) {
            continue;
        }
        auto const found{std::ranges::lower_bound(
            capital_order, parent, {}, [&](auto const row) { return capital_ids[row]; })};
        if (found != capital_order.end() && capital_ids[*found] == parent) {
            owners[index] = *found;
            ++counts[*found];
        }
    }
    auto const fighter_spans{entities.fighter_id_spans()};

    std::uint32_t offset{};
    for (std::uint32_t index{}; index < capital_count; ++index) {
        auto const count{counts[index]};
        fighter_spans[index] = {offset, count};
        counts[index] = offset;
        offset += count;
    }
    fighter_ids.resize(static_cast<std::size_t>(offset));
    for (std::uint32_t index{}; index < fighter_count; ++index) {
        if (owners[index] != capital_count) {
            fighter_ids[counts[owners[index]]++] = ids[index];
        }
    }
#ifndef NDEBUG
    std::uint32_t checked_offset{};
    for (std::uint32_t index{}; index < capital_count; ++index) {
        auto const span{fighter_spans[index]};
        assert(span.offset == checked_offset && counts[index] == span.end());
        checked_offset = span.end();
    }
    assert(checked_offset == fighter_ids.size());
#endif

    fighter_membership_revision_ = membership_revision;
    fighter_layout_revision_ = layout_revision;
    fighter_ids_current_ = true;
}
/* **************************************** */
// Orders
/* **************************************** */
void Sim::queue_fighter_orders(FrameMemoryResource* const scratch_resource) {
    SANDBOX_PROFILE_SCOPE("capital_ships::Sim::queue_fighter_orders");

    auto const n_capitals{get_num_instances()};
    auto const fighter_targets{fighters_interface.get_target_ids()};
    auto const target_count{static_cast<std::uint32_t>(fighter_targets.size())};
    FrameArray<std::uint32_t> order{scratch_resource};
    FrameArray<EntityInstanceHandle> targets{scratch_resource};
    order.set_num(target_count);
    targets.set_num(target_count);
    entity_tables_.lookups.lookup_handles(fighter_targets, order, targets);
    auto const handles{entity_tables_.lookups.for_type(EntityType::Fighter).entries()};
    auto const entities{this->entities.get_const_view()};
    fighter_order_queue.reset();
    auto const target_ids{entities.target_ids()};
    auto const fighter_spans{entities.fighter_id_spans()};

    for (std::uint32_t capital_index{}; capital_index < n_capitals; ++capital_index) {
        auto const capital_target{target_ids[capital_index]};
        auto const span{fighter_spans[capital_index]};
        auto const end{span.end()};

        for (auto index{span.start()}; index < end; ++index) {
            auto const fighter_id{fighter_ids[static_cast<std::size_t>(index)]};
            if (!capital_target.is_valid()) {
                fighter_order_queue.add(
                    fighter_id, FighterOrder{1, 1}, FighterTask::Standby, capital_target);
                continue;
            }

            auto const fighter_index{handles[fighter_id.index()].index()};
            if (!targets[fighter_index].is_valid()) {
                fighter_order_queue.add(fighter_id, FighterOrder{0, 1}, {}, capital_target);
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
void Sim::set_target_id(EntityId const ship_id, EntityId const target_id) {
    assert(ledger_.is_valid_unique_id(target_id));
    auto const ids{entities.get_const_view().entity_ids()};
    auto const found{std::ranges::find(ids, ship_id)};
    assert(found != ids.end());
    auto const entity_index{static_cast<EntityFrameIndex>(found - ids.begin())};
    auto const entities{this->entities.get_view()};
    entities.target_ids()[entity_index] = target_id;
}

/* **************************************** */
// Death handling
/* **************************************** */
void Sim::handle_dead_entities() {
    SANDBOX_PROFILE_SCOPE("capital_ships::Sim::handle_dead_entities");
    if (local_indices_to_remove.empty()) {
        return;
    }

    auto const entities{this->entities.get_const_view()};
    auto const entity_ids{entities.entity_ids()};

    for (auto const index : local_indices_to_remove) {
        frame_changes_.push_back(
            {.kind = EntityFrameChangeKind::RemoveSwap, .index = index, .id = entity_ids[index]});
    }

    entity_tables_.lookups.for_type(EntityType::CapitalShip)
        .retire_rows(entity_ids, local_indices_to_remove);
    this->entities.remove_at_swap(local_indices_to_remove);
    fighter_ids_current_ = false;
}
void Sim::reassign_fighters_of_dying_capital() {
    auto const entities{this->entities.get_const_view()};
    auto const capital_healths{
        entity_tables_.health.get_const_view<EntityType::CapitalShip>(entities.num())};
    EnumArray<Team, EntityId> replacements{};
    auto const count{entities.num()};
    auto const teams{entities.teams()};
    auto const entity_ids{entities.entity_ids()};

    for (std::uint32_t index{}; index < count; ++index) {
        auto& replacement{replacements[teams[index]]};
        if (is_alive(capital_healths.health(index)) &&
            (!replacement.is_valid() || entity_ids[index] < replacement)) {
            replacement = entity_ids[index];
        }
    }
    auto const ids{fighters_interface.get_entity_ids()};
    auto const parents{fighters_interface.get_parent_ids()};
    auto const healths{fighters_interface.get_healths()};
    auto const fighter_count{ids.size()};
    for (auto const dying_index : local_indices_to_remove) {
        auto const parent{entity_ids[dying_index]};
        auto const replacement{replacements[teams[dying_index]]};
        for (std::size_t index{}; index < fighter_count; ++index) {
            if (parents[index] != parent ||
                is_dead(healths.health(static_cast<std::uint32_t>(index)))) {
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
} // namespace capital_ships
