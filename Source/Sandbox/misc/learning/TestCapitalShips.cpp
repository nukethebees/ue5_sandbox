#include "TestCapitalShips.h"

#include "Sandbox/logging/SandboxLogCategories.h"
#include "TestCapitalShipsConfig.h"

#include <SandboxCore/Public/actor_components.h>
#include <SandboxCore/Public/array_utils.h>

#include <Components/BoxComponent.h>
#include <Components/InstancedStaticMeshComponent.h>
#include <Components/SceneComponent.h>

ATestCapitalShips::ATestCapitalShips()
    : instances{CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("instances"))} {
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("root"));

    instances->SetupAttachment(RootComponent);

    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;
}

// Actor Lifecycle
void ATestCapitalShips::PostInitializeComponents() {
    Super::PostInitializeComponents();

    clear_runtime_state();
}
void ATestCapitalShips::BeginPlay() {
    Super::BeginPlay();

    if (!ship_config) {
        UE_LOG(LogSandboxLearning, Warning, TEXT("ATestCapitalShips: ship_config is nullptr."));
        SetActorTickEnabled(false);
        return;
    }

    if (!ship_config) {
        UE_LOG(LogSandboxLearning, Warning, TEXT("ATestCapitalShips: fighters_actor is nullptr."));
        SetActorTickEnabled(false);
        return;
    }

    configure_ismc();
}
void ATestCapitalShips::Tick(float dt) {
    Super::Tick(dt);

    if (debugging_shapes_enabled) {
        draw_debugging_shapes();
    }
}

// Spawning
void ATestCapitalShips::spawn_ship(FTransform const& transform) {
    instances->AddInstance(transform, true);

    // Collision
    auto const collision_name{
        MakeUniqueObjectName(this, UBoxComponent::StaticClass(), TEXT("Box"))};
    auto* collision{NewObject<UBoxComponent>(this, collision_name)};

    collision->SetupAttachment(RootComponent);
    collision->RegisterComponent();
    AddInstanceComponent(collision);
    collision_boxes.Add(collision);

    // Data
    transforms.Add(transform);
    targets.Add(nullptr);
    target_entity_indexes.AddDefaulted();

    check(array_sizes_consistent());
}

// Visuals
void ATestCapitalShips::configure_ismc() {
    instances->SetStaticMesh(ship_config->mesh);
    instances->SetMobility(EComponentMobility::Movable);
}

// Misc
void ATestCapitalShips::clear_runtime_state() {
    ml::destroy_components_array(collision_boxes);

    instances->ClearInstances();

    transforms.Reset();

    targets.Reset();
    target_entity_indexes.Reset();
}

// Debugging
bool ATestCapitalShips::array_sizes_consistent() const {
    auto const n{instances->GetNumInstances()};

    return ml::all_num_equal_to(n, collision_boxes, transforms, targets, target_entity_indexes);
}
void ATestCapitalShips::draw_debugging_shapes() const {}
