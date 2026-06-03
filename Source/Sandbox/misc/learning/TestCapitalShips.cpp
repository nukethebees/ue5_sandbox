#include "TestCapitalShips.h"

#include "Sandbox/logging/SandboxLogCategories.h"
#include "TestCapitalShipFighters.h"
#include "TestCapitalShipProxy.h"
#include "TestCapitalShipsConfig.h"
#include "TestEntityRegistry.h"

#include <SandboxCore/actor_components.h>
#include <SandboxCore/array_math.h>
#include <SandboxCore/array_utils.h>
#include <SandboxCore/collision_settings.h>

#include <Components/BoxComponent.h>
#include <Components/InstancedStaticMeshComponent.h>
#include <Components/SceneComponent.h>
#include <EngineUtils.h>
#include <ProfilingDebugging/CountersTrace.h>

TRACE_DECLARE_INT_COUNTER(SandboxTestCapitalShipCount, TEXT("Sandbox/TestLaserCount"));

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

    TRACE_COUNTER_SET(SandboxTestCapitalShipCount, 0);

    SetActorTickEnabled(true);

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

    if (!entity_registry) {
        UE_LOG(LogSandboxLearning, Warning, TEXT("ATestCapitalShips: entity_registry is nullptr."));
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
    update_entity_registry();

    if (debugging_shapes_enabled) {
        draw_debugging_shapes();
    }

    TRACE_COUNTER_SET(SandboxTestCapitalShipCount, get_num_instances());
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
    TMap<ATestCapitalShipProxy const*, FGenerationIndex> proxy_to_index{};

    for (auto it{TActorIterator<ATestCapitalShipProxy>(world)}; it; ++it) {
        if (!IsValid(*it)) {
            continue;
        }

        if (it->batch_actor != this) {
            continue;
        }

        auto const index{proxies.Add(*it)};
    }

    auto const n_to_add{proxies.Num()};
    entity_indices = entity_registry->reserve_entities(n_to_add);
    for (int32 i{0}; i < n_to_add; ++i) {
        proxy_to_index.Add(proxies[i], entity_indices[i]);
    }

    TArray<FGenerationIndex> new_targets;
    new_targets.AddUninitialized(n_to_add);

    TArray<FTransform> new_transforms;
    new_transforms.AddUninitialized(n_to_add);

    TArray<ETestTeam> new_teams;
    new_teams.AddUninitialized(n_to_add);

    for (int32 i{0}; i < n_to_add; ++i) {
        FGenerationIndex target_index{};
        auto* proxy{proxies[i]};

        if (auto const found{proxy_to_index.Find(proxy->target_ship)}) {
            target_index = *found;
        } else {
            UE_LOG(LogSandboxLearning,
                   Error,
                   TEXT("ATestCapitalShips::register_all_proxies_in_level: Lookup failed"));
        }

        new_targets[i] = target_index;
        new_transforms[i] = proxy->GetActorTransform();
        new_teams[i] = proxy->team;
    }

    spawn_ships(entity_indices, new_transforms, new_teams, new_targets);
    update_entity_registry();

    for (ATestCapitalShipProxy* proxy : proxies) {
        proxy->Destroy();
    }
}
void ATestCapitalShips::spawn_ships(TConstArrayView<FGenerationIndex> const new_indices,
                                    TConstArrayView<FTransform> const new_transforms,
                                    TConstArrayView<ETestTeam> const new_teams,
                                    TConstArrayView<FGenerationIndex> const new_target_indices) {
    auto const n_to_add(new_indices.Num());

    check(n_to_add == new_transforms.Num());
    check(n_to_add == new_teams.Num());
    check(n_to_add == new_target_indices.Num());

    instances->AddInstances(TArray<FTransform>{new_transforms}, false, true, false);

    transforms.Append(new_transforms);
    target_entity_indices.Append(new_target_indices);
    spawn_timers.AddZeroed(n_to_add);
    teams.Append(new_teams);
    healths.AddUninitialized(n_to_add);

    auto const hp{ship_config->max_health};

    auto const collision_settings{ship_config->collision_settings};

    for (int32 i{0}; i < n_to_add; ++i) {
        auto const collision_name{
            MakeUniqueObjectName(this, UBoxComponent::StaticClass(), TEXT("Box"))};
        auto* collision{NewObject<UBoxComponent>(this, collision_name)};

        collision->SetupAttachment(RootComponent);
        collision->RegisterComponent();
        AddInstanceComponent(collision);
        collision_boxes.Add(collision);

        collision->SetWorldTransform(new_transforms[i]);
        collision->SetBoxExtent(ship_config->collision_box_extent);
        ml::apply_collision_settings(*collision, collision_settings);

        healths[i] = hp;
    }

    check(array_sizes_consistent());
}

// Fighter spawning
void ATestCapitalShips::handle_fighter_spawning() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShips::handle_fighter_spawning);

    auto const n_ships{get_num_instances()};
    ships_ready_to_spawn_fighters.SetNumUninitialized(n_ships, EAllowShrinking::No);
    auto const cooldown{ship_config->spawn_delay};

    auto const n_to_spawn{ml::collect_indices_less_equal(
        TConstArrayView<float>{spawn_timers.remaining_times}, 0.f, ships_ready_to_spawn_fighters)};

    auto const relative_transforms{ship_config->fighter_spawn_slots_relative_transforms};

    TArray<FTransform> new_transforms;
    TArray<ETestTeam> new_teams;

    for (int32 i{0}; i < n_to_spawn; ++i) {
        auto const ship_index{ships_ready_to_spawn_fighters[i]};

        // spawn fighters
        auto const base_transform{transforms[ship_index]};

        for (auto const& rt : relative_transforms) {
            auto const transform{rt * base_transform};
            new_transforms.Add(transform);
            new_teams.Add(teams[ship_index]);
        }

        spawn_timers.remaining_times[ship_index] = cooldown;
    }

    fighters_actor->spawn_instances(new_transforms, new_teams);
}

// Visuals
void ATestCapitalShips::configure_ismc() {
    instances->SetStaticMesh(ship_config->mesh);
    instances->SetMobility(EComponentMobility::Movable);

    // Collision
    ml::apply_collision_settings(*instances, ship_config->collision_settings);
}

// Misc
void ATestCapitalShips::clear_runtime_state() {
    instances->ClearInstances();
    ml::destroy_components_array(collision_boxes);

    entity_indices.Reset();

    transforms.Reset();

    spawn_timers.Reset();
    ships_ready_to_spawn_fighters.Reset();

    teams.Reset();

    target_entity_indices.Reset();
}
void ATestCapitalShips::update_entity_registry() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShips::update_entity_registry);

    auto const n{get_num_instances()};

    FTestEntityRegistryEntityData update_data;
    update_data.add_uninitialised(n);

    for (int32 i{0}; i < n; ++i) {
        update_data.locations[i] = transforms[i].GetLocation();
        update_data.velocities[i] = FVector::ZeroVector;
        update_data.healths[i] = healths[i];
        update_data.teams[i] = teams[i];
        update_data.alive[i] = true;
    }
    entity_registry->update_entities({entity_indices, update_data.get_const_view()});
}

// Debugging
bool ATestCapitalShips::array_sizes_consistent() const {
    auto const n{instances->GetNumInstances()};

    return ml::all_num_equal_to(n,
                                collision_boxes,
                                entity_indices,
                                transforms,
                                spawn_timers,
                                teams,
                                healths,
                                target_entity_indices);
}
void ATestCapitalShips::draw_debugging_shapes() const {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShips::draw_debugging_shapes);

    auto const n{get_num_instances()};
    auto drawer{ship_config->debug_drawer};
    drawer.world = GetWorld();

    for (int32 i{0}; i < n; ++i) {
        auto const ship_loc{transforms[i].GetLocation()};
        auto const target_index{target_entity_indices[i]};

        if (!entity_registry->is_valid_index(target_index)) {
            continue;
        }

        auto const target_loc{entity_registry->get_location(target_index)};
        drawer.draw_arrow(ship_loc, target_loc);
    }
}
