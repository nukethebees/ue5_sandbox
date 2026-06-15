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

    auto const view{entity_registry->get_damage_queue_view(owner_id)};
    auto const n{view.num()};

    for (int32 i{0}; i < n; ++i) {
        auto const ismc_index_hit{view.hit_items[i]};
        auto const entity_hit{entity_indices[ismc_index_hit]};

        auto const damage_done{view.damage_amounts[i]};
        healths[ismc_index_hit] -= damage_done;
    }
}
void ATestStaticTurrets::update_entity_registry() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestStaticTurrets::update_entity_registry);

    auto const data{get_entity_data()};
    ATestEntityRegistry::ConstView view{entity_indices, data.get_const_view()};
    entity_registry->update_entities(view);
}
void ATestStaticTurrets::sync_from_registry() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestStaticTurrets::sync_from_registry);

    auto const dead_entities{entity_registry->get_dead_entities_this_frame()};

    ml::collect_valid_indices_by_key(entity_indices, dead_entities, local_indices_to_remove);
    local_indices_to_remove.Sort(TGreater<int32>{});

    ml::remove_at_swap_many_sorted_desc(local_indices_to_remove,
                                        entity_indices,
                                        locations,
                                        teams,
                                        laser_cooldowns.remaining_times,
                                        healths,
                                        target_indices);

    auto const n{get_num_instances()};
    for (int32 i{0}; i < n; ++i) {
        healths[i] = entity_registry->get_health(entity_indices[i]);
    }

    validate_array_sizes();
}
void ATestStaticTurrets::update_visuals() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestStaticTurrets::update_visuals);
    // Clear old instances
    if (local_indices_to_remove.Num()) {
        constexpr bool is_reverse_sorted{true};
        instances->RemoveInstances(local_indices_to_remove, is_reverse_sorted);
    }
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
}

// Entity data
auto ATestStaticTurrets::get_entity_data() const -> FTestEntityRegistryEntityData {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestStaticTurrets::get_entity_data);

    auto const n{get_num_instances()};

    FTestEntityRegistryEntityData entity_data;
    ml::add_uninitialised(entity_data, n);

    entity_data.locations = locations;
    ml::fill(entity_data.velocities, 0.f);
    entity_data.healths = healths;
    entity_data.teams = teams;

    ml::add_uninitialised(entity_data.alive, n);
    for (int32 i{0}; i < n; ++i) {
        entity_data.alive[i] = static_cast<uint8>(healths[i]);
    }

    return entity_data;
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
            auto const turret_location{ml::get_vector3f(locations, i)};
            auto const this_team{teams[i]};

            TStaticArray<FRegistryEntityHandle, 128> elems;
            auto const n_entities{entity_registry->collect_non_team_entities_in_range(
                turret_location, this_team, radius, elems)};

            target_indices[i] = FRegistryEntityHandle{};

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

    FVector3f const fire_point_offset{actor_config->fire_point_offset.GetLocation()};

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

        ml::append(new_laser_locations, loc_x, loc_y, loc_z);
        ml::append(new_laser_rotations, fire_dir);
        laser_cooldowns[i] = cooldown;
    }

    laser_actor->spawn_lasers(new_laser_locations.get_const_view(),
                              new_laser_rotations.get_const_view());
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
    if (n == 0) {
        return;
    }

    auto new_entities{entity_registry->reserve_entities(n)};
    entity_indices = MoveTemp(new_entities.registry_indices);

    FTestEntityRegistryEntityData entity_data;
    entity_data.add_uninitialised(n);

    TArray<FTransform> ismc_transforms;

    ml::add_uninitialised(n, locations, teams, ismc_transforms);
    ml::append_n(healths, actor_config->max_health, n);
    target_indices.AddDefaulted(n);
    laser_cooldowns.remaining_times.AddZeroed(n);

    for (int32 i{0}; i < n; ++i) {
        auto const transform{proxies[i]->GetActorTransform()};
        ismc_transforms[i] = transform;
        ml::assign(locations, i, transform.GetLocation());
        teams[i] = proxies[i]->get_team();
    }

    instances->AddInstances(ismc_transforms, false);

    ml::fill(entity_data.velocities, 0.f);
    entity_data.locations = locations;
    entity_data.healths = healths;
    entity_data.teams = teams;
    ml::fill(entity_data.alive, uint8{1});

    ATestEntityRegistry::ConstView const update_view{entity_indices, entity_data.get_const_view()};
    entity_registry->update_entities(update_view);

    for (auto* proxy : proxies) {
        proxy->Destroy();
    }

    validate_array_sizes();
}

// Misc
void ATestStaticTurrets::clear_runtime_state() {
    instances->ClearInstances();
    ml::reset(entity_indices,
              local_indices_to_remove,
              locations,
              teams,
              laser_cooldowns,
              indices_ready_to_fire,
              target_indices,
              healths);
}
void ATestStaticTurrets::clear_tick_buffers() {
    ml::reset(
        local_indices_to_remove, indices_ready_to_fire, new_laser_locations, new_laser_rotations);
}

// Checks
void ATestStaticTurrets::validate_array_sizes() const {
    ml::fatal_if_nums_not_equal({
        SANDBOX_NAMED_NUM(entity_indices),
        SANDBOX_NAMED_NUM(locations),
        SANDBOX_NAMED_NUM(teams),
        SANDBOX_NAMED_NUM(laser_cooldowns),
        SANDBOX_NAMED_NUM(target_indices),
        SANDBOX_NAMED_NUM(healths),
    });

    auto const n{get_num_instances()};
    auto const n_ismc{instances->GetNumInstances()};
    if (n_ismc < n) {
        UE_LOG(LogSandbox,
               Fatal,
               TEXT("ATestStaticTurrets::validate_array_sizes %d entities, %d ISMC instances"),
               n,
               n_ismc);
    }
}
