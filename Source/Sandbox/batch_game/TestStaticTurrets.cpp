#include "TestStaticTurrets.h"

#include <Sandbox/batch_game/SpatialQueryManager.h>
#include <Sandbox/batch_game/test_entity_registry/TestEntityRegistry.h>
#include <Sandbox/batch_game/test_entity_registry/TestEntityRegistryData.h>
#include <Sandbox/batch_game/TestBatchActorCore.h>
#include <Sandbox/batch_game/TestLasers.h>
#include <Sandbox/batch_game/TestLasersConfig.h>
#include <Sandbox/batch_game/TestStaticTurretsConfig.h>
#include <Sandbox/batch_game/TestStaticTurretsProxy.h>
#include <Sandbox/batch_game/TestTeamVisualData.h>
#include <Sandbox/logging/SandboxLogCategories.h>
#include <Sandbox/utilities/actor_utils.h>
#include <Sandbox/utilities/mesh.h>

#include <SandboxCore/array_checks.h>
#include <SandboxCore/array_utils.h>
#include <SandboxCore/fixed_array.h>
#include <SandboxCore/projectile_intercept.h>
#include <SandboxCore/soa_rotator_utils.h>
#include <SandboxCore/soa_vector_utils.h>
#include <SandboxCoreEngine/actor_utils.h>
#include <SandboxCoreEngine/uobject_utils.h>

#include <Async/ParallelFor.h>
#include <Components/InstancedStaticMeshComponent.h>
#include <Components/SceneComponent.h>
#include <Engine/StaticMesh.h>
#include <NiagaraFunctionLibrary.h>
#include <ProfilingDebugging/CountersTrace.h>
#include <Templates/Greater.h>

TRACE_DECLARE_INT_COUNTER(SandboxTestStaticTurretCount, TEXT("Sandbox/TestStaticTurretCount"));

ATestStaticTurrets::ATestStaticTurrets()
    : instances{CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("instances"))} {
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("root"));

    RootComponent->SetMobility(EComponentMobility::Static);

    instances->SetupAttachment(RootComponent);

    PrimaryActorTick.bCanEverTick = false;
    PrimaryActorTick.bStartWithTickEnabled = false;

    ml::set_actor_component_mobility(*this, EComponentMobility::Static);
}

void
    ATestStaticTurrets::bind_simulation_clock(ATestBatchOrchestrator const& orchestrator) noexcept {
    simulation_clock.bind(orchestrator);
}

// Actor life cycle
void ATestStaticTurrets::begin_play() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestStaticTurrets::begin_play);

    TRACE_COUNTER_SET(SandboxTestStaticTurretCount, 0);
    check(entity_registry);
    check(spatial_query_manager);

    ml::fatal_if_uobject_ptrs_invalid({
        {
            SANDBOX_NAMED_UOBJECT_PTR(actor_config),
            SANDBOX_NAMED_UOBJECT_PTR(laser_actor),
        },
        {
            {actor_config->mesh.Get(), TEXT("ISMC Static Mesh")},
            SANDBOX_NAMED_UOBJECT_PTR(actor_config->mesh),
            SANDBOX_NAMED_UOBJECT_PTR(actor_config->team_visual_data),
        },
    });

    debug_drawer = actor_config->debug_drawer;
    debug_drawer.world = GetWorld();

    auto const cooldown_tick_period{
        simulation_clock.duration_to_tick_period(actor_config->attack_cooldown)};
    entities.laser_cooldowns.set_tick_value(cooldown_tick_period);

    target_refresh_next_offset = 0;

    configure_ismc();
    register_all_proxies_in_level();
}

void ATestStaticTurrets::begin_tick() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestStaticTurrets::begin_tick);
    clear_tick_buffers();
}
void ATestStaticTurrets::update_timers(float const) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestStaticTurrets::update_timers);

    entities.laser_cooldowns.tick();
    entities.target_refresh_countdowns.tick();
}
void ATestStaticTurrets::make_decisions() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestStaticTurrets::make_decisions);
    perform_search();
}
void ATestStaticTurrets::queue_commands() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestStaticTurrets::queue_commands);

    fire_at_enemies();
}
void ATestStaticTurrets::resolve_damage_events() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestStaticTurrets::resolve_damage_events);

    ml::batch::resolve_damage_events(*entity_registry,
                                     entities.handles,
                                     entities.healths,
                                     local_indices_to_remove,
                                     entity_death_info);
    validate_array_sizes();
}
void ATestStaticTurrets::update_entity_registry() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestStaticTurrets::update_entity_registry);

    prepare_entity_update_data();

    entity_registry->queue_entity_updates(
        {
            .indices = entities.handles,
            .data = entity_update_data.get_const_view(),
        },
        entity_death_info);
}
void ATestStaticTurrets::sync_from_registry() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestStaticTurrets::sync_from_registry);

    entity_registry->refresh_entity_data(entities.target_handles,
                                         entities.target_locations.get_view(),
                                         entities.target_velocities.get_view(),
                                         {});

    handle_dead_entities();
}
void ATestStaticTurrets::update_visual_data() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestStaticTurrets::update_visual_data);
    // Clear old instances
    if (local_indices_to_remove.Num()) {
        constexpr bool is_reverse_sorted{true};
        instances->RemoveInstances(local_indices_to_remove, is_reverse_sorted);

        constexpr bool mark_render_state_dirty{false};
        constexpr bool teleport{true};
        instances->BatchUpdateInstancesTransforms(
            0, ismc_transforms, is_world_space, mark_render_state_dirty, teleport);
    }
}
void ATestStaticTurrets::commit_visual_data() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestStaticTurrets::commit_visual_data);

    instances->MarkRenderStateDirty();
    if (draw_target_arrows_enabled || draw_debug_entity_info_enabled) {
        draw_debugging_shapes();
    }
}
void ATestStaticTurrets::end_tick() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestStaticTurrets::end_tick);
    TRACE_COUNTER_SET(SandboxTestStaticTurretCount, get_num_instances());

    validate_array_sizes();
}

// Visuals
void ATestStaticTurrets::configure_ismc() {
    RootComponent->SetMobility(EComponentMobility::Static);

    instances->SetCanEverAffectNavigation(false);
    instances->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

    check(instances->SetStaticMesh(actor_config->mesh));

    instances->SetRemoveSwap();

    instances->SetNumCustomDataFloats(n_custom_ismc_floats);
}

// Entity data
void ATestStaticTurrets::prepare_entity_update_data() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestStaticTurrets::prepare_entity_update_data);
    check(entity_update_data.num() == 0);

    auto const n{get_num_instances()};

    ml::add_uninitialised(entity_update_data, n);

    entity_update_data.locations = entities.locations;
    ml::fill(entity_update_data.velocities, 0.f);
    ml::fill(entity_update_data.radii, ml::get_mesh_sphere_bounds(*instances));
    entity_update_data.healths = entities.healths;
    entity_update_data.teams = entities.teams;
    entity_update_data.set_all_entity_types(ETestEntityType::Turret);

    for (int32 i{0}; i < n; ++i) {
        entity_update_data.alive[i] = static_cast<uint8>(entities.healths[i] > 0);
    }
}

// Accessors
auto ATestStaticTurrets::get_num_instances() const noexcept -> int32 {
    return entities.handles.Num();
}

auto ATestStaticTurrets::get_spatial_query_component() const -> UPrimitiveComponent const* {
    return instances.Get();
}

void ATestStaticTurrets::resolve_hits(
    TConstArrayView<ml::FSpatialQueryHit> const hits,
    TArrayView<FRegistryEntityHandle> const out_entity_handles) const {
    ml::batch::resolve_ismc_hits(hits, out_entity_handles, *instances, entities.handles);
}

auto ATestStaticTurrets::get_target_handles() const -> TConstArrayView<FRegistryEntityHandle> {
    return entities.target_handles;
}

// Searching
void ATestStaticTurrets::perform_search() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestStaticTurrets::perform_search);

    auto const n_turrets{get_num_instances()};
    auto const radius{actor_config->detection_radius};

    auto const min_turrets_per_slice{search_slice_size};
    auto const n_slices{FMath::DivideAndRoundUp(n_turrets, min_turrets_per_slice)};

    auto const search{[this, n_turrets, radius, min_turrets_per_slice](int32 const slice_index) {
        auto const begin{slice_index * min_turrets_per_slice};
        auto const end{FMath::Min(begin + min_turrets_per_slice, n_turrets)};

        for (int32 i{begin}; i < end; ++i) {
            if (!entities.target_refresh_countdowns.try_consume(i)) {
                continue;
            }

            if (entities.target_handles[i].is_null()) {
                auto const turret_location{ml::get_vector3f(entities.locations, i)};
                auto const this_team{entities.teams[i]};

                ml::TFixedArray<FRegistryEntityHandle, 128> target_handles;
                target_handles.set_num_uninitialised(
                    spatial_query_manager->collect_non_team_entities_in_range(
                        turret_location, this_team, radius, target_handles.capacity_view()));

                entities.target_handles[i] = FRegistryEntityHandle{};

                for (auto const& target_handle : target_handles) {
                    if (this_team == entity_registry->get_team(target_handle)) {
                        continue;
                    }

                    entities.target_handles[i] = target_handle;
                    break;
                }
            }
        }
    }};

    ParallelFor(n_slices, search);
}

// Attacking
void ATestStaticTurrets::fire_at_enemies() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestStaticTurrets::fire_at_enemies);

    auto const n{get_num_instances()};
    auto const laser_speed{actor_config->laser_speed};
    auto const laser_max_distance{actor_config->laser_max_distance};

    auto const disengage_radius{get_disengage_radius()};
    auto const disengage_radius_sq{disengage_radius * disengage_radius};

    auto const colour_cache{
        UTestTeamVisualData::build_team_colour_cache(actor_config->team_visual_data)};

    auto& candidate_indices{scratch_int_buffer};
    auto& start_locations{line_of_sight_start_locations};
    auto& end_locations{line_of_sight_end_locations};
    auto& hit_entity_handles{line_of_sight_hit_entity_handles};

    for (int32 i{0}; i < n; ++i) {
        auto const target_handle{entities.target_handles[i]};

        if (target_handle.is_null()) {
            continue;
        }

        if (!entity_registry->is_valid_alive(target_handle)) {
            entities.target_handles[i].reset();
            continue;
        }

        if (!entities.laser_cooldowns.is_ready(i)) {
            continue;
        }

        auto const turret_location{ml::get_vector3f(entities.locations, i)};
        auto const target_location{ml::get_vector3f(entities.target_locations, i)};

        auto const distance_sq{FVector3f::DistSquared(turret_location, target_location)};
        if (distance_sq >= disengage_radius_sq) {
            entities.target_handles[i].reset();
            continue;
        }

        candidate_indices.Add(i);
        ml::append(start_locations,
                   entities.fire_point_locations.xs[i],
                   entities.fire_point_locations.ys[i],
                   entities.fire_point_locations.zs[i]);
        ml::append(end_locations, target_location);

        entities.laser_cooldowns.restart_counter(i);
    }

    auto const n_candidates{candidate_indices.Num()};
    if (n_candidates == 0) {
        return;
    }

    hit_entity_handles.SetNumUninitialized(n_candidates, EAllowShrinking::No);
    spatial_query_manager->trace_line_of_sight(
        start_locations.get_const_view(), end_locations.get_const_view(), hit_entity_handles);

    for (int32 candidate_index{0}; candidate_index < n_candidates; ++candidate_index) {
        auto const i{candidate_indices[candidate_index]};
        if (hit_entity_handles[candidate_index] != entities.target_handles[i]) {
            continue;
        }

        auto const target_location{ml::get_vector3f(entities.target_locations, i)};
        auto const loc_x{entities.fire_point_locations.xs[i]};
        auto const loc_y{entities.fire_point_locations.ys[i]};
        auto const loc_z{entities.fire_point_locations.zs[i]};
        FVector3f const laser_location{
            loc_x,
            loc_y,
            loc_z,
        };

        auto const target_velocity{ml::get_vector3f(entities.target_velocities, i)};
        auto const intercept_time{ml::solve_intercept_time(
            laser_location, target_location, target_velocity, laser_speed)};

        FVector3f const intercept_pos{target_location + target_velocity * intercept_time};
        FVector3f const fire_dir{(intercept_pos - laser_location).GetSafeNormal()};

        ml::append(new_lasers.locations, loc_x, loc_y, loc_z);
        ml::append(new_lasers.rotations, fire_dir);
        ml::append(new_lasers.base_velocities, 0.f, 0.f, 0.f);
        new_lasers.damages.Add(entities.laser_damages[i]);
        new_lasers.speeds.Add(laser_speed);
        new_lasers.max_distances.Add(laser_max_distance);
        new_lasers.instigator_handles.Add(entities.handles[i]);
        new_lasers.colours.Add(colour_cache[entities.teams[i]]);
    }

    laser_actor->queue_laser_spawns(new_lasers);
}
auto ATestStaticTurrets::get_disengage_radius() const -> float {
    return actor_config->detection_radius * 1.2f;
}

// Spawning
void ATestStaticTurrets::register_all_proxies_in_level() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestStaticTurrets::register_all_proxies_in_level);

    auto* world{GetWorld()};
    auto const proxies{ml::get_actors<Proxy>(*world)};
    auto const n_to_add{proxies.Num()};
    if (n_to_add == 0) {
        return;
    }

    check(actor_config->target_refresh_frequency > 0.f);
    auto const target_refresh_tick_period_unsigned{
        simulation_clock.frequency_to_tick_period(actor_config->target_refresh_frequency)};
    check(FPeriodicTickCountdown16::valid_period(target_refresh_tick_period_unsigned));
    auto const target_refresh_tick_period{
        static_cast<FPeriodicTickCountdown16::counter_type>(target_refresh_tick_period_unsigned)};

    auto const colour_cache{
        UTestTeamVisualData::build_team_colour_cache(actor_config->team_visual_data)};
    FVector3f const fire_point_offset{actor_config->fire_point_offset.GetLocation()};

    TArray<float> custom_data_spawn_buffer;
    custom_data_spawn_buffer.SetNumUninitialized(n_to_add * n_custom_ismc_floats,
                                                 EAllowShrinking::No);

    // Set entity data
    ml::add_uninitialised(n_to_add, ismc_transforms, entities);
    entities.target_refresh_countdowns.initialise_last(target_refresh_tick_period, n_to_add);

    ml::fill_last(entities.target_handles, FRegistryEntityHandle{}, n_to_add);
    entities.laser_cooldowns.zero_last(n_to_add);

    for (int32 i{0}; i < n_to_add; ++i) {
        auto const transform{proxies[i]->GetActorTransform()};

        ismc_transforms[i] = transform;
        ml::assign(entities.locations, i, transform.GetLocation());
        ml::assign(entities.fire_point_locations,
                   i,
                   entities.locations.xs[i] + fire_point_offset.X,
                   entities.locations.ys[i] + fire_point_offset.Y,
                   entities.locations.zs[i] + fire_point_offset.Z);

        auto const team{proxies[i]->get_team()};
        entities.teams[i] = team;
        entities.healths[i] = proxies[i]->get_health().Get(actor_config->max_health);
        entities.laser_damages[i] = proxies[i]->get_laser_damage().Get(actor_config->laser_damage);

        entities.target_refresh_countdowns.remaining_ticks[i] =
            static_cast<FPeriodicTickCountdown16::counter_type>(target_refresh_next_offset);
        ++target_refresh_next_offset;
        if (target_refresh_next_offset == target_refresh_tick_period) {
            target_refresh_next_offset = 0;
        }

        // Custom ISMC data
        auto const base{i * n_custom_ismc_floats};
        auto const& colour{colour_cache[team]};
        custom_data_spawn_buffer[base + 0] = colour.R;
        custom_data_spawn_buffer[base + 1] = colour.G;
        custom_data_spawn_buffer[base + 2] = colour.B;
    }

    instances->AddInstances(ismc_transforms, false);
    instances->SetCustomData(0, n_to_add - 1, custom_data_spawn_buffer, false);

    prepare_entity_update_data();
    auto new_entities{entity_registry->add_entities(entity_update_data.get_const_view())};
    entities.handles = MoveTemp(new_entities.registry_handles);

    // Map the proxies to the new handles
    for (int32 i{0}; i < n_to_add; ++i) {
        proxies[i]->set_entity_handle(entities.handles[i]);
        entities.target_handles[i].reset();
    }

    validate_array_sizes();
}

// Death handling
void ATestStaticTurrets::trigger_death_effects() {
    auto const n{ml::num(local_indices_to_remove)};
    auto* world{GetWorld()};
    auto* explosion_system{actor_config->death_effect};

    if (!IsValid(explosion_system)) {
        UE_LOG(LogSandbox,
               Warning,
               TEXT("ATestStaticTurrets::trigger_death_effects: death_effect is nullptr"));
        return;
    }

    FVector const scale{actor_config->death_effect_scale};
    FVector const location_offset{actor_config->death_effect_offset};

    for (int32 i{0}; i < n; ++i) {
        auto const entity_index{local_indices_to_remove[i]};

        constexpr bool auto_destroy{true};
        constexpr bool auto_activate{true};

        UNiagaraFunctionLibrary::SpawnSystemAtLocation(
            world,
            explosion_system,
            ml::get_vector3d(entities.locations, entity_index) + location_offset,
            FRotator::ZeroRotator,
            scale,
            auto_destroy,
            auto_activate,
            ENCPoolMethod::AutoRelease);
    }
}
void ATestStaticTurrets::handle_dead_entities() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestStaticTurrets::handle_dead_entities);

    if (local_indices_to_remove.IsEmpty()) {
        return;
    }

    trigger_death_effects();

    local_indices_to_remove.Sort(TGreater<int32>{});
    ml::remove_at_swap_many_sorted_desc(local_indices_to_remove, ismc_transforms, entities);
}

// Misc
void ATestStaticTurrets::clear_runtime_state() {
    instances->ClearInstances();
    ml::reset(entities, ismc_transforms);
    target_refresh_next_offset = 0;
    clear_tick_buffers();
}
void ATestStaticTurrets::clear_tick_buffers() {
    ml::reset(entity_death_info,
              entity_update_data,
              local_indices_to_remove,
              scratch_int_buffer,
              line_of_sight_start_locations,
              line_of_sight_end_locations,
              line_of_sight_hit_entity_handles,
              new_lasers);
}

// Checks
void ATestStaticTurrets::validate_array_sizes() const {
    ml::fatal_if_nums_not_equal({
        SANDBOX_NAMED_NUM(entities),
        SANDBOX_NAMED_NUM(ismc_transforms),
        SANDBOX_NAMED_NUM(instances->GetNumInstances()),
    });

    entities.validate_array_sizes();
}
void ATestStaticTurrets::validate_proxy_handles() const {
    entity_registry->validate_handles(entities.handles);
}

// Debugging
void ATestStaticTurrets::draw_debugging_shapes() const {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestStaticTurrets::draw_debugging_shapes);

    auto const n{get_num_instances()};
    auto const text_offset{actor_config->debug_status_text_offset};

    auto& drawer{debug_drawer};
    for (int32 i{0}; i < n; ++i) {
        auto const turret_location{ml::get_vector3d(entities.locations, i)};

        if (draw_target_arrows_enabled) {
            auto const target_handle{entities.target_handles[i]};

            if (entity_registry->is_valid_handle(target_handle)) {
                auto const target_location{ml::get_vector3d(entities.target_locations, i)};
                drawer.draw_line(turret_location, target_location);
            }
        }

        if (draw_debug_entity_info_enabled) {
            auto const turret_handle{entities.handles[i]};

            auto const msg{FString::Printf(TEXT("[%d, %d] HP=%d"),
                                           turret_handle.index,
                                           turret_handle.generation,
                                           entities.healths[i])};
            auto const msg_location{turret_location + text_offset};
            drawer.draw_string(msg_location, msg);
        }
    }
}
