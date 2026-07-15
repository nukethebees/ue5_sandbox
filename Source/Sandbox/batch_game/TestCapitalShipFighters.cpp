#include "TestCapitalShipFighters.h"

#include <Sandbox/batch_game/test_entity_registry/TestEntityRegistry.h>
#include <Sandbox/batch_game/TestBatchActorCore.h>
#include <Sandbox/batch_game/TestCapitalShipFightersConfig.h>
#include <Sandbox/batch_game/TestLasers.h>
#include <Sandbox/batch_game/TestTeamVisualData.h>
#include <Sandbox/logging/SandboxLogCategories.h>
#include <Sandbox/utilities/actor_utils.h>

#include <SandboxCore/array_checks.h>
#include <SandboxCore/array_math.h>
#include <SandboxCore/array_utils.h>
#include <SandboxCore/soa_rotator_utils.h>
#include <SandboxCore/soa_vector_utils.h>
#include <SandboxCore/transforms.h>
#include <SandboxCore/uobject_utils.h>

#include <Async/ParallelFor.h>
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

    ensureAlways(IsValid(actor_config->team_visual_data));

    configure_ismc();

    debug_drawer = actor_config->debug_drawer;
    debug_drawer.world = GetWorld();

    turn_speed_radians = FMath::DegreesToRadians(actor_config->turn_speed_degrees);
}

void ATestCapitalShipFighters::begin_tick() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShipFighters::begin_tick);
    clear_tick_buffers();
    entity_registry->refresh_handles(entity_buffers.current().target_handles);
}
void ATestCapitalShipFighters::update_timers(float const dt) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShipFighters::update_timers);

    entity_buffers.current().laser_cooldowns.tick(dt);
}
void ATestCapitalShipFighters::make_decisions() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShipFighters::make_decisions);
}
void ATestCapitalShipFighters::move(float const dt) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShipFighters::move_ships);

    auto& data{entity_buffers.current()};

    ml::direction(target_directions, data.locations, target_locations);
    ml::lerp_in_place(data.directions, target_directions, turn_speed_unitless * dt);
    ml::add_scaled_in_place(data.locations, data.directions, data.speeds, dt);
}
void ATestCapitalShipFighters::queue_commands() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShipFighters::queue_commands);
    handle_firing();
}
void ATestCapitalShipFighters::resolve_hit_events() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShipFighters::resolve_hit_events);

    auto& data{entity_buffers.current()};

    ml::batch::resolve_hit_events(*entity_registry,
                                  owner_id,
                                  data.entity_handles,
                                  data.healths,
                                  local_indices_to_remove,
                                  entity_death_info);

    auto const view{entity_registry->get_damage_queue_view(owner_id)};
    auto const n{view.num()};
    for (int32 i{0}; i < n; ++i) {
        auto const ismc_index_hit{view.hit_items[i]};
        auto const instigator{view.instigators[i]};

        data.target_handles[ismc_index_hit] = instigator;
    }

    validate_array_sizes();
}
void ATestCapitalShipFighters::update_entity_registry() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShipFighters::update_entity_registry);

    prepare_entity_update_data();
    ATestEntityRegistry::ConstView view{entity_buffers.current().entity_handles,
                                        registry_update_data.get_const_view()};
    entity_registry->queue_entity_updates(view, entity_death_info);
}
void ATestCapitalShipFighters::sync_from_registry() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShipFighters::sync_from_registry);

    remove_dead_entities();
}
void ATestCapitalShipFighters::update_visuals() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShipFighters::update_visuals);

    prepare_ismc_transforms();
    update_ismc();

    if (enable_target_debug_drawing || enable_ship_location_debug_drawing) { draw_debug_shapes(); }
}
void ATestCapitalShipFighters::end_tick() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShipFighters::end_tick);
    TRACE_COUNTER_SET(SandboxTestFighterCount, get_num_instances());
}

// Accessors
auto ATestCapitalShipFighters::get_num_instances() const noexcept -> int32 {
    return ml::num(entity_buffers.current().num());
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

    instances->SetRemoveSwap();
    instances->SetNumCustomDataFloats(n_custom_ismc_floats);
}
void ATestCapitalShipFighters::prepare_ismc_transforms() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShipFighters::prepare_ismc_transforms);

    auto const n{get_num_instances()};

    auto const n_jobs{8};
    auto const updates_per_slice{FMath::DivideAndRoundUp(n, n_jobs)};

    auto const update_transforms{[this, updates_per_slice, n, n_jobs](int32 const job_index) {
        auto const& data{entity_buffers.current()};

        auto const begin{job_index * updates_per_slice};
        auto const end{FMath::Min(begin + updates_per_slice, n)};

        for (int32 i{begin}; i < end; ++i) {
            ismc_transforms[i].SetLocation(ml::get_vector3d(data.locations, i));

            auto const dir{ml::get_vector3d(data.directions, i)};
            auto const quat{FQuat::FindBetweenNormals(FVector::ForwardVector, dir)};

            ismc_transforms[i].SetRotation(quat);
        }
    }};

    ParallelFor(n_jobs, update_transforms);
}
void ATestCapitalShipFighters::update_ismc() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShipFighters::update_ismc);

    constexpr bool mark_render_state_dirty{true};
    constexpr bool teleport{true};
    instances->BatchUpdateInstancesTransforms(
        0, ismc_transforms, is_world_space, mark_render_state_dirty, teleport);
}
void ATestCapitalShipFighters::draw_debug_shapes() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShipFighters::draw_debug_shapes);

    auto const& data{entity_buffers.current()};

    auto const n{get_num_instances()};
    for (int32 i{0}; i < n; ++i) {
        FVector const ship_location{ml::get_vector3d(data.locations, i)};

        if (enable_ship_location_debug_drawing) { debug_drawer.draw_sphere(ship_location); }

        if (enable_target_debug_drawing) {
            if (!data.target_handles[i].is_valid()) { continue; }

            auto const target_location{ml::get_vector3d(target_locations, i)};
            debug_drawer.draw_line(ship_location, target_location);
        }
    }
}
void ATestCapitalShipFighters::write_ismc_custom_data(int32 const offset, int32 const count) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShipFighters::write_ismc_custom_data);

    check(count >= 0);
    if (count == 0) { return; }

    auto const& data{entity_buffers.current()};

    auto const colour_cache{
        UTestTeamVisualData::build_team_colour_cache(actor_config->team_visual_data)};
    custom_data_buffer.SetNumUninitialized(count * n_custom_ismc_floats, EAllowShrinking::No);

    auto const teams_slice{TConstArrayView<ETestTeam>{data.teams}.Slice(offset, count)};
    for (int32 i{0}; i < count; ++i) {
        auto const team{teams_slice[i]};

        // Custom ISMC data
        auto const base{i * n_custom_ismc_floats};
        auto const& colour{colour_cache[team]};
        custom_data_buffer[base + 0] = colour.R;
        custom_data_buffer[base + 1] = colour.G;
        custom_data_buffer[base + 2] = colour.B;
    }

    constexpr bool mark_render_dirty{false};
    instances->SetCustomData(offset, offset + count - 1, custom_data_buffer, mark_render_dirty);
}
void ATestCapitalShipFighters::write_ismc_custom_data() {
    write_ismc_custom_data(0, get_num_instances());
}

// Entity data
void ATestCapitalShipFighters::prepare_entity_update_data() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShipFighters::prepare_entity_update_data);

    auto const& data{entity_buffers.current()};
    auto const n{get_num_instances()};

    registry_update_data.reset();
    registry_update_data.locations = data.locations;

    ml::add_uninitialised(registry_update_data.velocities, n);
    registry_update_data.velocities = data.directions;
    ml::multiply_in_place(registry_update_data.velocities, data.speeds);

    registry_update_data.healths = data.healths;
    registry_update_data.teams = data.teams;

    ml::add_uninitialised(registry_update_data.alive, n);
    for (int32 i{0}; i < n; ++i) {
        registry_update_data.alive[i] = static_cast<uint8>(data.healths[i] > 0);
    }
}

// Spawning
void ATestCapitalShipFighters::spawn_instances(
    FVectors3f::ConstView const new_locations,
    FRotatorsf::ConstView const new_rotations,
    TConstArrayView<ETestTeam> const new_teams,
    TConstArrayView<FRegistryEntityHandle> const new_targets) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShipFighters::spawn_instances);
    auto& data{entity_buffers.current()};

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
    ml::append_n(data.tasks, Tasks::Attack, n_new);
    ml::append_from(data.locations, new_locations);
    ml::add_uninitialised(data.directions, n_new);
    ml::append_n(data.speeds, speed, n_new);
    data.teams.Append(new_teams);
    ml::append_n(data.healths, actor_config->health, n_new);
    data.target_handles.Append(new_targets);
    ml::add_zeroed(target_locations, n_new);
    ml::add_zeroed(target_directions, n_new);
    target_distance_sq.AddZeroed(n_new);
    data.laser_cooldowns.remaining_times.AddZeroed(n_new);

    // Fill entity data and set directions
    new_spawn_entity_data.add_uninitialised(n_new);

    for (int32 i{0}; i < n_new; ++i) {
        auto const index{n_cur + i};

        auto const direction{ml::get_vector3f(new_rotations, i)};

        ml::assign(data.directions, index, direction);
        ml::assign_from(new_spawn_entity_data.locations, i, data.locations, index);
        new_spawn_entity_data.healths[i] = data.healths[index];
        new_spawn_entity_data.teams[i] = data.teams[index];
        new_spawn_entity_data.alive[i] = true;
    }
    new_spawn_entity_data.set_all_entity_types(ETestEntityType::CapitalShipFighter);

    // Velocities
    TConstArrayView<float> const new_speeds{data.speeds.GetData() + n_cur, n_new};
    ml::assign_from_scaled(new_spawn_entity_data.velocities,
                           data.directions.get_const_view().right(n_new),
                           new_speeds);

    // Entity handles
    new_spawn_entity_handles =
        entity_registry->add_entities(new_spawn_entity_data.get_const_view());
    data.entity_handles.Append(new_spawn_entity_handles.registry_handles);

    // ISMC transforms
    ismc_transforms.AddDefaulted(n_new);
    for (int32 i{0}; i < n_new; ++i) {
        auto const idx{n_cur + i};
        ismc_transforms[idx].SetLocation(ml::get_vector3d(data.locations, idx));
    }

    instances->AddInstances(
        TArray<FTransform>{ismc_transforms.GetData() + n_cur, n_new}, is_world_space, false);
    write_ismc_custom_data(n_cur, n_new);

    validate_array_sizes();
}

void ATestCapitalShipFighters::self_destruct_fighter(FRegistryEntityHandle const handle) {
    auto& data{entity_buffers.current()};

    auto const index{data.entity_handles.Find(handle)};
    check(index != INDEX_NONE);

    data.healths[index] = 0;
    local_indices_to_remove.Add(index);
}

// Combat
void ATestCapitalShipFighters::handle_firing() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShipFighters::handle_firing);
    auto& data{entity_buffers.current()};

    ml::dist_sq(target_distance_sq, data.locations, target_locations);

    auto const n_ships{get_num_instances()};
    auto const cooldown{actor_config->fire_cooldown};
    auto const fire_point_offset{actor_config->fire_point_offset};
    auto const aim_threshold{fire_dot_product_threshold};

    auto const laser_damage{actor_config->laser_damage};
    auto const laser_speed{actor_config->laser_speed};
    auto const laser_max_distance{actor_config->laser_max_distance};
    auto const laser_max_distance_sq{laser_max_distance * laser_max_distance};

    auto const colour_cache{
        UTestTeamVisualData::build_team_colour_cache(actor_config->team_visual_data)};

    ml::reset(new_lasers);
    ml::add_uninitialised(n_ships, new_lasers);

    int32 write_index{0};
    for (int32 ship_index{0}; ship_index < n_ships; ++ship_index) {
        if (target_distance_sq[ship_index] > laser_max_distance_sq) { continue; }
        if (data.laser_cooldowns.remaining_times[ship_index] > 0.f) { continue; }
        if (!data.target_handles[ship_index].is_valid()) { continue; }

        auto const ship_location{ml::get_vector3f(data.locations, ship_index)};
        auto const target_direction{ml::get_vector3f(target_directions, ship_index)};
        auto const direction{ml::get_vector3f(data.directions, ship_index)};

        auto const dot_product{FVector3f::DotProduct(direction, target_direction)};
        if (dot_product < aim_threshold) { continue; }

        auto const laser_offset{fire_point_offset * direction};
        auto const laser_location{ship_location + laser_offset};

        ml::assign(new_lasers.locations, write_index, laser_location);
        ml::assign(new_lasers.rotations, write_index, direction.ToOrientationRotator());
        new_lasers.instigator_handles[write_index] = data.entity_handles[ship_index];
        new_lasers.colours[write_index] = colour_cache[data.teams[ship_index]];

        data.laser_cooldowns.remaining_times[ship_index] = cooldown;
        ++write_index;
    }

    if (write_index != n_ships) { ml::set_num(new_lasers, write_index, EAllowShrinking::No); }

    new_lasers.set_damages(laser_damage);
    new_lasers.set_speeds(laser_speed);
    new_lasers.set_max_distances(laser_max_distance);

    laser_actor->spawn_lasers(new_lasers);
}

// Misc
void ATestCapitalShipFighters::clear_runtime_state() {
    auto& data{entity_buffers.current()};
    instances->ClearInstances();

    ml::reset(local_indices_to_remove,
              data,
              target_locations,
              target_directions,
              target_distance_sq,
              new_lasers);
}
void ATestCapitalShipFighters::clear_tick_buffers() {
    ml::reset(local_indices_to_remove, new_lasers, entity_death_info, new_spawn_entity_data);
}
void ATestCapitalShipFighters::remove_dead_entities() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShipFighters::remove_dead_entities);
    auto& data{entity_buffers.current()};

    local_indices_to_remove.Sort(TGreater<int32>{});

    ml::remove_at_swap_many_sorted_desc(local_indices_to_remove,
                                        ismc_transforms,
                                        data,
                                        target_locations,
                                        target_directions,
                                        target_distance_sq);

    // Update target entity index
    auto const n{get_num_instances()};
    for (int32 i{0}; i < n; ++i) {
        auto const entity_index{data.entity_handles[i]};

        auto const target_entity_index{data.target_handles[i]};
        if (target_entity_index.is_valid()) {
            if (entity_registry->is_stale(target_entity_index)) {
                data.target_handles[i] = FRegistryEntityHandle{};
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

// Checks
void ATestCapitalShipFighters::validate_array_sizes() const {
    auto const& data{entity_buffers.current()};

    ml::fatal_if_nums_not_equal({
        SANDBOX_NAMED_NUM(data),
        SANDBOX_NAMED_NUM(target_locations),
        SANDBOX_NAMED_NUM(target_directions),
        SANDBOX_NAMED_NUM(target_distance_sq),
        SANDBOX_NAMED_NUM(ismc_transforms),
        SANDBOX_NAMED_NUM(instances->GetNumInstances()),
    });
}
