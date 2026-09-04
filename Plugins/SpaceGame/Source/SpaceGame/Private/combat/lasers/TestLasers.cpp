#include "SpaceGame/combat/lasers/TestLasers.h"

#include <SandboxGameShared/utilities/actor_utils.h>
#include <SpaceGame/entities/TestEntityRegistry.h>
#include <SpaceGame/simulation/SpatialQueryManager.h>
#include <SpaceGame/support/logging/SandboxLogCategories.h>

#include <SandboxCore/array_checks.h>
#include <SandboxCore/array_math.h>
#include <SandboxCore/array_utils.h>
#include <SandboxCore/invoke.h>
#include <SandboxCore/soa_rotator_utils.h>
#include <SandboxCore/soa_vector_utils.h>
#include <SandboxCoreEngine/uobject_utils.h>

#include <Async/ParallelFor.h>
#include <Components/InstancedStaticMeshComponent.h>
#include <Components/SceneComponent.h>
#include <NiagaraFunctionLibrary.h>
#include <ProfilingDebugging/CountersTrace.h>
#include <Templates/Greater.h>

TRACE_DECLARE_INT_COUNTER(SandboxTestLaserCount, TEXT("Sandbox/TestLaserCount"));
TRACE_DECLARE_INT_COUNTER(SandboxTestLaserISMCCount, TEXT("Sandbox/TestLaserISMCCount"));

namespace ml::test_lasers {
void SpawnRequests::set_damages(int32 const value) {
    ml::fill(damages, value);
}
void SpawnRequests::set_speeds(float const value) {
    ml::fill(speeds, value);
}
void SpawnRequests::set_max_distances(float const value) {
    ml::fill(max_distances, value);
}
void SpawnRequests::set_colours(FLinearColor const value) {
    ml::fill(colours, value);
}
}

ATestLasers::ATestLasers()
    : instances{CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("instances"))} {
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("root"));

    instances->SetupAttachment(RootComponent);

    PrimaryActorTick.bCanEverTick = false;
    PrimaryActorTick.bStartWithTickEnabled = false;

    ml::set_actor_component_mobility(*this, EComponentMobility::Static);
}

void ATestLasers::bind_simulation_clock(ATestBatchOrchestrator const& orchestrator) noexcept {
    simulation_clock.bind(orchestrator);
}

// Actor lifecycle
void ATestLasers::begin_play() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestLasers::begin_play);
    TRACE_COUNTER_SET(SandboxTestLaserCount, 0);
    TRACE_COUNTER_SET(SandboxTestLaserISMCCount, 0);
    check(entity_registry);
    check(query_manager);

    number_spawned = 0;

    if (!actor_config) {
        UE_LOG(LogSandboxLearning, Fatal, TEXT("actor_config is nullptr."));
    }
    if (!actor_config->mesh || !actor_config->material) {
        UE_LOG(LogSandboxLearning, Fatal, TEXT("actor_config is not ready."));
    }

    configure_ismc();
    preallocate_instances();

#if WITH_EDITOR
    debug_drawer.world = GetWorld();
#endif

    validate_array_sizes();
}

void ATestLasers::begin_tick() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestLasers::begin_tick);
    clear_hit_buffers();
}
void ATestLasers::commit_spawns() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestLasers::commit_spawns);
    process_pending_spawns();
    clear_spawn_buffers();
}
void ATestLasers::simulate(float const dt) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestLasers::tick);

    tick_lifetimes(dt);
    collect_old_instance_indices();
    remove_instances(to_remove);

    handle_collisions(dt);
    update_locations(dt);
}
void ATestLasers::update_visual_data() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestLasers::update_visual_data);

    prepare_ismc_transforms();
    update_ismc();
}
void ATestLasers::commit_visual_data() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestLasers::commit_visual_data);

    instances->MarkRenderStateDirty();
    spawn_hit_effects();
}
void ATestLasers::end_tick() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestLasers::end_tick);
    TRACE_COUNTER_SET(SandboxTestLaserCount, get_num_instances());
    TRACE_COUNTER_SET(SandboxTestLaserISMCCount, instances->GetNumInstances());

    validate_array_sizes();
}

// Accessors
auto ATestLasers::get_num_instances() const noexcept -> int32 {
    return entities.lifetimes_remaining.Num();
}

// Spawning / Configuration
void ATestLasers::queue_laser_spawns(SpawnRequests const& spawn_data) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestLasers::queue_laser_spawns);

    spawn_data.validate_array_sizes();
    pending_spawns.append_from(spawn_data);
}
void ATestLasers::preallocate_instances() {
    instances->PreAllocateInstancesMemory(n_preallocated_instances);
    entities.reserve(n_preallocated_instances);
}
void ATestLasers::process_pending_spawns() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestLasers::process_pending_spawns);

    pending_spawns.validate_array_sizes();
    auto const n_to_add{ml::num(pending_spawns)};
    if (n_to_add <= 0) {
        return;
    }

    auto const tick_period{static_cast<float>(simulation_clock.get_tick_period())};
    constexpr float fixed_spawn_offset{10.f};

    auto const offset{get_num_instances()};
    auto const new_total{offset + n_to_add};

    /* ---------------------------------------------- */
    // Instance data
    /* ---------------------------------------------- */
    custom_data_spawn_buffer.SetNumUninitialized(n_to_add * n_custom_ismc_floats,
                                                 EAllowShrinking::No);

    ml::append_from(entities.colours, pending_spawns.colours);
    ml::append_from(entities.locations, pending_spawns.locations);
    ml::append_from(entities.rotations, pending_spawns.rotations);
    entities.damages.Append(pending_spawns.damages);
    entities.instigator_handles.Append(pending_spawns.instigator_handles);

    entities.ismc_data.AddUninitialized(n_to_add);
    ml::add_uninitialised(n_to_add, entities.velocities, entities.lifetimes_remaining);

    auto const time{GetWorld()->GetTimeSeconds()};
    for (int32 i{0}; i < n_to_add; ++i) {
        auto const speed{pending_spawns.speeds[i]};
        auto const max_distance{pending_spawns.max_distances[i]};
        auto const lifetime{max_distance / speed};

        auto const index{offset + i};

        auto const forward_direction{ml::get_rotator3f(entities.rotations, index).Vector()};
        auto const forward_velocity{forward_direction * speed};

        auto const base_velocity{ml::get_vector3f(pending_spawns.base_velocities, i)};

        auto const velocity{base_velocity + forward_velocity};

        auto const base_spawn_location{ml::get_vector3f(entities.locations, index)};

        auto const spawn_location{base_spawn_location + forward_velocity * tick_period +
                                  forward_direction * fixed_spawn_offset};

        ml::assign(entities.locations, index, spawn_location);
        ml::assign(entities.velocities, index, velocity);
        entities.lifetimes_remaining[index] = lifetime;

        // Per-instance custom data
        auto const base{i * n_custom_ismc_floats};
        auto const& colour{pending_spawns.colours[i]};

        custom_data_spawn_buffer[base + 0] = colour.R;
        custom_data_spawn_buffer[base + 1] = colour.G;
        custom_data_spawn_buffer[base + 2] = colour.B;
        custom_data_spawn_buffer[base + 3] = lifetime;
        custom_data_spawn_buffer[base + 4] = time;
    }
    number_spawned += n_to_add;

    /* ---------------------------------------------- */
    // ISMC data
    /* ---------------------------------------------- */
    dummy_transforms_spawn_buffer.SetNum(n_to_add, EAllowShrinking::No);

    constexpr bool return_indices{false};
    constexpr bool update_navigation{false};
    instances->AddInstances(
        dummy_transforms_spawn_buffer, return_indices, is_world_space, update_navigation);
    instances->SetCustomData(offset, offset + n_to_add - 1, custom_data_spawn_buffer, false);

    validate_array_sizes();
    clear_spawn_buffers();
}

// Movement
void ATestLasers::update_locations(float const dt) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestLasers::update_locations);
    ml::add_scaled_in_place(entities.locations, entities.velocities, dt);
}
void ATestLasers::handle_collisions(float const dt) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestLasers::handle_collisions);

    auto const n{get_num_instances()};
    if (n < 1) {
        return;
    }

    auto& data{thread_local_collision_data};
    if (data.Num() < collision_jobs) {
        data.SetNum(collision_jobs);
    }

    auto const updates_per_slice{FMath::DivideAndRoundUp(n, collision_jobs)};

    ParallelFor(collision_jobs, [=, this](int32 const job_index) {
        check_collision_thread(
            job_index, updates_per_slice, dt, thread_local_collision_data[job_index], *this);
    });

    merge_collision_data();
    entity_registry->queue_direct_damage_events(collision_damage_events);

    to_remove.Sort(TGreater<int32>{});
    remove_instances(to_remove);
    hit_details.validate_array_sizes();
}
void ATestLasers::check_collision_thread(int32 const job_index,
                                         int32 const updates_per_slice,
                                         float const dt,
                                         ThreadLocalCollisionData& data,
                                         ATestLasers const& lasers) {
    auto const n{lasers.get_num_instances()};
    ml::reset(data.traces, data.trace_hits, data.damage_events, data.to_remove, data.hit_details);

    auto const i_start{job_index * updates_per_slice};
    auto const i_end{FMath::Min(i_start + updates_per_slice, n)};
    auto const trace_count{i_end - i_start};
    if (trace_count <= 0) {
        return;
    }

    data.traces.add_uninitialised(trace_count);
    data.trace_hits.add_defaulted(trace_count);

    for (int32 trace_index{}; trace_index < trace_count; ++trace_index) {
        auto const entity_index{i_start + trace_index};
        auto const start{ml::get_vector3f(lasers.entities.locations, entity_index)};
        auto const velocity{ml::get_vector3f(lasers.entities.velocities, entity_index)};
        data.traces.starts.set(trace_index, start);
        data.traces.ends.set(trace_index, start + dt * velocity);
    }

    auto const ignored_entities{
        TConstArrayView<FRegistryEntityHandle>{lasers.entities.instigator_handles}.Slice(
            i_start, trace_count)};
    lasers.query_manager->get_collision_system().get_uniform_grid().trace_aabbs(
        data.traces.get_const_view(), data.trace_hits.get_view(), ignored_entities);

    for (int32 trace_index{}; trace_index < trace_count; ++trace_index) {
        if (data.trace_hits.hits[trace_index] == 0) {
            continue;
        }

        auto const entity_index{i_start + trace_index};
        data.to_remove.Add(entity_index);

        auto const damaged_entity{data.trace_hits.entities[trace_index]};
        if (damaged_entity.is_valid()) {
            data.damage_events.damaged_entities.Add(damaged_entity);
            data.damage_events.damage_amounts.Add(lasers.entities.damages[entity_index]);
            data.damage_events.instigators.Add(lasers.entities.instigator_handles[entity_index]);
        }

        data.hit_details.locations.add(data.trace_hits.locations[trace_index]);
    }
}

void ATestLasers::merge_collision_data() {
    auto& data{thread_local_collision_data};
    ml::reset(to_remove, hit_details, collision_damage_events);

    for (int32 i{}; i < collision_jobs; ++i) {
        auto const& thread_data{data[i]};
        auto const n_hits{thread_data.to_remove.Num()};
        check(n_hits == ml::num(thread_data.hit_details.locations));

        for (int32 j{}; j < n_hits; ++j) {
            auto const entity_index{thread_data.to_remove[j]};
            to_remove.Add(entity_index);
            hit_details.locations.add(thread_data.hit_details.locations[j]);
            hit_details.colours.Add(entities.colours[entity_index]);
        }

        auto const damage_count{thread_data.damage_events.num()};
        for (int32 j{}; j < damage_count; ++j) {
            collision_damage_events.damaged_entities.Add(
                thread_data.damage_events.damaged_entities[j]);
            collision_damage_events.damage_amounts.Add(thread_data.damage_events.damage_amounts[j]);
            collision_damage_events.instigators.Add(thread_data.damage_events.instigators[j]);
        }
    }
}

// Visuals
void ATestLasers::configure_ismc() {
    instances->SetMobility(EComponentMobility::Movable);
    check(instances->SetStaticMesh(actor_config->mesh));
    instances->SetMobility(EComponentMobility::Static);
    instances->SetMaterial(0, actor_config->material);

    instances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    instances->SetGenerateOverlapEvents(false);

    instances->SetCanEverAffectNavigation(false);

    instances->SetCastShadow(false);
    instances->SetAffectDistanceFieldLighting(false);
    instances->SetReceivesDecals(false);

    instances->SetCullDistances(actor_config->min_cull_distance, actor_config->max_cull_distance);

    instances->SetNumCustomDataFloats(n_custom_ismc_floats);

    instances->SetRemoveSwap();
}
void ATestLasers::update_ismc() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestLasers::update_ismc);

    constexpr bool mark_render_dirty{false};
    constexpr bool teleport{true};

    instances->BatchUpdateInstancesData(
        0, entities.ismc_data.Num(), entities.ismc_data.GetData(), mark_render_dirty, teleport);
}
void ATestLasers::prepare_ismc_transforms() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestLasers::prepare_ismc_transforms);

    auto const n_ismc_instances{instances->GetNumInstances()};
    auto const n_laser_instances{get_num_instances()};

    auto const n_transforms{entities.ismc_data.Num()};
    auto const n_transforms_to_add(n_ismc_instances - n_transforms);

    if (!n_ismc_instances && !n_laser_instances) {
        return;
    }

    entities.ismc_data.Reset();
    entities.ismc_data.AddUninitialized(n_ismc_instances);

    auto const n_jobs{8};
    auto const updates_per_slice{FMath::DivideAndRoundUp(n_laser_instances, n_jobs)};

    auto const fill_array{[this, updates_per_slice, n_laser_instances, n_jobs, n_ismc_instances](
                              int32 const job_index) {
        if (job_index == n_jobs) {
            for (int32 i{n_laser_instances}; i < n_ismc_instances; ++i) {
                entities.ismc_data[i] = FMatrix::Identity;
            }
        } else {
            auto const begin{job_index * updates_per_slice};
            auto const end{FMath::Min(begin + updates_per_slice, n_laser_instances)};

            for (int32 i{begin}; i < end; ++i) {
                auto const rotation{ml::get_rotator3d(entities.rotations, i)};

                entities.ismc_data[i] =
                    FTransform{
                        rotation.Quaternion(),
                        ml::get_vector3d(entities.locations, i),
                    }
                        .ToMatrixWithScale();
            }
        }
    }};

    ParallelFor(n_jobs + 1, fill_array);
}
void ATestLasers::spawn_hit_effects() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestLasers::spawn_hit_effects);

    static FName const colour_parameter{TEXT("User.Colour")};
    static FName const ribbon_colour_parameter{TEXT("User.Ribbon_Colour")};

    auto hit_effect{actor_config->hit_effect};
    if (!IsValid(hit_effect)) {
        if (!have_warned_hit_effect) {
            UE_LOG(
                LogSandbox, Warning, TEXT("ATestLasers::spawn_hit_effects: hit_effect is nullptr"));
            have_warned_hit_effect = true;
        }

        return;
    }

    auto const n{ml::num(hit_details)};
    if (n < 1) {
        return;
    }

    FVector const scale{FVector::OneVector};

    auto* world{GetWorld()};
    for (int32 i{0}; i < n; ++i) {
        constexpr bool auto_destroy{true};
        constexpr bool auto_activate{false};

        auto* system{UNiagaraFunctionLibrary::SpawnSystemAtLocation(
            world,
            hit_effect,
            ml::get_vector3d(hit_details.locations, i),
            FRotator::ZeroRotator,
            scale,
            auto_destroy,
            auto_activate,
            ENCPoolMethod::AutoRelease)};
        if (!IsValid(system)) {
            continue;
        }

        constexpr double colour_scale{20.0};
        auto const colour{hit_details.colours[i] * colour_scale};
        system->SetVariableLinearColor(colour_parameter, colour);
        system->SetVariableLinearColor(ribbon_colour_parameter, colour);
        system->Activate();
    }
}

// Lifetimes
void ATestLasers::tick_lifetimes(float const dt) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestLasers::tick_lifetimes);

    ml::subtract_in_place(TArrayView<float>{entities.lifetimes_remaining}, dt);
}
void ATestLasers::collect_old_instance_indices() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestLasers::collect_old_instance_indices);

    auto const n{get_num_instances()};
    if (n < 1) {
        return;
    }

    to_remove.Reset();

    for (int32 i{n - 1}; i >= 0; --i) {
        if (entities.lifetimes_remaining[i] <= 0.f) {
            to_remove.Add(i);
        }
    }
}

// Misc
void ATestLasers::clear_runtime_state() {
    instances->ClearInstances();
    ml::reset(entities);
    clear_spawn_buffers();
}
void ATestLasers::remove_instances(TConstArrayView<int32> const indices) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestLasers::remove_instances);

    auto const n{indices.Num()};
    if (n < 1) {
        return;
    }

    {
        TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestLasers::remove_instances::remove_at_swap);
        ml::remove_at_swap_many_sorted_desc(indices, entities);
    }

    {
        TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestLasers::remove_instances::ismc);

        constexpr bool is_reverse_sorted{true};
        instances->RemoveInstances({indices.GetData(), n}, is_reverse_sorted);
    }

    validate_array_sizes();
}
void ATestLasers::clear_spawn_buffers() {
    ml::reset(pending_spawns, to_remove);
}
void ATestLasers::clear_hit_buffers() {
    ml::reset(hit_details, collision_damage_events);
}

// Checks
void ATestLasers::validate_array_sizes() const {
    ml::fatal_if_nums_not_equal({
        SANDBOX_NAMED_NUM(entities),
        SANDBOX_NAMED_NUM(instances->GetNumInstances()),
    });
    entities.validate_array_sizes();
}
