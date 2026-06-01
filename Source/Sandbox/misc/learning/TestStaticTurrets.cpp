#include "TestStaticTurrets.h"

#include "Sandbox/logging/SandboxLogCategories.h"
#include "TestEntityRegistry.h"
#include "TestLasers.h"
#include "TestStaticTurretsConfig.h"
#include "TestStaticTurretsProxy.h"

#include <Components/InstancedStaticMeshComponent.h>
#include <Components/SceneComponent.h>
#include <EngineUtils.h>

ATestStaticTurrets::ATestStaticTurrets()
    : instances{CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("instances"))} {
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("root"));

    RootComponent->SetMobility(EComponentMobility::Static);
    instances->SetMobility(EComponentMobility::Static);

    instances->SetupAttachment(RootComponent);
}

// Actor life cycle
void ATestStaticTurrets::OnConstruction(FTransform const& transform) {
    Super::OnConstruction(transform);

    if (!actor_config) {
        return;
    }

    configure_ismc();
}
void ATestStaticTurrets::BeginPlay() {
    Super::BeginPlay();

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

    configure_ismc();
    register_all_proxies_in_level();
}
void ATestStaticTurrets::Tick(float dt) {
    Super::Tick(dt);

    laser_cooldowns.tick(dt);

    perform_search();
    fire_at_enemies();
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
    return instances->GetNumInstances();
}

// Searching
void ATestStaticTurrets::perform_search() {
    auto const n_turrets{get_num_instances()};
    auto const radius{actor_config->detection_radius};

    FTransform turret_transform;
    for (int32 i{0}; i < n_turrets; ++i) {
        instances->GetInstanceTransform(i, turret_transform, true);
        auto const turret_location{turret_transform.GetLocation()};

        TStaticArray<FGenerationIndex, 128> elems;
        auto const n_entities{
            entity_registry->collect_entities_in_range(turret_location, radius, elems)};

        for (int32 j{0}; j < n_entities; ++j) {
            // Check if enemy

            // Check if LOS

            // Mark for attack
        }
    }
}

// Attacking
void ATestStaticTurrets::fire_at_enemies() {}

// Spawning
void ATestStaticTurrets::spawn_instance(FTransform const& transform, ETestTeam const team) {
    instances->AddInstance(transform, true);
    teams.Add(team);
    healths.Add(actor_config->max_health);
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
    indices = entity_registry->reserve_entities(n);

    FTestEntityRegistryEntityData entity_data;
    entity_data.add_uninitialised(n);

    teams.AddUninitialized(n);
    healths.AddUninitialized(n);

    auto const hp{actor_config->max_health};

    for (int32 i{0}; i < n; ++i) {
        auto const transform{proxies[i]->GetActorTransform()};
        instances->AddInstance(transform);
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
}
