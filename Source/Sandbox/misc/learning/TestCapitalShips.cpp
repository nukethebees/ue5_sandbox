#include "TestCapitalShips.h"

#include "Sandbox/logging/SandboxLogCategories.h"
#include "TestCapitalShipFighters.h"
#include "TestCapitalShipProxy.h"
#include "TestCapitalShipsConfig.h"

#include <SandboxCore/Public/actor_components.h>
#include <SandboxCore/Public/array_math.h>
#include <SandboxCore/Public/array_utils.h>

#include <Components/BoxComponent.h>
#include <Components/InstancedStaticMeshComponent.h>
#include <Components/SceneComponent.h>
#include <EngineUtils.h>

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

    if (!fighters_actor) {
        UE_LOG(LogSandboxLearning, Warning, TEXT("ATestCapitalShips: fighters_actor is nullptr."));
        SetActorTickEnabled(false);
        return;
    }

    configure_ismc();
    register_all_proxies_in_level();
}
void ATestCapitalShips::Tick(float dt) {
    Super::Tick(dt);

    spawn_timers.tick(dt);

    handle_fighter_spawning();

    if (debugging_shapes_enabled) {
        draw_debugging_shapes();
    }
}

// Accessors
auto ATestCapitalShips::get_num_instances() const -> int32 {
    return instances->GetNumInstances();
}
auto ATestCapitalShips::get_location(FGenerationIndex const index) const -> FVector {
    if (!is_valid(index)) {
        return FVector::ZeroVector;
    }

    return collision_boxes[index.index]->GetComponentLocation();
}
auto ATestCapitalShips::is_valid(FGenerationIndex const index) const -> bool {
    if (!index.is_valid()) {
        return false;
    }

    if (!transforms.IsValidIndex(index.index)) {
        return false;
    }

    return true;
}

// Ship spawning
void ATestCapitalShips::register_all_proxies_in_level() {
    auto* world{GetWorld()};

    if (!world) {
        return;
    }

    TArray<ATestCapitalShipProxy*> proxies{};
    TMap<ATestCapitalShipProxy const*, int32> proxy_to_index{};

    for (auto it{TActorIterator<ATestCapitalShipProxy>(world)}; it; ++it) {
        if (!IsValid(*it)) {
            continue;
        }

        if (it->batch_actor != this) {
            continue;
        }

        auto const index{proxies.Add(*it)};
        proxy_to_index.Add(*it, index);
    }

    for (ATestCapitalShipProxy* proxy : proxies) {
        FGenerationIndex target_index{};
        if (proxy->target_ship) {
            target_index.generation = 0;
            target_index.index = *proxy_to_index.Find(proxy->target_ship);
        }

        spawn_ship(proxy->GetActorTransform(), proxy->team, this, target_index);
    }

    for (ATestCapitalShipProxy* proxy : proxies) {
        proxy->Destroy();
    }
}
void ATestCapitalShips::spawn_ship(FTransform const& transform,
                                   ETestTeam const team,
                                   ATestCapitalShips* target_actor,
                                   FGenerationIndex target_index) {
    instances->AddInstance(transform, true);

    // Collision
    auto const collision_name{
        MakeUniqueObjectName(this, UBoxComponent::StaticClass(), TEXT("Box"))};
    auto* collision{NewObject<UBoxComponent>(this, collision_name)};

    collision->SetupAttachment(RootComponent);
    collision->RegisterComponent();
    AddInstanceComponent(collision);
    collision_boxes.Add(collision);

    collision->SetWorldTransform(transform);
    collision->SetBoxExtent(ship_config->collision_box_extent);

    // Data
    transforms.Add(transform);
    target_actors.Add(target_actor);
    target_entity_indexes.Add(target_index);
    teams.Add(team);
    spawn_timers.remaining_times.Add(0.f);

    check(array_sizes_consistent());
}

// Fighter spawning
void ATestCapitalShips::handle_fighter_spawning() {
    auto const n_ships{get_num_instances()};
    ships_ready_to_spawn_fighters.SetNumUninitialized(n_ships, EAllowShrinking::No);
    auto const cooldown{ship_config->spawn_delay};

    auto const n_to_spawn{ml::collect_indices_less_equal(
        TConstArrayView<float>{spawn_timers.remaining_times}, 0.f, ships_ready_to_spawn_fighters)};

    auto const relative_transforms{ship_config->fighter_spawn_slots_relative_transforms};

    for (int32 i{0}; i < n_to_spawn; ++i) {
        auto const ship_index{ships_ready_to_spawn_fighters[i]};

        // spawn fighters
        auto const base_transform{transforms[ship_index]};

        for (auto const& rt : relative_transforms) {
            auto const transform{rt * base_transform};
            fighters_actor->spawn_instance(transform);
        }

        spawn_timers.remaining_times[ship_index] = cooldown;
    }
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

    target_actors.Reset();
    target_entity_indexes.Reset();
}

// Debugging
bool ATestCapitalShips::array_sizes_consistent() const {
    auto const n{instances->GetNumInstances()};

    return ml::all_num_equal_to(
        n, collision_boxes, transforms, spawn_timers, teams, target_actors, target_entity_indexes);
}
void ATestCapitalShips::draw_debugging_shapes() const {
    auto const n{get_num_instances()};
    auto drawer{ship_config->debug_drawer};
    drawer.world = GetWorld();

    for (int32 i{0}; i < n; ++i) {
        auto const ship_loc{transforms[i].GetLocation()};

        auto const target_actor{target_actors[i]};
        auto const target_index{target_entity_indexes[i]};

        if (!target_actor.IsValid()) {
            continue;
        }

        auto const target_loc{target_actor->get_location(target_index)};

        drawer.draw_arrow(ship_loc, target_loc);
    }
}
