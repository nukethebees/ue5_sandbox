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
Sim::Sim(EntityRegistry& in_entity_registry,
         SpatialQueryManager const& in_spatial_query_manager,
         fighters::Sim& fighters,
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
    profiling::plot("Sandbox/CapitalShipCount", 0);
    assert(static_cast<std::size_t>(config.fighter_spawn_slots) ==
           config.fighter_spawn_slots_relative_transforms.size());
    validate_array_sizes();
}
void Sim::prepare_tick(float const dt) {
    SANDBOX_PROFILE_SCOPE("Sandbox::capital_ships::Sim::prepare_tick");
    tick_buffers.cycle();
    clear_tick_buffers();
    fighter_self_destruct_requests_.clear();
    ml::tick_countdowns(entities.fighter_spawn_timers, dt);
}
void Sim::think(float const) {
    SANDBOX_PROFILE_SCOPE("Sandbox::capital_ships::Sim::think");

    queue_fighter_spawns();
    refresh_fighter_handles();
    fighter_reassignment_queue.reset();
    entity_registry.refresh_handles(entities.target_handles);
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
    batch::resolve_damage_events(entity_registry,
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
void Sim::cleanup_entities() {
    SANDBOX_PROFILE_SCOPE("Sandbox::capital_ships::Sim::cleanup_entities");
    handle_dead_entities();
}
void Sim::finish_action() {
    SANDBOX_PROFILE_SCOPE("Sandbox::capital_ships::Sim::finish_action");
    profiling::plot("Sandbox/CapitalShipCount", get_num_instances());
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
    return handle.is_valid() &&
           std::ranges::find(entities.handles, handle) != entities.handles.end();
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
auto Sim::get_team(RegistryEntityHandle const handle) const noexcept -> Team {
    auto const found{std::ranges::find(entities.handles, handle)};
    if (found != entities.handles.end()) {
        return entities.teams[found - entities.handles.begin()];
    }

    ml::fatal_error("Invalid capital ship handle passed");
}
auto Sim::get_health(RegistryEntityHandle const handle) const noexcept -> std::int32_t {
    auto const found{std::ranges::find(entities.handles, handle)};
    assert(found != entities.handles.end());
    return entities.healths[found - entities.handles.begin()];
}
auto Sim::find_first_index_on_team(Team const team) const noexcept -> std::optional<std::int32_t> {
    auto const found{std::ranges::find(entities.teams, team)};
    if (found == entities.teams.end()) {
        return std::nullopt;
    }
    return static_cast<std::int32_t>(found - entities.teams.begin());
}
auto Sim::find_first_handle_on_team(Team const team) const noexcept
    -> std::optional<RegistryEntityHandle> {
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

    RegistryEntityData new_entity_data;
    new_entity_data.add_uninitialised(n_to_add);
    for (std::int32_t i{}; i < n_to_add; ++i) {
        new_entity_data.locations.set(i, spawn_data.locations[i]);
        new_entity_data.rotations.set(i, spawn_data.rotations[i]);
    }
    new_entity_data.velocities.each_column([](auto& column) { std::ranges::fill(column, 0.f); });
    std::ranges::fill(new_entity_data.entity_types, EntityType::CapitalShip);
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
void Sim::spawn_ships(CapitalSpawnDataConstView const spawn_data) {
    SANDBOX_PROFILE_SCOPE("Sandbox::capital_ships::Sim::spawn_ships");
    spawn_data.validate_array_sizes();
    auto const n_to_add{spawn_data.num()};

    entities.add_defaulted(n_to_add);
    auto const appended{entities.right(n_to_add)};
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
    entity_update_data.locations = entities.locations;
    entity_update_data.rotations = entities.rotations;
    entity_update_data.velocities.each_column([](auto& column) { std::ranges::fill(column, 0.f); });
    entity_update_data.healths = entities.healths;
    entity_update_data.teams = entities.teams;
    std::ranges::fill(entity_update_data.entity_types, EntityType::CapitalShip);
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
    if (!diagnostics_enabled_) {
        diagnostic_spawn_reports = 0;
    }

    auto& fighter_queue{tick_buffers.current()};
    fighter_queue.reset();

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
        auto const accepted_count{fighters_interface.queue_spawns(spawn_wave)};
        fighter_queue.append_from(spawn_wave.left(accepted_count));
        entities.fighter_spawn_timers[capital_index] =
            entities.fighter_spawn_cooldowns[capital_index];
    }
}
void Sim::refresh_fighter_handles() {
    SANDBOX_PROFILE_SCOPE("Sandbox::capital_ships::Sim::refresh_fighter_handles");

    auto const& previous{tick_buffers.previous()};
    [[maybe_unused]] auto const invalid_index{
        refresh_registry_handles(make_native_query_view(entity_registry), fighter_handles)};
    assert(invalid_index < 0);

    auto const& spawn_data{fighters_interface.get_new_spawn_entity_data()};
    spawn_data.validate_array_sizes();
    assert(previous.num() == spawn_data.num());

    auto const& spawn_handles{fighters_interface.get_new_spawn_entity_handles()};
    assert(spawn_handles.registry_handles.num() == previous.num());
    auto const n_capitals{get_num_instances()};
    auto const queue_count{static_cast<std::size_t>(previous.num())};
    ml::FrameArray<RegistryEntityHandle> fighters_to_self_destruct{&frame_memory_resource};
    auto const capital_handles{std::span<RegistryEntityHandle const>{
        entities.handles.data(), static_cast<std::size_t>(n_capitals)}};
    auto const registry{make_native_query_view(entity_registry)};
    auto const spawn_count{std::min(static_cast<std::size_t>(spawn_handles.num()), queue_count)};
    [[maybe_unused]] std::int32_t surviving_spawn_count{};
    for (std::size_t spawn_index{}; spawn_index < spawn_count; ++spawn_index) {
        auto const fighter{spawn_handles.get_handle(static_cast<std::int32_t>(spawn_index))};
        if (!is_valid_alive(registry, fighter)) {
            continue;
        }

        auto destination{previous.parents[spawn_index]};
        if (std::ranges::find(capital_handles, destination) == capital_handles.end()) {
            auto const team{previous.teams[spawn_index]};
            auto const replacement{std::ranges::find(entities.teams, team)};
            if (replacement == entities.teams.end()) {
                fighters_to_self_destruct.add(fighter);
                continue;
            }
            destination = entities.handles[replacement - entities.teams.begin()];
        }

        fighter_reassignment_queue.add(destination, fighter);
        ++surviving_spawn_count;
    }
    for (auto const fighter : fighters_to_self_destruct) {
        fighter_self_destruct_requests_.push_back(fighter);
    }

    fighter_handles_scratch.resize(fighter_handles.size() +
                                   static_cast<std::size_t>(fighter_reassignment_queue.num()));
    std::int32_t fighter_count{};
    for (std::int32_t capital_index{}; capital_index < n_capitals; ++capital_index) {
        auto const old_span{entities.fighter_handle_spans[capital_index]};
        auto const old_end{old_span.end()};
        assert(old_span.offset >= 0 && old_span.count >= 0);
        assert(static_cast<std::size_t>(old_end) <= fighter_handles.size());
        IndexSpan new_span{.offset = fighter_count, .count = 0};
        for (auto fighter_index{old_span.offset}; fighter_index < old_end; ++fighter_index) {
            auto const fighter{fighter_handles[static_cast<std::size_t>(fighter_index)]};
            if (!fighter.is_null()) {
                fighter_handles_scratch[static_cast<std::size_t>(fighter_count++)] = fighter;
            }
        }

        for (auto index{fighter_reassignment_queue.num() - 1}; index >= 0; --index) {
            auto const destination{fighter_reassignment_queue.capital_handles[index]};
            auto const found{std::ranges::find(capital_handles, destination)};
            assert(found != capital_handles.end());
            if (found - capital_handles.begin() == capital_index) {
                fighter_handles_scratch[static_cast<std::size_t>(fighter_count++)] =
                    fighter_reassignment_queue.fighter_handles[index];
                fighter_reassignment_queue.remove_at_swap(index, 1);
            }
        }
        new_span.count = fighter_count - new_span.offset;
        entities.fighter_handle_spans[capital_index] = new_span;
    }
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
    auto const registry{make_native_query_view(entity_registry)};
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

            auto const found{std::ranges::find(all_fighters, fighter)};
            assert(found != all_fighters.end());
            auto const fighter_index{static_cast<std::size_t>(found - all_fighters.begin())};
            auto const target{fighter_targets[fighter_index]};
            auto const target_is_dead{analyse_handle(registry, target) ==
                                          RegistryHandleState::Active &&
                                      registry.alive[static_cast<std::size_t>(target.index)] == 0};
            if (target.is_null() || target_is_dead) {
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

    auto const batch_index{static_cast<std::int32_t>(deaths_.size())};
    deaths_.reserve(deaths_.size() + static_cast<std::size_t>(local_indices_to_remove.size()));
    for (auto const index : local_indices_to_remove) {
        deaths_.push_back({entities.locations[index], batch_index});
        frame_changes_.push_back({.kind = EntityFrameChangeKind::RemoveSwap,
                                  .index = index,
                                  .handle = entities.handles[index]});
    }

    for (auto const index : local_indices_to_remove) {
        entities.remove_at_swap(index, 1);
    }
}
void Sim::reassign_fighter_handles_of_dying_capital() {
    auto const n{get_num_instances()};
    ml::FrameArray<RegistryEntityHandle> fighters_to_self_destruct{&frame_memory_resource};
    constexpr auto team_count{static_cast<std::size_t>(Team::COUNT)};
    std::array<std::int32_t, team_count> replacements{};
    replacements.fill(-1);
    std::array<bool, team_count> needs_replacement{};

    for (auto const capital_index : local_indices_to_remove) {
        assert(capital_index >= 0 && capital_index < n);
        needs_replacement[static_cast<std::size_t>(entities.teams[capital_index])] = true;
    }

    auto teams_remaining{static_cast<std::int32_t>(std::ranges::count(needs_replacement, true))};
    for (std::int32_t capital_index{}; capital_index < n && teams_remaining > 0; ++capital_index) {
        auto const team{static_cast<std::size_t>(entities.teams[capital_index])};
        if (!needs_replacement[team] || std::ranges::find(local_indices_to_remove, capital_index) !=
                                            local_indices_to_remove.end()) {
            continue;
        }

        replacements[team] = capital_index;
        needs_replacement[team] = false;
        --teams_remaining;
    }

    for (auto const capital_index : local_indices_to_remove) {
        auto const team{static_cast<std::size_t>(entities.teams[capital_index])};
        auto const replacement_index{replacements[team]};
        auto const fighter_span{entities.fighter_handle_spans[capital_index]};
        assert(fighter_span.offset >= 0 && fighter_span.count >= 0);
        assert(static_cast<std::size_t>(fighter_span.end()) <= fighter_handles.size());

        for (auto fighter_index{fighter_span.offset}; fighter_index < fighter_span.end();
             ++fighter_index) {
            auto const fighter{fighter_handles[static_cast<std::size_t>(fighter_index)]};
            if (replacement_index < 0) {
                fighters_to_self_destruct.add(fighter);
            } else {
                fighter_reassignment_queue.add(entities.handles[replacement_index], fighter);
            }
        }
    }

    auto const& new_spawns{tick_buffers.current()};
    auto const& new_handles{fighters_interface.get_new_spawn_entity_handles()};
    assert(new_spawns.num() == new_handles.num());
    auto const new_count{new_spawns.num()};
    for (std::int32_t index{}; index < new_count; ++index) {
        auto const parent{new_spawns.parents[index]};
        auto const found{std::ranges::find(entities.handles, parent)};
        if (found == entities.handles.end()) {
            continue;
        }
        auto const parent_index{static_cast<std::int32_t>(found - entities.handles.begin())};
        if (std::ranges::contains(local_indices_to_remove, parent_index) &&
            replacements[static_cast<std::size_t>(new_spawns.teams[index])] < 0) {
            fighters_to_self_destruct.add(new_handles.get_handle(index));
        }
    }

    for (auto const fighter : fighters_to_self_destruct) {
        fighter_self_destruct_requests_.push_back(fighter);
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
} // namespace capital_ships
