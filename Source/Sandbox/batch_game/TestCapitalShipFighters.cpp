#include "TestCapitalShipFighters.h"

#include <Sandbox/batch_game/test_entity_registry/TestEntityRegistry.h>
#include <Sandbox/batch_game/TestBatchActorCore.h>
#include <Sandbox/batch_game/TestCapitalShipFightersConfig.h>
#include <Sandbox/batch_game/TestLasers.h>
#include <Sandbox/logging/SandboxLogCategories.h>
#include <Sandbox/utilities/actor_utils.h>

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
}
void ATestCapitalShipFighters::move(float const dt) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShipFighters::move_ships);

    ml::direction(target_directions, locations, target_locations);
    ml::lerp_in_place(directions, target_directions, turn_speed_unitless * dt);
    ml::add_scaled_in_place(locations, directions, speeds, dt);
}
void ATestCapitalShipFighters::queue_commands() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShipFighters::queue_commands);
    handle_firing();
}
void ATestCapitalShipFighters::resolve_hit_events() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShipFighters::resolve_hit_events);

    ml::batch::resolve_hit_events(*entity_registry,
                                  owner_id,
                                  entity_handles,
                                  healths,
                                  local_indices_to_remove,
                                  entity_death_info);

    validate_array_sizes();
}
void ATestCapitalShipFighters::update_entity_registry() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShipFighters::update_entity_registry);

    auto const data{get_entity_data()};
    ATestEntityRegistry::ConstView view{entity_handles, data.get_const_view()};
    entity_registry->update_entities(view);

    entity_registry->set_death_infos(entity_death_info);
}
void ATestCapitalShipFighters::sync_from_registry() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShipFighters::sync_from_registry);

    auto const dead_entities{entity_registry->get_dead_entities_this_frame()};

    ml::collect_valid_indices_by_key(entity_handles, dead_entities, local_indices_to_remove);
    local_indices_to_remove.Sort(TGreater<int32>{});

    ml::remove_at_swap_many_sorted_desc(local_indices_to_remove,
                                        ismc_transforms,
                                        entity_handles,
                                        locations,
                                        directions,
                                        speeds,
                                        teams,
                                        healths,
                                        laser_cooldowns.remaining_times,
                                        target_indices,
                                        target_locations,
                                        target_directions);

    // Update health and the target entity index
    auto const n{get_num_instances()};
    for (int32 i{0}; i < n; ++i) {
        auto const entity_index{entity_handles[i]};
        healths[i] = entity_registry->get_health(entity_index);

        auto const target_entity_index{target_indices[i]};
        if (target_entity_index.is_valid()) {
            if (entity_registry->is_stale(target_entity_index)) {
                target_indices[i] = FRegistryEntityHandle{};
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

    constexpr bool mark_render_state_dirty{true};
    constexpr bool teleport{true};
    instances->BatchUpdateInstancesTransforms(
        0, ismc_transforms, is_world_space, mark_render_state_dirty, teleport);

    if (enable_target_debug_drawing || enable_ship_location_debug_drawing) { draw_debug_shapes(); }
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

// Visuals
void ATestCapitalShipFighters::configure_ismc() {
    instances->SetStaticMesh(actor_config->mesh);

    instances->SetCanEverAffectNavigation(false);

    instances->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

    instances->SetGenerateOverlapEvents(false);
    instances->SetReceivesDecals(false);
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

        if (enable_ship_location_debug_drawing) { debug_drawer.draw_sphere(ship_location); }

        if (enable_target_debug_drawing) {
            if (!target_indices[i].is_valid()) { continue; }

            auto const target_location{ml::get_vector3d(target_locations, i)};
            debug_drawer.draw_line(ship_location, target_location);
        }
    }
}

// Entity data
auto ATestCapitalShipFighters::get_entity_data() const -> FTestEntityRegistryEntityData {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShipFighters::get_entity_data);

    auto const n{get_num_instances()};

    FTestEntityRegistryEntityData entity_data;
    entity_data.locations = locations;

    ml::add_uninitialised(entity_data.velocities, n);
    entity_data.velocities = directions;
    ml::multiply_in_place(entity_data.velocities, speeds);

    entity_data.healths = healths;
    entity_data.teams = teams;

    ml::add_uninitialised(entity_data.alive, n);
    for (int32 i{0}; i < n; ++i) {
        entity_data.alive[i] = static_cast<uint8>(healths[i] > 0);
    }

    return entity_data;
}

// Spawning
void ATestCapitalShipFighters::spawn_instances(
    FVectors3f::ConstView const new_locations,
    FRotatorsf::ConstView const new_rotations,
    TConstArrayView<ETestTeam> const new_teams,
    TConstArrayView<FRegistryEntityHandle> const new_targets) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShipFighters::spawn_instances);

    auto const n_cur{get_num_instances()};
    auto const n_new{ml::num(new_locations)};

    ml::fatal_if_nums_not_equal({
        SANDBOX_NAMED_NUM(new_locations),
        SANDBOX_NAMED_NUM(new_rotations),
        SANDBOX_NAMED_NUM(new_teams),
        SANDBOX_NAMED_NUM(new_targets),
    });
    if (n_new < 1) { return; }

    auto const speed{actor_config->speed};

    // Add new data
    ml::append_from(locations, new_locations);
    ml::add_uninitialised(directions, n_new);
    ml::append_n(speeds, speed, n_new);
    teams.Append(new_teams);
    ml::append_n(healths, actor_config->health, n_new);
    target_indices.Append(new_targets);
    ml::add_zeroed(target_locations, n_new);
    ml::add_zeroed(target_directions, n_new);
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
    auto new_entities{entity_registry->add_entities(entity_data.get_const_view())};
    entity_handles.Append(MoveTemp(new_entities.registry_handles));

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
    auto const cooldown{actor_config->fire_cooldown};
    auto const fire_point_offset{actor_config->fire_point_offset};
    auto const aim_threshold{fire_dot_product_threshold};

    auto const laser_damage{actor_config->laser_damage};
    auto const laser_speed{actor_config->laser_speed};
    auto const laser_max_distance{actor_config->laser_max_distance};

    ml::reset(new_lasers);
    ml::add_uninitialised(n_ships, new_lasers);

    int32 write_index{0};
    for (int32 ship_index{0}; ship_index < n_ships; ++ship_index) {
        if (laser_cooldowns.remaining_times[ship_index] > 0.f) { continue; }

        if (!target_indices[ship_index].is_valid()) { continue; }

        auto const ship_location{ml::get_vector3f(locations, ship_index)};
        auto const target_direction{ml::get_vector3f(target_directions, ship_index)};
        auto const direction{ml::get_vector3f(directions, ship_index)};

        auto const dot_product{FVector3f::DotProduct(direction, target_direction)};
        if (dot_product < aim_threshold) { continue; }

        auto const laser_offset{fire_point_offset * direction};
        auto const laser_location{ship_location + laser_offset};

        ml::assign(new_lasers.locations, write_index, laser_location);
        ml::assign(new_lasers.rotations, write_index, direction.ToOrientationRotator());
        new_lasers.instigator_handles[write_index] = entity_handles[ship_index];

        laser_cooldowns.remaining_times[ship_index] = cooldown;
        ++write_index;
    }

    new_lasers.locations.set_num(write_index, EAllowShrinking::No);
    new_lasers.rotations.set_num(write_index, EAllowShrinking::No);
    new_lasers.instigator_handles.SetNumUninitialized(write_index, EAllowShrinking::No);

    new_lasers.damages.SetNumUninitialized(write_index);
    new_lasers.speeds.SetNumUninitialized(write_index);
    new_lasers.max_distances.SetNumUninitialized(write_index);

    new_lasers.set_damages(laser_damage);
    new_lasers.set_speeds(laser_speed);
    new_lasers.set_max_distances(laser_max_distance);

    laser_actor->spawn_lasers(new_lasers);
}

// Misc
void ATestCapitalShipFighters::clear_runtime_state() {
    instances->ClearInstances();

    ml::reset(entity_handles,
              local_indices_to_remove,
              locations,
              directions,
              speeds,
              teams,
              healths,
              target_indices,
              target_locations,
              target_directions,
              laser_cooldowns,
              new_lasers);
}
void ATestCapitalShipFighters::clear_tick_buffers() {
    ml::reset(local_indices_to_remove, new_lasers, entity_death_info);
}

// Checks
void ATestCapitalShipFighters::validate_array_sizes() const {
    ml::fatal_if_nums_not_equal({
        SANDBOX_NAMED_NUM(entity_handles),
        SANDBOX_NAMED_NUM(locations),
        SANDBOX_NAMED_NUM(directions),
        SANDBOX_NAMED_NUM(speeds),
        SANDBOX_NAMED_NUM(teams),
        SANDBOX_NAMED_NUM(healths),
        SANDBOX_NAMED_NUM(laser_cooldowns),
        SANDBOX_NAMED_NUM(target_indices),
        SANDBOX_NAMED_NUM(target_locations),
        SANDBOX_NAMED_NUM(target_directions),
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
