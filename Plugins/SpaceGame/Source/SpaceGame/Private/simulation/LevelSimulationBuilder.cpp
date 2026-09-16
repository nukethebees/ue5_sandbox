#include "SpaceGame/simulation/LevelSimulationBuilder.h"
#include <ioj/sim/entity_registry.h>
#include <ioj/sim/levels/fighter_spawn_slot_validation.h>
#include <ioj/sim/levels/level_initialisation_data.h>
#include <ioj/sim/rotator_math.h>
#include <SpaceGameSimulation/entities/NativeEntityTypes.h>
#include <SpaceGameSimulation/missions/NativeMissionTypes.h>
#include <SpaceGameSimulation/simulation/NativeRotatorTypes.h>
#include <SpaceGameSimulation/simulation/NativeTransformTypes.h>
#include <SpaceGameSimulation/simulation/NativeVectorTypes.h>

#include <SandboxGameShared/core/SandboxDeveloperSettings.h>
#include <SpaceGame/defences/spinners/TestTubeSpinnerProxy.h>
#include <SpaceGame/defences/turrets/TestStaticTurretsProxy.h>
#include <SpaceGame/entities/TestEntity.h>
#include <SpaceGame/levels/CompileLevelEvents.h>
#include <SpaceGame/levels/LevelEntityResolution.h>
#include <SpaceGame/ships/capital/TestCapitalShipProxy.h>
#include <SpaceGame/simulation/SimulationConfigConversion.h>

#include <SpaceGame/simulation/LevelCollisionHost.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>
#include <SpaceGameSimulation/entities/TestEntityType.h>
#include <SpaceGameSimulation/simulation/EntityWorldBounds.h>

#include <Engine/StaticMesh.h>
#include <Engine/StaticMeshSocket.h>
#include <Engine/World.h>
#include <EngineUtils.h>
#include <Kismet/GameplayStatics.h>

#include <SandboxCore/soa_rotator_utils.h>
#include <SandboxCore/soa_vector_utils.h>

#include <vector>

namespace ml::level_simulation_builder {
using FProxyEntityIndexMap = TMap<AActor const*, int32>;

auto resolve_entity_index(FProxyEntityIndexMap const& entities, AActor const& actor) -> int32 {
    auto const* const index{entities.Find(&actor)};
    check(index);
    return *index;
}

auto compile_proxy_mission(::FLevelMissionDefinition const& definition,
                           FProxyEntityIndexMap const& entities)
    -> ::ioj::sim::LevelMissionInitialisationData {
    auto const mode{[&] {
        switch (definition.mission_mode) {
            case ETestMissionMode::None:
                return ::ioj::sim::levels::LevelMissionMode::Unspecified;
            case ETestMissionMode::SurviveTime:
                return ::ioj::sim::levels::LevelMissionMode::SurviveTime;
            case ETestMissionMode::KillEnemies:
                return ::ioj::sim::levels::LevelMissionMode::KillEnemies;
            case ETestMissionMode::KillEnemiesWithinTime:
                return ::ioj::sim::levels::LevelMissionMode::KillEnemiesWithinTime;
        }
        checkNoEntry();
        return ::ioj::sim::levels::LevelMissionMode::Unspecified;
    }()};
    ::ioj::sim::LevelMissionInitialisationData result{
        .mode = mode,
        .time_limit_seconds = definition.target_time,
        .kill_count = definition.kill_target,
        .level_id = TCHAR_TO_UTF8(*definition.level_id.ToString()),
        .level_title = TCHAR_TO_UTF8(*definition.level_display_name),
    };
    auto append_indices{[&entities](std::vector<int32>& output, auto const& actors) {
        for (auto const actor : actors) {
            if (IsValid(actor)) {
                output.push_back(resolve_entity_index(entities, *actor));
            }
        }
    }};
    append_indices(result.hero_entity_indices, definition.startup_data.hero_entities);
    append_indices(result.must_survive_entity_indices,
                   definition.startup_data.entities_must_survive);
    append_indices(result.required_kill_entity_indices,
                   definition.startup_data.entities_required_to_kill);
    result.save_results = definition.save_mission_results;
    return result;
}

template <typename TProxy>
auto collect_proxy_actors(UWorld& world) -> TArray<TProxy*> {
    TArray<TProxy*> result;
    for (TActorIterator<TProxy> it{&world}; it; ++it) {
        result.Add(*it);
    }
    return result;
}

template <typename TProxy>
void add_proxy_bindings(TArray<TProxy*> const& proxies,
                        ::ioj::sim::EntityRegistry const& entity_registry,
                        FProxyEntityMap& bindings) {
    for (auto* const proxy : proxies) {
        check(IsValid(proxy));
        auto const handle{proxy->get_entity_handle()};
        check(entity_registry.is_valid_handle(handle));
        auto const unique_id{entity_registry.find_unique_id(handle)};
        check(entity_registry.is_valid_unique_id(unique_id));
        bindings.Add(proxy, {.handle = handle, .unique_id = unique_id});
    }
}

template <typename TProxy>
void destroy_proxy_actors(TArray<TProxy*> const& proxies) {
    for (auto* const proxy : proxies) {
        check(IsValid(proxy));
        check(proxy->Destroy());
    }
}

void append_fighter_spawn_slot_errors(
    std::vector<::ioj::sim::levels::FighterSpawnSlotValidationError> const& validation_errors,
    ::ioj::sim::CapitalShipSimConfig const& capital_config,
    FLevelStartErrors& errors) {
    for (auto const& error : validation_errors) {
        switch (error.kind) {
            case ::ioj::sim::levels::FighterSpawnSlotValidationErrorKind::IntersectsCapital: {
                auto const location{::ioj::sim::to_float(
                    capital_config.fighter_spawn_slots_relative_transforms[error.first_slot]
                        .location)};
                errors.add(FString::Printf(
                    TEXT("fighter spawn slot %d at %s intersects the capital collision bounds plus "
                         "fighter clearance"),
                    error.first_slot,
                    *ml::to_unreal(location).ToString()));
                break;
            }
            case ::ioj::sim::levels::FighterSpawnSlotValidationErrorKind::SlotsOverlap:
                errors.add(FString::Printf(
                    TEXT("fighter spawn slots %d and %d have overlapping collision bounds plus "
                         "%.1f cm clearance"),
                    error.first_slot,
                    error.second_slot,
                    error.clearance));
                break;
            case ::ioj::sim::levels::FighterSpawnSlotValidationErrorKind::WorldIntersectsCapital:
                errors.add(FString::Printf(
                    TEXT("Capital at %s rotation %s: fighter spawn slot %d intersects world "
                         "collision bounds plus fighter clearance"),
                    *ml::to_unreal(error.capital_position).ToString(),
                    *ml::to_unreal(error.capital_rotation).ToString(),
                    error.first_slot));
                break;
        }
    }
}

void validate_fighter_spawn_slots(::ioj::sim::CapitalShipSimConfig const& capital_config,
                                  ::ioj::sim::FighterSimConfig const& fighter_config,
                                  ::ioj::sim::collision::EntityAABBs const& entity_bounds,
                                  FLevelStartErrors& errors) {
    append_fighter_spawn_slot_errors(::ioj::sim::levels::validate_fighter_spawn_slots(
                                         capital_config, fighter_config, entity_bounds),
                                     capital_config,
                                     errors);
}
}

namespace ml {
auto FProxyLevelSimBuild::bind_proxy_entities(::ioj::sim::LevelSim const& simulation) const
    -> FProxyEntityMap {
    auto const capital_count{capital_proxies.Num()};
    for (int32 i{}; i < capital_count; ++i) {
        capital_proxies[i]->set_entity_handle(simulation.get_capital_ships().get_handle(i));
    }

    auto const turret_entities{simulation.get_turrets().get_read_view().entities};
    auto const turret_count{turret_proxies.Num()};
    for (int32 i{}; i < turret_count; ++i) {
        turret_proxies[i]->set_entity_handle(turret_entities.handles[i]);
    }

    auto const spinner_entities{simulation.get_spinners().get_read_view().entities};
    auto const spinner_count{spinner_proxies.Num()};
    for (int32 i{}; i < spinner_count; ++i) {
        spinner_proxies[i]->set_entity_handle(spinner_entities.handles[i]);
    }

    FProxyEntityMap bindings;
    auto const& entity_registry{simulation.get_entity_registry()};
    level_simulation_builder::add_proxy_bindings(capital_proxies, entity_registry, bindings);
    level_simulation_builder::add_proxy_bindings(turret_proxies, entity_registry, bindings);
    level_simulation_builder::add_proxy_bindings(spinner_proxies, entity_registry, bindings);
    return bindings;
}

void FProxyLevelSimBuild::destroy_proxy_actors() const {
    level_simulation_builder::destroy_proxy_actors(capital_proxies);
    level_simulation_builder::destroy_proxy_actors(turret_proxies);
    level_simulation_builder::destroy_proxy_actors(spinner_proxies);
}

void validate_world_fighter_spawn_slots(::ioj::sim::LevelSimInitData const& data,
                                        FLevelStartErrors& errors) {
    level_simulation_builder::append_fighter_spawn_slot_errors(
        ::ioj::sim::levels::validate_world_fighter_spawn_slots(data), data.capital_ships, errors);
}

auto make_level_simulation_init_data(USpaceGameLevelConfig const& config,
                                     FFixedTickLoop const& clock_settings,
                                     TOptional<::ioj::sim::player::PlayerSpawnData> player,
                                     UStaticMesh const* const player_collision_mesh)
    -> FLevelSimBuildResult {
    FLevelStartErrors errors;
    auto const require_mesh{[&errors](UStaticMesh const* const mesh, TCHAR const* const name) {
        if (!IsValid(mesh)) {
            errors.add(FString::Printf(TEXT("%s is unavailable"), name));
        }
    }};
    require_mesh(config.capital_ships.mesh, TEXT("capital_ships.mesh"));
    require_mesh(config.fighters.mesh, TEXT("fighters.mesh"));
    require_mesh(config.turrets.mesh, TEXT("turrets.mesh"));
    require_mesh(config.tube_spinners.mesh, TEXT("tube_spinners.mesh"));

    if (player.IsSet()) {
        require_mesh(player_collision_mesh, TEXT("player ship mesh"));
    }
    if (errors.has_errors()) {
        return FLevelSimBuildResult{std::unexpect, MoveTemp(errors)};
    }

    FLevelSimBuildResult result{std::in_place};
    auto& data{result.value()};
    data.clock_settings = ml::to_native(clock_settings);
    data.lasers = make_simulation_config(config.laser_projectiles);
    data.capital_ships = make_simulation_config(config.capital_ships);
    data.capital_ships.fighter_spawn_slots_relative_transforms.resize(
        data.capital_ships.fighter_spawn_slots);
    data.fighters = make_simulation_config(config.fighters);
    data.fighters.max_live_fighters =
        GetDefault<USandboxDeveloperSettings>()->get_effective_max_live_fighters();
    data.turrets = make_simulation_config(config.turrets);
    data.spinners = make_simulation_config(config.tube_spinners);
    if (player.IsSet()) {
        data.player.emplace(MoveTemp(player.GetValue()));
    }
    auto const* socket{config.fighters.mesh->FindSocket(TEXT("Gun"))};
    data.fighter_fire_point_distance =
        IsValid(socket) ? static_cast<float>(socket->RelativeLocation.Size()) : 0.f;
    auto const dimensions{config.collision_grid.calculate_grid_dimensions()};
    data.grid_dimensions = {dimensions.X, dimensions.Y, dimensions.Z};
    data.cell_size = ml::to_native(config.collision_grid.cell_size);

    ioj::FLevelCollisionHost::EntityMeshes meshes{};
    meshes[ETestEntityType::PlayerShip] = player_collision_mesh;
    meshes[ETestEntityType::CapitalShip] = config.capital_ships.mesh;
    meshes[ETestEntityType::Fighter] = config.fighters.mesh;
    meshes[ETestEntityType::Turret] = config.turrets.mesh;
    meshes[ETestEntityType::TubeSpinner] = config.tube_spinners.mesh;
    auto bounds{ioj::FLevelCollisionHost::extract_entity_bounds(meshes)};
    if (!bounds) {
        return FLevelSimBuildResult{std::unexpect, MoveTemp(bounds.error())};
    }
    data.entity_bounds = MoveTemp(bounds.value());
    level_simulation_builder::validate_fighter_spawn_slots(
        data.capital_ships, data.fighters, data.entity_bounds, errors);
    if (errors.has_errors()) {
        return FLevelSimBuildResult{std::unexpect, MoveTemp(errors)};
    }
    return result;
}

auto make_level_simulation_init_data(USpaceGameLevelConfig const& config,
                                     FFixedTickLoop const& clock_settings,
                                     FLevelDefinition const& definition,
                                     TOptional<::ioj::sim::player::PlayerSpawnData> player,
                                     WorldAABBs static_bounds,
                                     UStaticMesh const* const player_collision_mesh)
    -> FLevelSimBuildResult {
    auto const validation{validate_level(definition)};
    if (!validation) {
        FLevelStartErrors errors;
        for (auto const& error : validation.errors) {
            errors.add(error.message);
        }
        return FLevelSimBuildResult{std::unexpect, MoveTemp(errors)};
    }

    auto result{make_level_simulation_init_data(
        config, clock_settings, MoveTemp(player), player_collision_mesh)};
    if (!result) {
        return result;
    }
    auto& data{result.value()};
    data.static_bounds = MoveTemp(static_bounds);
    data.participating_teams.reserve(definition.teams.Num());
    for (auto const team_id : definition.teams) {
        auto const team{resolve_level_team(team_id)};
        check(team.IsSet());
        data.participating_teams.add(ml::to_native(team.GetValue()));
    }

    ::ioj::sim::SimClock clock;
    clock.initialise(ml::to_native(clock_settings));
    auto compiled{compile_level_events(definition, clock, data.capital_ships, data.turrets)};
    if (!compiled) {
        return FLevelSimBuildResult{std::unexpect, MoveTemp(compiled.error())};
    }
    data.level_events = MoveTemp(compiled.value());
    FLevelStartErrors spawn_errors;
    validate_world_fighter_spawn_slots(data, spawn_errors);
    if (spawn_errors.has_errors()) {
        return FLevelSimBuildResult{std::unexpect, MoveTemp(spawn_errors)};
    }

    return result;
}

auto make_proxy_level_simulation_init_data(USpaceGameLevelConfig const& config,
                                           FFixedTickLoop const& clock_settings,
                                           UWorld& world,
                                           ::FLevelMissionDefinition& mission_definition,
                                           TOptional<::ioj::sim::player::PlayerSpawnData> player,
                                           AActor const* const player_actor,
                                           UStaticMesh const* const player_collision_mesh)
    -> FProxyLevelSimBuildResult {
    auto base_result{make_level_simulation_init_data(
        config, clock_settings, MoveTemp(player), player_collision_mesh)};
    if (!base_result) {
        return FProxyLevelSimBuildResult{std::unexpect, MoveTemp(base_result.error())};
    }

    FProxyLevelSimBuildResult result{std::in_place};
    auto& build{result.value()};
    build.data = MoveTemp(base_result.value());
    build.capital_proxies =
        level_simulation_builder::collect_proxy_actors<ATestCapitalShipProxy>(world);
    build.turret_proxies =
        level_simulation_builder::collect_proxy_actors<ATestStaticTurretsProxy>(world);
    build.spinner_proxies =
        level_simulation_builder::collect_proxy_actors<ATestTubeSpinnerProxy>(world);

    auto& initialisation{build.data.level_events.initialisation};
    auto& initial_spawns{build.data.level_events.initial_spawns};
    level_simulation_builder::FProxyEntityIndexMap entity_indices;
    auto allocate_entity_index{[&](AActor const& actor) {
        auto const index{initialisation.entity_count++};
        check(!entity_indices.Contains(&actor));
        entity_indices.Add(&actor, index);
        return index;
    }};
    if (build.data.player.has_value()) {
        check(IsValid(player_actor));
        initialisation.player_entity_index = allocate_entity_index(*player_actor);
    }

    {
        auto const count{build.capital_proxies.Num()};
        auto const default_spawn_cooldown{config.capital_ships.spawn_delay};
        auto& storage{initial_spawns.capital_spawns};
        storage.add_defaulted(count);
        auto const events{storage.get_view().columns()};
        for (int32 i{}; i < count; ++i) {
            auto const* const proxy{build.capital_proxies[i]};
            auto const& transform{proxy->GetActorTransform()};
            events.entity_indices[i] = allocate_entity_index(*proxy);
            events.target_entity_indices[i] = INDEX_NONE;
            events.locations.set(i, ml::to_native(FVector3f{transform.GetLocation()}));
            events.rotations.set(i, ml::to_native(FRotator3f{transform.Rotator()}));
            events.teams[i] = ml::to_native(proxy->get_team());
            events.healths[i] = proxy->get_health().Get(config.capital_ships.max_health);
            events.initial_fighter_spawn_delays[i] = proxy->get_initial_spawn_delay().Get(0.f);
            events.fighter_spawn_cooldowns[i] =
                proxy->get_spawn_cooldown().Get(default_spawn_cooldown);
        }
    }
    {
        auto const count{build.turret_proxies.Num()};
        auto& storage{initial_spawns.turret_spawns};
        storage.add_defaulted(count);
        auto const events{storage.get_view().columns()};
        build.initial_turret_transforms.Reserve(count);
        for (int32 i{}; i < count; ++i) {
            auto const* const proxy{build.turret_proxies[i]};
            auto const transform{proxy->GetActorTransform()};
            build.initial_turret_transforms.Add(transform);
            events.entity_indices[i] = allocate_entity_index(*proxy);
            events.locations.set(i, ml::to_native(FVector3f{transform.GetLocation()}));
            events.rotations.set(i, ml::to_native(FRotator3f{transform.Rotator()}));
            events.teams[i] = ml::to_native(proxy->get_team());
            events.healths[i] = proxy->get_health().Get(config.turrets.max_health);
            events.laser_damages[i] = proxy->get_laser_damage().Get(config.turrets.laser.damage);
        }
    }
    {
        auto const count{build.spinner_proxies.Num()};
        auto& storage{initial_spawns.spinner_spawns};
        storage.add_defaulted(count);
        auto const events{storage.get_view().columns()};
        for (int32 i{}; i < count; ++i) {
            auto const* const proxy{build.spinner_proxies[i]};
            auto const& transform{proxy->GetActorTransform()};
            events.entity_indices[i] = allocate_entity_index(*proxy);
            events.locations.set(i, ml::to_native(FVector3f{transform.GetLocation()}));
            events.yaws[i] = transform.Rotator().Yaw;
            events.initial_fire_point_indices[i] = proxy->get_initial_active_fire_point();
        }
    }

    auto const capital_events{initial_spawns.capital_spawns.get_view().columns()};
    auto const capital_count{capital_events.num()};
    for (int32 i{}; i < capital_count; ++i) {
        auto const* const target{build.capital_proxies[i]->get_target_ship().Get()};
        if (IsValid(target)) {
            capital_events.target_entity_indices[i] =
                level_simulation_builder::resolve_entity_index(entity_indices, *target);
        }
    }

    if (mission_definition.level_id.IsNone()) {
        mission_definition.level_id = FName{UGameplayStatics::GetCurrentLevelName(&world)};
    }
    if (mission_definition.level_display_name.IsEmpty()) {
        mission_definition.level_display_name = mission_definition.level_id.ToString();
    }
    initialisation.mission =
        level_simulation_builder::compile_proxy_mission(mission_definition, entity_indices);

    FLevelStartErrors errors;
    validate_world_fighter_spawn_slots(build.data, errors);
    if (errors.has_errors()) {
        return FProxyLevelSimBuildResult{std::unexpect, MoveTemp(errors)};
    }
    return result;
}
}
