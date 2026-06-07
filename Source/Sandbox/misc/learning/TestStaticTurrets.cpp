#include "TestStaticTurrets.h"

#include "Sandbox/logging/SandboxLogCategories.h"
#include "Sandbox/misc/learning/TestEntityRegistry.h"
#include "Sandbox/misc/learning/TestEntityRegistryData.h"
#include "Sandbox/misc/learning/TestLasers.h"
#include "Sandbox/misc/learning/TestLasersConfig.h"
#include "Sandbox/misc/learning/TestStaticTurretsConfig.h"
#include "Sandbox/misc/learning/TestStaticTurretsProxy.h"
#include "Sandbox/utilities/actor_utils.h"

#include <SandboxCore/actor_utils.h>
#include <SandboxCore/array_utils.h>
#include <SandboxCore/projectile_intercept.h>
#include <SandboxCore/uobject_utils.h>

#include <Async/ParallelFor.h>
#include <Components/InstancedStaticMeshComponent.h>
#include <Components/SceneComponent.h>
#include <Engine/StaticMesh.h>
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
        SANDBOX_NAMED_UOBJECT_PTR(laser_actor),
        SANDBOX_NAMED_UOBJECT_PTR(entity_registry),
    });

    configure_ismc();
    register_all_proxies_in_level();
}
void ATestStaticTurrets::tick(float const dt) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestStaticTurrets::tick);

    laser_cooldowns.tick(dt);
    log_config.tick(dt);

    perform_search();
    fire_at_enemies();

    log_config.on_tick_end();
}
void ATestStaticTurrets::update_entity_registry() {}
void ATestStaticTurrets::resolve_damage_targets() {
    auto& reg{*entity_registry};

    auto const view{reg.get_damage_queue_view()};
    auto const n{view.num()};

    for (int32 i{0}; i < n; ++i) {
        if (view.damaged_actors[i] != this) {
            continue;
        }

        view.targets[i] = entity_indices[view.damaged_hit_items[i]];
    }
}
void ATestStaticTurrets::sync_from_registry() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestStaticTurrets::sync_from_registry);

    local_indices_to_remove.Reset();

    auto const dead_entities{entity_registry->get_dead_entities_this_frame()};
    for (auto const& dead_entity : dead_entities) {
        auto const key{entity_indices.IndexOfByKey(dead_entity)};
        if (key == INDEX_NONE) {
            continue;
        }
        local_indices_to_remove.Add(key);
    }

    // Remove from largest index to smallest
    local_indices_to_remove.Sort(TGreater<int32>{});

    ml::remove_at_swap_many_sorted_desc(local_indices_to_remove,
                                        entity_indices,
                                        locations,
                                        teams,
                                        laser_cooldowns.remaining_times,
                                        healths,
                                        target_indices);

    {
        auto const n{get_num_instances()};
        for (int32 i{0}; i < n; ++i) {
            healths[i] = entity_registry->get_health(entity_indices[i]);
        }
    }

    check(array_sizes_consistent());
}
void ATestStaticTurrets::update_visuals() {
    // Clear old instances
    if (local_indices_to_remove.Num()) {
        constexpr bool is_reverse_sorted{true};
        instances->RemoveInstances(local_indices_to_remove, is_reverse_sorted);
    }
}
void ATestStaticTurrets::end_frame() {
    TRACE_COUNTER_SET(SandboxTestStaticTurretCount, get_num_instances());
}

// Visuals
void ATestStaticTurrets::configure_ismc() {
    RootComponent->SetMobility(EComponentMobility::Static);

    instances->SetCanEverAffectNavigation(false);
    instances->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

    instances->SetStaticMesh(actor_config->mesh);
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
            auto const turret_location{locations[i]};
            auto const this_team{teams[i]};

            TStaticArray<FGenerationIndex, 128> elems;
            auto const n_entities{entity_registry->collect_non_team_entities_in_range(
                turret_location, this_team, radius, elems)};

            target_indices[i] = FGenerationIndex{};

            for (int32 j{0}; j < n_entities; ++j) {
                auto const target_index{elems[j]};

                if (this_team == entity_registry->get_team(target_index)) {
                    continue;
                }

                target_indices[i] = target_index;
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
    auto const laser_speed{laser_actor->get_config()->speed};

    TArray<FTransform> laser_transforms;
    auto const fire_point_offset{actor_config->fire_point_offset.GetLocation()};

    for (int32 i{0}; i < n; ++i) {
        auto const target_index{target_indices[i]};

        if (!target_index.is_valid()) {
            continue;
        }
        if (!(laser_cooldowns[i] <= 0.f)) {
            continue;
        }

        auto const target_location{entity_registry->get_location(target_index)};
        auto const target_velocity{entity_registry->get_velocity(target_index)};

        FTransform laser_transform{FTransform::Identity};
        instances->GetInstanceTransform(i, laser_transform, is_world_space);
        auto const laser_location{laser_transform.GetLocation() + fire_point_offset};
        laser_transform.SetLocation(laser_location);

        auto const intercept_time{ml::solve_intercept_time(
            laser_location, target_location, target_velocity, laser_speed)};

        FVector const intercept_pos{target_location + target_velocity * intercept_time};
        FVector const fire_dir{(intercept_pos - laser_location).GetSafeNormal()};
        laser_transform.SetRotation(fire_dir.ToOrientationQuat());

        laser_transforms.Add(laser_transform);
        laser_cooldowns[i] = cooldown;
    }

    laser_actor->spawn_lasers(laser_transforms);
}

// Spawning
void ATestStaticTurrets::spawn_instance(FTransform const& transform, ETestTeam const team) {
    instances->AddInstance(transform, is_world_space);
    teams.Add(team);
    healths.Add(actor_config->max_health);
    target_indices.AddDefaulted();

    check(array_sizes_consistent());
}
void ATestStaticTurrets::register_all_proxies_in_level() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestStaticTurrets::register_all_proxies_in_level);

    auto* world{GetWorld()};

    if (!world) {
        UE_LOG(LogSandboxLearning,
               Warning,
               TEXT("ATestStaticTurrets::register_all_proxies_in_level: world is nullptr."));
        return;
    }

    auto const proxies{ml::get_actors<Proxy>(*world)};

    auto const n{proxies.Num()};
    if (n == 0) {
        UE_LOG(LogSandboxLearning,
               Warning,
               TEXT("ATestStaticTurrets::register_all_proxies_in_level: 0 proxies detected."));
        return;
    } else {
        UE_LOG(LogSandboxLearning,
               Display,
               TEXT("ATestStaticTurrets::register_all_proxies_in_level: Spawning %d proxies."),
               n);
    }

    entity_indices = entity_registry->reserve_entities(n);

    FTestEntityRegistryEntityData entity_data;
    entity_data.add_uninitialised(n);

    locations.AddUninitialized(n);
    teams.AddUninitialized(n);
    healths.AddUninitialized(n);
    target_indices.AddDefaulted(n);
    laser_cooldowns.remaining_times.AddZeroed(n);

    auto const hp{actor_config->max_health};

    TArray<FTransform> ismc_transforms;
    ismc_transforms.AddUninitialized(n);
    for (int32 i{0}; i < n; ++i) {
        auto const transform{proxies[i]->GetActorTransform()};
        ismc_transforms[i] = transform;
        locations[i] = transform.GetLocation();
        healths[i] = hp;
        teams[i] = proxies[i]->get_team();

        entity_data.velocities[i] = FVector::ZeroVector;
        entity_data.alive[i] = true;
    }

    instances->AddInstances(ismc_transforms, false);

    entity_data.locations = locations;
    entity_data.healths = healths;
    entity_data.teams = teams;

    ATestEntityRegistry::ConstView const update_view{entity_indices, entity_data.get_const_view()};
    entity_registry->update_entities(update_view);

    for (auto* proxy : proxies) {
        proxy->Destroy();
    }

    check(array_sizes_consistent());
}

// Debugging
bool ATestStaticTurrets::array_sizes_consistent() const {
    return ml::all_num_equal(
        *instances, entity_indices, locations, teams, laser_cooldowns, healths, target_indices);
}

// Misc
void ATestStaticTurrets::clear_runtime_state() {
    instances->ClearInstances();
    ml::reset_arrays(entity_indices,
                     local_indices_to_remove,
                     locations,
                     teams,
                     laser_cooldowns,
                     indices_ready_to_fire,
                     target_indices,
                     healths);
}
