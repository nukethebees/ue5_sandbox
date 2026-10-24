#include "SpaceGame/simulation/LevelSimulationBuilder.h"

#include <ioj/column_math.h>
#include <ioj/rotator_math.h>
#include <ioj/sim/collision/entity_world_bounds.h>
#include <ioj/sim/entities/entity_type.h>
#include <ioj/sim/lasers/laser_source.h>
#include <ioj/sim/levels/fighter_spawn_slot_validation.h>
#include <ioj/sim/levels/initial_spawns.h>
#include <ioj/sim/levels/level_compilation.h>
#include <ioj/sim/levels/level_initialisation_data.h>
#include <ioj/sim/missions/mission_compilation.h>
#include <SpaceGame/defences/spinners/TestTubeSpinnerProxy.h>
#include <SpaceGame/defences/turrets/TestStaticTurretsProxy.h>
#include <SpaceGame/entities/TestEntity.h>
#include <SpaceGame/levels/LevelEntityResolution.h>
#include <SpaceGame/levels/native_level_definition_conversion.h>
#include <SpaceGame/ships/capital/TestCapitalShipProxy.h>
#include <SpaceGame/simulation/LevelCollisionHost.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>
#include <SpaceGamePresentation/entities/TestTeamConversion.h>
#include <SpaceGamePresentation/integration/RotatorConversion.h>
#include <SpaceGamePresentation/integration/TransformConversion.h>
#include <SpaceGamePresentation/integration/VectorConversion.h>

#include <SandboxCoreEngine/strings.h>

#include <Engine/StaticMesh.h>
#include <Engine/StaticMeshSocket.h>
#include <Engine/World.h>
#include <EngineUtils.h>
#include <Kismet/GameplayStatics.h>

#include <vector>

namespace ioj::level_simulation_builder {
using FProxyEntityIndexMap = TMap<AActor const*, uint32>;

template <typename T>
auto to_optional(TOptional<T> const& value) -> std::optional<T> {
    return value.IsSet() ? std::optional<T>{value.GetValue()} : std::nullopt;
}

auto resolve_entity_index(FProxyEntityIndexMap const& entities, AActor const& actor) -> uint32 {
    auto const* const index{entities.Find(&actor)};
    check(index);
    return *index;
}

auto compile_proxy_mission(::FLevelMissionDefinition const& definition,
                           FProxyEntityIndexMap const& entities,
                           sim::SimClock const& clock)
    -> std::expected<sim::LevelMissionInitialisationData, FLevelStartErrors> {
    std::vector<levels::EntityId> ids(static_cast<std::size_t>(entities.Num()));
    for (auto const& entity : entities) {
        ids[entity.Value] = levels::EntityId{"proxy-" + std::to_string(entity.Value)};
    }
    FLevelStartErrors errors;
    for (auto const& binding : definition.entity_bindings) {
        auto const* index{entities.Find(binding.Value)};
        if (!index) {
            errors.add(FString::Printf(TEXT("Mission binding '%s' has no simulation entity."),
                                       *binding.Key.ToString()));
            return std::unexpected{MoveTemp(errors)};
        }
        ids[*index] = levels::EntityId{TCHAR_TO_UTF8(*binding.Key.ToString().ToLower())};
    }
    auto compiled{sim::compile_mission(definition.objectives, ids, clock)};
    if (!compiled) {
        for (auto const& message : compiled.error()) {
            errors.add(to_fstring(message));
        }
        return std::unexpected{MoveTemp(errors)};
    }
    return sim::LevelMissionInitialisationData{
        .definition = std::move(*compiled),
        .level_id = TCHAR_TO_UTF8(*definition.level_id.ToString()),
        .level_title = TCHAR_TO_UTF8(*definition.level_display_name),
        .save_results = definition.save_mission_results};
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
void add_proxy_bindings(TArray<TProxy*> const& proxies, FProxyEntityMap& bindings) {
    for (auto* const proxy : proxies) {
        check(IsValid(proxy));
        auto const id{proxy->get_unique_id()};
        check(id.is_valid());
        bindings.Add(proxy, id);
    }
}

template <typename TProxy>
void destroy_proxy_actors(TArray<TProxy*> const& proxies) {
    for (auto* const proxy : proxies) {
        check(IsValid(proxy));
        check(proxy->Destroy());
    }
}

auto proxy_fits_collision_grid(AActor const& actor,
                               sim::EntityType const type,
                               sim::LevelSimInitData const& data,
                               USpaceGameLevelConfig const& config,
                               FLevelStartErrors& errors) -> bool {
    auto const transform{actor.GetActorTransform()};
    auto const bounds{sim::collision::make_entity_world_bounds(
        data.collision.entity_bounds,
        type,
        to_native(FVector3f{transform.GetLocation()}),
        to_quaternion(to_native(FRotator3f{transform.Rotator()})))};
    auto const coordinates{
        sim::collision::to_cell_coord_bounds(data.collision.grid_geometry, bounds.min, bounds.max)};
    if (sim::collision::is_cell_coord_in_bounds(data.collision.grid_geometry, coordinates.min) &&
        sim::collision::is_cell_coord_in_bounds(data.collision.grid_geometry, coordinates.max)) {
        return true;
    }

    auto const& grid{data.collision.grid_geometry};
    auto const half_size{FVector3f{grid.dimensions.x() * grid.cell_dimensions.x() * 0.5f,
                                   grid.dimensions.y() * grid.cell_dimensions.y() * 0.5f,
                                   grid.dimensions.z() * grid.cell_dimensions.z() * 0.5f}};
    auto const min{to_unreal(bounds.min)};
    auto const max{to_unreal(bounds.max)};
    errors.add(FString::Printf(
        TEXT("Actor '%s' has collision bounds %s to %s outside the grid (%s to %s) in '%s'. "
             "Increase collision_grid.grid_size or move the actor."),
        *actor.GetName(),
        *min.ToString(),
        *max.ToString(),
        *(-half_size).ToString(),
        *half_size.ToString(),
        *config.GetPathName()));
    return false;
}

void append_fighter_spawn_slot_errors(
    std::vector<levels::FighterSpawnSlotValidationError> const& validation_errors,
    sim::CapitalShipSimConfig const& capital_config,
    FLevelStartErrors& errors) {
    for (auto const& error : validation_errors) {
        switch (error.kind) {
            case ::ioj::levels::FighterSpawnSlotValidationErrorKind::IntersectsCapital: {
                auto const location{to_float(
                    capital_config.fighter_spawn_slots_relative_transforms[error.first_slot]
                        .location)};
                errors.add(FString::Printf(
                    TEXT("fighter spawn slot %d at %s intersects the capital collision bounds plus "
                         "fighter clearance"),
                    error.first_slot,
                    *to_unreal(location).ToString()));
                break;
            }
            case ::ioj::levels::FighterSpawnSlotValidationErrorKind::SlotsOverlap:
                errors.add(FString::Printf(
                    TEXT("fighter spawn slots %d and %d have overlapping collision bounds plus "
                         "%.1f cm clearance"),
                    error.first_slot,
                    error.second_slot,
                    error.clearance));
                break;
        }
    }
}

void validate_fighter_spawn_slots(sim::CapitalShipSimConfig const& capital_config,
                                  sim::FighterSimConfig const& fighter_config,
                                  sim::collision::EntityAABBs const& entity_bounds,
                                  FLevelStartErrors& errors) {
    append_fighter_spawn_slot_errors(
        levels::validate_fighter_spawn_slots(capital_config, fighter_config, entity_bounds),
        capital_config,
        errors);
}
}

namespace ioj {
auto FProxyLevelSimBuild::bind_proxy_entities(sim::LevelSim const& simulation) const
    -> FProxyEntityMap {
    auto const capital_count{capital_proxies.Num()};
    auto const capital_entities{simulation.get_capital_ships().get_entities()};
    for (int32 i{}; i < capital_count; ++i) {
        capital_proxies[i]->set_unique_id(capital_entities.entity_ids()[i]);
    }

    auto const turret_entities{simulation.get_turrets().get_entities()};
    auto const turret_count{turret_proxies.Num()};
    for (int32 i{}; i < turret_count; ++i) {
        turret_proxies[i]->set_unique_id(turret_entities.entity_ids()[i]);
    }

    auto const spinner_entities{simulation.get_spinners().get_entities()};
    auto const spinner_count{spinner_proxies.Num()};
    for (int32 i{}; i < spinner_count; ++i) {
        spinner_proxies[i]->set_unique_id(spinner_entities.entity_ids()[i]);
    }

    FProxyEntityMap bindings;
    level_simulation_builder::add_proxy_bindings(capital_proxies, bindings);
    level_simulation_builder::add_proxy_bindings(turret_proxies, bindings);
    level_simulation_builder::add_proxy_bindings(spinner_proxies, bindings);
    return bindings;
}

void FProxyLevelSimBuild::destroy_proxy_actors() const {
    level_simulation_builder::destroy_proxy_actors(capital_proxies);
    level_simulation_builder::destroy_proxy_actors(turret_proxies);
    level_simulation_builder::destroy_proxy_actors(spinner_proxies);
}

auto make_level_simulation_init_data(USpaceGameLevelConfig const& config,
                                     sim::FixedTickLoop const& clock_settings,
                                     TOptional<sim::player::PlayerSpawnData> player,
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
    data.clock_settings = clock_settings;
    data.collision.overlap_response = config.overlap_response;
    data.lasers = config.laser_projectiles.simulation;
    data.capital_ships = config.capital_ships.simulation;
    data.capital_ships.fighter_spawn_slots_relative_transforms.clear();
    for (auto const& transform : config.capital_ships.fighter_spawn_slots_relative_transforms) {
        data.capital_ships.fighter_spawn_slots_relative_transforms.push_back(to_native(transform));
    }
    data.capital_ships.fighter_spawn_slots = static_cast<std::int32_t>(
        data.capital_ships.fighter_spawn_slots_relative_transforms.size());
    data.fighters = config.fighters.simulation;
    data.turrets = config.turrets.simulation;
    data.turrets.fire_point_offset =
        to_native(FVector3f{config.turrets.fire_point_offset.GetLocation()});
    data.spinners = config.tube_spinners.simulation;
    data.spinners.fire_point_offsets.clear();
    for (auto const& offset : config.tube_spinners.fire_point_offsets) {
        data.spinners.fire_point_offsets.push_back(
            {to_native(FVector3f{offset.GetLocation()}), to_native(FRotator3f{offset.Rotator()})});
    }
    if (player.IsSet()) {
        data.player.emplace(MoveTemp(player.GetValue()));
    }
    auto const* socket{config.fighters.mesh->FindSocket(TEXT("Gun"))};
    data.fighters.fire_point_distance =
        IsValid(socket) ? static_cast<float>(socket->RelativeLocation.Size()) : 0.f;
    auto const dimensions{config.collision_grid.calculate_grid_dimensions()};
    data.collision.grid_geometry = {{dimensions.X, dimensions.Y, dimensions.Z},
                                    to_native(config.collision_grid.cell_size)};

    FLevelCollisionHost::EntityMeshes meshes{};
    meshes[sim::EntityType::PlayerShip] = player_collision_mesh;
    meshes[sim::EntityType::CapitalShip] = config.capital_ships.mesh;
    meshes[sim::EntityType::Fighter] = config.fighters.mesh;
    meshes[sim::EntityType::Turret] = config.turrets.mesh;
    meshes[sim::EntityType::TubeSpinner] = config.tube_spinners.mesh;
    auto bounds{FLevelCollisionHost::extract_entity_bounds(meshes)};
    if (!bounds) {
        return FLevelSimBuildResult{std::unexpect, MoveTemp(bounds.error())};
    }
    data.collision.entity_bounds = MoveTemp(bounds.value());
    // A capital's rigid world transform preserves slot clearance. Comparing expanded world AABBs
    // would report false overlaps when the capital is rotated.
    level_simulation_builder::validate_fighter_spawn_slots(
        data.capital_ships, data.fighters, data.collision.entity_bounds, errors);
    if (errors.has_errors()) {
        return FLevelSimBuildResult{std::unexpect, MoveTemp(errors)};
    }
    return result;
}

auto make_level_simulation_init_data(USpaceGameLevelConfig const& config,
                                     sim::FixedTickLoop const& clock_settings,
                                     levels::LevelDefinition const& definition,
                                     TOptional<sim::player::PlayerSpawnData> player,
                                     sim::collision::WorldAABBs static_bounds,
                                     UStaticMesh const* const player_collision_mesh)
    -> FLevelSimBuildResult {
    auto result{make_level_simulation_init_data(
        config, clock_settings, MoveTemp(player), player_collision_mesh)};
    if (!result) {
        return result;
    }
    auto& data{result.value()};
    data.collision.static_bounds = MoveTemp(static_bounds);
    sim::SimClock clock;
    clock.initialise(clock_settings);
    auto compiled{levels::compile_level(definition, clock, data.capital_ships, data.turrets)};
    if (!compiled) {
        FLevelStartErrors errors;
        for (auto const& error : compiled.error()) {
            errors.add(to_fstring(error));
        }
        return FLevelSimBuildResult{std::unexpect, MoveTemp(errors)};
    }
    data.level_events = MoveTemp(compiled.value());
    return result;
}

auto make_proxy_level_simulation_init_data(USpaceGameLevelConfig const& config,
                                           sim::FixedTickLoop const& clock_settings,
                                           UWorld& world,
                                           ::FLevelMissionDefinition& mission_definition,
                                           TOptional<sim::player::PlayerSpawnData> player,
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

    for (auto const* proxy : build.capital_proxies) {
        allocate_entity_index(*proxy);
    }
    for (auto const* proxy : build.turret_proxies) {
        allocate_entity_index(*proxy);
    }
    for (auto const* proxy : build.spinner_proxies) {
        allocate_entity_index(*proxy);
    }

    std::vector<sim::InitialCapital> capitals;
    capitals.reserve(build.capital_proxies.Num());
    for (auto const* proxy : build.capital_proxies) {
        auto const& transform{proxy->GetActorTransform()};
        auto const* target{proxy->get_target_ship().Get()};
        capitals.push_back({
            .entity = level_simulation_builder::resolve_entity_index(entity_indices, *proxy),
            .target = IsValid(target)
                        ? level_simulation_builder::resolve_entity_index(entity_indices, *target)
                        : sim::invalid_level_entity_index,
            .location = to_native(FVector3f{transform.GetLocation()}),
            .rotation = to_native(FRotator3f{transform.Rotator()}),
            .team = to_native(proxy->get_team()),
            .health = level_simulation_builder::to_optional(proxy->get_health()),
            .initial_spawn_delay =
                level_simulation_builder::to_optional(proxy->get_initial_spawn_delay()),
            .spawn_cooldown = level_simulation_builder::to_optional(proxy->get_spawn_cooldown()),
        });
    }
    std::vector<sim::InitialTurret> turrets;
    turrets.reserve(build.turret_proxies.Num());
    for (auto const* proxy : build.turret_proxies) {
        auto const transform{proxy->GetActorTransform()};
        build.initial_turret_transforms.Add(transform);
        turrets.push_back({
            .entity = level_simulation_builder::resolve_entity_index(entity_indices, *proxy),
            .location = to_native(FVector3f{transform.GetLocation()}),
            .rotation = to_native(FRotator3f{transform.Rotator()}),
            .team = to_native(proxy->get_team()),
            .health = level_simulation_builder::to_optional(proxy->get_health()),
            .laser_damage = level_simulation_builder::to_optional(proxy->get_laser_damage()),
        });
    }
    std::vector<sim::InitialSpinner> spinners;
    spinners.reserve(build.spinner_proxies.Num());
    for (auto const* proxy : build.spinner_proxies) {
        auto const& transform{proxy->GetActorTransform()};
        spinners.push_back({
            .entity = level_simulation_builder::resolve_entity_index(entity_indices, *proxy),
            .location = to_native(FVector3f{transform.GetLocation()}),
            .yaw = static_cast<float>(transform.Rotator().Yaw),
            .initial_fire_point = proxy->get_initial_active_fire_point(),
        });
    }
    sim::initialise_spawns(build.data, capitals, turrets, spinners);

    if (mission_definition.level_id.IsNone()) {
        mission_definition.level_id = FName{UGameplayStatics::GetCurrentLevelName(&world)};
    }
    if (mission_definition.level_display_name.IsEmpty()) {
        mission_definition.level_display_name = mission_definition.level_id.ToString();
    }
    sim::SimClock clock;
    clock.initialise(build.data.clock_settings);
    auto mission{
        level_simulation_builder::compile_proxy_mission(mission_definition, entity_indices, clock)};
    if (!mission) {
        return FProxyLevelSimBuildResult{std::unexpect, MoveTemp(mission.error())};
    }
    initialisation.mission = std::move(*mission);

    FLevelStartErrors bounds_errors;
    auto const fits{[&](AActor const& actor, sim::EntityType const type) {
        return level_simulation_builder::proxy_fits_collision_grid(
            actor, type, build.data, config, bounds_errors);
    }};
    if (build.data.player.has_value() && !fits(*player_actor, sim::EntityType::PlayerShip)) {
        return FProxyLevelSimBuildResult{std::unexpect, MoveTemp(bounds_errors)};
    }
    for (auto const* const proxy : build.capital_proxies) {
        if (!fits(*proxy, sim::EntityType::CapitalShip)) {
            return FProxyLevelSimBuildResult{std::unexpect, MoveTemp(bounds_errors)};
        }
    }
    for (auto const* const proxy : build.turret_proxies) {
        if (!fits(*proxy, sim::EntityType::Turret)) {
            return FProxyLevelSimBuildResult{std::unexpect, MoveTemp(bounds_errors)};
        }
    }
    for (auto const* const proxy : build.spinner_proxies) {
        if (!fits(*proxy, sim::EntityType::TubeSpinner)) {
            return FProxyLevelSimBuildResult{std::unexpect, MoveTemp(bounds_errors)};
        }
    }

    return result;
}
}
