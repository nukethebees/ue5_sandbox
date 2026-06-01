#include "TestStaticTurrets.h"

#include "Sandbox/logging/SandboxLogCategories.h"
#include "TestEntityRegistry.h"
#include "TestLasers.h"
#include "TestLasersConfig.h"
#include "TestStaticTurretsConfig.h"
#include "TestStaticTurretsProxy.h"

#include <SandboxCore/array_utils.h>
#include <SandboxCore/projectile_intercept.h>

#include <Components/InstancedStaticMeshComponent.h>
#include <Components/SceneComponent.h>
#include <EngineUtils.h>
#include <ProfilingDebugging/CountersTrace.h>

TRACE_DECLARE_INT_COUNTER(SandboxTestStaticTurretCount, TEXT("Sandbox/TestStaticTurretCount"));

ATestStaticTurrets::ATestStaticTurrets()
    : instances{CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("instances"))} {
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("root"));

    RootComponent->SetMobility(EComponentMobility::Static);
    instances->SetMobility(EComponentMobility::Static);

    instances->SetupAttachment(RootComponent);

    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;
}

// Actor life cycle
void ATestStaticTurrets::OnConstruction(FTransform const& transform) {
    Super::OnConstruction(transform);

    if (!actor_config) {
        return;
    }

    configure_ismc();
}
void ATestStaticTurrets::PostInitializeComponents() {
    Super::PostInitializeComponents();

    clear_runtime_state();
}
void ATestStaticTurrets::BeginPlay() {
    Super::BeginPlay();

    SetActorTickEnabled(true);

    if (actor_config == nullptr) {
        UE_LOG(LogSandboxLearning, Warning, TEXT("ATestStaticTurrets: actor_config is nullptr."));
        SetActorTickEnabled(false);
        return;
    }
    if (entity_registry == nullptr) {
        UE_LOG(
            LogSandboxLearning, Warning, TEXT("ATestStaticTurrets: entity_registry is nullptr."));
        SetActorTickEnabled(false);
        return;
    }
    if (laser_actor == nullptr) {
        UE_LOG(LogSandboxLearning, Warning, TEXT("ATestStaticTurrets: laser_actor is nullptr."));
        SetActorTickEnabled(false);
        return;
    }

    configure_ismc();
    register_all_proxies_in_level();
}
void ATestStaticTurrets::Tick(float dt) {
    Super::Tick(dt);

    laser_cooldowns.tick(dt);
    log_config.tick(dt);

    perform_search();
    fire_at_enemies();

    log_config.on_tick_end();
    TRACE_COUNTER_SET(SandboxTestStaticTurretCount, get_num_instances());
}

// Visuals
void ATestStaticTurrets::configure_ismc() {
    RootComponent->SetMobility(EComponentMobility::Static);

    instances->SetCanEverAffectNavigation(false);
    instances->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

    instances->SetStaticMesh(actor_config->mesh);
}

// Getters
auto ATestStaticTurrets::get_num_instances() const noexcept -> int32 {
    return teams.Num();
}

// Searching
void ATestStaticTurrets::perform_search() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestStaticTurrets::perform_search);

    auto const n_turrets{get_num_instances()};
    auto const radius{actor_config->detection_radius};

    FTransform turret_transform;
    for (int32 i{0}; i < n_turrets; ++i) {
        instances->GetInstanceTransform(i, turret_transform, true);
        auto const turret_location{turret_transform.GetLocation()};
        auto const this_team{teams[i]};

        TStaticArray<FGenerationIndex, 128> elems;
        auto const n_entities{entity_registry->collect_non_team_entities_in_range(
            turret_location, this_team, radius, elems)};

        for (int32 j{0}; j < n_entities; ++j) {
            auto const target_index{elems[j]};

            if (this_team == entity_registry->get_team(target_index)) {
                continue;
            }

            target_indices[i] = target_index;
            break;
        }
    }
}

// Attacking
void ATestStaticTurrets::fire_at_enemies() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestStaticTurrets::fire_at_enemies);

    auto const n{get_num_instances()};
    auto const cooldown{actor_config->attack_cooldown};
    auto const laser_speed{laser_actor->get_config()->speed};

    TArray<FTransform> laser_transforms;

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
        instances->GetInstanceTransform(i, laser_transform, true);
        auto const laser_location{laser_transform.GetLocation()};

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
    instances->AddInstance(transform, true);
    teams.Add(team);
    healths.Add(actor_config->max_health);
    target_indices.AddDefaulted();

    check(array_sizes_consistent());
}
void ATestStaticTurrets::register_all_proxies_in_level() {
    auto* world{GetWorld()};

    if (!world) {
        UE_LOG(LogSandboxLearning,
               Warning,
               TEXT("ATestStaticTurrets::register_all_proxies_in_level: world is nullptr."));
        return;
    }

    TArray<Proxy*> proxies{};
    for (auto it{TActorIterator<Proxy>(world)}; it; ++it) {
        if (!IsValid(*it)) {
            continue;
        }

        if (it->get_batch_actor() != this) {
            continue;
        }

        auto const index{proxies.Add(*it)};
    }

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

    indices = entity_registry->reserve_entities(n);

    FTestEntityRegistryEntityData entity_data;
    entity_data.add_uninitialised(n);

    teams.AddUninitialized(n);
    healths.AddUninitialized(n);
    target_indices.AddDefaulted(n);
    laser_cooldowns.remaining_times.AddZeroed(n);

    auto const hp{actor_config->max_health};

    for (int32 i{0}; i < n; ++i) {
        auto const transform{proxies[i]->GetActorTransform()};
        instances->AddInstance(transform, true);
        healths[i] = hp;
        teams[i] = proxies[i]->get_team();

        entity_data.locations[i] = transform.GetTranslation();
        entity_data.velocities[i] = FVector::ZeroVector;
        entity_data.healths[i] = healths[i];
        entity_data.teams[i] = teams[i];
        entity_data.alive[i] = true;
    }

    ATestEntityRegistry::ConstView const update_view{indices, entity_data.get_const_view()};
    entity_registry->update_entities(update_view);

    for (auto* proxy : proxies) {
        proxy->Destroy();
    }

    check(array_sizes_consistent());
}

// Debugging
bool ATestStaticTurrets::array_sizes_consistent() const {
    auto const n{instances->GetNumInstances()};

    return ml::all_num_equal_to(n, indices, teams, laser_cooldowns, healths, target_indices);
}

// Misc
void ATestStaticTurrets::clear_runtime_state() {
    instances->ClearInstances();

    teams.Reset();
    laser_cooldowns.remaining_times.Reset();
    indices_ready_to_fire.Reset();
    target_indices.Reset();
    healths.Reset();
}
