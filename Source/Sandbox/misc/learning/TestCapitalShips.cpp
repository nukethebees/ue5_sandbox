#include "TestCapitalShips.h"

#include "Sandbox/logging/SandboxLogCategories.h"
#include "Sandbox/misc/learning/TestCapitalShipFighters.h"
#include "Sandbox/misc/learning/TestCapitalShipProxy.h"
#include "Sandbox/misc/learning/TestCapitalShipsConfig.h"
#include "Sandbox/misc/learning/TestEntityRegistry.h"
#include "Sandbox/utilities/actor_utils.h"

#include <SandboxCore/actor_utils.h>
#include <SandboxCore/array_math.h>
#include <SandboxCore/array_utils.h>
#include <SandboxCore/collision_settings.h>
#include <SandboxCore/invoke.h>
#include <SandboxCore/uobject_utils.h>

#include <Components/InstancedStaticMeshComponent.h>
#include <Components/SceneComponent.h>
#include <EngineUtils.h>
#include <ProfilingDebugging/CountersTrace.h>
#include <Templates/Greater.h>

TRACE_DECLARE_INT_COUNTER(SandboxTestCapitalShipCount, TEXT("Sandbox/TestLaserCount"));

ATestCapitalShips::ATestCapitalShips()
    : instances{CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("instances"))} {
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("root"));

    instances->SetupAttachment(RootComponent);

    PrimaryActorTick.bCanEverTick = false;
    PrimaryActorTick.bStartWithTickEnabled = false;

    ml::set_actor_component_mobility(*this, EComponentMobility::Static);
}

// Actor Lifecycle
void ATestCapitalShips::begin_play() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShips::begin_play);
    TRACE_COUNTER_SET(SandboxTestCapitalShipCount, 0);

    auto* world{GetWorld()};
    ml::fatal_if_uobject_ptrs_invalid({
        SANDBOX_NAMED_UOBJECT_PTR(ship_config),
        SANDBOX_NAMED_UOBJECT_PTR(fighters_actor),
        SANDBOX_NAMED_UOBJECT_PTR(entity_registry),
        SANDBOX_NAMED_UOBJECT_PTR(world),
    });

    debug_drawer = ship_config->debug_drawer;
    debug_drawer.world = world;

    configure_ismc();
    register_all_proxies_in_level();
}
void ATestCapitalShips::tick(float const dt) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShips::tick);

    spawn_timers.tick(dt);

    handle_fighter_spawning();
    update_entity_registry();
}
void ATestCapitalShips::sync_from_registry() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShips::sync_from_registry);
    TRACE_COUNTER_SET(SandboxTestCapitalShipCount, get_num_instances());

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
                                        transforms,
                                        spawn_timers.remaining_times,
                                        teams,
                                        healths,
                                        target_entity_indices);

    {
        auto const n{get_num_instances()};
        for (int32 i{0}; i < n; ++i) {
            healths[i] = entity_registry->get_health(entity_indices[i]);
        }
    }

    check(array_sizes_consistent());
}
void ATestCapitalShips::update_visuals() {
    // Clear old instances
    if (local_indices_to_remove.Num()) {
        constexpr bool is_reverse_sorted{true};
        instances->RemoveInstances(local_indices_to_remove, is_reverse_sorted);
    }

    if (debugging_shapes_enabled) {
        draw_debugging_shapes();
    }
}

// Accessors
auto ATestCapitalShips::get_num_instances() const -> int32 {
    return entity_indices.Num();
}
auto ATestCapitalShips::get_location(FGenerationIndex const index) const -> FVector {
    if (!is_valid(index)) {
        return FVector::ZeroVector;
    }

    return transforms[index.index].GetLocation();
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
auto ATestCapitalShips::get_entity_from_hit_slot(int32 const hit_slot) const -> FGenerationIndex {
    return entity_indices.IsValidIndex(hit_slot) ? entity_indices[hit_slot] : FGenerationIndex{};
}

// Ship spawning
void ATestCapitalShips::register_all_proxies_in_level() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShips::register_all_proxies_in_level);

    auto* world{GetWorld()};

    if (!world) {
        return;
    }

    TMap<Proxy const*, FGenerationIndex> proxy_to_index{};
    auto const proxies{ml::get_actors<Proxy>(*world)};

    auto const n_to_add{proxies.Num()};
    entity_indices = entity_registry->reserve_entities(n_to_add);
    for (int32 i{0}; i < n_to_add; ++i) {
        proxy_to_index.Add(proxies[i], entity_indices[i]);
    }

    TArray<FGenerationIndex> new_targets;
    TArray<FTransform> new_transforms;
    TArray<ETestTeam> new_teams;

    ml::invoke_on_all([n_to_add](auto& a) { a.AddUninitialized(n_to_add); },
                      new_targets,
                      new_transforms,
                      new_teams);

    for (int32 i{0}; i < n_to_add; ++i) {
        FGenerationIndex target_index{};
        auto* proxy{proxies[i]};

        if (auto const found{proxy_to_index.Find(proxy->target_ship)}) {
            target_index = *found;
        } else {
            UE_LOG(LogSandboxLearning,
                   Fatal,
                   TEXT("ATestCapitalShips::register_all_proxies_in_level: Lookup failed"));
        }

        new_targets[i] = target_index;
        new_transforms[i] = proxy->GetActorTransform();
        new_teams[i] = proxy->team;
    }

    spawn_ships(entity_indices, new_transforms, new_teams, new_targets);
    update_entity_registry();

    for (auto* proxy : proxies) {
        proxy->Destroy();
    }
}
void ATestCapitalShips::spawn_ships(TConstArrayView<FGenerationIndex> const new_indices,
                                    TConstArrayView<FTransform> const new_transforms,
                                    TConstArrayView<ETestTeam> const new_teams,
                                    TConstArrayView<FGenerationIndex> const new_target_indices) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShips::spawn_ships);

    auto const n_to_add(new_indices.Num());

    check(n_to_add == new_transforms.Num());
    check(n_to_add == new_teams.Num());
    check(n_to_add == new_target_indices.Num());

    instances->AddInstances(TArray<FTransform>{new_transforms}, false, is_world_space, false);

    transforms.Append(new_transforms);
    target_entity_indices.Append(new_target_indices);
    spawn_timers.AddZeroed(n_to_add);
    teams.Append(new_teams);

    ml::append_n(healths, ship_config->max_health, n_to_add);

    check(array_sizes_consistent());
}

// Fighter spawning
void ATestCapitalShips::handle_fighter_spawning() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShips::handle_fighter_spawning);

    auto const n_ships{get_num_instances()};
    ships_ready_to_spawn_fighters_buffer.SetNumUninitialized(n_ships, EAllowShrinking::No);
    auto const cooldown{ship_config->spawn_delay};

    auto const ships_ready_to_spawn_fighters_indices{
        ml::collect_indices_less_equal(TConstArrayView<float>{spawn_timers.remaining_times},
                                       0.f,
                                       ships_ready_to_spawn_fighters_buffer)};

    auto const relative_transforms{ship_config->fighter_spawn_slots_relative_transforms};

    TArray<FTransform> new_transforms;
    TArray<ETestTeam> new_teams;

    for (auto const i : ships_ready_to_spawn_fighters_indices) {
        // spawn fighters
        auto const base_transform{transforms[i]};

        for (auto const& rt : relative_transforms) {
            auto const transform{rt * base_transform};
            new_transforms.Add(transform);
            new_teams.Add(teams[i]);
        }

        spawn_timers.remaining_times[i] = cooldown;
    }

    fighters_actor->spawn_instances(new_transforms, new_teams);
}

// Visuals
void ATestCapitalShips::configure_ismc() {
    instances->SetStaticMesh(ship_config->mesh);
    instances->SetCanEverAffectNavigation(false);

    // Collision
    ml::apply_collision_settings(*instances, ship_config->collision_settings);
}

// Misc
void ATestCapitalShips::clear_runtime_state() {
    instances->ClearInstances();

    ml::reset_arrays(entity_indices,
                     local_indices_to_remove,
                     transforms,
                     spawn_timers,
                     ships_ready_to_spawn_fighters_buffer,
                     teams,
                     healths,
                     target_entity_indices);
}
void ATestCapitalShips::update_entity_registry() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShips::update_entity_registry);

    auto const n{get_num_instances()};

    FTestEntityRegistryEntityData update_data;
    update_data.add_uninitialised(n);

    ml::fill(update_data.velocities, FVector::ZeroVector);

    update_data.healths = healths;
    update_data.teams = teams;

    for (int32 i{0}; i < n; ++i) {
        update_data.locations[i] = transforms[i].GetLocation();
        update_data.alive[i] = healths[i] > 0;
    }

    entity_registry->update_entities({entity_indices, update_data.get_const_view()});
}

// Debugging
bool ATestCapitalShips::array_sizes_consistent() const {
    auto const n{get_num_instances()};

    return ml::all_num_equal(
               entity_indices, transforms, spawn_timers, teams, healths, target_entity_indices) &&
           (instances->GetNumInstances() >= n);
}
void ATestCapitalShips::draw_debugging_shapes() const {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShips::draw_debugging_shapes);

    auto const n{get_num_instances()};
    auto const collision_extent{ship_config->collision_box_extent};

    auto const text_offset{ship_config->debug_status_text_offset};

    auto& drawer{debug_drawer};
    for (int32 i{0}; i < n; ++i) {
        auto const ship_loc{transforms[i].GetLocation()};

        // Draw target
        auto const target_index{target_entity_indices[i]};
        if (entity_registry->is_valid_index(target_index)) {
            auto const target_loc{entity_registry->get_location(target_index)};
            drawer.draw_arrow(ship_loc, target_loc);
        }

        // Draw collision
        drawer.draw_box(ship_loc, collision_extent);

        // Draw HP
        auto const ship_index{entity_indices[i]};

        auto const msg{FString::Printf(
            TEXT("[%d, %d] HP=%d"), ship_index.index, ship_index.generation, healths[i])};
        auto const msg_location{ship_loc + text_offset};
        drawer.draw_string(msg_location, msg);
    }
}
