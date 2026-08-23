#include "TestCapitalShips.h"

#include <Sandbox/batch_game/SpatialQueryManager.h>
#include <Sandbox/batch_game/test_entity_registry/TestEntityRegistry.h>
#include <Sandbox/batch_game/TestBatchActorCore.h>
#include <Sandbox/batch_game/TestBatchOrchestrator.h>
#include <Sandbox/batch_game/TestCapitalShipProxy.h>
#include <Sandbox/batch_game/TestCapitalShipsConfig.h>
#include <Sandbox/batch_game/TestTeamVisualData.h>
#include <Sandbox/environment/effects/DelayedNiagaraSpawner.h>
#include <Sandbox/logging/SandboxLogCategories.h>
#include <Sandbox/logging/SandboxVisualLoggerStyle.h>
#include <Sandbox/utilities/IndexSpan.h>
#include <Sandbox/utilities/mesh.h>
#include <SandboxGameShared/utilities/actor_utils.h>

#include <NiagaraFunctionLibrary.h>
#include <SandboxCore/array_checks.h>
#include <SandboxCore/array_math.h>
#include <SandboxCore/array_utils.h>
#include <SandboxCore/container_ops.h>
#include <SandboxCore/invoke.h>
#include <SandboxCore/soa_rotator_utils.h>
#include <SandboxCore/soa_vector_utils.h>
#include <SandboxCore/transforms.h>
#include <SandboxCoreEngine/actor_utils.h>
#include <SandboxCoreEngine/uobject_utils.h>

#include <Components/InstancedStaticMeshComponent.h>
#include <Components/SceneComponent.h>
#include <Engine/StaticMesh.h>
#include <EngineUtils.h>
#include <ProfilingDebugging/CountersTrace.h>
#include <Templates/Greater.h>
#include <VisualLogger/VisualLogger.h>

#include <array>
#include <limits>

TRACE_DECLARE_INT_COUNTER(SandboxTestCapitalShipCount, TEXT("Sandbox/TestLaserCount"));

ATestCapitalShips::ATestCapitalShips()
    : instances{CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("instances"))} {
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("root"));

    instances->SetupAttachment(RootComponent);

    PrimaryActorTick.bCanEverTick = false;
    PrimaryActorTick.bStartWithTickEnabled = false;

    ml::set_actor_component_mobility(*this, EComponentMobility::Static);
}

// Actor Lifecycle
void ATestCapitalShips::begin_play() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShips::begin_play);
    TRACE_COUNTER_SET(SandboxTestCapitalShipCount, 0);

    auto* world{GetWorld()};
    check(entity_registry);
    check(spatial_query_manager);
    ml::fatal_if_uobject_ptrs_invalid({
        {
            SANDBOX_NAMED_UOBJECT_PTR(actor_config),
            SANDBOX_NAMED_UOBJECT_PTR(world),
        },
        {
            SANDBOX_NAMED_UOBJECT_PTR(actor_config->mesh),
            SANDBOX_NAMED_UOBJECT_PTR(actor_config->team_visual_data),
        },
    });

    debug_drawer = actor_config->debug_drawer;
    debug_drawer.world = world;

    ensureAlways(actor_config->fighter_spawn_slots ==
                 actor_config->fighter_spawn_slots_relative_transforms.Num());

    configure_ismc();

    ATestBatchOrchestrator::on_proxy_entities_bound.RemoveAll(this);
    ATestBatchOrchestrator::on_proxy_entities_bound.AddUObject(this,
                                                               &ThisClass::bind_proxy_entities);

    register_all_proxies_in_level();
}
void ATestCapitalShips::bind_proxy_entities(FProxyEntityMap const& proxy_entities) {
    ATestBatchOrchestrator::on_proxy_entities_bound.RemoveAll(this);

    auto* const world{GetWorld()};
    check(world);

    for (TActorIterator<Proxy> it{world}; it; ++it) {
        auto const& proxy{**it};
        auto const* const identifiers{proxy_entities.Find(&proxy)};
        check(identifiers);

        check(entity_registry->is_valid_handle(identifiers->handle));

        auto const entity_index{entities.handles.Find(identifiers->handle)};
        check(entity_index != INDEX_NONE);

        auto const target{proxy.get_target_ship()};
        if (!target) {
            continue;
        }

        auto const target_handle{[&] {
            if (auto const* const proxy_target{proxy_entities.Find(target)}) {
                return proxy_target->handle;
            }

            auto const* const target_entity{Cast<ITestEntity>(target)};
            check(target_entity);
            return target_entity->get_entity_handle();
        }()};

        check(entity_registry->is_valid_handle(target_handle));
        entities.target_handles[entity_index] = target_handle;
    }
}

void ATestCapitalShips::begin_tick() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShips::begin_tick);

    tick_buffers.cycle();

    clear_tick_buffers();
}
void ATestCapitalShips::update_timers(float const dt) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShips::update_timers);

    entities.fighter_spawn_timers.tick(dt);
}
void ATestCapitalShips::make_decisions() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShips::make_decisions);

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
void ATestCapitalShips::resolve_damage_events() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShips::resolve_damage_events);

    ml::batch::resolve_damage_events(*entity_registry,
                                     entities.handles,
                                     entities.healths,
                                     local_indices_to_remove,
                                     entity_death_info);
}
void ATestCapitalShips::update_entity_registry() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShips::update_entity_registry);

    prepare_entity_update_data();

    entity_registry->queue_entity_updates({entities.handles, entity_update_data.get_const_view()},
                                          entity_death_info);
}
void ATestCapitalShips::sync_from_registry() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShips::sync_from_registry);

    handle_dead_entities();
}
void ATestCapitalShips::update_visual_data() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShips::update_visual_data);

    // Clear old instances
    if (local_indices_to_remove.Num()) {
        constexpr bool is_reverse_sorted{true};
        instances->RemoveInstances(local_indices_to_remove, is_reverse_sorted);
    }
}
void ATestCapitalShips::commit_visual_data() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShips::commit_visual_data);

    instances->MarkRenderStateDirty();
    if (debugging_shapes_enabled) {
        draw_debugging_shapes();
    }
}
void ATestCapitalShips::end_tick() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShips::end_tick);
    TRACE_COUNTER_SET(SandboxTestCapitalShipCount, get_num_instances());

    fighters_spawned += ml::num(tick_buffers.current().fighter_queue);
    validate_array_sizes();
    visual_log_state();
}

void ATestCapitalShips::visual_log_state() const {
#if ENABLE_VISUAL_LOG
    if (!FVisualLogger::IsRecording()) {
        return;
    }
#else
    return;
#endif

    if (!entity_registry) {
        UE_LOG(LogSandboxEntities,
               Error,
               TEXT("ATestCapitalShips::visual_log_state entity registry is null"));
        return;
    }

    if (auto const msg{ml::report_invalid_uobject_ptrs({
            SANDBOX_NAMED_UOBJECT_PTR(actor_config),
        })}) {
        UE_LOG(LogSandboxEntities,
               Error,
               TEXT("ATestCapitalShips::visual_log_state UObject ptrs are invalid:\n%s"),
               *msg);
        return;
    }

    if (auto const msg{ml::report_invalid_uobject_ptrs({
            SANDBOX_NAMED_UOBJECT_PTR(actor_config->visual_logger_style),
        })}) {
        UE_LOG(LogSandboxEntities,
               Error,
               TEXT("ATestCapitalShips::visual_log_state UObject ptrs are invalid:\n%s"),
               *msg);
        return;
    }

    auto const& style{*actor_config->visual_logger_style};
    FVector const capital_extent{style.entities.capital_ship_box_extent};
    auto const normal_line_thickness{static_cast<uint16>(style.lines.normal_line_thickness)};
    auto const highlighted_line_thickness{
        static_cast<uint16>(style.lines.highlighted_line_thickness)};

    auto const capital_count{get_num_instances()};
    for (int32 capital_index{0}; capital_index < capital_count; ++capital_index) {
        FVector const capital_location{ml::get_vector3f(entities.locations, capital_index)};
        FBox const capital_box{capital_location - capital_extent,
                               capital_location + capital_extent};
        UE_VLOG_BOX(this,
                    LogSandboxEntities,
                    Log,
                    capital_box,
                    style.entities.capital_ship_colour,
                    TEXT("Capital %d Team %d"),
                    entities.handles[capital_index].index,
                    static_cast<int32>(entities.teams[capital_index]));

        auto const fighter_handles_view{get_fighter_handles(capital_index)};
        auto const fighter_count{fighter_handles_view.Num()};
        for (int32 fighter_index{0}; fighter_index < fighter_count; ++fighter_index) {
            auto const fighter_handle{fighter_handles_view[fighter_index]};
            if (!entity_registry->is_valid_alive(fighter_handle)) {
                continue;
            }

            FVector const fighter_location{entity_registry->get_location(fighter_handle)};
            UE_VLOG_SEGMENT_THICK(this,
                                  LogSandboxEntities,
                                  Log,
                                  capital_location,
                                  fighter_location,
                                  style.navigation.parent_to_child_line_colour,
                                  normal_line_thickness,
                                  TEXT("Fighter %d"),
                                  fighter_handle.index);
        }

        auto const target_handle{entities.target_handles[capital_index]};
        if (!entity_registry->is_valid_alive(target_handle)) {
            continue;
        }

        FVector const target_location{entity_registry->get_location(target_handle)};
        UE_VLOG_WIRESPHERE(this,
                           LogSandboxTargeting,
                           Log,
                           target_location,
                           style.entities.fighter_entity_radius,
                           style.combat.selected_target_colour,
                           TEXT("Target %d"),
                           target_handle.index);
        UE_VLOG_SEGMENT_THICK(this,
                              LogSandboxTargeting,
                              Log,
                              capital_location,
                              target_location,
                              style.combat.selected_target_colour,
                              highlighted_line_thickness,
                              TEXT("Target %d"),
                              target_handle.index);
    }
}

// Accessors
auto ATestCapitalShips::get_num_instances() const -> int32 {
    return entities.handles.Num();
}
auto ATestCapitalShips::is_valid(FRegistryEntityHandle const index) const -> bool {
    if (!index.is_valid()) {
        return false;
    }

    if (entities.handles.Find(index) == INDEX_NONE) {
        return false;
    }

    return true;
}
auto ATestCapitalShips::get_spatial_query_component() const -> UPrimitiveComponent const* {
    return instances.Get();
}
void ATestCapitalShips::resolve_hits(
    TConstArrayView<ml::FSpatialQueryHit> const hits,
    TArrayView<FRegistryEntityHandle> const out_entity_handles) const {
    ml::batch::resolve_ismc_hits(hits, out_entity_handles, *instances, entities.handles);
}

auto ATestCapitalShips::get_niagara_spawner() const -> ADelayedNiagaraSpawner const* {
    return niagara_spawner;
}
void ATestCapitalShips::set_niagara_spawner(ADelayedNiagaraSpawner& spawner) {
    niagara_spawner = &spawner;
}

auto ATestCapitalShips::get_team(FRegistryEntityHandle handle) const noexcept -> ETestTeam {
    auto const n{get_num_instances()};

    for (int32 i{}; i < n; ++i) {
        if (handle == entities.handles[i]) {
            return entities.teams[i];
        }
    }

    UE_LOG(LogSandbox, Fatal, TEXT("Invalid handle passed"));
    return ETestTeam::White;
}

auto ATestCapitalShips::find_first_index_on_team(ETestTeam team) const noexcept
    -> std::optional<int32> {
    auto const n{get_num_instances()};

    for (int32 i{0}; i < n; ++i) {
        if (entities.teams[i] == team) {
            return i;
        }
    }

    return {};
}

auto ATestCapitalShips::get_health(FRegistryEntityHandle handle) const noexcept -> int32 {
    auto const idx{entities.handles.Find(handle)};
    check(idx != INDEX_NONE);
    return entities.healths[idx];
}

// Ship spawning
void ATestCapitalShips::register_all_proxies_in_level() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShips::register_all_proxies_in_level);

    check(entities.handles.IsEmpty());
    auto* world{GetWorld()};
    check(world);

    auto const proxies{ml::get_actors<Proxy>(*world)};
    auto const n_to_add{proxies.Num()};
    auto const default_spawn_cooldown{actor_config->spawn_delay};

    SpawnData spawn_data;
    ml::add_uninitialised(n_to_add, entities.handles, spawn_data);

    for (int32 i{0}; i < n_to_add; ++i) {
        auto const& proxy_transform{proxies[i]->GetActorTransform()};
        ml::assign(spawn_data.locations, i, proxy_transform.GetLocation());
        ml::assign(spawn_data.rotations, i, proxy_transform.Rotator());
        spawn_data.teams[i] = proxies[i]->get_team();
        spawn_data.healths[i] = proxies[i]->get_health().Get(actor_config->max_health);

        spawn_data.initial_spawn_delays[i] = proxies[i]->get_initial_spawn_delay().Get(0.f);
        spawn_data.spawn_cooldowns[i] =
            proxies[i]->get_spawn_cooldown().Get(default_spawn_cooldown);
    }

    spawn_ships(spawn_data);

    check(entities.num() == ml::num(spawn_data));

    prepare_entity_update_data();
    auto new_entities{entity_registry->add_entities(entity_update_data.get_const_view())};
    entities.handles = new_entities.registry_handles.to_array();

    // Map the proxies to the new handles
    for (int32 i{0}; i < n_to_add; ++i) {
        proxies[i]->set_entity_handle(entities.handles[i]);
        entities.target_handles[i].reset();
    }
}
void ATestCapitalShips::spawn_ships(SpawnData const& spawn_data) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShips::spawn_ships);

    auto const n_existing{get_num_instances()};
    auto const n_to_add{ml::num(spawn_data)};

    spawn_data.validate_array_sizes();

    ml::append_from(entities.locations, spawn_data.locations);
    ml::append_from(entities.rotations, spawn_data.rotations);

    ml::append_from(entities.fighter_spawn_timers.remaining_times, spawn_data.initial_spawn_delays);
    entities.fighter_spawn_cooldowns.Append(spawn_data.spawn_cooldowns);

    entities.teams.Append(spawn_data.teams);
    entities.healths.Append(spawn_data.healths);
    entities.capital_fighter_handle_spans.AddZeroed(n_to_add);
    entities.target_handles.Append(spawn_data.target_handles);

    auto const colour_cache{
        UTestTeamVisualData::build_team_colour_cache(actor_config->team_visual_data)};
    TArray<float> custom_data_spawn_buffer;
    custom_data_spawn_buffer.SetNumUninitialized(n_to_add * n_custom_ismc_floats,
                                                 EAllowShrinking::No);

    for (int32 new_i{0}; new_i < n_to_add; ++new_i) {
        auto const team{spawn_data.teams[new_i]};

        // Custom ISMC data
        auto const base{new_i * n_custom_ismc_floats};
        auto const& colour{colour_cache[team]};
        custom_data_spawn_buffer[base + 0] = colour.R;
        custom_data_spawn_buffer[base + 1] = colour.G;
        custom_data_spawn_buffer[base + 2] = colour.B;
    }

    auto const new_transforms{ml::make_transforms(spawn_data.locations, spawn_data.rotations)};
    instances->AddInstances(new_transforms, false, is_world_space, false);
    instances->SetCustomData(0, n_to_add - 1, custom_data_spawn_buffer, false);
    validate_array_sizes();
}

// Entity data
void ATestCapitalShips::prepare_entity_update_data() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShips::prepare_entity_update_data);
    check(entity_update_data.num() == 0);

    auto const n{get_num_instances()};

    entity_update_data.add_uninitialised(n);

    entity_update_data.locations = entities.locations;
    ml::fill(entity_update_data.velocities, 0.f);
    ml::fill(entity_update_data.radii, ml::get_mesh_sphere_bounds(*instances));
    entity_update_data.healths = entities.healths;
    entity_update_data.teams = entities.teams;
    entity_update_data.set_all_entity_types(ETestEntityType::CapitalShip);

    for (int32 i{0}; i < n; ++i) {
        entity_update_data.alive[i] = entities.healths[i] > 0;
    }
}

// Fighter spawning
auto ATestCapitalShips::get_fighter_spawn_slots() const noexcept -> int32 {
    return actor_config->fighter_spawn_slots;
}
void ATestCapitalShips::queue_fighter_spawns() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShips::queue_fighter_spawns);

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

    // Resize based on how many actually need to spawn
    data.ships_ready_to_spawn_fighters_buffer.SetNumUninitialized(
        ships_ready_to_spawn_fighters_indices.Num(), EAllowShrinking::No);

    {
        auto const n_ready_to_spawn{ships_ready_to_spawn_fighters_indices.Num()};

        for (int32 i{n_ready_to_spawn - 1}; i >= 0; --i) {
            auto const capital_index{ships_ready_to_spawn_fighters_indices[i]};
            if (entities.target_handles[capital_index].is_null()) {
                data.ships_ready_to_spawn_fighters_buffer.RemoveAtSwap(i, EAllowShrinking::No);
            }
        }

        ships_ready_to_spawn_fighters_indices = data.ships_ready_to_spawn_fighters_buffer;
    }

    if (ships_ready_to_spawn_fighters_indices.IsEmpty()) {
        return;
    }

    auto const relative_transforms{actor_config->fighter_spawn_slots_relative_transforms};

    for (auto const i : ships_ready_to_spawn_fighters_indices) {
        auto const target_handle{entities.target_handles[i]};
        auto const base_location{ml::get_vector3f(entities.locations, i)};
        auto const base_rotation{ml::get_rotator3f(entities.rotations, i)};

        FTransform const base_transform{
            FRotator{base_rotation},
            FVector{base_location},
            FVector::OneVector,
        };

        for (auto const& rt : relative_transforms) {
            auto const new_transform{rt * base_transform};

            ml::append(fighter_queue.locations, new_transform.GetLocation());
            ml::append(fighter_queue.rotations, new_transform.Rotator());
            fighter_queue.teams.Add(entities.teams[i]);
            fighter_queue.targets.Add(target_handle);
        }

        entities.fighter_spawn_timers.remaining_times[i] = entities.fighter_spawn_cooldowns[i];
    }

    fighters_interface.queue_spawns(fighter_queue);
}
void ATestCapitalShips::refresh_fighter_handles() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShips::refresh_fighter_handles);

    fighter_handles_scratch.Reset();

    auto const& prev{tick_buffers.previous()};

    entity_registry->refresh_handles(fighter_handles);

    auto const& spawn_data{fighters_interface.get_new_spawn_entity_data()};
    spawn_data.validate_array_sizes();

    auto const n_spawned_per_capital{get_fighter_spawn_slots()};
    auto const n_fighters_spawned_expected{prev.fighter_queue.num()};
    auto const n_fighters_spawned{spawn_data.num()};
    ensure(n_fighters_spawned_expected == n_fighters_spawned);

    int32 spawning_capital_idx{0};
    int32 spawned_fighter_idx{0};

    auto const& spawn_handles{fighters_interface.get_new_spawn_entity_handles()};
    auto const n_capitals{get_num_instances()};
    for (int32 capital_idx{0}; capital_idx < n_capitals; ++capital_idx) {
        FIndexSpan new_span{
            .offset = fighter_handles_scratch.Num(),
            .count = 0,
        };

        // Add existing non-null fighters
        auto const old_span{entities.capital_fighter_handle_spans[capital_idx]};
        auto const loop_end{old_span.end()};
        for (int32 local_fighter_index{old_span.offset}; local_fighter_index < loop_end;
             ++local_fighter_index) {
            auto const fighter_handle{fighter_handles[local_fighter_index]};

            if (!fighter_handle.is_null()) {
                fighter_handles_scratch.Add(fighter_handle);
                ++new_span.count;
            }
        }

        // Add newly spawned fighters
        if (prev.ships_ready_to_spawn_fighters_buffer.IsValidIndex(spawning_capital_idx) &&
            prev.ships_ready_to_spawn_fighters_buffer[spawning_capital_idx] == capital_idx) {

            auto const end{spawned_fighter_idx + n_spawned_per_capital};
            for (; spawned_fighter_idx < end; ++spawned_fighter_idx, ++new_span.count) {
                fighter_handles_scratch.Add(spawn_handles.registry_handles[spawned_fighter_idx]);
            }

            ++spawning_capital_idx;
        }

        auto const n_fighters_reassigned{fighter_reassignment_queue.num()};
        if (n_fighters_reassigned > 0) {
            for (int32 i_reassigned{n_fighters_reassigned - 1}; i_reassigned >= 0; --i_reassigned) {
                auto const new_owning_capital_handle{
                    fighter_reassignment_queue.capital_handles[i_reassigned]};
                auto const new_owning_capital_idx{entities.handles.Find(new_owning_capital_handle)};
                check(new_owning_capital_idx != INDEX_NONE);

                if (capital_idx == new_owning_capital_idx) {
                    fighter_handles_scratch.Add(
                        fighter_reassignment_queue.fighter_handles[i_reassigned]);
                    fighter_reassignment_queue.fighter_handles.RemoveAtSwap(i_reassigned,
                                                                            EAllowShrinking::No);
                    ++new_span.count;
                }
            }
        }

        entities.capital_fighter_handle_spans[capital_idx] = new_span;
    }

    check(fighter_handles_scratch.Num() >= n_fighters_spawned);
    Swap(fighter_handles, fighter_handles_scratch);
}

// Orders
void ATestCapitalShips::queue_fighter_orders() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShips::queue_fighter_orders);

    auto const n_capitals{get_num_instances()};
    fighter_order_queue.reset();
    for (int32 capital_idx{0}; capital_idx < n_capitals; ++capital_idx) {
        auto const capital_target{entities.target_handles[capital_idx]};
        auto const span{entities.capital_fighter_handle_spans[capital_idx]};
        auto const end{span.end()};

        if (capital_target.is_null()) {
            for (int32 fighter_span_idx{span.start()}; fighter_span_idx < end; ++fighter_span_idx) {
                fighter_order_queue.add(fighter_handles[fighter_span_idx],
                                        TestCapitalShipFighterOrderQueue::Order{
                                            .task = 1,
                                            .target = 1,
                                        },
                                        ETestCapitalShipFightersTask::Standby,
                                        capital_target);
            }
        } else {
            for (int32 fighter_span_idx{span.start()}; fighter_span_idx < end; ++fighter_span_idx) {
                auto const fighter_handle{fighter_handles[fighter_span_idx]};
                auto const fighter_target_handle{
                    fighters_interface.get_target_handle(fighter_handle)};

                if (fighter_target_handle.is_null() ||
                    entity_registry->is_valid_dead(fighter_target_handle)) {
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
    }

    if (fighter_order_queue.num() > 0) {
        fighters_interface.queue_orders(fighter_order_queue);
    }
}

// Visuals
void ATestCapitalShips::configure_ismc() {
    ml::batch::configure_ismc(*instances,
                              {
                                  .mesh = actor_config->mesh.Get(),
                                  .material = actor_config->material.Get(),
                                  .num_custom_data_floats = n_custom_ismc_floats,
                              });
}

void ATestCapitalShips::trigger_death_effects() {
    auto const n{ml::num(local_indices_to_remove)};
    auto* world{GetWorld()};
    auto* small_death_explosion{actor_config->small_death_explosion};
    auto* main_death_explosion{actor_config->main_death_explosion};

    if (!IsValid(small_death_explosion)) {
        UE_LOG(LogSandbox,
               Warning,
               TEXT("ATestCapitalShips::trigger_death_effects: small_death_explosion is nullptr"));
        return;
    }
    if (!IsValid(main_death_explosion)) {
        UE_LOG(LogSandbox,
               Warning,
               TEXT("ATestCapitalShips::trigger_death_effects: main_death_explosion is nullptr"));
        return;
    }
    if (!IsValid(niagara_spawner)) {
        UE_LOG(LogSandbox,
               Warning,
               TEXT("ATestCapitalShips::trigger_death_effects: niagara_spawner is nullptr"));
        return;
    }

    constexpr bool auto_destroy{true};
    constexpr bool auto_activate{true};

    auto const n_small_burst_explosions{actor_config->n_small_explosions};
    auto const time_between_explosions{actor_config->time_between_explosions};
    auto const large_explosion_delay{actor_config->large_explosion_delay};
    auto const main_explosion_delay_mode{actor_config->main_explosion_delay_mode};

    TArray<UNiagaraSystem*> spawn_systems;
    TArray<FVector> spawn_locations;
    TArray<float> spawn_delays;

    auto const expected_instances{n * (n_small_burst_explosions + 1)};
    ml::reserve(expected_instances, spawn_systems, spawn_locations, spawn_delays);

    auto const min_range{actor_config->min_small_explosion_range};
    auto const max_range{actor_config->max_small_explosion_range};

    auto main_explosion_delay{large_explosion_delay};
    if (main_explosion_delay_mode ==
        ETestCapitalShipsMainExplosionDelayMode::AfterSmallExplosions) {
        // If there is only 1 explosion then there will be no small delays
        // the small delay is between explosions
        if (n_small_burst_explosions > 1) {
            main_explosion_delay += (n_small_burst_explosions * (n_small_burst_explosions - 1));
        }
    }

    float current_delay{0.f};
    for (int32 entity_remove_i{0}; entity_remove_i < n; ++entity_remove_i) {
        auto const entity_index{local_indices_to_remove[entity_remove_i]};

        auto const base_location{ml::get_vector3d(entities.locations, entity_index)};

        for (int32 explosion_i{0}; explosion_i < n_small_burst_explosions; ++explosion_i) {
            if (explosion_i > 0) {
                current_delay += time_between_explosions;
            }

            auto const offset{FVector{
                FMath::FRandRange(min_range.X, max_range.X),
                FMath::FRandRange(min_range.Y, max_range.Y),
                FMath::FRandRange(min_range.Z, max_range.Z),
            }};

            spawn_systems.Add(small_death_explosion);
            spawn_locations.Add(base_location + offset);
            spawn_delays.Add(current_delay);
        }

        spawn_systems.Add(main_death_explosion);
        spawn_locations.Add(ml::get_vector3d(entities.locations, entity_index));
        spawn_delays.Add(main_explosion_delay);
    }

    niagara_spawner->add_spawns(spawn_systems, spawn_locations, spawn_delays);
}
void ATestCapitalShips::handle_dead_entities() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShips::handle_dead_entities);

    if (local_indices_to_remove.IsEmpty()) {
        return;
    }

    trigger_death_effects();
    reassign_fighter_handles_of_dying_capital();

    local_indices_to_remove.Sort(TGreater<int32>{});
    ml::remove_at_swap_many_sorted_desc(local_indices_to_remove, entities);
}
void ATestCapitalShips::reassign_fighter_handles_of_dying_capital() {
    std::array<int32, static_cast<std::size_t>(ETestTeam::COUNT)> replacements{};
    replacements.fill(-1);

    constexpr auto team_count{ml::EnumCountTrait<ETestTeam>::count_value};
    TArray<ETestTeam, TInlineAllocator<team_count>> teams_to_replace;
    for (auto const capital_idx : local_indices_to_remove) {
        auto const team{entities.teams[capital_idx]};
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

    for (auto const capital_idx : local_indices_to_remove) {
        auto const team{entities.teams[capital_idx]};
        auto const replacement_idx{replacements[std::to_underlying(team)]};
        auto const fighter_span{entities.capital_fighter_handle_spans[capital_idx]};
        auto const span_end{fighter_span.end()};

        if (replacement_idx < 0) {
            // No replacement found. Destroy them all.
            for (int32 i{fighter_span.offset}; i < span_end; ++i) {
                fighters_interface.self_destruct_fighter(fighter_handles[i]);
            }
        } else {
            // Reassign to someone else on the team
            for (int32 i{fighter_span.offset}; i < span_end; ++i) {
                fighter_reassignment_queue.add(entities.handles[replacement_idx],
                                               fighter_handles[i]);
            }
        }
    }
}

// Misc
void ATestCapitalShips::clear_runtime_state() {
    instances->ClearInstances();

    ml::reset(entities, local_indices_to_remove, tick_buffers.current(), tick_buffers.previous());
}
void ATestCapitalShips::clear_tick_buffers() {
    ml::reset(local_indices_to_remove,
              tick_buffers.current(),
              entity_update_data,
              entity_death_info,
              fighter_handles_scratch);
}

// Debugging
void ATestCapitalShips::draw_debugging_shapes() const {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShips::draw_debugging_shapes);

    auto const n{get_num_instances()};
    auto const text_offset{actor_config->debug_status_text_offset};

    auto& drawer{debug_drawer};
    for (int32 i{0}; i < n; ++i) {
        auto const ship_location{ml::get_vector3d(entities.locations, i)};

        // Draw target
        auto const target_handle{entities.target_handles[i]};
        if (entity_registry->is_valid_handle(target_handle)) {
            FVector3d const target_location{entity_registry->get_location(target_handle)};
            drawer.draw_arrow(ship_location, target_location);
        }

        // Draw HP
        auto const ship_index{entities.handles[i]};

        auto const msg{FString::Printf(
            TEXT("[%d, %d] HP=%d"), ship_index.index, ship_index.generation, entities.healths[i])};
        auto const msg_location{ship_location + text_offset};
        drawer.draw_string(msg_location, msg);
    }
}

// Checks
void ATestCapitalShips::validate_array_sizes() const {
    ml::fatal_if_nums_not_equal({
        SANDBOX_NAMED_NUM(entities),
        SANDBOX_NAMED_NUM(instances->GetNumInstances()),
    });

    entities.validate_array_sizes();
}
void ATestCapitalShips::validate_proxy_handles() const {
    entity_registry->validate_handles(entities.handles);
}
