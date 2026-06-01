#include "TestCapitalShipFighters.h"

#include "Sandbox/logging/SandboxLogCategories.h"
#include "TestCapitalShipFightersConfig.h"
#include "TestEntityRegistry.h"
#include "TestLasers.h"

#include <SandboxCore/array_math.h>
#include <SandboxCore/array_utils.h>

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "ProfilingDebugging/CountersTrace.h"

TRACE_DECLARE_INT_COUNTER(SandboxTestFighterCount, TEXT("Sandbox/TestFighterCount"));

ATestCapitalShipFighters::ATestCapitalShipFighters()
    : instances{CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("instances"))} {
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("root"));

    instances->SetupAttachment(RootComponent);

    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;
}

// Actor life cycle
void ATestCapitalShipFighters::OnConstruction(FTransform const& transform) {
    Super::OnConstruction(transform);

    if (!actor_config) {
        return;
    }

    instances->SetStaticMesh(actor_config->mesh);
}
void ATestCapitalShipFighters::PostInitializeComponents() {
    Super::PostInitializeComponents();

    clear_runtime_state();
}
void ATestCapitalShipFighters::BeginPlay() {
    Super::BeginPlay();

    SetActorTickEnabled(true);

    if (!instances->GetStaticMesh()) {
        UE_LOG(LogSandboxLearning, Warning, TEXT("ATestCapitalShipFighters: mesh is nullptr"));
        SetActorTickEnabled(false);
        return;
    }

    if (!actor_config) {
        UE_LOG(
            LogSandboxLearning, Warning, TEXT("ATestCapitalShipFighters: actor_config is nullptr"));
        SetActorTickEnabled(false);
        return;
    }

    if (!laser_actor) {
        UE_LOG(
            LogSandboxLearning, Warning, TEXT("ATestCapitalShipFighters: laser_actor is nullptr"));
        SetActorTickEnabled(false);
        return;
    }

    if (!entity_registry) {
        UE_LOG(LogSandboxLearning,
               Warning,
               TEXT("ATestCapitalShipFighters: entity_registry is nullptr."));
        SetActorTickEnabled(false);
        return;
    }
}
void ATestCapitalShipFighters::Tick(float dt) {
    Super::Tick(dt);

    laser_cooldowns.tick(dt);

    move_ships(dt);
    handle_firing();
    update_entity_registry();

    TRACE_COUNTER_SET(SandboxTestFighterCount, get_num_instances());
}

// Getters
auto ATestCapitalShipFighters::get_num_instances() const noexcept -> int32 {
    return world_transforms.Num();
}
bool ATestCapitalShipFighters::array_sizes_consistent() const {
    auto const n{instances->GetNumInstances()};

    return ml::all_num_equal_to(
        n, indices, world_transforms, velocities, teams, healths, laser_cooldowns);
}

// Movement
void ATestCapitalShipFighters::move_ships(float const dt) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShipFighters::spawn_instances);

    auto const n{get_num_instances()};
    auto const speed{actor_config->speed};

    for (int32 i{0}; i < n; ++i) {
        auto const delta_move{velocities[i] * dt};
        world_transforms[i].AddToTranslation(delta_move);
    }

    instances->BatchUpdateInstancesTransforms(0, world_transforms, true, true);
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

    instances->AddInstances(TArray<FTransform>{new_transforms}, false, true);
    world_transforms.Append(new_transforms);
    teams.Append(new_teams);

    healths.AddUninitialized(n_new);
    laser_cooldowns.remaining_times.AddZeroed(n_new);

    velocities.AddUninitialized(n_new);

    FTestEntityRegistryEntityData entity_data;
    entity_data.add_uninitialised(n_new);
    for (int32 i{0}; i < n_new; ++i) {
        auto const index{n_cur + i};

        velocities[index] = actor_config->speed * world_transforms[index].GetRotation().Vector();

        entity_data.locations[i] = world_transforms[i].GetLocation();
        entity_data.velocities[i] = velocities[index];
        entity_data.healths[i] = healths[index];
        entity_data.teams[i] = teams[index];
        entity_data.alive[i] = true;
    }

    auto const new_indices{entity_registry->add_entities(entity_data.get_const_view())};
    indices.Append(new_indices);

    check(array_sizes_consistent());
}

// Combat
void ATestCapitalShipFighters::handle_firing() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShipFighters::handle_firing);

    auto const n_ships{get_num_instances()};
    indices_ready_to_fire.SetNumUninitialized(n_ships, EAllowShrinking::No);
    auto const cooldown{actor_config->fire_cooldown};

    auto const n_to_fire{ml::collect_indices_less_equal(
        TConstArrayView<float>{laser_cooldowns.remaining_times}, 0.f, indices_ready_to_fire)};

    TArray<FTransform> laser_transforms;

    for (int32 i{0}; i < n_to_fire; ++i) {
        auto const ship_index{indices_ready_to_fire[i]};
        laser_transforms.Add(world_transforms[i]);
        laser_cooldowns.remaining_times[ship_index] = cooldown;
    }

    laser_actor->spawn_lasers(laser_transforms);
}

// Misc
void ATestCapitalShipFighters::clear_runtime_state() {
    instances->ClearInstances();
}
void ATestCapitalShipFighters::update_entity_registry() {
    auto const data{get_entity_data(0, get_num_instances())};
    ATestEntityRegistry::ConstView view{indices, data.get_const_view()};
    entity_registry->update_entities(view);
}
auto ATestCapitalShipFighters::get_entity_data(int32 const offset, int32 const count) const
    -> FTestEntityRegistryEntityData {
    FTestEntityRegistryEntityData entity_data;

    entity_data.add_uninitialised(count);
    for (int32 i{0}; i < count; ++i) {
        auto const index{offset + i};

        entity_data.locations[i] = world_transforms[i].GetLocation();
        entity_data.velocities[i] = velocities[index];
        entity_data.healths[i] = healths[index];
        entity_data.teams[i] = teams[index];
        entity_data.alive[i] = true;
    }

    return entity_data;
}
