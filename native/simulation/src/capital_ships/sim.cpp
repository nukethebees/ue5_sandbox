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
#include <ioj/sim/capital_ship_queries.h>
#include <ioj/sim/capital_ship_spawning.h>
#include <ioj/sim/entity_registry.h>
#include <ioj/sim/entity_registry_refresh.h>
#include <ioj/sim/entity_registry_view.h>
#include <ioj/sim/fighter_diagnostics.h>
#include <ioj/sim/fighter_frame_spawn_queue.h>
#include <ioj/sim/fighter_orders.h>
#include <ioj/sim/fighter_reassignment.h>
#include <ioj/sim/fighters/sim.h>
#include <ioj/sim/profiling.h>
#include <ioj/sim/spatial_query_manager.h>

#include <sandbox/core/frame_array.h>

namespace ioj::sim::capital_ships {

/* **************************************** */
// Configuration
/* **************************************** */
void Sim::set_config(CapitalShipSimConfig const& new_config) noexcept {
    config = new_config;
}
Sim::Sim(EntityRegistry& in_entity_registry,
         SpatialQueryManager const& in_spatial_query_manager,
         ioj::sim::fighters::Sim& fighters,
         std::pmr::memory_resource& in_frame_memory_resource)
    : entity_registry{in_entity_registry}
    , spatial_query_manager{in_spatial_query_manager}
    , frame_memory_resource{in_frame_memory_resource}
    , fighters_interface{fighters} {}

/* **************************************** */
// Sim phases
/* **************************************** */
void Sim::begin_play() {
    SANDBOX_PROFILE_SCOPE("Sandbox::capital_ships::Sim::begin_play");
    ioj::sim::profiling::plot("Sandbox/CapitalShipCount", 0);
    assert(static_cast<std::size_t>(config.fighter_spawn_slots) ==
           config.fighter_spawn_slots_relative_transforms.size());
    validate_array_sizes();
}
void Sim::begin_tick() {
    SANDBOX_PROFILE_SCOPE("Sandbox::capital_ships::Sim::begin_tick");
    tick_buffers.cycle();
    clear_tick_buffers();
}
void Sim::update_timers(float const dt) {
    SANDBOX_PROFILE_SCOPE("Sandbox::capital_ships::Sim::update_timers");
    ml::tick_countdowns(entities.fighter_spawn_timers, dt);
}
void Sim::make_decisions() {
    SANDBOX_PROFILE_SCOPE("Sandbox::capital_ships::Sim::make_decisions");

    queue_fighter_spawns();
    refresh_fighter_handles();
    fighter_reassignment_queue.reset();
    entity_registry.refresh_handles(entities.target_handles);
    ml::FrameArray<std::int32_t> indices_without_targets{&frame_memory_resource};
    auto const n_capitals{entities.target_handles.size()};
    (void)ioj::sim::collect_capitals_without_targets(
        {entities.target_handles.data(), static_cast<std::size_t>(n_capitals)},
        indices_without_targets);
    for (auto const index : indices_without_targets) {
        entities.target_handles[index] = spatial_query_manager.get_any_non_team_entity(
            entities.teams[index], ioj::sim::EntityType::CapitalShip);
    }
    queue_fighter_orders();
}
void Sim::resolve_damage_events() {
    SANDBOX_PROFILE_SCOPE("Sandbox::capital_ships::Sim::resolve_damage_events");
    ioj::sim::batch::resolve_damage_events(entity_registry,
                                           entities.handles,
                                           entities.healths,
                                           local_indices_to_remove,
                                           entity_death_info);
}
void Sim::update_entity_registry() {
    SANDBOX_PROFILE_SCOPE("Sandbox::capital_ships::Sim::update_entity_registry");
    prepare_entity_update_data();
    entity_registry.queue_entity_updates({entities.handles, entity_update_data.get_const_view()},
                                         entity_death_info);
}
void Sim::sync_from_registry() {
    SANDBOX_PROFILE_SCOPE("Sandbox::capital_ships::Sim::sync_from_registry");
    handle_dead_entities();
}
void Sim::end_tick() {
    SANDBOX_PROFILE_SCOPE("Sandbox::capital_ships::Sim::end_tick");
    ioj::sim::profiling::plot("Sandbox/CapitalShipCount", get_num_instances());
    fighters_spawned += tick_buffers.current().num();
    validate_array_sizes();
}

/* **************************************** */
// Accessors
/* **************************************** */
auto Sim::get_num_instances() const noexcept -> std::int32_t {
    return entities.num();
}
auto Sim::is_valid(RegistryEntityHandle const handle) const noexcept -> bool {
    auto const handles{
        std::span{entities.handles.data(), static_cast<std::size_t>(entities.handles.size())}};
    return handle.is_valid() && ioj::sim::find_capital_ship_index(handles, handle).has_value();
}
auto Sim::get_fighter_handles(std::int32_t const index) const noexcept
    -> std::span<RegistryEntityHandle const> {
    return get_fighter_handles(entities.fighter_handle_spans[index]);
}
auto Sim::get_fighter_handles(IndexSpan const span) const noexcept
    -> std::span<RegistryEntityHandle const> {
    return get_fighter_handles().subspan(static_cast<std::size_t>(span.offset),
                                         static_cast<std::size_t>(span.count));
}
auto Sim::get_team(RegistryEntityHandle const handle) const noexcept -> ioj::sim::Team {
    auto const handles{
        std::span{entities.handles.data(), static_cast<std::size_t>(entities.handles.size())}};
    if (auto const index{ioj::sim::find_capital_ship_index(handles, handle)}) {
        return entities.teams[*index];
    }

    ml::fatal_error("Invalid capital ship handle passed");
}
auto Sim::get_health(RegistryEntityHandle const handle) const noexcept -> std::int32_t {
    auto const handles{
        std::span{entities.handles.data(), static_cast<std::size_t>(entities.handles.size())}};
    auto const index{ioj::sim::find_capital_ship_index(handles, handle)};
    assert(index.has_value());
    return entities.healths[*index];
}
auto Sim::find_first_index_on_team(ioj::sim::Team const team) const noexcept
    -> std::optional<std::int32_t> {
    auto const n{get_num_instances()};
    auto const teams{std::span{entities.teams.data(), static_cast<std::size_t>(n)}};
    return ioj::sim::find_first_capital_ship_on_team(std::as_bytes(teams), team);
}
auto Sim::find_first_handle_on_team(ioj::sim::Team const team) const noexcept
    -> std::optional<RegistryEntityHandle> {
    auto const result{find_first_index_on_team(team)};
    return result ? std::optional<RegistryEntityHandle>{entities.handles[*result]} : std::nullopt;
}

/* **************************************** */
// Ship spawning
/* **************************************** */
auto Sim::register_ships(SpawnDataConstView const spawn_data) -> std::vector<RegistryEntityHandle> {
    SANDBOX_PROFILE_SCOPE("Sandbox::capital_ships::Sim::register_ships");
    auto const n_to_add{spawn_data.num()};
    if (n_to_add == 0) {
        return {};
    }

    auto const first_new_index{entities.num()};
    spawn_ships(spawn_data);

    RegistryEntityData new_entity_data;
    new_entity_data.add_uninitialised(n_to_add);
    for (std::int32_t i{}; i < n_to_add; ++i) {
        new_entity_data.locations.set(i, spawn_data.locations[i]);
        new_entity_data.rotations.set(i, spawn_data.rotations[i]);
    }
    new_entity_data.velocities.each_column([](auto& column) { std::ranges::fill(column, 0.f); });
    std::ranges::fill(new_entity_data.entity_types, ioj::sim::EntityType::CapitalShip);
    for (std::int32_t i{}; i < n_to_add; ++i) {
        new_entity_data.healths[i] = spawn_data.healths[i];
        new_entity_data.teams[i] = spawn_data.teams[i];
        new_entity_data.alive[i] = spawn_data.healths[i] > 0;
    }

    auto const new_entities{entity_registry.add_entities(new_entity_data.get_const_view())};
    std::vector<RegistryEntityHandle> new_handles;
    new_handles.reserve(static_cast<std::size_t>(n_to_add));
    for (std::int32_t i{}; i < n_to_add; ++i) {
        new_handles.push_back(new_entities.get_handle(i));
    }
    for (std::int32_t i{}; i < n_to_add; ++i) {
        entities.handles[first_new_index + i] = new_handles[i];
    }
    validate_array_sizes();
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
void Sim::spawn_ships(SpawnDataConstView const spawn_data) {
    SANDBOX_PROFILE_SCOPE("Sandbox::capital_ships::Sim::spawn_ships");
    spawn_data.validate_array_sizes();
    auto const n_to_add{spawn_data.num()};

    entities.add_defaulted(n_to_add);
    auto const appended{entities.right(n_to_add)};
    auto const count{static_cast<std::size_t>(n_to_add)};
    ioj::sim::capitals::initialize_spawned_ships(
        {.locations = appended.locations,
         .rotations = appended.rotations,
         .remaining_spawn_times = {appended.fighter_spawn_timers.data(), count},
         .spawn_cooldowns = {appended.fighter_spawn_cooldowns.data(), count},
         .teams = std::as_writable_bytes(std::span{appended.teams.data(), count}),
         .healths = {appended.healths.data(), count},
         .targets = {appended.target_handles.data(), count}},
        spawn_data);
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
    entity_update_data.locations = entities.locations;
    entity_update_data.rotations = entities.rotations;
    entity_update_data.velocities.each_column([](auto& column) { std::ranges::fill(column, 0.f); });
    entity_update_data.healths = entities.healths;
    entity_update_data.teams = entities.teams;
    std::ranges::fill(entity_update_data.entity_types, ioj::sim::EntityType::CapitalShip);
    for (std::int32_t i{0}; i < n; ++i) {
        entity_update_data.alive[i] = entities.healths[i] > 0;
    }
}

/* **************************************** */
// Fighter spawning
/* **************************************** */
auto Sim::get_fighter_spawn_slots() const noexcept -> std::int32_t {
    return config.fighter_spawn_slots;
}
void Sim::queue_fighter_spawns() {
    SANDBOX_PROFILE_SCOPE("Sandbox::capital_ships::Sim::queue_fighter_spawns");
    if (!diagnostics_enabled) {
        diagnostic_spawn_reports = 0;
    }

    auto& fighter_queue{tick_buffers.current()};
    fighter_queue.reset();

    auto const n_capital_ships{get_num_instances()};
    ml::FrameArray<std::int32_t> ships_ready_to_spawn_fighters_indices{&frame_memory_resource};
    ioj::sim::capitals::collect_ships_ready_to_spawn_fighters(
        {entities.fighter_spawn_timers.data(), static_cast<std::size_t>(n_capital_ships)},
        {entities.target_handles.data(), static_cast<std::size_t>(n_capital_ships)},
        ships_ready_to_spawn_fighters_indices);
    if (ships_ready_to_spawn_fighters_indices.num() == 0) {
        return;
    }

    auto const& relative_transforms{config.fighter_spawn_slots_relative_transforms};
    ioj::sim::fighters::FrameSpawnQueue fighter_spawn_wave{&frame_memory_resource};
    assert(std::in_range<std::int32_t>(relative_transforms.size()));
    fighter_spawn_wave.reserve(static_cast<std::int32_t>(relative_transforms.size()));
    for (auto const capital_index : ships_ready_to_spawn_fighters_indices) {
        fighter_spawn_wave.clear();
        auto const base_location{entities.locations[capital_index]};
        auto const base_rotation{entities.rotations[capital_index]};
        ioj::sim::Transform3d const base_transform{
            ioj::sim::to_quaternion(
                ioj::sim::Rotator3d{base_rotation.pitch, base_rotation.yaw, base_rotation.roll}),
            {base_location.X, base_location.Y, base_location.Z},
            {1.0, 1.0, 1.0}};

        for (auto const& relative_transform : relative_transforms) {
            auto const new_transform{relative_transform * base_transform};
            if (fighters::diagnostics::take_report(
                    diagnostics_enabled, diagnostic_spawn_reports, 64)) {
                /* ml::log_error(std::format("[FighterSpawn] Enqueue parentRegistryIndex={}
                   capitalIndex={} base=({}, {}, {}) slot=({}, {}, {}) world=({}, {}, {})",
                    entities.handles[capital_index].index, capital_index,
                    base_transform.location.x, base_transform.location.y, base_transform.location.z,
                    relative_transform.location.x, relative_transform.location.y,
                   relative_transform.location.z, new_transform.location.x,
                   new_transform.location.y, new_transform.location.z)); */
            }
            fighter_spawn_wave.add(ioj::sim::to_float(new_transform.location),
                                   ioj::sim::to_float(new_transform.rotator()),
                                   entities.teams[capital_index],
                                   entities.handles[capital_index],
                                   entities.target_handles[capital_index]);
        }
        auto const spawn_wave{fighter_spawn_wave.get_const_view()};
        auto const accepted_count{fighters_interface.queue_spawns(spawn_wave)};
        fighter_queue.append_from(spawn_wave.left(accepted_count));
        entities.fighter_spawn_timers[capital_index] =
            entities.fighter_spawn_cooldowns[capital_index];
    }
}
void Sim::refresh_fighter_handles() {
    SANDBOX_PROFILE_SCOPE("Sandbox::capital_ships::Sim::refresh_fighter_handles");

    auto const& previous{tick_buffers.previous()};
    [[maybe_unused]] auto const invalid_index{ioj::sim::refresh_registry_handles(
        ioj::sim::make_native_query_view(entity_registry), fighter_handles)};
    assert(invalid_index < 0);

    auto const& spawn_data{fighters_interface.get_new_spawn_entity_data()};
    spawn_data.validate_array_sizes();
    assert(previous.num() == spawn_data.num());

    auto const& spawn_handles{fighters_interface.get_new_spawn_entity_handles()};
    assert(spawn_handles.registry_handles.num() == previous.num());
    auto const n_capitals{get_num_instances()};
    auto const queue_count{static_cast<std::size_t>(previous.num())};
    ml::FrameArray<RegistryEntityHandle> fighters_to_self_destruct{&frame_memory_resource};
    [[maybe_unused]] auto const surviving_spawn_count{ioj::sim::assign_spawned_fighters(
        {entities.handles.data(), static_cast<std::size_t>(n_capitals)},
        std::as_bytes(std::span{entities.teams.data(), static_cast<std::size_t>(n_capitals)}),
        spawn_handles,
        {previous.parents.data(), queue_count},
        std::as_bytes(std::span{previous.teams.data(), queue_count}),
        ioj::sim::make_native_query_view(entity_registry),
        fighter_reassignment_queue,
        fighters_to_self_destruct)};
    for (auto const fighter : fighters_to_self_destruct) {
        fighters_interface.self_destruct_fighter(fighter);
    }

    fighter_handles_scratch.resize(fighter_handles.size() +
                                   static_cast<std::size_t>(fighter_reassignment_queue.num()));
    auto const fighter_count{ioj::sim::rebuild_fighter_rosters(
        {entities.handles.data(), static_cast<std::size_t>(n_capitals)},
        {entities.fighter_handle_spans.data(), static_cast<std::size_t>(n_capitals)},
        fighter_handles,
        fighter_reassignment_queue,
        fighter_handles_scratch)};
    fighter_handles_scratch.resize(static_cast<std::size_t>(fighter_count));

    assert(fighter_handles_scratch.size() >= static_cast<std::size_t>(surviving_spawn_count));
    fighter_handles.swap(fighter_handles_scratch);
}

/* **************************************** */
// Orders
/* **************************************** */
void Sim::queue_fighter_orders() {
    SANDBOX_PROFILE_SCOPE("Sandbox::capital_ships::Sim::queue_fighter_orders");

    auto const n_capitals{get_num_instances()};
    auto const all_fighters{fighters_interface.get_handles()};
    auto const fighter_targets{fighters_interface.get_target_handles()};
    ioj::sim::build_fighter_orders(
        {entities.target_handles.data(), static_cast<std::size_t>(n_capitals)},
        {entities.fighter_handle_spans.data(), static_cast<std::size_t>(n_capitals)},
        fighter_handles,
        {all_fighters.data(), static_cast<std::size_t>(all_fighters.size())},
        {fighter_targets.data(), static_cast<std::size_t>(fighter_targets.size())},
        ioj::sim::make_native_query_view(entity_registry),
        fighter_order_queue);

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

    ioj::sim::batch::sort_and_deduplicate_removal_indices(local_indices_to_remove);

    auto const batch_index{static_cast<std::int32_t>(deaths_.size())};
    deaths_.reserve(deaths_.size() + static_cast<std::size_t>(local_indices_to_remove.size()));
    for (auto const index : local_indices_to_remove) {
        deaths_.push_back({entities.locations[index], batch_index});
        frame_changes_.push_back({.kind = EntityFrameChangeKind::RemoveSwap,
                                  .index = index,
                                  .handle = entities.handles[index]});
    }

    reassign_fighter_handles_of_dying_capital();
    for (auto const index : local_indices_to_remove) {
        entities.remove_at_swap(index, 1);
    }
}
void Sim::reassign_fighter_handles_of_dying_capital() {
    auto const n{get_num_instances()};
    ml::FrameArray<RegistryEntityHandle> fighters_to_self_destruct{&frame_memory_resource};
    auto const teams{std::span{entities.teams.data(), static_cast<std::size_t>(n)}};
    ioj::sim::plan_fighter_reassignment(
        {entities.handles.data(), static_cast<std::size_t>(n)},
        std::as_bytes(teams),
        {entities.fighter_handle_spans.data(), static_cast<std::size_t>(n)},
        fighter_handles,
        {local_indices_to_remove.data(), static_cast<std::size_t>(local_indices_to_remove.size())},
        fighter_reassignment_queue,
        fighters_to_self_destruct);

    for (auto const fighter : fighters_to_self_destruct) {
        fighters_interface.self_destruct_fighter(fighter);
    }
}

/* **************************************** */
// Misc
/* **************************************** */
void Sim::clear_tick_buffers() {
    local_indices_to_remove.clear();
    tick_buffers.current().reset();
    entity_update_data.reset();
    entity_death_info.reset();
}

/* **************************************** */
// Checks
/* **************************************** */
void Sim::validate_array_sizes() const {
    entities.validate_array_sizes();
}
void Sim::validate_entity_handles() const {
    entity_registry.validate_handles(entities.handles);
}
} // namespace ioj::sim::capital_ships
