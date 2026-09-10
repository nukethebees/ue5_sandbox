#include "SpaceGameSimulation/ships/capital/TestCapitalShipsSimulation.h"

#include <SpaceGameSimulation/entities/BatchSimulation.h>
#include <SpaceGameSimulation/entities/TestEntityRegistry.h>
#include <SpaceGameSimulation/ships/fighters/TestCapitalShipFighterFrameSpawnQueue.h>
#include <SpaceGameSimulation/ships/fighters/TestCapitalShipFightersSimulation.h>
#include <SpaceGameSimulation/simulation/FighterDiagnostics.h>
#include <SpaceGameSimulation/simulation/LevelSimulationConfig.h>
#include <SpaceGameSimulation/simulation/SpatialQueryManager.h>
#include <SpaceGameSimulation/support/logging/SandboxLogCategories.h>

#include <SandboxCore/array_checks.h>
#include <SandboxCore/array_math.h>
#include <SandboxCore/array_utils.h>
#include <SandboxCore/container_ops.h>
#include <SandboxCore/frame_array.h>
#include <SandboxCore/soa_rotator_utils.h>
#include <SandboxCore/soa_vector_utils.h>

#include <ProfilingDebugging/CountersTrace.h>

#include <array>

TRACE_DECLARE_INT_COUNTER(SandboxTestCapitalShipCount, TEXT("Sandbox/TestCapitalShipCount"));

namespace ml::test_capital_ships {

/* **************************************** */
// Configuration
/* **************************************** */
void Simulation::set_config(FCapitalSimulationConfig const& new_config) noexcept {
    config = new_config;
}
Simulation::Simulation(FTestEntityRegistry& in_entity_registry,
                       FSpatialQueryManager const& in_spatial_query_manager,
                       ml::test_capital_ship_fighters::Simulation& fighters,
                       std::pmr::memory_resource& in_frame_memory_resource)
    : entity_registry{in_entity_registry}
    , spatial_query_manager{in_spatial_query_manager}
    , frame_memory_resource{in_frame_memory_resource}
    , fighters_interface{fighters} {}

/* **************************************** */
// Simulation phases
/* **************************************** */
void Simulation::begin_play() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::test_capital_ships::Simulation::begin_play);
    TRACE_COUNTER_SET(SandboxTestCapitalShipCount, 0);
    check(entity_radius > 0.f);
    ensureAlways(config.fighter_spawn_slots ==
                 config.fighter_spawn_slots_relative_transforms.Num());
    validate_array_sizes();
}
void Simulation::begin_tick() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::test_capital_ships::Simulation::begin_tick);
    tick_buffers.cycle();
    clear_tick_buffers();
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
    entity_registry.refresh_handles(entities.target_handles);
    TFrameArray<int32> indices_without_targets{&frame_memory_resource};
    auto const n_capitals{entities.target_handles.Num()};
    indices_without_targets.reserve(n_capitals);
    for (int32 i{}; i < n_capitals; ++i) {
        if (entities.target_handles[i].is_null()) {
            indices_without_targets.add(i);
        }
    }
    for (auto const index : indices_without_targets) {
        entities.target_handles[index] = spatial_query_manager.get_any_non_team_entity(
            entities.teams[index], ETestEntityType::CapitalShip);
    }
    queue_fighter_orders();
}
void Simulation::resolve_damage_events() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::test_capital_ships::Simulation::resolve_damage_events);
    ml::batch::resolve_damage_events(entity_registry,
                                     entities.handles,
                                     entities.healths,
                                     local_indices_to_remove,
                                     entity_death_info);
}
void Simulation::update_entity_registry() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::test_capital_ships::Simulation::update_entity_registry);
    prepare_entity_update_data();
    entity_registry.queue_entity_updates({entities.handles, entity_update_data.get_const_view()},
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

/* **************************************** */
// Accessors
/* **************************************** */
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

/* **************************************** */
// Ship spawning
/* **************************************** */
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

    RegistryEntityData new_entity_data;
    new_entity_data.add_uninitialised(n_to_add);
    ml::assign_from(new_entity_data.locations, spawn_data.locations);
    for (int32 i{}; i < n_to_add; ++i) {
        ml::assign(new_entity_data.rotations, i, ml::get_rotator3d(spawn_data.rotations, i));
    }
    ml::fill(new_entity_data.velocities, 0.f);
    ml::fill(new_entity_data.radii, entity_radius);
    new_entity_data.set_all_entity_types(ETestEntityType::CapitalShip);
    for (int32 i{}; i < n_to_add; ++i) {
        new_entity_data.healths[i] = spawn_data.healths[i];
        new_entity_data.teams[i] = spawn_data.teams[i];
        new_entity_data.alive[i] = spawn_data.healths[i] > 0;
    }

    auto const new_entities{entity_registry.add_entities(new_entity_data.get_const_view())};
    auto new_handles{new_entities.registry_handles.to_array()};
    for (int32 i{}; i < n_to_add; ++i) {
        entities.handles[first_new_index + i] = new_handles[i];
    }
    validate_array_sizes();
    for (int32 i{}; i < n_to_add; ++i) {
        auto const index{first_new_index + i};
        frame_changes_.Add({.kind = EEntityFrameChange::Spawn,
                            .index = index,
                            .transform = FTransform{ml::get_rotator3d(entities.rotations, index),
                                                    ml::get_vector3d(entities.locations, index)},
                            .team = entities.teams[index],
                            .handle = entities.handles[index]});
    }
    return new_handles;
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

/* **************************************** */
// Entity data
/* **************************************** */
void Simulation::prepare_entity_update_data() {
    TRACE_CPUPROFILER_EVENT_SCOPE(
        Sandbox::test_capital_ships::Simulation::prepare_entity_update_data);
    check(entity_update_data.num() == 0);
    auto const n{get_num_instances()};
    entity_update_data.add_uninitialised(n);
    entity_update_data.locations = entities.locations;
    entity_update_data.rotations = entities.rotations;
    ml::fill(entity_update_data.velocities, 0.f);
    ml::fill(entity_update_data.radii, entity_radius);
    entity_update_data.healths = entities.healths;
    entity_update_data.teams = entities.teams;
    entity_update_data.set_all_entity_types(ETestEntityType::CapitalShip);
    for (int32 i{0}; i < n; ++i) {
        entity_update_data.alive[i] = entities.healths[i] > 0;
    }
}

/* **************************************** */
// Fighter spawning
/* **************************************** */
auto Simulation::get_fighter_spawn_slots() const noexcept -> int32 {
    return config.fighter_spawn_slots;
}
void Simulation::queue_fighter_spawns() {
    if (fighter_diagnostics::enabled.GetValueOnGameThread() == 0) {
        diagnostic_spawn_reports = 0;
    }
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::test_capital_ships::Simulation::queue_fighter_spawns);

    auto& data{tick_buffers.current()};
    auto& fighter_queue{data.fighter_queue};
    fighter_queue.reset();

    auto const n_capital_ships{get_num_instances()};
    TFrameArray<int32> ships_ready_to_spawn_fighters_buffer{&frame_memory_resource};
    ships_ready_to_spawn_fighters_buffer.set_num(n_capital_ships);
    auto ships_ready_to_spawn_fighters_indices{ml::collect_indices_less_equal(
        entities.fighter_spawn_timers.get_const_view().remaining_times,
        0.f,
        ships_ready_to_spawn_fighters_buffer.view())};
    ships_ready_to_spawn_fighters_buffer.set_num(ships_ready_to_spawn_fighters_indices.Num());

    auto const n_ready_to_spawn{ships_ready_to_spawn_fighters_indices.Num()};
    for (int32 i{n_ready_to_spawn - 1}; i >= 0; --i) {
        auto const capital_index{ships_ready_to_spawn_fighters_indices[i]};
        if (entities.target_handles[capital_index].is_null()) {
            ships_ready_to_spawn_fighters_buffer.remove_at_swap(i);
        }
    }
    ships_ready_to_spawn_fighters_indices = ships_ready_to_spawn_fighters_buffer.view();
    if (ships_ready_to_spawn_fighters_indices.IsEmpty()) {
        return;
    }

    auto const& relative_transforms{config.fighter_spawn_slots_relative_transforms};
    ml::test_capital_ship_fighters::FrameSpawnQueue fighter_spawn_wave{&frame_memory_resource};
    fighter_spawn_wave.reserve(relative_transforms.Num());
    for (auto const capital_index : ships_ready_to_spawn_fighters_indices) {
        fighter_spawn_wave.clear();
        auto const base_location{ml::get_vector3f(entities.locations, capital_index)};
        auto const base_rotation{ml::get_rotator3f(entities.rotations, capital_index)};
        FTransform const base_transform{
            FRotator{base_rotation},
            FVector{base_location},
            FVector::OneVector,
        };

        for (auto const& relative_transform : relative_transforms) {
            auto const new_transform{relative_transform * base_transform};
            if (fighter_diagnostics::take_report(diagnostic_spawn_reports, 64)) {
                UE_LOG(LogSandbox,
                       Display,
                       TEXT("[FighterSpawn] Enqueue parentRegistryIndex=%d capitalIndex=%d "
                            "base=%s slot=%s world=%s"),
                       entities.handles[capital_index].index,
                       capital_index,
                       *base_transform.ToHumanReadableString(),
                       *relative_transform.ToHumanReadableString(),
                       *new_transform.ToHumanReadableString());
            }
            fighter_spawn_wave.add(FVector3f{new_transform.GetLocation()},
                                   FRotator3f{new_transform.Rotator()},
                                   entities.teams[capital_index],
                                   entities.handles[capital_index],
                                   entities.target_handles[capital_index]);
        }
        auto const spawn_wave{fighter_spawn_wave.get_const_view()};
        auto const accepted_count{fighters_interface.queue_spawns(spawn_wave)};
        fighter_queue.append_from(spawn_wave.left(accepted_count));
        entities.fighter_spawn_timers.remaining_times[capital_index] =
            entities.fighter_spawn_cooldowns[capital_index];
    }
}
void Simulation::refresh_fighter_handles() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::test_capital_ships::Simulation::refresh_fighter_handles);

    fighter_handles_scratch.Reset();
    auto const& previous{tick_buffers.previous()};
    entity_registry.refresh_handles(fighter_handles);

    auto const& spawn_data{fighters_interface.get_new_spawn_entity_data()};
    spawn_data.validate_array_sizes();
    ensure(previous.fighter_queue.num() == spawn_data.num());

    auto const& spawn_handles{fighters_interface.get_new_spawn_entity_handles()};
    ensure(spawn_handles.registry_handles.num() == previous.fighter_queue.num());
    auto const spawned_count{
        FMath::Min(spawn_handles.registry_handles.num(), previous.fighter_queue.num())};
    for (int32 spawn_index{}; spawn_index < spawned_count; ++spawn_index) {
        auto const parent_handle{previous.fighter_queue.parents[spawn_index]};
        auto destination_handle{parent_handle};
        if (entities.handles.Find(parent_handle) == INDEX_NONE) {
            auto const replacement{
                find_first_handle_on_team(previous.fighter_queue.teams[spawn_index])};
            if (!replacement) {
                fighters_interface.self_destruct_fighter(
                    spawn_handles.registry_handles[spawn_index]);
                continue;
            }
            destination_handle = *replacement;
        }
        fighter_reassignment_queue.add(destination_handle,
                                       spawn_handles.registry_handles[spawn_index]);
    }

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

        auto const n_reassigned{fighter_reassignment_queue.num()};
        for (int32 reassigned_index{n_reassigned - 1}; reassigned_index >= 0; --reassigned_index) {
            auto const new_capital_handle{
                fighter_reassignment_queue.capital_handles[reassigned_index]};
            auto const new_capital_index{entities.handles.Find(new_capital_handle)};
            check(new_capital_index != INDEX_NONE);
            if (capital_index == new_capital_index) {
                fighter_handles_scratch.Add(
                    fighter_reassignment_queue.fighter_handles[reassigned_index]);
                fighter_reassignment_queue.remove_at_swap(reassigned_index, 1, EAllowShrinking::No);
                ++new_span.count;
            }
        }

        entities.capital_fighter_handle_spans[capital_index] = new_span;
    }

    check(fighter_handles_scratch.Num() >= spawn_data.num());
    Swap(fighter_handles, fighter_handles_scratch);
}

/* **************************************** */
// Orders
/* **************************************** */
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
            if (fighter_target.is_null() || entity_registry.is_valid_dead(fighter_target)) {
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

/* **************************************** */
// Targets
/* **************************************** */
void Simulation::set_target_handle(FRegistryEntityHandle const ship_handle,
                                   FRegistryEntityHandle const target_handle) {
    check(entity_registry.is_valid_handle(ship_handle));
    check(entity_registry.is_valid_handle(target_handle));
    auto const entity_index{entities.handles.Find(ship_handle)};
    check(entity_index != INDEX_NONE);
    entities.target_handles[entity_index] = target_handle;
}

/* **************************************** */
// Death handling
/* **************************************** */
void Simulation::handle_dead_entities() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::test_capital_ships::Simulation::handle_dead_entities);
    if (local_indices_to_remove.IsEmpty()) {
        return;
    }

    ml::batch::sort_and_deduplicate_removal_indices(local_indices_to_remove);

    auto const batch_index{deaths_.Num()};
    deaths_.Reserve(batch_index + local_indices_to_remove.Num());
    for (auto const index : local_indices_to_remove) {
        deaths_.Add({ml::get_vector3f(entities.locations, index), batch_index});
        frame_changes_.Add({.kind = EEntityFrameChange::RemoveSwap,
                            .index = index,
                            .handle = entities.handles[index]});
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

/* **************************************** */
// Misc
/* **************************************** */
void Simulation::clear_tick_buffers() {
    ml::reset(local_indices_to_remove,
              tick_buffers.current(),
              entity_update_data,
              entity_death_info,
              fighter_handles_scratch);
}

/* **************************************** */
// Checks
/* **************************************** */
void Simulation::validate_array_sizes() const {
    entities.validate_array_sizes();
}
void Simulation::validate_entity_handles() const {
    entity_registry.validate_handles(entities.handles);
}
} // namespace ml::test_capital_ships
