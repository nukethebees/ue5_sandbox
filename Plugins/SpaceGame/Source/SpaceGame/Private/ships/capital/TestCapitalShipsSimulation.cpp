#include "SpaceGame/ships/capital/TestCapitalShipsSimulation.h"

#include <SpaceGame/entities/TestBatchActorCore.h>
#include <SpaceGame/entities/TestEntityRegistry.h>
#include <SpaceGame/ships/fighters/TestCapitalShipFightersSimulation.h>
#include <SpaceGame/simulation/LevelSimulationConfig.h>
#include <SpaceGame/simulation/SpatialQueryManager.h>
#include <SpaceGame/support/logging/SandboxLogCategories.h>

#include <SandboxCore/array_checks.h>
#include <SandboxCore/array_math.h>
#include <SandboxCore/array_utils.h>
#include <SandboxCore/container_ops.h>
#include <SandboxCore/soa_rotator_utils.h>
#include <SandboxCore/soa_vector_utils.h>

#include <ProfilingDebugging/CountersTrace.h>

#include <array>

TRACE_DECLARE_INT_COUNTER(SandboxTestCapitalShipCount, TEXT("Sandbox/TestCapitalShipCount"));

namespace ml::test_capital_ships {
void Simulation::set_config(FCapitalSimulationConfig const& new_config) noexcept {
    config = new_config;
}

void Simulation::set_entity_registry(FTestEntityRegistry& new_entity_registry) noexcept {
    entity_registry = &new_entity_registry;
}

void Simulation::set_spatial_query_manager(FSpatialQueryManager const& new_query_manager) noexcept {
    spatial_query_manager = &new_query_manager;
}

void Simulation::bind_fighters(ml::test_capital_ship_fighters::Simulation& fighters) {
    fighters_interface.bind(fighters);
}

void Simulation::clear_runtime_state() {
    ml::reset(entities,
              local_indices_to_remove,
              tick_buffers.current(),
              tick_buffers.previous(),
              entity_death_info,
              entity_update_data,
              fighter_handles,
              fighter_handles_scratch,
              fighter_reassignment_queue,
              indices_without_targets_buffer,
              fighter_order_queue);
    fighters_spawned = 0;
    clear_presentation_events();
}

void Simulation::begin_play() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::test_capital_ships::Simulation::begin_play);
    TRACE_COUNTER_SET(SandboxTestCapitalShipCount, 0);
    check(entity_registry);
    check(spatial_query_manager);
    check(entity_radius > 0.f);
    ensureAlways(config.fighter_spawn_slots ==
                 config.fighter_spawn_slots_relative_transforms.Num());
    validate_array_sizes();
}

void Simulation::begin_tick() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::test_capital_ships::Simulation::begin_tick);
    tick_buffers.cycle();
    clear_tick_buffers();
    clear_presentation_events();
}

void Simulation::update_timers(float const dt) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::test_capital_ships::Simulation::update_timers);
    entities.fighter_spawn_timers.tick(dt);
}

void Simulation::make_decisions() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::test_capital_ships::Simulation::make_decisions);

    queue_fighter_spawns();
    refresh_fighter_handles();
    fighter_reassignment_queue.reset();
    ml::batch::refresh_targets(*entity_registry,
                               *spatial_query_manager,
                               entities.target_handles,
                               indices_without_targets_buffer,
                               entities.teams,
                               ETestEntityType::CapitalShip);
    queue_fighter_orders();
}

void Simulation::resolve_damage_events() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::test_capital_ships::Simulation::resolve_damage_events);
    ml::batch::resolve_damage_events(*entity_registry,
                                     entities.handles,
                                     entities.healths,
                                     local_indices_to_remove,
                                     entity_death_info);
}

void Simulation::update_entity_registry() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::test_capital_ships::Simulation::update_entity_registry);
    prepare_entity_update_data();
    entity_registry->queue_entity_updates({entities.handles, entity_update_data.get_const_view()},
                                          entity_death_info);
}

void Simulation::sync_from_registry() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::test_capital_ships::Simulation::sync_from_registry);
    handle_dead_entities();
}

void Simulation::end_tick() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::test_capital_ships::Simulation::end_tick);
    TRACE_COUNTER_SET(SandboxTestCapitalShipCount, get_num_instances());
    fighters_spawned += ml::num(tick_buffers.current().fighter_queue);
    validate_array_sizes();
}

auto Simulation::get_num_instances() const noexcept -> int32 {
    return entities.handles.Num();
}

auto Simulation::is_valid(FRegistryEntityHandle const handle) const noexcept -> bool {
    return handle.is_valid() && entities.handles.Find(handle) != INDEX_NONE;
}

auto Simulation::get_fighter_handles(int32 const index) const noexcept
    -> TConstArrayView<FRegistryEntityHandle> {
    return get_fighter_handles(entities.capital_fighter_handle_spans[index]);
}

auto Simulation::get_fighter_handles(FIndexSpan const span) const noexcept
    -> TConstArrayView<FRegistryEntityHandle> {
    return TConstArrayView<FRegistryEntityHandle>{fighter_handles}.Slice(span.offset, span.count);
}

auto Simulation::get_team(FRegistryEntityHandle const handle) const noexcept -> ETestTeam {
    auto const n{get_num_instances()};
    for (int32 i{}; i < n; ++i) {
        if (handle == entities.handles[i]) {
            return entities.teams[i];
        }
    }

    UE_LOG(LogSandbox, Fatal, TEXT("Invalid capital ship handle passed"));
    return ETestTeam::White;
}

auto Simulation::get_health(FRegistryEntityHandle const handle) const noexcept -> int32 {
    auto const index{entities.handles.Find(handle)};
    check(index != INDEX_NONE);
    return entities.healths[index];
}

auto Simulation::find_first_index_on_team(ETestTeam const team) const noexcept
    -> std::optional<int32> {
    auto const n{get_num_instances()};
    for (int32 i{0}; i < n; ++i) {
        if (entities.teams[i] == team) {
            return i;
        }
    }
    return {};
}

auto Simulation::find_first_handle_on_team(ETestTeam const team) const noexcept
    -> std::optional<FRegistryEntityHandle> {
    auto const result{find_first_index_on_team(team)};
    return result ? std::optional<FRegistryEntityHandle>{entities.handles[*result]} : std::nullopt;
}

auto Simulation::register_ships(SpawnDataConstView const spawn_data)
    -> TArray<FRegistryEntityHandle> {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::test_capital_ships::Simulation::register_ships);
    auto const n_to_add{spawn_data.num()};
    if (n_to_add == 0) {
        return {};
    }

    auto const first_new_index{entities.num()};
    entities.handles.AddDefaulted(n_to_add);
    spawn_ships(spawn_data);
    presentation_spawn_start = first_new_index;
    presentation_spawn_count = n_to_add;

    RegistryEntityData new_entity_data;
    new_entity_data.add_uninitialised(n_to_add);
    ml::assign_from(new_entity_data.locations, spawn_data.locations);
    ml::fill(new_entity_data.velocities, 0.f);
    ml::fill(new_entity_data.radii, entity_radius);
    new_entity_data.set_all_entity_types(ETestEntityType::CapitalShip);
    for (int32 i{}; i < n_to_add; ++i) {
        new_entity_data.healths[i] = spawn_data.healths[i];
        new_entity_data.teams[i] = spawn_data.teams[i];
        new_entity_data.alive[i] = spawn_data.healths[i] > 0;
    }

    auto const new_entities{entity_registry->add_entities(new_entity_data.get_const_view())};
    auto new_handles{new_entities.registry_handles.to_array()};
    for (int32 i{}; i < n_to_add; ++i) {
        entities.handles[first_new_index + i] = new_handles[i];
    }
    validate_array_sizes();
    return new_handles;
}

void Simulation::set_target_handle(FRegistryEntityHandle const ship_handle,
                                   FRegistryEntityHandle const target_handle) {
    check(entity_registry->is_valid_handle(ship_handle));
    check(entity_registry->is_valid_handle(target_handle));
    auto const entity_index{entities.handles.Find(ship_handle)};
    check(entity_index != INDEX_NONE);
    entities.target_handles[entity_index] = target_handle;
}

void Simulation::spawn_ships(SpawnDataConstView const spawn_data) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::test_capital_ships::Simulation::spawn_ships);
    spawn_data.validate_array_sizes();
    auto const n_to_add{spawn_data.num()};

    ml::append_from(entities.locations, spawn_data.locations);
    ml::append_from(entities.rotations, spawn_data.rotations);
    ml::append_from(entities.fighter_spawn_timers.remaining_times, spawn_data.initial_spawn_delays);
    entities.fighter_spawn_cooldowns.Append(spawn_data.spawn_cooldowns);
    entities.teams.Append(spawn_data.teams);
    entities.healths.Append(spawn_data.healths);
    entities.capital_fighter_handle_spans.AddZeroed(n_to_add);
    entities.target_handles.Append(spawn_data.target_handles);
    validate_array_sizes();
}

void Simulation::prepare_entity_update_data() {
    TRACE_CPUPROFILER_EVENT_SCOPE(
        Sandbox::test_capital_ships::Simulation::prepare_entity_update_data);
    check(entity_update_data.num() == 0);
    auto const n{get_num_instances()};
    entity_update_data.add_uninitialised(n);
    entity_update_data.locations = entities.locations;
    ml::fill(entity_update_data.velocities, 0.f);
    ml::fill(entity_update_data.radii, entity_radius);
    entity_update_data.healths = entities.healths;
    entity_update_data.teams = entities.teams;
    entity_update_data.set_all_entity_types(ETestEntityType::CapitalShip);
    for (int32 i{0}; i < n; ++i) {
        entity_update_data.alive[i] = entities.healths[i] > 0;
    }
}

auto Simulation::get_fighter_spawn_slots() const noexcept -> int32 {
    return config.fighter_spawn_slots;
}

void Simulation::queue_fighter_spawns() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::test_capital_ships::Simulation::queue_fighter_spawns);

    auto& data{tick_buffers.current()};
    auto& fighter_queue{data.fighter_queue};
    fighter_queue.reset();

    auto const n_capital_ships{get_num_instances()};
    data.ships_ready_to_spawn_fighters_buffer.SetNumUninitialized(n_capital_ships,
                                                                  EAllowShrinking::No);
    auto ships_ready_to_spawn_fighters_indices{ml::collect_indices_less_equal(
        entities.fighter_spawn_timers.get_const_view().remaining_times,
        0.f,
        data.ships_ready_to_spawn_fighters_buffer)};
    data.ships_ready_to_spawn_fighters_buffer.SetNumUninitialized(
        ships_ready_to_spawn_fighters_indices.Num(), EAllowShrinking::No);

    auto const n_ready_to_spawn{ships_ready_to_spawn_fighters_indices.Num()};
    for (int32 i{n_ready_to_spawn - 1}; i >= 0; --i) {
        auto const capital_index{ships_ready_to_spawn_fighters_indices[i]};
        if (entities.target_handles[capital_index].is_null()) {
            data.ships_ready_to_spawn_fighters_buffer.RemoveAtSwap(i, EAllowShrinking::No);
        }
    }
    ships_ready_to_spawn_fighters_indices = data.ships_ready_to_spawn_fighters_buffer;
    if (ships_ready_to_spawn_fighters_indices.IsEmpty()) {
        return;
    }

    auto const relative_transforms{config.fighter_spawn_slots_relative_transforms};
    for (auto const capital_index : ships_ready_to_spawn_fighters_indices) {
        auto const base_location{ml::get_vector3f(entities.locations, capital_index)};
        auto const base_rotation{ml::get_rotator3f(entities.rotations, capital_index)};
        FTransform const base_transform{
            FRotator{base_rotation},
            FVector{base_location},
            FVector::OneVector,
        };

        for (auto const& relative_transform : relative_transforms) {
            auto const new_transform{relative_transform * base_transform};
            ml::append(fighter_queue.locations, new_transform.GetLocation());
            ml::append(fighter_queue.rotations, new_transform.Rotator());
            fighter_queue.teams.Add(entities.teams[capital_index]);
            fighter_queue.targets.Add(entities.target_handles[capital_index]);
        }
        entities.fighter_spawn_timers.remaining_times[capital_index] =
            entities.fighter_spawn_cooldowns[capital_index];
    }

    fighters_interface.queue_spawns(fighter_queue);
}

void Simulation::refresh_fighter_handles() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::test_capital_ships::Simulation::refresh_fighter_handles);

    fighter_handles_scratch.Reset();
    auto const& previous{tick_buffers.previous()};
    entity_registry->refresh_handles(fighter_handles);

    auto const& spawn_data{fighters_interface.get_new_spawn_entity_data()};
    spawn_data.validate_array_sizes();
    auto const n_spawned_per_capital{get_fighter_spawn_slots()};
    ensure(previous.fighter_queue.num() == spawn_data.num());

    int32 spawning_capital_index{0};
    int32 spawned_fighter_index{0};
    auto const& spawn_handles{fighters_interface.get_new_spawn_entity_handles()};
    auto const n_capitals{get_num_instances()};
    for (int32 capital_index{0}; capital_index < n_capitals; ++capital_index) {
        FIndexSpan new_span{.offset = fighter_handles_scratch.Num(), .count = 0};

        auto const old_span{entities.capital_fighter_handle_spans[capital_index]};
        auto const old_end{old_span.end()};
        for (int32 fighter_index{old_span.offset}; fighter_index < old_end; ++fighter_index) {
            auto const fighter_handle{fighter_handles[fighter_index]};
            if (!fighter_handle.is_null()) {
                fighter_handles_scratch.Add(fighter_handle);
                ++new_span.count;
            }
        }

        if (previous.ships_ready_to_spawn_fighters_buffer.IsValidIndex(spawning_capital_index) &&
            previous.ships_ready_to_spawn_fighters_buffer[spawning_capital_index] ==
                capital_index) {
            auto const end{spawned_fighter_index + n_spawned_per_capital};
            for (; spawned_fighter_index < end; ++spawned_fighter_index, ++new_span.count) {
                fighter_handles_scratch.Add(spawn_handles.registry_handles[spawned_fighter_index]);
            }
            ++spawning_capital_index;
        }

        auto const n_reassigned{fighter_reassignment_queue.num()};
        for (int32 reassigned_index{n_reassigned - 1}; reassigned_index >= 0; --reassigned_index) {
            auto const new_capital_handle{
                fighter_reassignment_queue.capital_handles[reassigned_index]};
            auto const new_capital_index{entities.handles.Find(new_capital_handle)};
            check(new_capital_index != INDEX_NONE);
            if (capital_index == new_capital_index) {
                fighter_handles_scratch.Add(
                    fighter_reassignment_queue.fighter_handles[reassigned_index]);
                fighter_reassignment_queue.fighter_handles.RemoveAtSwap(reassigned_index,
                                                                        EAllowShrinking::No);
                ++new_span.count;
            }
        }

        entities.capital_fighter_handle_spans[capital_index] = new_span;
    }

    check(fighter_handles_scratch.Num() >= spawn_data.num());
    Swap(fighter_handles, fighter_handles_scratch);
}

void Simulation::queue_fighter_orders() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::test_capital_ships::Simulation::queue_fighter_orders);

    auto const n_capitals{get_num_instances()};
    fighter_order_queue.reset();
    for (int32 capital_index{0}; capital_index < n_capitals; ++capital_index) {
        auto const capital_target{entities.target_handles[capital_index]};
        auto const span{entities.capital_fighter_handle_spans[capital_index]};
        auto const end{span.end()};
        for (int32 fighter_span_index{span.start()}; fighter_span_index < end;
             ++fighter_span_index) {
            auto const fighter_handle{fighter_handles[fighter_span_index]};
            if (capital_target.is_null()) {
                fighter_order_queue.add(fighter_handle,
                                        TestCapitalShipFighterOrderQueue::Order{
                                            .task = 1,
                                            .target = 1,
                                        },
                                        ETestCapitalShipFightersTask::Standby,
                                        capital_target);
                continue;
            }

            auto const fighter_target{fighters_interface.get_target_handle(fighter_handle)};
            if (fighter_target.is_null() || entity_registry->is_valid_dead(fighter_target)) {
                fighter_order_queue.add(fighter_handle,
                                        TestCapitalShipFighterOrderQueue::Order{
                                            .task = 0,
                                            .target = 1,
                                        },
                                        {},
                                        capital_target);
            }
        }
    }

    if (fighter_order_queue.num() > 0) {
        fighters_interface.queue_orders(fighter_order_queue);
    }
}

void Simulation::handle_dead_entities() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::test_capital_ships::Simulation::handle_dead_entities);
    if (local_indices_to_remove.IsEmpty()) {
        return;
    }

    ml::batch::sort_and_deduplicate_removal_indices(local_indices_to_remove);
    presentation_indices_to_remove = local_indices_to_remove;
    presentation_death_locations.Reserve(local_indices_to_remove.Num());
    for (auto const index : local_indices_to_remove) {
        presentation_death_locations.Add(ml::get_vector3f(entities.locations, index));
    }

    reassign_fighter_handles_of_dying_capital();
    ml::remove_at_swap_many_sorted_desc(local_indices_to_remove, entities);
}

void Simulation::reassign_fighter_handles_of_dying_capital() {
    std::array<int32, static_cast<std::size_t>(ETestTeam::COUNT)> replacements{};
    replacements.fill(-1);

    constexpr auto team_count{ml::EnumCountTrait<ETestTeam>::count_value};
    TArray<ETestTeam, TInlineAllocator<team_count>> teams_to_replace;
    for (auto const capital_index : local_indices_to_remove) {
        auto const team{entities.teams[capital_index]};
        if (!teams_to_replace.Contains(team)) {
            teams_to_replace.Add(team);
        }
    }

    auto const n{get_num_instances()};
    for (int32 i{0}; i < n; ++i) {
        auto const team{entities.teams[i]};
        if (teams_to_replace.Contains(team) && !local_indices_to_remove.Contains(i)) {
            replacements[std::to_underlying(team)] = i;
            teams_to_replace.RemoveSwap(team, EAllowShrinking::No);
        }
        if (teams_to_replace.IsEmpty()) {
            break;
        }
    }

    for (auto const capital_index : local_indices_to_remove) {
        auto const replacement_index{
            replacements[std::to_underlying(entities.teams[capital_index])]};
        auto const fighter_span{entities.capital_fighter_handle_spans[capital_index]};
        auto const span_end{fighter_span.end()};
        if (replacement_index < 0) {
            for (int32 i{fighter_span.offset}; i < span_end; ++i) {
                fighters_interface.self_destruct_fighter(fighter_handles[i]);
            }
        } else {
            for (int32 i{fighter_span.offset}; i < span_end; ++i) {
                fighter_reassignment_queue.add(entities.handles[replacement_index],
                                               fighter_handles[i]);
            }
        }
    }
}

void Simulation::clear_tick_buffers() {
    ml::reset(local_indices_to_remove,
              tick_buffers.current(),
              entity_update_data,
              entity_death_info,
              fighter_handles_scratch);
}

void Simulation::clear_presentation_events() {
    presentation_indices_to_remove.Reset();
    presentation_death_locations.Reset();
    presentation_spawn_start = 0;
    presentation_spawn_count = 0;
}

void Simulation::validate_array_sizes() const {
    entities.validate_array_sizes();
}

void Simulation::validate_proxy_handles() const {
    entity_registry->validate_handles(entities.handles);
}
} // namespace ml::test_capital_ships
