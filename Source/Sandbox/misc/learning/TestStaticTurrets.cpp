#include "TestStaticTurrets.h"

#include "Sandbox/logging/SandboxLogCategories.h"
#include "TestEntityRegistry.h"
#include "TestLasers.h"
#include "TestStaticTurretsConfig.h"
#include "TestStaticTurretsProxy.h"

#include <Components/InstancedStaticMeshComponent.h>
#include <Components/SceneComponent.h>

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
void ATestStaticTurrets::spawn_instance(FTransform const& transform) {
    instances->AddInstance(transform, true);
}
