#include "SpaceGame/simulation/TestBatchOrchestrator.h"

#include <SpaceGame/defences/spinners/TestTubeSpinnerProxy.h>
#include <SpaceGame/defences/turrets/TestStaticTurretsProxy.h>
#include <SpaceGame/entities/TestEntity.h>
#include <SpaceGame/ships/capital/TestCapitalShipProxy.h>
#include <SpaceGame/ships/player/TestSpaceShip.h>
#include <SpaceGame/support/mesh.h>

#include <SandboxCore/array_utils.h>
#include <SandboxCore/soa_rotator_utils.h>
#include <SandboxCore/soa_vector_utils.h>
#include <SandboxCoreEngine/actor_utils.h>
#include <SandboxCoreEngine/uobject_utils.h>

#include <Engine/StaticMesh.h>
#include <Engine/StaticMeshSocket.h>
#include <EngineUtils.h>
#include <Kismet/GameplayStatics.h>

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

void ATestBatchOrchestrator::initialise_simulation() {
    auto& world{*GetWorld()};
    auto const& config{*level_config};
    FLevelSimulationInitData data;
    data.clock_settings = simulation_tick_loop;
    data.lasers = make_simulation_config(config.laser_projectiles);
    data.capital_ships = make_simulation_config(config.capital_ships);
    data.fighters = make_simulation_config(config.fighters);
    data.turrets = make_simulation_config(config.turrets);
    data.spinners = make_simulation_config(config.tube_spinners);
    if (IsValid(player_ship)) {
        data.player.Emplace(player_ship->make_spawn_data());
    }
    data.capital_radius = ml::get_mesh_sphere_bounds(*config.capital_ships.mesh);
    data.fighter_radius = ml::get_mesh_sphere_bounds(*config.fighters.mesh);
    data.turret_radius = ml::get_mesh_sphere_bounds(*config.turrets.mesh);
    data.spinner_radius = ml::get_mesh_sphere_bounds(*config.tube_spinners.mesh);
    auto const* socket{config.fighters.mesh->FindSocket(TEXT("Gun"))};
    data.fighter_fire_point_distance =
        IsValid(socket) ? static_cast<float>(socket->RelativeLocation.Size()) : 0.f;
    data.grid_dimensions = config.collision_grid.calculate_grid_dimensions();
    data.cell_size = config.collision_grid.cell_size;
    ml::ioj::FLevelCollisionHost::EntityMeshes meshes{};
    meshes[ETestEntityType::PlayerShip] =
        IsValid(player_ship) ? player_ship->get_collision_mesh() : nullptr;
    meshes[ETestEntityType::CapitalShip] = config.capital_ships.mesh;
    meshes[ETestEntityType::CapitalShipFighter] = config.fighters.mesh;
    meshes[ETestEntityType::Turret] = config.turrets.mesh;
    meshes[ETestEntityType::TubeSpinner] = config.tube_spinners.mesh;
    data.entity_bounds = ml::ioj::FLevelCollisionHost::extract_entity_bounds(meshes);

    auto const capital_proxies{ml::get_actors<ATestCapitalShipProxy>(world)};
    auto const turret_proxies{ml::get_actors<ATestStaticTurretsProxy>(world)};
    auto const spinner_proxies{ml::get_actors<ATestTubeSpinnerProxy>(world)};
    {
        auto const n_to_add{capital_proxies.Num()};
        auto const default_spawn_cooldown{level_config->capital_ships.spawn_delay};

        ml::test_capital_ships::SpawnData spawn_data;
        ml::add_uninitialised(n_to_add, spawn_data);
        for (int32 i{0}; i < n_to_add; ++i) {
            auto const& proxy_transform{capital_proxies[i]->GetActorTransform()};
            ml::assign(spawn_data.locations, i, proxy_transform.GetLocation());
            ml::assign(spawn_data.rotations, i, proxy_transform.Rotator());
            spawn_data.teams[i] = capital_proxies[i]->get_team();
            spawn_data.healths[i] =
                capital_proxies[i]->get_health().Get(level_config->capital_ships.max_health);
            spawn_data.initial_spawn_delays[i] =
                capital_proxies[i]->get_initial_spawn_delay().Get(0.f);
            spawn_data.spawn_cooldowns[i] =
                capital_proxies[i]->get_spawn_cooldown().Get(default_spawn_cooldown);
        }

        data.capital_spawns = MoveTemp(spawn_data);
    }
    {
        auto const n_to_add{turret_proxies.Num()};

        ml::test_static_turrets::SpawnData spawn_data;
        spawn_data.add_uninitialised(n_to_add);
        TArray<FTransform> initial_transforms;
        initial_transforms.SetNumUninitialized(n_to_add, EAllowShrinking::No);
        for (int32 i{0}; i < n_to_add; ++i) {
            auto const transform{turret_proxies[i]->GetActorTransform()};
            initial_transforms[i] = transform;
            ml::assign(spawn_data.locations, i, transform.GetLocation());
            spawn_data.teams[i] = turret_proxies[i]->get_team();
            spawn_data.healths[i] =
                turret_proxies[i]->get_health().Get(level_config->turrets.max_health);
            spawn_data.laser_damages[i] =
                turret_proxies[i]->get_laser_damage().Get(level_config->turrets.laser.damage);
        }
        data.turret_spawns = MoveTemp(spawn_data);
        data.turret_transforms = MoveTemp(initial_transforms);
    }
    {
        auto const n_to_add{spinner_proxies.Num()};

        FVectors3f new_locations;
        TArray<float> new_yaws;
        TArray<int32> new_fire_point_indices;

        ml::add_uninitialised(n_to_add, new_locations, new_yaws, new_fire_point_indices);

        for (int32 i{0}; i < n_to_add; ++i) {
            auto* proxy{spinner_proxies[i]};
            auto const& transform{proxy->GetActorTransform()};

            ml::assign(new_locations, i, transform.GetLocation());
            new_yaws[i] = transform.Rotator().Yaw;
            new_fire_point_indices[i] = proxy->get_initial_active_fire_point();
        }

        data.spinner_locations = MoveTemp(new_locations);
        data.spinner_yaws = MoveTemp(new_yaws);
        data.spinner_fire_points = MoveTemp(new_fire_point_indices);
    }
    auto const presentation{make_presentation_resources()};
    level_simulation_.Emplace(MoveTemp(data), presentation_enabled ? &presentation : nullptr);
    auto const capital_count{capital_proxies.Num()};
    for (int32 i{}; i < capital_count; ++i) {
        capital_proxies[i]->set_entity_handle(get_capital_ships()->get_handle(i));
    }
    auto const turret_count{turret_proxies.Num()};
    for (int32 i{}; i < turret_count; ++i) {
        turret_proxies[i]->set_entity_handle(get_turrets()->entities.handles[i]);
    }
    auto const spinner_count{spinner_proxies.Num()};
    for (int32 i{}; i < spinner_count; ++i) {
        spinner_proxies[i]->set_entity_handle(get_spinners()->entities.handles[i]);
    }
    validate_proxy_handles();
}
void
    ATestBatchOrchestrator::bind_capital_ship_proxy_targets(FProxyEntityMap const& proxy_entities) {
    auto* const world{GetWorld()};
    check(world);

    for (TActorIterator<ATestCapitalShipProxy> it{world}; it; ++it) {
        auto const& proxy{**it};
        auto const* const identifiers{proxy_entities.Find(&proxy)};
        check(identifiers);
        check(get_entity_registry().is_valid_handle(identifiers->handle));

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

        get_capital_ships()->bind_proxy_target(identifiers->handle, target_handle);
    }
}

void ATestBatchOrchestrator::bind_and_destroy_proxies() {
    auto& world{*GetWorld()};
    FProxyEntityMap proxy_entities;
    add_proxy_handles<ATestCapitalShipProxy>(world, get_entity_registry(), proxy_entities);
    add_proxy_handles<ATestStaticTurretsProxy>(world, get_entity_registry(), proxy_entities);
    add_proxy_handles<ATestTubeSpinnerProxy>(world, get_entity_registry(), proxy_entities);

    if (mission_definition.level_id.IsNone()) {
        mission_definition.level_id = FName{UGameplayStatics::GetCurrentLevelName(&world)};
    }
    if (mission_definition.level_display_name.IsEmpty()) {
        mission_definition.level_display_name = mission_definition.level_id.ToString();
    }
    mission_definition.apply(get_mission_manager(), proxy_entities, get_entity_registry());
    bind_capital_ship_proxy_targets(proxy_entities);
    on_proxy_entities_bound.Broadcast(proxy_entities);

    destroy_proxy_actors<ATestCapitalShipProxy>(world);
    destroy_proxy_actors<ATestStaticTurretsProxy>(world);
    destroy_proxy_actors<ATestTubeSpinnerProxy>(world);
}
