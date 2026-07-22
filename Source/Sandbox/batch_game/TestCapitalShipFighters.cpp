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

namespace {
inline auto get_span(FVectors3f& vec, FIndexSpan const span) {
    return vec.get_view().slice(span.offset, span.count);
}
inline auto get_span(FVectors3f const& vec, FIndexSpan const span) {
    return vec.get_view().slice(span.offset, span.count);
}
}

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

    auto& data{entity_buffers.current()};

    clear_tick_buffers();
    refresh_target_handles();
    refresh_task_views();
    refresh_target_locations();
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

    move(dt, task_views[std::to_underlying(Task::MoveToDestination)]);
    move(dt, task_views[std::to_underlying(Task::Attack)]);
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

    tasks_are_contiguous(); // This should be true before pruning

    remove_dead_entities();
    commit_spawns();
    commit_orders();

    if (!tasks_are_contiguous()) { refresh_layout(); }
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

// Movement
void ATestCapitalShipFighters::move(float const dt, TaskView& fighters) {
    ml::direction(fighters.target_directions, fighters.locations, fighters.target_locations);
    ml::lerp_in_place(fighters.directions, fighters.target_directions, turn_speed_unitless * dt);
    ml::add_scaled_in_place(fighters.locations, fighters.directions, fighters.speeds, dt);
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

auto ATestCapitalShipFighters::get_task_spans() const -> TaskSpans {
    check_fighter_tasks();
    return task_spans;
}
auto ATestCapitalShipFighters::get_task_counts() const -> TaskCounts {
    TaskCounts counts{};

    auto const& data{entity_buffers.current()};
    auto const n_tasks{data.tasks.Num()};

    for (int32 i{}; i < n_tasks; ++i) {
        ++counts[std::to_underlying(data.tasks[i])];
    }

    return counts;
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

            auto const target_location{ml::get_vector3d(data.target_locations, i)};
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
bool ATestCapitalShipFighters::tasks_are_contiguous() const noexcept {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShipFighters::tasks_are_contiguous);

    auto current_task_group{Task::Standby};
    TaskSpans checked_task_spans{};

    auto const& data{entity_buffers.current()};
    auto const n_tasks{data.tasks.Num()};

    for (int32 i{}; i < n_tasks; ++i) {
        auto const task{data.tasks[i]};

        if (task > current_task_group) {
            current_task_group = task;
        } else if (task < current_task_group) {
            return false;
        }
    }

    return false;
}
void ATestCapitalShipFighters::refresh_layout() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShipFighters::refresh_layout);

    auto const task_counts{get_task_counts()};
    auto const n_fighters{get_num_instances()};

    TaskCounts write_indexes{};

    {
        int32 offset{0};

        for (int32 i{0}; i < n_task_types; ++i) {
            auto const count{task_counts[i]};

            write_indexes[i] = offset;

            task_spans[i].offset = offset;
            task_spans[i].count = count;

            offset += count;
        }

        check(offset == n_fighters);
    }

    entity_buffers.cycle();
    auto const& old_data{entity_buffers.previous()};
    auto& new_data{entity_buffers.current()};
    new_data.reset();
    new_data.add_uninitialised(n_fighters);

    check(old_data.num() == new_data.num());

    for (int32 i{0}; i < n_fighters; i++) {
        auto const task{old_data.tasks[i]};
        auto const task_value{std::to_underlying(task)};

        auto const write_index{write_indexes[task_value]++};

        static_assert(ml::SupportsCopyElement<TArray<ETestCapitalShipFightersTask>>, "Copy");

        new_data.copy_element(write_index, old_data, i);
    }

    check_fighter_tasks();
}

// Spawning
void ATestCapitalShipFighters::queue_spawns(TestCapitalShipFighterSpawnQueue const& new_spawns) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShipFighters::queue_spawns);

    spawn_queue.append_from(new_spawns);
}
void ATestCapitalShipFighters::commit_spawns() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShipFighters::commit_spawns);

    new_spawn_entity_data.reset();

    auto const& new_locations{spawn_queue.locations};
    auto const& new_rotations{spawn_queue.rotations};
    auto const& new_teams{spawn_queue.teams};
    auto const& new_targets{spawn_queue.targets};

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
    ml::append_n(data.tasks, Task::Attack, n_new);
    ml::append_from(data.locations, new_locations);
    ml::add_uninitialised(data.directions, n_new);
    ml::append_n(data.speeds, speed, n_new);
    data.teams.Append(new_teams);
    ml::append_n(data.healths, actor_config->health, n_new);
    data.target_handles.Append(new_targets);
    ml::add_zeroed(data.target_locations, n_new);
    ml::add_zeroed(data.target_directions, n_new);
    data.target_distance_sq.AddZeroed(n_new);
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

// Destruction
void ATestCapitalShipFighters::self_destruct_fighter(FRegistryEntityHandle const handle) {
    auto& data{entity_buffers.current()};

    auto const index{data.entity_handles.Find(handle)};
    check(index != INDEX_NONE);

    data.healths[index] = 0;
    local_indices_to_remove.Add(index);
}
void ATestCapitalShipFighters::remove_dead_entities() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShipFighters::remove_dead_entities);
    auto& data{entity_buffers.current()};

    local_indices_to_remove.Sort(TGreater<int32>{});

    ml::remove_at_swap_many_sorted_desc(local_indices_to_remove, ismc_transforms, data);

    // Update target handle
    auto const n{get_num_instances()};
    for (int32 i{0}; i < n; ++i) {
        auto const target_handle{data.target_handles[i]};
        if (target_handle.is_valid()) {
            if (entity_registry->is_stale(target_handle)) {
                data.target_handles[i] = FRegistryEntityHandle{};
            } else {
                ml::assign(data.target_locations, i, entity_registry->get_location(target_handle));
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

// Combat
void ATestCapitalShipFighters::handle_firing() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShipFighters::handle_firing);
    auto& data{entity_buffers.current()};

    ml::dist_sq(data.target_distance_sq, data.locations, data.target_locations);

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
        if (data.target_distance_sq[ship_index] > laser_max_distance_sq) { continue; }
        if (data.laser_cooldowns.remaining_times[ship_index] > 0.f) { continue; }
        if (!data.target_handles[ship_index].is_valid()) { continue; }

        auto const ship_location{ml::get_vector3f(data.locations, ship_index)};
        auto const target_direction{ml::get_vector3f(data.target_directions, ship_index)};
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

// Orders
void ATestCapitalShipFighters::refresh_task_views() {
    auto const n{task_spans.Num()};
    auto& data{entity_buffers.current()};

    for (int32 i{0}; i < n; ++i) {
        auto const span{task_spans[i]};
        const_task_views[i] = data.get_const_view(span.offset, span.count);
        task_views[i] = data.get_view(span.offset, span.count);
    }
}
void ATestCapitalShipFighters::queue_orders(TestCapitalShipFighterOrderQueue const& queue) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShipFighters::queue_orders);

    order_queue.append_from(queue);
}
void ATestCapitalShipFighters::commit_orders() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShipFighters::commit_orders);

    auto& data{entity_buffers.current()};

    auto const n_orders{ml::num(order_queue)};
    if (n_orders < 1) { return; }

    index_buffer.SetNumUninitialized(n_orders, EAllowShrinking::No);

    for (int32 i{0}; i < n_orders; ++i) {
        index_buffer[i] = data.entity_handles.Find(order_queue.handles[i]);
    }

    for (int32 i{0}; i < n_orders; ++i) {
        auto const fighter_index{index_buffer[i]};
        if (fighter_index == INDEX_NONE) { continue; }

        auto const order{order_queue.orders[i]};
        if (order.task) { data.tasks[fighter_index] = order_queue.tasks[i]; }
        if (order.target) { data.target_handles[fighter_index] = order_queue.targets[i]; }
    }
}

// Targets
void ATestCapitalShipFighters::refresh_target_handles() {
    entity_registry->refresh_handles(entity_buffers.current().target_handles);
}
void ATestCapitalShipFighters::refresh_target_locations() {
    auto& data{entity_buffers.current()};
    entity_registry->refresh_locations(data.target_handles, data.target_locations);
}

// Misc
void ATestCapitalShipFighters::clear_runtime_state() {
    auto& data{entity_buffers.current()};
    instances->ClearInstances();

    ml::reset(local_indices_to_remove, data, new_lasers);
}
void ATestCapitalShipFighters::clear_tick_buffers() {
    ml::reset(local_indices_to_remove, new_lasers, entity_death_info, spawn_queue, order_queue);
}

// Checks
#if DO_CHECK
void ATestCapitalShipFighters::validate_array_sizes() const {
    auto const& data{entity_buffers.current()};

    ml::fatal_if_nums_not_equal({
        SANDBOX_NAMED_NUM(data),
        SANDBOX_NAMED_NUM(ismc_transforms),
        SANDBOX_NAMED_NUM(instances->GetNumInstances()),
    });
}
void ATestCapitalShipFighters::check_fighter_tasks() const {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShipFighters::check_fighter_tasks);

    auto current_task_group{Task::Standby};
    TaskSpans checked_task_spans{};

    auto const& data{entity_buffers.current()};
    auto const n_tasks{data.tasks.Num()};

    for (int32 i{}; i < n_tasks; ++i) {
        auto const task{data.tasks[i]};
        auto const task_value{std::to_underlying(task)};

        if (task == current_task_group) {
            ++checked_task_spans[task_value].count;
            continue;
        } else if (task > current_task_group) {
            current_task_group = task;
            checked_task_spans[task_value].offset = i;
            checked_task_spans[task_value].count = 1;
        } else {
            UE_LOG(LogSandbox,
                   Fatal,
                   TEXT("Found task %s when current group was %s"),
                   *ml::to_string_without_type_prefix(task),
                   *ml::to_string_without_type_prefix(current_task_group));
        }
    }

    // Set offset for any remaining tasks
    auto const n_fighters{get_num_instances()};
    for (int32 i{1}; i < n_task_types; ++i) {
        auto const last_span{checked_task_spans[i - 1]};
        auto const last_end{last_span.end()};
        auto& cur_span{checked_task_spans[i]};

        if (cur_span.offset < last_end) {
            cur_span.offset = last_end;
            check(cur_span.count == 0);
        }
    }

    if (checked_task_spans != task_spans) {
        FString msg{"Incorrect task spans."};
        for (int32 i{0}; i < n_task_types; ++i) {
            msg += FString::Printf(TEXT("\n    %s: Exp: %s, Got: %s"),
                                   *ml::to_string_without_type_prefix(static_cast<Task>(i)),
                                   *task_spans[i].to_compact_string(),
                                   *checked_task_spans[i].to_compact_string());
        }

        UE_LOG(LogSandbox, Fatal, TEXT("%s"), *msg);
    }
}
#endif
