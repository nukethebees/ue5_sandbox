#include "TestCapitalShips.h"

#include <Sandbox/logging/SandboxLogCategories.h>
#include <Sandbox/misc/learning/test_entity_registry/DamageEvents.h>
#include <Sandbox/misc/learning/test_entity_registry/TestEntityRegistry.h>
#include <Sandbox/misc/learning/TestCapitalShipFighters.h>
#include <Sandbox/misc/learning/TestCapitalShipProxy.h>
#include <Sandbox/misc/learning/TestCapitalShipsConfig.h>
#include <Sandbox/utilities/actor_utils.h>

#include <SandboxCore/actor_utils.h>
#include <SandboxCore/array_checks.h>
#include <SandboxCore/array_math.h>
#include <SandboxCore/array_utils.h>
#include <SandboxCore/collision_settings.h>
#include <SandboxCore/invoke.h>
#include <SandboxCore/soa_rotator_utils.h>
#include <SandboxCore/soa_vector_utils.h>
#include <SandboxCore/transforms.h>
#include <SandboxCore/uobject_utils.h>

#include <Components/InstancedStaticMeshComponent.h>
#include <Components/SceneComponent.h>
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
        SANDBOX_NAMED_UOBJECT_PTR(actor_config),
        SANDBOX_NAMED_UOBJECT_PTR(fighters_actor),
        SANDBOX_NAMED_UOBJECT_PTR(entity_registry),
        SANDBOX_NAMED_UOBJECT_PTR(world),
    });

    debug_drawer = actor_config->debug_drawer;
    debug_drawer.world = world;

    configure_ismc();
    register_all_proxies_in_level();
}
void ATestCapitalShips::begin_tick() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShips::begin_tick);
    clear_tick_buffers();
}
void ATestCapitalShips::commit_spawns() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShips::commit_spawns);
    handle_fighter_spawning();
}
void ATestCapitalShips::tick(float const dt) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShips::tick);

    spawn_timers.tick(dt);
}
void ATestCapitalShips::resolve_hit_events() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShips::resolve_hit_events);

    auto const view{entity_registry->get_damage_queue_view(owner_id)};
    auto const n{view.num()};

    for (int32 i{0}; i < n; ++i) {
        auto const ismc_index_hit{view.hit_items[i]};

        healths[ismc_index_hit] -= view.damage_amounts[i];
        if (healths[ismc_index_hit] <= 0) {
            if (!local_indices_to_remove.Contains(ismc_index_hit)) {
                local_indices_to_remove.Add(ismc_index_hit);
            }
            entity_death_info.add(
                ETestDeathReason::Combat, entity_handles[ismc_index_hit], view.instigators[i]);
        }
    }

    validate_array_sizes();
}
void ATestCapitalShips::update_entity_registry() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShips::update_entity_registry);

    auto const n{get_num_instances()};

    FTestEntityRegistryEntityData update_data;
    update_data.add_uninitialised(n);

    update_data.locations = locations;
    ml::fill(update_data.velocities, 0.f);
    update_data.healths = healths;
    update_data.teams = teams;

    for (int32 i{0}; i < n; ++i) {
        update_data.alive[i] = healths[i] > 0;
    }

    entity_registry->update_entities({entity_handles, update_data.get_const_view()});
    entity_registry->set_death_infos(entity_death_info);
}
void ATestCapitalShips::sync_from_registry() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShips::sync_from_registry);

    auto const dead_entities{entity_registry->get_dead_entities_this_frame()};

    ml::collect_valid_indices_by_key(entity_handles, dead_entities, local_indices_to_remove);
    local_indices_to_remove.Sort(TGreater<int32>{});

    ml::remove_at_swap_many_sorted_desc(local_indices_to_remove,
                                        entity_handles,
                                        locations,
                                        rotations,
                                        spawn_timers.remaining_times,
                                        teams,
                                        healths,
                                        target_entity_indices);

    {
        auto const n{get_num_instances()};
        for (int32 i{0}; i < n; ++i) {
            healths[i] = entity_registry->get_health(entity_handles[i]);
        }
    }

    validate_array_sizes();
}
void ATestCapitalShips::update_visuals() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShips::update_visuals);

    // Clear old instances
    if (local_indices_to_remove.Num()) {
        constexpr bool is_reverse_sorted{true};
        instances->RemoveInstances(local_indices_to_remove, is_reverse_sorted);
    }

    if (debugging_shapes_enabled) { draw_debugging_shapes(); }
}
void ATestCapitalShips::end_tick() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShips::end_tick);
    TRACE_COUNTER_SET(SandboxTestCapitalShipCount, get_num_instances());
}

// Accessors
auto ATestCapitalShips::get_num_instances() const -> int32 {
    return entity_handles.Num();
}
auto ATestCapitalShips::is_valid(FRegistryEntityHandle const index) const -> bool {
    if (!index.is_valid()) { return false; }

    if (!locations.xs.IsValidIndex(index.index)) { return false; }

    return true;
}
auto ATestCapitalShips::get_entity_from_hit_slot(int32 const hit_slot) const
    -> FRegistryEntityHandle {
    return entity_handles.IsValidIndex(hit_slot) ? entity_handles[hit_slot]
                                                 : FRegistryEntityHandle{};
}

void ATestCapitalShips::set_owner_id(TestEntityOwnerId const new_owner_id) {
    owner_id = new_owner_id;
}
auto ATestCapitalShips::get_owner_id() const -> TestEntityOwnerId {
    return owner_id;
}

// Ship spawning
void ATestCapitalShips::register_all_proxies_in_level() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShips::register_all_proxies_in_level);

    auto* world{GetWorld()};

    if (!world) { return; }

    TMap<Proxy const*, FRegistryEntityHandle> proxy_to_index{};
    auto const proxies{ml::get_actors<Proxy>(*world)};

    auto const n_to_add{proxies.Num()};

    auto new_entities{entity_registry->reserve_entities(n_to_add)};

    entity_handles = MoveTemp(new_entities.registry_handles);
    for (int32 i{0}; i < n_to_add; ++i) {
        proxy_to_index.Add(proxies[i], entity_handles[i]);
    }

    TArray<FRegistryEntityHandle> new_targets;
    FVectors3f new_locations;
    FRotatorsf new_rotations;
    TArray<ETestTeam> new_teams;

    ml::add_uninitialised(n_to_add, new_targets, new_locations, new_rotations, new_teams);

    for (int32 i{0}; i < n_to_add; ++i) {
        FRegistryEntityHandle target_index{};
        auto* proxy{proxies[i]};

        if (auto const found{proxy_to_index.Find(proxy->target_ship)}) {
            target_index = *found;
        } else {
            UE_LOG(LogSandboxLearning,
                   Fatal,
                   TEXT("ATestCapitalShips::register_all_proxies_in_level: Lookup failed"));
        }

        new_targets[i] = target_index;
        auto const& proxy_transform{proxy->GetActorTransform()};

        ml::assign(new_locations, i, proxy_transform.GetLocation());
        ml::assign(new_rotations, i, proxy_transform.Rotator());
        new_teams[i] = proxy->team;
    }

    spawn_ships(entity_handles,
                new_locations.get_const_view(),
                new_rotations.get_const_view(),
                new_teams,
                new_targets);
    update_entity_registry();
    ml::destroy_all_actors(proxies);
}
void ATestCapitalShips::spawn_ships(
    TConstArrayView<FRegistryEntityHandle> const new_indices,
    FVectors3f::ConstView const new_locations,
    FRotatorsf::ConstView const new_rotations,
    TConstArrayView<ETestTeam> const new_teams,
    TConstArrayView<FRegistryEntityHandle> const new_target_indices) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShips::spawn_ships);

    auto const n_to_add(new_indices.Num());

    ml::fatal_if_nums_not_equal({
        SANDBOX_NAMED_NUM(new_indices),
        SANDBOX_NAMED_NUM(new_locations),
        SANDBOX_NAMED_NUM(new_rotations),
        SANDBOX_NAMED_NUM(new_teams),
        SANDBOX_NAMED_NUM(new_target_indices),
    });

    ml::append_from(locations, new_locations);
    ml::append_from(rotations, new_rotations);
    target_entity_indices.Append(new_target_indices);
    spawn_timers.AddZeroed(n_to_add);
    teams.Append(new_teams);

    ml::append_n(healths, actor_config->max_health, n_to_add);

    auto const new_transforms{ml::make_transforms(new_locations, new_rotations)};
    instances->AddInstances(new_transforms, false, is_world_space, false);

    validate_array_sizes();
}

// Fighter spawning
void ATestCapitalShips::handle_fighter_spawning() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShips::handle_fighter_spawning);

    auto const n_ships{get_num_instances()};
    ships_ready_to_spawn_fighters_buffer.SetNumUninitialized(n_ships, EAllowShrinking::No);
    auto const cooldown{actor_config->spawn_delay};

    auto const ships_ready_to_spawn_fighters_indices{
        ml::collect_indices_less_equal(TConstArrayView<float>{spawn_timers.remaining_times},
                                       0.f,
                                       ships_ready_to_spawn_fighters_buffer)};

    auto const relative_transforms{actor_config->fighter_spawn_slots_relative_transforms};

    ml::reset(new_fighter_locations, new_fighter_rotations, new_fighter_teams, new_fighter_targets);

    for (auto const i : ships_ready_to_spawn_fighters_indices) {
        auto const target_index{target_entity_indices[i]};
        auto const base_location{ml::get_vector3f(locations, i)};
        auto const base_rotation{ml::get_rotator3f(rotations, i)};

        FTransform const base_transform{
            FRotator{base_rotation},
            FVector{base_location},
            FVector::OneVector,
        };

        for (auto const& rt : relative_transforms) {
            auto const new_transform{rt * base_transform};

            ml::append(new_fighter_locations, new_transform.GetLocation());
            ml::append(new_fighter_rotations, new_transform.Rotator());
            new_fighter_teams.Add(teams[i]);
            new_fighter_targets.Add(target_index);
        }

        spawn_timers.remaining_times[i] = cooldown;
    }

    fighters_actor->spawn_instances(new_fighter_locations.get_const_view(),
                                    new_fighter_rotations.get_const_view(),
                                    TConstArrayView<ETestTeam>(new_fighter_teams),
                                    TConstArrayView<FRegistryEntityHandle>(new_fighter_targets));
}

// Visuals
void ATestCapitalShips::configure_ismc() {
    instances->SetStaticMesh(actor_config->mesh);
    instances->SetCanEverAffectNavigation(false);

    // Collision
    ml::apply_collision_settings(*instances, actor_config->collision_settings);
}

// Misc
void ATestCapitalShips::clear_runtime_state() {
    instances->ClearInstances();

    ml::reset(entity_handles,
              local_indices_to_remove,
              locations,
              rotations,
              spawn_timers,
              ships_ready_to_spawn_fighters_buffer,
              new_fighter_locations,
              new_fighter_rotations,
              teams,
              healths,
              target_entity_indices);
}
void ATestCapitalShips::clear_tick_buffers() {
    ml::reset(ships_ready_to_spawn_fighters_buffer,
              new_fighter_locations,
              new_fighter_rotations,
              new_fighter_targets);
}

// Debugging
void ATestCapitalShips::draw_debugging_shapes() const {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShips::draw_debugging_shapes);

    auto const n{get_num_instances()};
    auto const text_offset{actor_config->debug_status_text_offset};

    auto& drawer{debug_drawer};
    for (int32 i{0}; i < n; ++i) {
        auto const ship_location{ml::get_vector3d(locations, i)};

        // Draw target
        auto const target_index{target_entity_indices[i]};
        if (entity_registry->is_valid_handle(target_index)) {
            FVector3d const target_location{entity_registry->get_location(target_index)};
            drawer.draw_line(ship_location, target_location);
        }

        // Draw HP
        auto const ship_index{entity_handles[i]};

        auto const msg{FString::Printf(
            TEXT("[%d, %d] HP=%d"), ship_index.index, ship_index.generation, healths[i])};
        auto const msg_location{ship_location + text_offset};
        drawer.draw_string(msg_location, msg);
    }
}

// Checks
void ATestCapitalShips::validate_array_sizes() const {
    ml::fatal_if_nums_not_equal({
        SANDBOX_NAMED_NUM(entity_handles),
        SANDBOX_NAMED_NUM(locations),
        SANDBOX_NAMED_NUM(rotations),
        SANDBOX_NAMED_NUM(spawn_timers),
        SANDBOX_NAMED_NUM(teams),
        SANDBOX_NAMED_NUM(healths),
        SANDBOX_NAMED_NUM(target_entity_indices),
    });

    auto const n{get_num_instances()};
    auto const n_ismc{instances->GetNumInstances()};
    if (n_ismc < n) {
        UE_LOG(LogSandbox,
               Fatal,
               TEXT("ATestCapitalShips::validate_array_sizes %d entities, %d ISMC instances"),
               n,
               n_ismc);
    }
}
