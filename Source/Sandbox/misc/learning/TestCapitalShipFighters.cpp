#include "TestCapitalShipFighters.h"

#include "Sandbox/logging/SandboxLogCategories.h"
#include "Sandbox/misc/learning/TestCapitalShipFightersConfig.h"
#include "Sandbox/misc/learning/TestEntityRegistry.h"
#include "Sandbox/misc/learning/TestLasers.h"
#include "Sandbox/utilities/actor_utils.h"

#include <SandboxCore/array_checks.h>
#include <SandboxCore/array_math.h>
#include <SandboxCore/array_utils.h>
#include <SandboxCore/soa_vector_utils.h>
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
void ATestCapitalShipFighters::tick(float const dt) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShipFighters::tick);

    laser_cooldowns.tick(dt);

    move_ships(dt);
    handle_firing();
}
void ATestCapitalShipFighters::update_entity_registry() {
    auto const data{get_entity_data(0, get_num_instances())};
    ATestEntityRegistry::ConstView view{entity_indices, data.get_const_view()};
    entity_registry->update_entities(view);
}
void ATestCapitalShipFighters::resolve_damage_targets() {
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
                                        world_transforms,
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
    // Clear old instances
    if (local_indices_to_remove.Num()) {
        constexpr bool is_reverse_sorted{true};
        instances->RemoveInstances(local_indices_to_remove, is_reverse_sorted);
    }

    instances->BatchUpdateInstancesTransforms(0, world_transforms, is_world_space, true);
}
void ATestCapitalShipFighters::end_frame() {
    TRACE_COUNTER_SET(SandboxTestFighterCount, get_num_instances());
}

// Accessors
auto ATestCapitalShipFighters::get_num_instances() const noexcept -> int32 {
    return world_transforms.Num();
}
bool ATestCapitalShipFighters::array_sizes_consistent() const {
    return ml::all_num_equal(
        *instances, entity_indices, world_transforms, velocities, teams, healths, laser_cooldowns);
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

    auto const n{get_num_instances()};
    for (int32 i{0}; i < n; ++i) {
        world_transforms[i].AddToTranslation(ml::scaled_fvector(velocities, i, dt));
    }
}

// Visuals
void ATestCapitalShipFighters::configure_ismc() {
    instances->SetStaticMesh(actor_config->mesh);

    instances->SetCanEverAffectNavigation(false);

    instances->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
}

// Spawning
void ATestCapitalShipFighters::spawn_instances(TConstArrayView<FTransform> const new_transforms,
                                               TConstArrayView<ETestTeam> const new_teams) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShipFighters::spawn_instances);

    if (!actor_config) {
        UE_LOG(
            LogSandboxLearning, Warning, TEXT("ATestCapitalShipFighters: actor_config is nullptr"));
        return;
    }

    auto const n_cur{get_num_instances()};
    auto const n_new{new_transforms.Num()};
    check(n_new == new_teams.Num());

    instances->AddInstances(TArray<FTransform>{new_transforms}, is_world_space, true);
    world_transforms.Append(new_transforms);
    teams.Append(new_teams);

    ml::append_n(healths, actor_config->health, n_new);
    laser_cooldowns.remaining_times.AddZeroed(n_new);

    ml::add_uninitialised(velocities, n_new);

    FTestEntityRegistryEntityData entity_data;
    entity_data.add_uninitialised(n_new);

    for (int32 i{0}; i < n_new; ++i) {
        auto const index{n_cur + i};

        auto const vec{actor_config->speed * world_transforms[index].GetRotation().Vector()};
        ml::assign(velocities, index, vec);

        entity_data.locations[i] = world_transforms[i].GetLocation();
        ml::assign_from(entity_data.velocities, i, velocities, index);
        entity_data.healths[i] = healths[index];
        entity_data.teams[i] = teams[index];
        entity_data.alive[i] = true;
    }

    auto const new_indices{entity_registry->add_entities(entity_data.get_const_view())};
    entity_indices.Append(new_indices);

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

    for (auto const i : indices_to_fire) {
        auto transform{world_transforms[i]};
        auto const forward{transform.GetUnitAxis(EAxis::X)};
        transform.AddToTranslation(forward * fire_point_offset);
        laser_transforms.Add(transform);

        laser_cooldowns.remaining_times[i] = cooldown;
    }

    laser_actor->spawn_lasers(laser_transforms);
}

// Misc
void ATestCapitalShipFighters::clear_runtime_state() {
    instances->ClearInstances();

    ml::reset(entity_indices,
              local_indices_to_remove,
              world_transforms,
              velocities,
              teams,
              healths,
              laser_cooldowns,
              indices_ready_to_fire_buffer);
}
auto ATestCapitalShipFighters::get_entity_data(int32 const offset, int32 const count) const
    -> FTestEntityRegistryEntityData {
    FTestEntityRegistryEntityData entity_data;

    entity_data.add_uninitialised(count);
    for (int32 i{0}; i < count; ++i) {
        auto const index{offset + i};

        entity_data.locations[i] = world_transforms[i].GetLocation();

        entity_data.velocities.xs[i] = velocities.xs[i];
        entity_data.velocities.ys[i] = velocities.ys[i];
        entity_data.velocities.zs[i] = velocities.zs[i];

        entity_data.healths[i] = healths[index];
        entity_data.teams[i] = teams[index];
        entity_data.alive[i] = true;
    }

    return entity_data;
}

// Checks
void ATestCapitalShipFighters::validate_array_sizes() const {
    ml::fatal_if_nums_not_equal({
        SANDBOX_NAMED_NUM(entity_indices),
        SANDBOX_NAMED_NUM(world_transforms),
        SANDBOX_NAMED_NUM(velocities),
        SANDBOX_NAMED_NUM(teams),
        SANDBOX_NAMED_NUM(healths),
        SANDBOX_NAMED_NUM(laser_cooldowns),
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
