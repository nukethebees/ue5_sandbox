#include "ioj/sim/capital_ships/sim.h"
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <format>
#include <ioj/sim/sim_config.h>
#include <optional>
#include <sandbox/core/countdown.h>
#include <sandbox/core/diagnostics.h>
#include <span>
#include <vector>

#include <ioj/sim/batch_operations.h>
#include <ioj/sim/entity_registry.h>
#include <ioj/sim/entity_registry_refresh.h>
#include <ioj/sim/entity_registry_view.h>
#include <ioj/sim/fighter_diagnostics.h>
#include <ioj/sim/fighter_frame_spawn_queue.h>
#include <ioj/sim/fighters/sim.h>
#include <ioj/sim/health.h>
#include <ioj/sim/profiling.h>
#include <ioj/sim/spatial_query_manager.h>

#include <sandbox/core/array_math.h>
#include <sandbox/core/frame_array.h>

namespace ioj::sim::capital_ships {
namespace {
void copy_vectors(Vectors3fView const destination, Vectors3fConstView const source) {
    std::ranges::copy(source.xs_span(), destination.xs);
    std::ranges::copy(source.ys_span(), destination.ys);
    std::ranges::copy(source.zs_span(), destination.zs);
}
void copy_rotators(Rotators3fView const destination, Rotators3fConstView const source) {
    std::ranges::copy(source.pitches, destination.pitches.begin());
    std::ranges::copy(source.yaws, destination.yaws.begin());
    std::ranges::copy(source.rolls, destination.rolls.begin());
}
} // namespace

/* **************************************** */
// Configuration
/* **************************************** */
void Sim::set_config(CapitalShipSimConfig const& new_config) noexcept {
    config = new_config;
}
Sim::Sim(EntityRegistry& in_entity_registry,
         AgentAccessor const& agents,
         SpatialQueryManager const& in_spatial_query_manager,
         fighters::Sim& fighters,
         std::pmr::memory_resource& in_frame_memory_resource)
    : entity_registry{in_entity_registry}
    , agents_{agents}
    , spatial_query_manager{in_spatial_query_manager}
    , frame_memory_resource{in_frame_memory_resource}
    , fighters_interface{fighters} {}

/* **************************************** */
// Sim phases
/* **************************************** */
void Sim::begin_play() {
    SANDBOX_PROFILE_SCOPE("Sandbox::capital_ships::Sim::begin_play");
    profiling::plot("Sandbox/CapitalShipCount", 0);
    assert(static_cast<std::size_t>(config.fighter_spawn_slots) ==
           config.fighter_spawn_slots_relative_transforms.size());
    validate_array_sizes();
}
void Sim::prepare_tick(float const dt) {
    SANDBOX_PROFILE_SCOPE("Sandbox::capital_ships::Sim::prepare_tick");
    clear_tick_buffers();
    fighter_self_destruct_requests_.clear();
    auto const entities{this->entities.get_view().columns()};
    ml::tick_countdowns(entities.fighter_spawn_timers, dt);
}
void Sim::think(float const) {
    SANDBOX_PROFILE_SCOPE("Sandbox::capital_ships::Sim::think");

    auto const entities{this->entities.get_view().columns()};
    for (auto& target : entities.target_handles) {
        if (!agents_.read_alive(entity_registry.get_current_id(target))) {
            target = {};
        }
    }
    ml::FrameArray<std::int32_t> indices_without_targets{&frame_memory_resource};
    auto const n_capitals{static_cast<std::int32_t>(entities.target_handles.size())};
    indices_without_targets.reserve(n_capitals);
    for (std::int32_t index{}; index < n_capitals; ++index) {
        if (entities.target_handles[index].is_null()) {
            indices_without_targets.add(index);
        }
    }
    for (auto const index : indices_without_targets) {
        entities.target_handles[index] = spatial_query_manager.get_any_non_team_entity(
            entities.teams[index], EntityType::CapitalShip);
    }
    queue_fighter_spawns();
    queue_fighter_orders();
}
void Sim::execute_fighter_self_destruct_requests() {
    for (auto const fighter : fighter_self_destruct_requests_) {
        fighters_interface.self_destruct_fighter(fighter);
    }
    fighter_self_destruct_requests_.clear();
}
void Sim::resolve_fighters_of_dying_capitals() {
    batch::sort_and_deduplicate_removal_indices(local_indices_to_remove);
    reassign_fighter_handles_of_dying_capital();
    execute_fighter_self_destruct_requests();
}
void Sim::resolve_damage_events() {
    SANDBOX_PROFILE_SCOPE("Sandbox::capital_ships::Sim::resolve_damage_events");
    auto const entities{this->entities.get_view().columns()};
    batch::resolve_damage_events(entity_registry,
                                 agents_.indexes(),
                                 entities.handles,
                                 entities.healths,
                                 local_indices_to_remove,
                                 entity_death_info);
    auto const batch_index{static_cast<std::int32_t>(deaths_.size())};
    for (auto const index : local_indices_to_remove) {
        deaths_.push_back({entities.locations[index], batch_index});
        frame_changes_.push_back({.kind = EntityFrameChangeKind::Died,
                                  .index = index,
                                  .handle = entities.handles[index]});
    }
}
void Sim::update_entity_registry() {
    SANDBOX_PROFILE_SCOPE("Sandbox::capital_ships::Sim::update_entity_registry");
    prepare_entity_update_data();
    auto const entities{this->entities.get_const_view().columns()};
    entity_registry.queue_entity_updates(
        {entities.handles, entity_update_data.get_const_view().columns()}, entity_death_info);
}
void Sim::cleanup_entities() {
    agents_.indexes().assert_structural_mutation_allowed();
    SANDBOX_PROFILE_SCOPE("Sandbox::capital_ships::Sim::cleanup_entities");
    handle_dead_entities();
    local_indices_to_remove.clear();
    entity_death_info.reset();
}
void Sim::finish_action() {
    SANDBOX_PROFILE_SCOPE("Sandbox::capital_ships::Sim::finish_action");
    profiling::plot("Sandbox/CapitalShipCount", get_num_instances());
    validate_array_sizes();
}

/* **************************************** */
// Accessors
/* **************************************** */
auto Sim::get_num_instances() const noexcept -> std::int32_t {
    return entities.num();
}
auto Sim::is_valid(RegistryEntityHandle const handle) const noexcept -> bool {
    auto const entities{this->entities.get_const_view().columns()};
    return handle.is_valid() &&
           std::ranges::find(entities.handles, handle) != entities.handles.end();
}
auto Sim::get_fighter_handles(std::int32_t const index) const noexcept
    -> std::span<RegistryEntityHandle const> {
    auto const entities{this->entities.get_const_view().columns()};
    return get_fighter_handles(entities.fighter_handle_spans[index]);
}
auto Sim::get_fighter_handles(IndexSpan const span) const noexcept
    -> std::span<RegistryEntityHandle const> {
    return get_fighter_handles().subspan(static_cast<std::size_t>(span.offset),
                                         static_cast<std::size_t>(span.count));
}
auto Sim::get_team(RegistryEntityHandle const handle) const noexcept -> Team {
    auto const entities{this->entities.get_const_view().columns()};
    auto const found{std::ranges::find(entities.handles, handle)};
    if (found != entities.handles.end()) {
        return entities.teams[found - entities.handles.begin()];
    }

    ml::fatal_error("Invalid capital ship handle passed");
}
auto Sim::get_health(RegistryEntityHandle const handle) const noexcept -> Health {
    auto const entities{this->entities.get_const_view().columns()};
    auto const found{std::ranges::find(entities.handles, handle)};
    assert(found != entities.handles.end());
    return entities.healths[found - entities.handles.begin()];
}
auto Sim::find_first_index_on_team(Team const team) const noexcept -> std::optional<std::int32_t> {
    auto const entities{this->entities.get_const_view().columns()};
    auto const found{std::ranges::find(entities.teams, team)};
    if (found == entities.teams.end()) {
        return std::nullopt;
    }
    return static_cast<std::int32_t>(found - entities.teams.begin());
}
auto Sim::find_first_handle_on_team(Team const team) const noexcept
    -> std::optional<RegistryEntityHandle> {
    auto const entities{this->entities.get_const_view().columns()};
    auto const result{find_first_index_on_team(team)};
    return result ? std::optional<RegistryEntityHandle>{entities.handles[*result]} : std::nullopt;
}

/* **************************************** */
// Ship spawning
/* **************************************** */
auto Sim::register_ships(CapitalSpawnDataConstView const spawn_data)
    -> std::vector<RegistryEntityHandle> {
    SANDBOX_PROFILE_SCOPE("Sandbox::capital_ships::Sim::register_ships");
    auto const n_to_add{spawn_data.num()};
    if (n_to_add == 0) {
        return {};
    }

    auto const first_new_index{entities.num()};
    spawn_ships(spawn_data);

    SingleAllocationRegistryEntityData new_entity_data;
    new_entity_data.add_uninitialised(n_to_add);
    auto const new_entity_columns{new_entity_data.get_view().columns()};
    for (std::int32_t i{}; i < n_to_add; ++i) {
        new_entity_columns.locations.set(i, spawn_data.locations[i]);
        new_entity_columns.rotations.set(i, spawn_data.rotations[i]);
    }
    new_entity_columns.velocities.each_column(
        [](auto const column) { std::ranges::fill(column, 0.f); });
    std::ranges::fill(new_entity_columns.entity_types, EntityType::CapitalShip);
    for (std::int32_t i{}; i < n_to_add; ++i) {
        new_entity_columns.healths[i] = spawn_data.healths[i];
        new_entity_columns.teams[i] = spawn_data.teams[i];
    }

    auto const new_entities{
        entity_registry.add_entities(new_entity_data.get_const_view().columns())};
    std::vector<RegistryEntityHandle> new_handles;
    new_handles.reserve(static_cast<std::size_t>(n_to_add));
    for (std::int32_t i{}; i < n_to_add; ++i) {
        new_handles.push_back(new_entities.get_handle(i));
    }
    for (std::int32_t i{}; i < n_to_add; ++i) {
        this->entities.get_view().entity_ids()[first_new_index + i] =
            new_entities.get_id(i, EntityType::CapitalShip);
        this->entities.get_view().handles()[first_new_index + i] = new_handles[i];
    }
    validate_array_sizes();
    auto const entities{this->entities.get_const_view().columns()};
    for (std::int32_t i{}; i < n_to_add; ++i) {
        auto const index{first_new_index + i};
        frame_changes_.push_back({.kind = EntityFrameChangeKind::Spawn,
                                  .index = index,
                                  .location = entities.locations[index],
                                  .rotation = entities.rotations[index],
                                  .team = entities.teams[index],
                                  .handle = entities.handles[index]});
    }
    return new_handles;
}
void Sim::spawn_ships(CapitalSpawnDataConstView const spawn_data) {
    agents_.indexes().assert_structural_mutation_allowed();
    SANDBOX_PROFILE_SCOPE("Sandbox::capital_ships::Sim::spawn_ships");
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
        appended.healths[index] = spawn_data.healths[index];
        appended.target_handles[index] = spawn_data.target_handles[index];
    }
    validate_array_sizes();
}

/* **************************************** */
// Entity data
/* **************************************** */
void Sim::prepare_entity_update_data() {
    SANDBOX_PROFILE_SCOPE("Sandbox::capital_ships::Sim::prepare_entity_update_data");
    entity_update_data.reset();
    auto const n{get_num_instances()};
    entity_update_data.add_uninitialised(n);
    auto const entities{this->entities.get_const_view().columns()};
    auto const updates{entity_update_data.get_view().columns()};
    copy_vectors(updates.locations, entities.locations);
    copy_rotators(updates.rotations, entities.rotations);
    updates.velocities.each_column([](auto const column) { std::ranges::fill(column, 0.f); });
    std::ranges::copy(entities.healths, updates.healths.begin());
    std::ranges::copy(entities.teams, updates.teams.begin());
    std::ranges::fill(updates.entity_types, EntityType::CapitalShip);
}

/* **************************************** */
// Fighter spawning
/* **************************************** */
auto Sim::get_fighter_spawn_slots() const noexcept -> std::int32_t {
    return config.fighter_spawn_slots;
}
void Sim::queue_fighter_spawns() {
    SANDBOX_PROFILE_SCOPE("Sandbox::capital_ships::Sim::queue_fighter_spawns");
    if (!diagnostics_enabled_) {
        diagnostic_spawn_reports = 0;
    }

    auto const entities{this->entities.get_view().columns()};

    auto const n_capital_ships{get_num_instances()};
    ml::FrameArray<std::int32_t> ships_ready_to_spawn_fighters_indices{&frame_memory_resource};
    ships_ready_to_spawn_fighters_indices.set_num(n_capital_ships);
    ships_ready_to_spawn_fighters_indices.set_num(
        ml::kernel::collect_indices_less_equal(entities.fighter_spawn_timers.data(),
                                               n_capital_ships,
                                               0.f,
                                               ships_ready_to_spawn_fighters_indices.data()));
    for (auto index{ships_ready_to_spawn_fighters_indices.num() - 1}; index >= 0; --index) {
        if (entities.target_handles[ships_ready_to_spawn_fighters_indices[index]].is_null()) {
            ships_ready_to_spawn_fighters_indices.remove_at_swap(index);
        }
    }
    if (ships_ready_to_spawn_fighters_indices.num() == 0) {
        return;
    }

    auto const& relative_transforms{config.fighter_spawn_slots_relative_transforms};
    fighters::FrameSpawnQueue fighter_spawn_wave{&frame_memory_resource};
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
            if (fighters::diagnostics::take_report(
                    diagnostics_enabled_, diagnostic_spawn_reports, 64)) {
                /* ml::log_error(std::format("[FighterSpawn] Enqueue parentRegistryIndex={}
                   capitalIndex={} base=({}, {}, {}) slot=({}, {}, {}) world=({}, {}, {})",
                    entities.handles[capital_index].index, capital_index,
                    base_transform.location.x, base_transform.location.y, base_transform.location.z,
                    relative_transform.location.x, relative_transform.location.y,
                   relative_transform.location.z, new_transform.location.x,
                   new_transform.location.y, new_transform.location.z)); */
            }
            fighter_spawn_wave.add(to_float(new_transform.location),
                                   to_float(new_transform.rotator()),
                                   entities.teams[capital_index],
                                   entities.handles[capital_index],
                                   entities.target_handles[capital_index]);
        }
        auto const spawn_wave{fighter_spawn_wave.get_const_view()};
        fighters_interface.queue_spawns(spawn_wave);
        entities.fighter_spawn_timers[capital_index] =
            entities.fighter_spawn_cooldowns[capital_index];
    }
}
void Sim::refresh_fighter_handles() {
    auto const entities{this->entities.get_view().columns()};
    auto const handles{fighters_interface.get_handles()};
    auto const parents{fighters_interface.get_parent_handles()};
    auto const healths{fighters_interface.get_healths()};
    auto const capital_count{entities.num()};
    auto const fighter_count{handles.size()};
    std::vector<std::int32_t> counts(static_cast<std::size_t>(capital_count));
    std::vector<std::int32_t> owners(fighter_count, -1);
    for (std::size_t index{}; index < fighter_count; ++index) {
        if (is_dead(healths[index])) {
            continue;
        }
        auto const parent{entity_registry.get_current_id(parents[index])};
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
        entities.fighter_handle_spans[index] = {offset, count};
        counts[index] = offset;
        offset += count;
    }
    fighter_handles.resize(static_cast<std::size_t>(offset));
    for (std::size_t index{}; index < fighter_count; ++index) {
        if (owners[index] >= 0) {
            fighter_handles[counts[owners[index]]++] = handles[index];
        }
    }
}
/* **************************************** */
// Orders
/* **************************************** */
void Sim::queue_fighter_orders() {
    SANDBOX_PROFILE_SCOPE("Sandbox::capital_ships::Sim::queue_fighter_orders");

    auto const n_capitals{get_num_instances()};
    auto const fighter_targets{fighters_interface.get_target_handles()};
    auto const entities{this->entities.get_const_view().columns()};
    fighter_order_queue.reset();
    for (std::int32_t capital_index{}; capital_index < n_capitals; ++capital_index) {
        auto const capital_target{entities.target_handles[capital_index]};
        auto const span{entities.fighter_handle_spans[capital_index]};
        auto const end{span.end()};
        assert(span.offset >= 0 && span.count >= 0);
        assert(static_cast<std::size_t>(end) <= fighter_handles.size());

        for (auto index{span.start()}; index < end; ++index) {
            auto const fighter{fighter_handles[static_cast<std::size_t>(index)]};
            if (capital_target.is_null()) {
                fighter_order_queue.add(fighter,
                                        FighterOrder{.task = 1, .target = 1},
                                        FighterTask::Standby,
                                        capital_target);
                continue;
            }

            auto const fighter_index{
                agents_.indexes().find(entity_registry.get_current_id(fighter))};
            assert(fighter_index >= 0);
            auto const target{fighter_targets[fighter_index]};
            if (!agents_.read_alive(entity_registry.get_current_id(target))) {
                fighter_order_queue.add(
                    fighter, FighterOrder{.task = 0, .target = 1}, {}, capital_target);
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
void Sim::set_target_handle(RegistryEntityHandle const ship_handle,
                            RegistryEntityHandle const target_handle) {
    assert(entity_registry.is_valid_handle(ship_handle));
    assert(entity_registry.is_valid_handle(target_handle));
    auto const entities{this->entities.get_view().columns()};
    auto const found{std::ranges::find(entities.handles, ship_handle)};
    assert(found != entities.handles.end());
    auto const entity_index{std::distance(entities.handles.begin(), found)};
    entities.target_handles[entity_index] = target_handle;
}

/* **************************************** */
// Death handling
/* **************************************** */
void Sim::handle_dead_entities() {
    SANDBOX_PROFILE_SCOPE("Sandbox::capital_ships::Sim::handle_dead_entities");
    if (local_indices_to_remove.empty()) {
        return;
    }

    batch::sort_and_deduplicate_removal_indices(local_indices_to_remove);
    auto const entities{this->entities.get_const_view().columns()};

    for (auto const index : local_indices_to_remove) {
        frame_changes_.push_back({.kind = EntityFrameChangeKind::RemoveSwap,
                                  .index = index,
                                  .handle = entities.handles[index]});
    }

    for (auto const index : local_indices_to_remove) {
        this->entities.remove_at_swap(index, 1);
    }
}
void Sim::reassign_fighter_handles_of_dying_capital() {
    auto const entities{this->entities.get_const_view().columns()};
    std::array<RegistryEntityHandle, static_cast<std::size_t>(Team::COUNT)> replacements{};
    auto const count{entities.num()};
    for (std::int32_t index{}; index < count; ++index) {
        auto& replacement{replacements[static_cast<std::size_t>(entities.teams[index])]};
        if (is_alive(entities.healths[index]) &&
            (replacement.is_null() || entities.entity_ids[index].index() <
                                          entity_registry.get_current_id(replacement).index())) {
            replacement = entities.handles[index];
        }
    }
    auto const handles{fighters_interface.get_handles()};
    auto const parents{fighters_interface.get_parent_handles()};
    auto const healths{fighters_interface.get_healths()};
    auto const fighter_count{handles.size()};
    for (auto const dying_index : local_indices_to_remove) {
        auto const parent{entities.handles[dying_index]};
        auto const replacement{replacements[static_cast<std::size_t>(entities.teams[dying_index])]};
        for (std::size_t index{}; index < fighter_count; ++index) {
            if (parents[index] != parent || is_dead(healths[index])) {
                continue;
            }
            if (replacement.is_null()) {
                fighter_self_destruct_requests_.push_back(handles[index]);
            } else {
                fighters_interface.set_parent_handle(handles[index], replacement);
            }
        }
    }
}

/* **************************************** */
// Misc
/* **************************************** */
void Sim::clear_tick_buffers() {
    local_indices_to_remove.clear();
    entity_update_data.reset();
    entity_death_info.reset();
}

/* **************************************** */
// Checks
/* **************************************** */
void Sim::validate_array_sizes() const {
    entities.get_const_view().columns().validate_array_sizes();
}
void Sim::validate_entity_handles() const {
    entity_registry.validate_handles(entities.get_const_view().handles());
}
} // namespace capital_ships
