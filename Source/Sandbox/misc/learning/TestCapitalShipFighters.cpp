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
}
void ATestCapitalShipFighters::begin_tick() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShipFighters::begin_tick);
}
void ATestCapitalShipFighters::tick(float const dt) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShipFighters::tick);

    laser_cooldowns.tick(dt);

    move_ships(dt);
    handle_firing();
}
void ATestCapitalShipFighters::update_entity_registry() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShipFighters::update_entity_registry);

    auto const data{get_entity_data(0, get_num_instances())};
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
                                        entity_indices,
                                        locations,
                                        rotations,
                                        velocities,
                                        teams,
                                        healths,
                                        laser_cooldowns.remaining_times);

    {
        auto const n{get_num_instances()};
        for (int32 i{0}; i < n; ++i) {
            healths[i] = entity_registry->get_health(entity_indices[i]);
        }
    }

    validate_array_sizes();
}
void ATestCapitalShipFighters::update_visuals() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShipFighters::update_visuals);

    // Clear old instances
    if (local_indices_to_remove.Num()) {
        constexpr bool is_reverse_sorted{true};
        instances->RemoveInstances(local_indices_to_remove, is_reverse_sorted);
    }

    update_ismc_transforms();
    instances->BatchUpdateInstancesTransforms(0, ismc_transforms, is_world_space, true);
}
void ATestCapitalShipFighters::end_tick() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShipFighters::end_tick);
    TRACE_COUNTER_SET(SandboxTestFighterCount, get_num_instances());

    ml::reset(local_indices_to_remove,
              indices_ready_to_fire_buffer,
              new_laser_locations,
              new_laser_rotations);
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
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShipFighters::spawn_instances);

    ml::add_scaled_in_place(locations, velocities, dt);
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
    ismc_transforms.Reset();
    ismc_transforms.AddUninitialized(n);

    for (int32 i{0}; i < n; ++i) {
        ismc_transforms[i] = ml::make_transform(locations, rotations, i);
    }
}

// Spawning
void
    ATestCapitalShipFighters::spawn_instances(FVectors3f::ConstView const new_locations,
                                              FRotatorsf::ConstView const new_rotations,
                                              TConstArrayView<ETestTeam> const new_teams,
                                              TConstArrayView<FGenerationIndex> const new_targets) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShipFighters::spawn_instances);

    if (!actor_config) {
        UE_LOG(
            LogSandboxLearning, Warning, TEXT("ATestCapitalShipFighters: actor_config is nullptr"));
        return;
    }

    auto const n_cur{get_num_instances()};
    auto const n_new{ml::num(new_locations)};

    ml::fatal_if_nums_not_equal({
        SANDBOX_NAMED_NUM(new_locations),
        SANDBOX_NAMED_NUM(new_rotations),
        SANDBOX_NAMED_NUM(new_teams),
        SANDBOX_NAMED_NUM(new_targets),
    });

    ml::append_from(locations, new_locations);
    ml::append_from(rotations, new_rotations);
    teams.Append(new_teams);
    ml::append_n(healths, actor_config->health, n_new);
    laser_cooldowns.remaining_times.AddZeroed(n_new);
    target_indices.Append(new_targets);

    ml::add_uninitialised(velocities, n_new);

    FTestEntityRegistryEntityData entity_data;
    entity_data.add_uninitialised(n_new);

    auto const speed{actor_config->speed};

    for (int32 i{0}; i < n_new; ++i) {
        auto const index{n_cur + i};

        ml::assign(velocities, index, ml::get_vector3f(rotations, index) * speed);

        ml::assign_from(entity_data.locations, i, locations, index);
        ml::assign_from(entity_data.velocities, i, velocities, index);
        entity_data.healths[i] = healths[index];
        entity_data.teams[i] = teams[index];
        entity_data.alive[i] = true;
    }

    auto const new_indices{entity_registry->add_entities(entity_data.get_const_view())};
    entity_indices.Append(new_indices);

    update_ismc_transforms();
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

        auto const direction{ml::get_vector3f(velocities, index_to_fire).GetSafeNormal()};
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
              rotations,
              velocities,
              teams,
              healths,
              laser_cooldowns,
              indices_ready_to_fire_buffer,
              new_laser_locations,
              new_laser_rotations);
}
auto ATestCapitalShipFighters::get_entity_data(int32 const offset, int32 const count) const
    -> FTestEntityRegistryEntityData {
    FTestEntityRegistryEntityData entity_data;

    entity_data.add_uninitialised(count);
    for (int32 i{0}; i < count; ++i) {
        auto const index{offset + i};

        ml::assign_from(entity_data.locations, i, locations, index);
        ml::assign_from(entity_data.velocities, i, velocities, index);
        entity_data.healths[i] = healths[index];
        entity_data.teams[i] = teams[index];
        entity_data.alive[i] = healths[index] > 0u;
    }

    return entity_data;
}

// Checks
void ATestCapitalShipFighters::validate_array_sizes() const {
    ml::fatal_if_nums_not_equal({
        SANDBOX_NAMED_NUM(entity_indices),
        SANDBOX_NAMED_NUM(locations),
        SANDBOX_NAMED_NUM(rotations),
        SANDBOX_NAMED_NUM(velocities),
        SANDBOX_NAMED_NUM(teams),
        SANDBOX_NAMED_NUM(healths),
        SANDBOX_NAMED_NUM(laser_cooldowns),
        SANDBOX_NAMED_NUM(target_indices),
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
