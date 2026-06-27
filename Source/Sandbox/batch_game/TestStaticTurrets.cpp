#include "TestStaticTurrets.h"

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

#include <SandboxCore/actor_utils.h>
#include <SandboxCore/array_checks.h>
#include <SandboxCore/array_utils.h>
#include <SandboxCore/projectile_intercept.h>
#include <SandboxCore/soa_rotator_utils.h>
#include <SandboxCore/soa_vector_utils.h>
#include <SandboxCore/uobject_utils.h>

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

// Actor life cycle
void ATestStaticTurrets::begin_play() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestStaticTurrets::begin_play);

    TRACE_COUNTER_SET(SandboxTestStaticTurretCount, 0);

    ml::fatal_if_uobject_ptrs_invalid({
        {actor_config->mesh.Get(), TEXT("ISMC Static Mesh")},
        SANDBOX_NAMED_UOBJECT_PTR(actor_config),
        SANDBOX_NAMED_UOBJECT_PTR(actor_config->mesh),
        SANDBOX_NAMED_UOBJECT_PTR(laser_actor),
        SANDBOX_NAMED_UOBJECT_PTR(entity_registry),
    });

    ensureAlways(IsValid(actor_config->team_visual_data));

    debug_drawer = actor_config->debug_drawer;
    debug_drawer.world = GetWorld();

    configure_ismc();
    register_all_proxies_in_level();
}
void ATestStaticTurrets::begin_tick() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestStaticTurrets::begin_tick);
    clear_tick_buffers();
}
void ATestStaticTurrets::tick(float const dt) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestStaticTurrets::tick);

    laser_cooldowns.tick(dt);
    log_config.tick(dt);

    perform_search();
}
void ATestStaticTurrets::queue_commands() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestStaticTurrets::queue_commands);

    fire_at_enemies();
}
void ATestStaticTurrets::resolve_hit_events() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestStaticTurrets::resolve_hit_events);

    ml::batch::resolve_hit_events(*entity_registry,
                                  owner_id,
                                  entity_handles,
                                  healths,
                                  local_indices_to_remove,
                                  entity_death_info);
    validate_array_sizes();
}
void ATestStaticTurrets::update_entity_registry() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestStaticTurrets::update_entity_registry);

    prepare_entity_update_data();

    entity_registry->update_entities({
        .indices = entity_handles,
        .data = entity_update_data.get_const_view(),
    });

    entity_registry->set_death_infos(entity_death_info);
}
void ATestStaticTurrets::sync_from_registry() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestStaticTurrets::sync_from_registry);

    handle_dead_entities();
}
void ATestStaticTurrets::update_visuals() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestStaticTurrets::update_visuals);
    // Clear old instances
    if (local_indices_to_remove.Num()) {
        constexpr bool is_reverse_sorted{true};
        instances->RemoveInstances(local_indices_to_remove, is_reverse_sorted);

        instances->BatchUpdateInstancesTransforms(0, ismc_transforms, is_world_space, true, true);
    }

    if (draw_target_arrows_enabled || draw_debug_entity_info_enabled) { draw_debugging_shapes(); }
}
void ATestStaticTurrets::end_tick() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestStaticTurrets::end_tick);
    TRACE_COUNTER_SET(SandboxTestStaticTurretCount, get_num_instances());

    validate_array_sizes();
    log_config.on_tick_end();
}

// Visuals
void ATestStaticTurrets::configure_ismc() {
    RootComponent->SetMobility(EComponentMobility::Static);

    instances->SetCanEverAffectNavigation(false);
    instances->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

    instances->SetStaticMesh(actor_config->mesh);

    instances->SetRemoveSwap();
}

// Entity data
void ATestStaticTurrets::prepare_entity_update_data() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestStaticTurrets::prepare_entity_update_data);
    check(entity_update_data.get_num() == 0);

    auto const n{get_num_instances()};

    ml::add_uninitialised(entity_update_data, n);

    entity_update_data.locations = locations;
    ml::fill(entity_update_data.velocities, 0.f);
    entity_update_data.healths = healths;
    entity_update_data.teams = teams;

    for (int32 i{0}; i < n; ++i) {
        entity_update_data.alive[i] = static_cast<uint8>(healths[i] > 0);
    }
}

// Accessors
auto ATestStaticTurrets::get_num_instances() const noexcept -> int32 {
    return teams.Num();
}

void ATestStaticTurrets::set_owner_id(TestEntityOwnerId const new_owner_id) {
    owner_id = new_owner_id;
}
auto ATestStaticTurrets::get_owner_id() const -> TestEntityOwnerId {
    return owner_id;
}

auto ATestStaticTurrets::get_target_handles() const -> TConstArrayView<FRegistryEntityHandle> {
    return target_handles;
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
            if (!target_handles[i].is_null()) { continue; }

            auto const turret_location{ml::get_vector3f(locations, i)};
            auto const this_team{teams[i]};

            TStaticArray<FRegistryEntityHandle, 128> elems;
            auto const n_entities{entity_registry->collect_non_team_entities_in_range(
                turret_location, this_team, radius, elems)};

            target_handles[i] = FRegistryEntityHandle{};

            for (int32 j{0}; j < n_entities; ++j) {
                auto const target_index{elems[j]};

                if (this_team == entity_registry->get_team(target_index)) { continue; }

                target_handles[i] = target_index;
                break;
            }
        }
    }};

    ParallelFor(n_slices, search);
}

// Attacking
void ATestStaticTurrets::fire_at_enemies() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestStaticTurrets::fire_at_enemies);

    auto const n{get_num_instances()};
    auto const cooldown{actor_config->attack_cooldown};
    auto const laser_speed{actor_config->laser_speed};
    auto const laser_damage{actor_config->laser_damage};
    auto const laser_max_distance{actor_config->laser_max_distance};

    auto const disengage_radius{get_disengage_radius()};
    auto const disengage_radius_sq{disengage_radius * disengage_radius};

    FVector3f const fire_point_offset{actor_config->fire_point_offset.GetLocation()};
    auto const colour_cache{
        UTestTeamVisualData::build_team_colour_cache(actor_config->team_visual_data)};

    for (int32 i{0}; i < n; ++i) {
        auto const target_handle{target_handles[i]};

        if (target_handle.is_null()) { continue; }

        if (!entity_registry->is_valid_alive(target_handle)) {
            target_handles[i].reset();
            continue;
        }

        if (!(laser_cooldowns[i] <= 0.f)) { continue; }

        auto const turret_location{ml::get_vector3f(locations, i)};
        auto const target_location{entity_registry->get_location(target_handle)};

        auto const distance_sq{FVector3f::DistSquared(turret_location, target_location)};
        if (distance_sq >= disengage_radius_sq) {
            target_handles[i].reset();
            continue;
        }

        auto const target_velocity{entity_registry->get_velocity(target_handle)};

        auto const loc_x{locations.xs[i] + fire_point_offset.X};
        auto const loc_y{locations.ys[i] + fire_point_offset.Y};
        auto const loc_z{locations.zs[i] + fire_point_offset.Z};
        FVector3f const laser_location{
            loc_x,
            loc_y,
            loc_z,
        };

        auto const intercept_time{ml::solve_intercept_time(
            laser_location, target_location, target_velocity, laser_speed)};

        FVector3f const intercept_pos{target_location + target_velocity * intercept_time};
        FVector3f const fire_dir{(intercept_pos - laser_location).GetSafeNormal()};

        ml::append(new_lasers.locations, loc_x, loc_y, loc_z);
        ml::append(new_lasers.rotations, fire_dir);
        new_lasers.damages.Add(laser_damage);
        new_lasers.speeds.Add(laser_speed);
        new_lasers.max_distances.Add(laser_max_distance);
        new_lasers.instigator_handles.Add(entity_handles[i]);
        new_lasers.colours.Add(colour_cache[teams[i]]);

        laser_cooldowns[i] = cooldown;
    }

    laser_actor->spawn_lasers(new_lasers);
}
auto ATestStaticTurrets::get_disengage_radius() const -> float {
    return actor_config->detection_radius * 1.2f;
}

// Spawning
void ATestStaticTurrets::register_all_proxies_in_level() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestStaticTurrets::register_all_proxies_in_level);

    auto* world{GetWorld()};
    ml::fatal_if_uobject_ptrs_invalid({
        SANDBOX_NAMED_UOBJECT_PTR(world),
    });

    auto const proxies{ml::get_actors<Proxy>(*world)};
    auto const n{proxies.Num()};
    if (n == 0) { return; }

    // Set entity data
    ml::add_uninitialised(n, ismc_transforms, locations, teams, healths);

    ml::fill(healths, actor_config->max_health);
    target_handles.AddDefaulted(n);
    laser_cooldowns.remaining_times.AddZeroed(n);

    for (int32 i{0}; i < n; ++i) {
        auto const transform{proxies[i]->GetActorTransform()};

        ismc_transforms[i] = transform;
        ml::assign(locations, i, transform.GetLocation());

        teams[i] = proxies[i]->get_team();
    }

    instances->AddInstances(ismc_transforms, false);

    prepare_entity_update_data();
    auto new_entities{entity_registry->add_entities(entity_update_data.get_const_view())};
    entity_handles = MoveTemp(new_entities.registry_handles);

    ml::destroy_all_actors(proxies);
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

        UNiagaraFunctionLibrary::SpawnSystemAtLocation(world,
                                                       explosion_system,
                                                       ml::get_vector3d(locations, entity_index) +
                                                           location_offset,
                                                       FRotator::ZeroRotator,
                                                       scale,
                                                       auto_destroy,
                                                       auto_activate,
                                                       ENCPoolMethod::AutoRelease);
    }
}
void ATestStaticTurrets::handle_dead_entities() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestStaticTurrets::handle_dead_entities);

    if (local_indices_to_remove.IsEmpty()) { return; }

    trigger_death_effects();

    local_indices_to_remove.Sort(TGreater<int32>{});
    ml::remove_at_swap_many_sorted_desc(local_indices_to_remove,
                                        ismc_transforms,
                                        entity_handles,
                                        locations,
                                        teams,
                                        laser_cooldowns.remaining_times,
                                        healths,
                                        target_handles);
}

// Misc
void ATestStaticTurrets::clear_runtime_state() {
    instances->ClearInstances();
    ml::reset(entity_handles,
              ismc_transforms,
              locations,
              teams,
              laser_cooldowns,
              target_handles,
              healths);
    clear_tick_buffers();
}
void ATestStaticTurrets::clear_tick_buffers() {
    ml::reset(entity_death_info,
              entity_update_data,
              local_indices_to_remove,
              indices_ready_to_fire,
              new_lasers);
}

// Checks
void ATestStaticTurrets::validate_array_sizes() const {
    ml::fatal_if_nums_not_equal({
        SANDBOX_NAMED_NUM(entity_handles),
        SANDBOX_NAMED_NUM(ismc_transforms),
        SANDBOX_NAMED_NUM(locations),
        SANDBOX_NAMED_NUM(teams),
        SANDBOX_NAMED_NUM(laser_cooldowns),
        SANDBOX_NAMED_NUM(target_handles),
        SANDBOX_NAMED_NUM(healths),
        SANDBOX_NAMED_NUM(instances->GetNumInstances()),
    });
}

// Debugging
void ATestStaticTurrets::draw_debugging_shapes() const {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestStaticTurrets::draw_debugging_shapes);

    auto const n{get_num_instances()};
    auto const text_offset{actor_config->debug_status_text_offset};

    auto& drawer{debug_drawer};
    for (int32 i{0}; i < n; ++i) {
        auto const turret_location{ml::get_vector3d(locations, i)};

        if (draw_target_arrows_enabled) {
            auto const target_handle{target_handles[i]};

            if (entity_registry->is_valid_handle(target_handle)) {
                FVector3d const target_location{entity_registry->get_location(target_handle)};
                drawer.draw_line(turret_location, target_location);
            }
        }

        if (draw_debug_entity_info_enabled) {
            auto const turret_handle{entity_handles[i]};

            auto const msg{FString::Printf(
                TEXT("[%d, %d] HP=%d"), turret_handle.index, turret_handle.generation, healths[i])};
            auto const msg_location{turret_location + text_offset};
            drawer.draw_string(msg_location, msg);
        }
    }
}
