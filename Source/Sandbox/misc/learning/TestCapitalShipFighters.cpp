#include "TestCapitalShipFighters.h"

#include "Sandbox/logging/SandboxLogCategories.h"
#include "Sandbox/misc/learning/TestCapitalShipFightersConfig.h"
#include "Sandbox/misc/learning/TestEntityRegistry.h"
#include "Sandbox/misc/learning/TestLasers.h"
#include "Sandbox/utilities/actor_utils.h"

#include <SandboxCore/array_checks.h>
#include <SandboxCore/array_math.h>
#include <SandboxCore/array_utils.h>
#include <SandboxCore/soa_rotator_utils.h>
#include <SandboxCore/soa_vector_utils.h>
#include <SandboxCore/transforms.h>
#include <SandboxCore/uobject_utils.h>

#include <Components/InstancedStaticMeshComponent.h>
#include <Components/SceneComponent.h>
#include <Engine/StaticMesh.h>
#include <Engine/World.h>
#include <ProfilingDebugging/CountersTrace.h>
#include <Templates/Greater.h>

TRACE_DECLARE_INT_COUNTER(SandboxTestFighterCount, TEXT("Sandbox/TestFighterCount"));

ATestCapitalShipFighters::ATestCapitalShipFighters()
    : instances{CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("instances"))} {
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("root"));

    instances->SetupAttachment(RootComponent);

    PrimaryActorTick.bCanEverTick = false;
    PrimaryActorTick.bStartWithTickEnabled = false;

    ml::set_actor_component_mobility(*this, EComponentMobility::Static);
}

// Actor life cycle
void ATestCapitalShipFighters::begin_play() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShipFighters::begin_play);
    TRACE_COUNTER_SET(SandboxTestFighterCount, 0);

    ml::fatal_if_uobject_ptrs_invalid({
        SANDBOX_NAMED_UOBJECT_PTR(actor_config),
        SANDBOX_NAMED_UOBJECT_PTR(actor_config->mesh),
        SANDBOX_NAMED_UOBJECT_PTR(laser_actor),
        SANDBOX_NAMED_UOBJECT_PTR(entity_registry),
    });

    configure_ismc();

    debug_drawer = actor_config->debug_drawer;
    debug_drawer.world = GetWorld();

    turn_speed_radians = FMath::DegreesToRadians(actor_config->turn_speed_degrees);
}
void ATestCapitalShipFighters::begin_tick() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShipFighters::begin_tick);
    clear_tick_buffers();
}
void ATestCapitalShipFighters::tick(float const dt) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShipFighters::tick);

    laser_cooldowns.tick(dt);

    move_ships(dt);
    handle_firing();
}
void ATestCapitalShipFighters::update_entity_registry() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShipFighters::update_entity_registry);

    auto const data{get_entity_data()};
    ATestEntityRegistry::ConstView view{entity_indices, data.get_const_view()};
    entity_registry->update_entities(view);
}
void ATestCapitalShipFighters::resolve_damage_targets() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShipFighters::resolve_damage_targets);

    auto const view{entity_registry->get_damage_queue_view()};
    auto const n{view.num()};

    for (int32 i{0}; i < n; ++i) {
        if (view.damaged_actors[i] != this) {
            continue;
        }

        view.targets[i] = entity_indices[view.damaged_hit_items[i]];
    }
}
void ATestCapitalShipFighters::sync_from_registry() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShipFighters::sync_from_registry);

    auto const dead_entities{entity_registry->get_dead_entities_this_frame()};

    ml::collect_valid_indices_by_key(entity_indices, dead_entities, local_indices_to_remove);
    local_indices_to_remove.Sort(TGreater<int32>{});

    ml::remove_at_swap_many_sorted_desc(local_indices_to_remove,
                                        ismc_transforms,
                                        entity_indices,
                                        locations,
                                        directions,
                                        speeds,
                                        teams,
                                        healths,
                                        laser_cooldowns.remaining_times,
                                        target_indices,
                                        target_locations);

    // Update health and the target entity index
    auto const n{get_num_instances()};
    for (int32 i{0}; i < n; ++i) {
        auto const entity_index{entity_indices[i]};
        healths[i] = entity_registry->get_health(entity_index);

        auto const target_entity_index{target_indices[i]};
        if (target_entity_index.is_valid()) {
            if (entity_registry->is_stale(target_entity_index)) {
                target_indices[i] = FGenerationIndex{};
            } else {
                ml::assign(target_locations, i, entity_registry->get_location(target_entity_index));
            }
        }
    }

    // Remove ISMC instances
    if (local_indices_to_remove.Num()) {
        constexpr bool is_reverse_sorted{true};
        instances->RemoveInstances(local_indices_to_remove, is_reverse_sorted);
    }

    validate_array_sizes();
}
void ATestCapitalShipFighters::update_visuals() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShipFighters::update_visuals);

    update_ismc_transforms();
    instances->BatchUpdateInstancesTransforms(0, ismc_transforms, is_world_space, true);

    if (enable_target_debug_drawing || enable_ship_location_debug_drawing) {
        draw_debug_shapes();
    }
}
void ATestCapitalShipFighters::end_tick() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShipFighters::end_tick);
    TRACE_COUNTER_SET(SandboxTestFighterCount, get_num_instances());
}

// Accessors
auto ATestCapitalShipFighters::get_num_instances() const noexcept -> int32 {
    return ml::num(locations);
}

void ATestCapitalShipFighters::set_owner_id(TestEntityOwnerId const new_owner_id) {
    owner_id = new_owner_id;
}
auto ATestCapitalShipFighters::get_owner_id() const -> TestEntityOwnerId {
    return owner_id;
}

// Movement
void ATestCapitalShipFighters::move_ships(float const dt) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShipFighters::move_ships);

    auto const n{get_num_instances()};

    auto const max_turn_unitless{turn_speed_unitless * dt};

    for (int32 i{0}; i < n; ++i) {
        auto const speed{speeds[i]};
        auto const delta_distance{speed * dt};

        auto const current_location{ml::get_vector3f(locations, i)};
        auto const target_location{ml::get_vector3f(target_locations, i)};

        auto const current_direction{ml::get_vector3f(directions, i)};
        auto const target_direction{(target_location - current_location).GetSafeNormal()};

        auto const new_direction(
            FMath::Lerp(current_direction, target_direction, max_turn_unitless));

        auto const delta_location{new_direction * delta_distance};

        ml::assign(locations, i, current_location + delta_location);
        ml::assign(directions, i, new_direction);
    }
}

// Visuals
void ATestCapitalShipFighters::configure_ismc() {
    instances->SetStaticMesh(actor_config->mesh);

    instances->SetCanEverAffectNavigation(false);

    instances->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
}
void ATestCapitalShipFighters::update_ismc_transforms() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShipFighters::update_ismc_transforms);

    auto const n{get_num_instances()};
    for (int32 i{0}; i < n; ++i) {
        ismc_transforms[i].SetLocation(ml::get_vector3d(locations, i));

        auto const dir{ml::get_vector3d(directions, i)};
        auto const quat{FQuat::FindBetweenNormals(FVector::ForwardVector, dir)};

        ismc_transforms[i].SetRotation(quat);
    }
}
void ATestCapitalShipFighters::draw_debug_shapes() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShipFighters::draw_debug_shapes);

    auto const n{get_num_instances()};
    for (int32 i{0}; i < n; ++i) {
        FVector const ship_location{ml::get_vector3d(locations, i)};

        if (enable_ship_location_debug_drawing) {
            debug_drawer.draw_sphere(ship_location);
        }

        if (enable_target_debug_drawing) {
            if (!target_indices[i].is_valid()) {
                continue;
            }

            auto const target_location{ml::get_vector3d(target_locations, i)};
            debug_drawer.draw_line(ship_location, target_location);
        }
    }
}

// Spawning
void
    ATestCapitalShipFighters::spawn_instances(FVectors3f::ConstView const new_locations,
                                              FRotatorsf::ConstView const new_rotations,
                                              TConstArrayView<ETestTeam> const new_teams,
                                              TConstArrayView<FGenerationIndex> const new_targets) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShipFighters::spawn_instances);

    auto const n_cur{get_num_instances()};
    auto const n_new{ml::num(new_locations)};

    ml::fatal_if_nums_not_equal({
        SANDBOX_NAMED_NUM(new_locations),
        SANDBOX_NAMED_NUM(new_rotations),
        SANDBOX_NAMED_NUM(new_teams),
        SANDBOX_NAMED_NUM(new_targets),
    });
    if (n_new < 1) {
        return;
    }

    auto const speed{actor_config->speed};

    // Add new data
    ml::append_from(locations, new_locations);
    ml::add_uninitialised(directions, n_new);
    ml::append_n(speeds, speed, n_new);
    teams.Append(new_teams);
    ml::append_n(healths, actor_config->health, n_new);
    target_indices.Append(new_targets);
    ml::add_zeroed(target_locations, n_new);
    laser_cooldowns.remaining_times.AddZeroed(n_new);

    // Fill entity data and set directions
    FTestEntityRegistryEntityData entity_data;
    entity_data.add_uninitialised(n_new);

    for (int32 i{0}; i < n_new; ++i) {
        auto const index{n_cur + i};

        auto const direction{ml::get_vector3f(new_rotations, i)};

        ml::assign(directions, index, direction);
        ml::assign_from(entity_data.locations, i, locations, index);
        entity_data.healths[i] = healths[index];
        entity_data.teams[i] = teams[index];
        entity_data.alive[i] = true;
    }

    // Velocities
    TConstArrayView<float> const new_speeds{speeds.GetData() + n_cur, n_new};
    ml::assign_from_scaled(
        entity_data.velocities, directions.get_const_view().right(n_new), new_speeds);

    // Entity indices
    auto const new_indices{entity_registry->add_entities(entity_data.get_const_view())};
    entity_indices.Append(new_indices);

    // ISMC transforms
    ismc_transforms.AddDefaulted(n_new);
    for (int32 i{0}; i < n_new; ++i) {
        auto const idx{n_cur + i};
        ismc_transforms[idx].SetLocation(ml::get_vector3d(locations, idx));
    }
    instances->AddInstances(
        TArray<FTransform>{ismc_transforms.GetData() + n_cur, n_new}, is_world_space, false);

    validate_array_sizes();
}

// Combat
void ATestCapitalShipFighters::handle_firing() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShipFighters::handle_firing);

    auto const n_ships{get_num_instances()};
    indices_ready_to_fire_buffer.SetNumUninitialized(n_ships, EAllowShrinking::No);
    auto const cooldown{actor_config->fire_cooldown};

    auto const indices_to_fire{
        ml::collect_indices_less_equal(TConstArrayView<float>{laser_cooldowns.remaining_times},
                                       0.f,
                                       indices_ready_to_fire_buffer)};

    TArray<FTransform> laser_transforms;
    auto const fire_point_offset{actor_config->fire_point_offset};

    auto const n_to_fire{ml::num(indices_to_fire)};

    ml::reset(new_laser_locations, new_laser_rotations);
    ml::add_uninitialised(n_to_fire, new_laser_locations, new_laser_rotations);

    for (int32 i{0}; i < n_to_fire; ++i) {
        auto const index_to_fire{indices_to_fire[i]};

        auto const direction{ml::get_vector3f(directions, index_to_fire)};
        ml::assign(new_laser_locations,
                   i,
                   ml::get_vector3f(locations, index_to_fire) + fire_point_offset * direction);
        ml::assign(new_laser_rotations, i, direction.Rotation());

        laser_cooldowns.remaining_times[index_to_fire] = cooldown;
    }

    laser_actor->spawn_lasers(new_laser_locations.get_const_view(),
                              new_laser_rotations.get_const_view());
}

// Misc
void ATestCapitalShipFighters::clear_runtime_state() {
    instances->ClearInstances();

    ml::reset(entity_indices,
              local_indices_to_remove,
              locations,
              directions,
              speeds,
              teams,
              healths,
              target_indices,
              target_locations,
              laser_cooldowns,
              indices_ready_to_fire_buffer,
              new_laser_locations,
              new_laser_rotations);
}
auto ATestCapitalShipFighters::get_entity_data() const -> FTestEntityRegistryEntityData {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShipFighters::get_entity_data);

    auto const n{get_num_instances()};

    FTestEntityRegistryEntityData entity_data;
    entity_data.locations = locations;

    entity_data.velocities.add_uninitialized(n);
    entity_data.velocities = directions;
    ml::multiply_in_place(entity_data.velocities, speeds);

    entity_data.healths = healths;
    entity_data.teams = teams;

    ml::add_uninitialised(entity_data.alive, n);
    for (int32 i{0}; i < n; ++i) {
        entity_data.alive[i] = static_cast<uint8>(healths[i]);
    }

    return entity_data;
}
void ATestCapitalShipFighters::clear_tick_buffers() {
    ml::reset(local_indices_to_remove,
              indices_ready_to_fire_buffer,
              new_laser_locations,
              new_laser_rotations);
}

// Checks
void ATestCapitalShipFighters::validate_array_sizes() const {
    ml::fatal_if_nums_not_equal({
        SANDBOX_NAMED_NUM(entity_indices),
        SANDBOX_NAMED_NUM(locations),
        SANDBOX_NAMED_NUM(directions),
        SANDBOX_NAMED_NUM(speeds),
        SANDBOX_NAMED_NUM(teams),
        SANDBOX_NAMED_NUM(healths),
        SANDBOX_NAMED_NUM(laser_cooldowns),
        SANDBOX_NAMED_NUM(target_indices),
        SANDBOX_NAMED_NUM(target_locations),
    });

    auto const n{get_num_instances()};
    auto const n_ismc{instances->GetNumInstances()};
    if (n_ismc < n) {
        UE_LOG(
            LogSandbox,
            Fatal,
            TEXT("ATestCapitalShipFighters::validate_array_sizes %d entities, %d ISMC instances"),
            n,
            n_ismc);
    }
}
