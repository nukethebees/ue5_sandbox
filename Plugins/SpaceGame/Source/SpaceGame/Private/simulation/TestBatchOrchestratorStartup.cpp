#include "SpaceGame/simulation/TestBatchOrchestrator.h"

#include <SpaceGame/defences/spinners/TestTubeSpinnerProxy.h>
#include <SpaceGame/defences/turrets/TestStaticTurretsProxy.h>
#include <SpaceGame/entities/TestEntity.h>
#include <SpaceGame/ships/capital/TestCapitalShipProxy.h>
#include <SpaceGame/support/mesh.h>

#include <SandboxCore/array_utils.h>
#include <SandboxCore/soa_rotator_utils.h>
#include <SandboxCore/soa_vector_utils.h>
#include <SandboxCoreEngine/actor_utils.h>
#include <SandboxCoreEngine/uobject_utils.h>

#include <Engine/StaticMesh.h>
#include <Engine/StaticMeshSocket.h>
#include <EngineUtils.h>

namespace {
template <typename TProxy>
void add_proxy_handles(UWorld& world,
                       FTestEntityRegistry const& entity_registry,
                       FProxyEntityMap& proxy_entities) {
    for (TActorIterator<TProxy> it{&world}; it; ++it) {
        auto* const proxy{*it};
        check(IsValid(proxy));

        auto const* const entity{Cast<ITestEntity>(proxy)};
        check(entity);

        auto const handle{entity->get_entity_handle()};
        check(entity_registry.is_valid_handle(handle));
        auto const unique_id{entity_registry.find_unique_id(handle)};
        check(entity_registry.is_valid_unique_id(unique_id));
        check(!proxy_entities.Contains(proxy));
        proxy_entities.Add(proxy,
                           FRegistryEntityIdentifiers{
                               .handle = handle,
                               .unique_id = unique_id,
                           });
    }
}

template <typename TProxy>
void destroy_proxy_actors(UWorld& world) {
    for (TActorIterator<TProxy> it{&world}; it;) {
        auto* const proxy{*it};
        ++it;

        check(IsValid(proxy));
        check(proxy->Destroy());
    }
}

} // namespace

void ATestBatchOrchestrator::initialise_batch_geometry() {
    auto const& config{*level_config};
    ml::fatal_if_uobject_ptrs_invalid({
        SANDBOX_NAMED_UOBJECT_PTR(config.capital_ships.mesh.Get()),
        SANDBOX_NAMED_UOBJECT_PTR(config.fighters.mesh.Get()),
        SANDBOX_NAMED_UOBJECT_PTR(config.turrets.mesh.Get()),
        SANDBOX_NAMED_UOBJECT_PTR(config.tube_spinners.mesh.Get()),
    });

    capital_ships_simulation.entity_radius = ml::get_mesh_sphere_bounds(*config.capital_ships.mesh);
    capital_ship_fighters_simulation.collision_radius =
        ml::get_mesh_sphere_bounds(*config.fighters.mesh);
    turrets_simulation.entity_radius = ml::get_mesh_sphere_bounds(*config.turrets.mesh);
    spinners_simulation.entity_radius = ml::get_mesh_sphere_bounds(*config.tube_spinners.mesh);

    static FName const socket_name{TEXT("Gun")};
    auto const* const socket{config.fighters.mesh->FindSocket(socket_name)};
    capital_ship_fighters_simulation.fire_point_distance =
        IsValid(socket) ? static_cast<float>(socket->RelativeLocation.Size()) : 0.f;
}

void ATestBatchOrchestrator::register_capital_ship_proxies() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestBatchOrchestrator::register_capital_ship_proxies);

    auto* const world{GetWorld()};
    check(world);

    auto const proxies{ml::get_actors<ATestCapitalShipProxy>(*world)};
    auto const n_to_add{proxies.Num()};
    auto const default_spawn_cooldown{level_config->capital_ships.spawn_delay};

    ml::test_capital_ships::SpawnData spawn_data;
    ml::add_uninitialised(n_to_add, spawn_data);
    for (int32 i{0}; i < n_to_add; ++i) {
        auto const& proxy_transform{proxies[i]->GetActorTransform()};
        ml::assign(spawn_data.locations, i, proxy_transform.GetLocation());
        ml::assign(spawn_data.rotations, i, proxy_transform.Rotator());
        spawn_data.teams[i] = proxies[i]->get_team();
        spawn_data.healths[i] =
            proxies[i]->get_health().Get(level_config->capital_ships.max_health);
        spawn_data.initial_spawn_delays[i] = proxies[i]->get_initial_spawn_delay().Get(0.f);
        spawn_data.spawn_cooldowns[i] =
            proxies[i]->get_spawn_cooldown().Get(default_spawn_cooldown);
    }

    capital_ships_simulation.register_ships(spawn_data);

    for (int32 i{0}; i < n_to_add; ++i) {
        proxies[i]->set_entity_handle(capital_ships_simulation.get_handle(i));
    }
}

auto ATestBatchOrchestrator::register_turret_proxies() -> TArray<FTransform> {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestBatchOrchestrator::register_turret_proxies);
    auto* const world{GetWorld()};
    check(world);
    auto const proxies{ml::get_actors<ATestStaticTurretsProxy>(*world)};
    auto const n_to_add{proxies.Num()};
    if (n_to_add == 0) {
        return {};
    }

    ml::test_static_turrets::SpawnData spawn_data;
    spawn_data.add_uninitialised(n_to_add);
    TArray<FTransform> initial_transforms;
    initial_transforms.SetNumUninitialized(n_to_add, EAllowShrinking::No);
    for (int32 i{0}; i < n_to_add; ++i) {
        auto const transform{proxies[i]->GetActorTransform()};
        initial_transforms[i] = transform;
        ml::assign(spawn_data.locations, i, transform.GetLocation());
        spawn_data.teams[i] = proxies[i]->get_team();
        spawn_data.healths[i] = proxies[i]->get_health().Get(level_config->turrets.max_health);
        spawn_data.laser_damages[i] =
            proxies[i]->get_laser_damage().Get(level_config->turrets.laser.damage);
    }
    turrets_simulation.register_turrets(spawn_data);

    for (int32 i{0}; i < n_to_add; ++i) {
        proxies[i]->set_entity_handle(turrets_simulation.entities.handles[i]);
    }
    return initial_transforms;
}

void ATestBatchOrchestrator::register_spinner_proxies() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestBatchOrchestrator::register_spinner_proxies);

    auto* world{GetWorld()};

    check(world);

    TArray<ATestTubeSpinnerProxy*> proxies{};
    ml::append_actors(*world, proxies);
    auto const n_to_add{proxies.Num()};

    FVectors3f new_locations;
    TArray<float> new_yaws;
    TArray<int32> new_fire_point_indices;

    ml::add_uninitialised(n_to_add, new_locations, new_yaws, new_fire_point_indices);

    for (int32 i{0}; i < n_to_add; ++i) {
        auto* proxy{proxies[i]};
        auto const& transform{proxy->GetActorTransform()};

        ml::assign(new_locations, i, transform.GetLocation());
        new_yaws[i] = transform.Rotator().Yaw;
        new_fire_point_indices[i] = proxy->get_initial_active_fire_point();
    }

    spinners_simulation.spawn_instances(
        new_locations.get_const_view(), new_yaws, new_fire_point_indices);

    auto const& entities{spinners_simulation.entities};

    auto const first_new_handle{entities.handles.Num() - n_to_add};
    for (int32 i{0}; i < n_to_add; ++i) {
        proxies[i]->set_entity_handle(entities.handles[first_new_handle + i]);
    }
}

void
    ATestBatchOrchestrator::bind_capital_ship_proxy_targets(FProxyEntityMap const& proxy_entities) {
    auto* const world{GetWorld()};
    check(world);

    for (TActorIterator<ATestCapitalShipProxy> it{world}; it; ++it) {
        auto const& proxy{**it};
        auto const* const identifiers{proxy_entities.Find(&proxy)};
        check(identifiers);
        check(entity_registry.is_valid_handle(identifiers->handle));

        auto const* const target{proxy.get_target_ship().Get()};
        if (!target) {
            continue;
        }

        auto const target_handle{[&] {
            if (auto const* const proxy_target{proxy_entities.Find(target)}) {
                return proxy_target->handle;
            }

            auto const* const target_entity{Cast<ITestEntity>(target)};
            check(target_entity);
            return target_entity->get_entity_handle();
        }()};

        capital_ships_simulation.bind_proxy_target(identifiers->handle, target_handle);
    }
}

void ATestBatchOrchestrator::bind_and_destroy_proxies() {
    auto& world{*GetWorld()};
    FProxyEntityMap proxy_entities;
    add_proxy_handles<ATestCapitalShipProxy>(world, entity_registry, proxy_entities);
    add_proxy_handles<ATestStaticTurretsProxy>(world, entity_registry, proxy_entities);
    add_proxy_handles<ATestTubeSpinnerProxy>(world, entity_registry, proxy_entities);

    mission_manager.on_proxy_entities_bound(proxy_entities);
    bind_capital_ship_proxy_targets(proxy_entities);
    on_proxy_entities_bound.Broadcast(proxy_entities);

    destroy_proxy_actors<ATestCapitalShipProxy>(world);
    destroy_proxy_actors<ATestStaticTurretsProxy>(world);
    destroy_proxy_actors<ATestTubeSpinnerProxy>(world);
}
