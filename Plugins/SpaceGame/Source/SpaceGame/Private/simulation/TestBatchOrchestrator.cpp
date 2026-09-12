#include "SpaceGame/simulation/TestBatchOrchestrator.h"
#include <SpaceGame/telemetry/LevelTelemetryJson.h>

#include "SpaceGame/levels/LevelLoader.h"
#include "SpaceGame/system/GameSubsystem.h"

#include <SandboxGameShared/utilities/actor_utils.h>
#include <SpaceGame/defences/spinners/TestTubeSpinnerProxy.h>
#include <SpaceGame/defences/turrets/TestStaticTurretsProxy.h>
#include <SpaceGame/entities/TestEntity.h>
#include <SpaceGame/ships/capital/TestCapitalShipProxy.h>
#include <SpaceGame/ships/player/SpaceGamePlayerController.h>
#include <SpaceGame/ships/player/TestSpaceShip.h>
#include <SpaceGame/simulation/LevelSimulationBuilder.h>
#include <SpaceGame/telemetry/LevelTelemetryMetadata.h>
#include <SpaceGamePresentation/presentation/HUDManager.h>
#include <SpaceGamePresentation/simulation/CollisionGridVisualizationComponent.h>
#include <SpaceGamePresentation/support/mesh.h>
#include <SpaceGameSimulation/entities/TestEntityRegistry.h>
#include <SpaceGameSimulation/missions/TestMissionManager.h>
#include <SpaceGameSimulation/support/logging/SandboxLogCategories.h>

#include <SandboxCore/array_utils.h>
#include <SandboxCore/invoke.h>
#include <SandboxCore/soa_rotator_utils.h>
#include <SandboxCore/soa_vector_utils.h>
#include <SandboxCoreEngine/actor_utils.h>
#include <SandboxCoreEngine/uobject_utils.h>
#include <SandboxISMCComponent.h>
#include <SpaceGameRendering/SparkRendererComponent.h>

#include <CoreGlobals.h>
#include <Engine/GameInstance.h>
#include <Engine/LevelScriptActor.h>
#include <Engine/StaticMesh.h>
#include <Engine/StaticMeshSocket.h>
#include <EngineUtils.h>
#include <GameFramework/GameModeBase.h>
#include <GameFramework/GameStateBase.h>
#include <GameFramework/HUD.h>
#include <GameFramework/PhysicsVolume.h>
#include <GameFramework/PlayerController.h>
#include <GameFramework/PlayerState.h>
#include <GameFramework/WorldSettings.h>
#include <HAL/PlatformTime.h>
#include <Kismet/GameplayStatics.h>
#include <Misc/DateTime.h>
#include <SpaceGame/persistence/SpaceSaveGame.h>
#include <SpaceGame/persistence/SpaceSaveSubsystem.h>

namespace {
auto end_play_reason_name(EEndPlayReason::Type const reason) -> FString {
    switch (reason) {
        case EEndPlayReason::Destroyed:
            return TEXT("destroyed");
        case EEndPlayReason::LevelTransition:
            return TEXT("level_transition");
        case EEndPlayReason::EndPlayInEditor:
            return TEXT("end_play_in_editor");
        case EEndPlayReason::RemovedFromWorld:
            return TEXT("removed_from_world");
        case EEndPlayReason::Quit:
            return TEXT("quit");
    }

    return TEXT("unknown");
}

template <typename TActor, typename TConfig>
void apply_actor_config(TActor& actor, TConfig* const config) {
#if WITH_EDITOR
    auto const* const world{actor.GetWorld()};
    if (IsValid(world) && !world->IsGameWorld()) {
        actor.Modify();
    }
#endif
    actor.set_actor_config(config);
}

template <typename TActor, typename TConfig>
void set_actor_config_on_all(UWorld& world, TConfig* const config) {
    for (TActorIterator<TActor> it{&world}; it; ++it) {
        apply_actor_config(**it, config);
    }
}

void set_capital_proxy_config_on_all(UWorld& world, USpaceGameLevelConfig& config) {
    for (TActorIterator<ATestCapitalShipProxy> it{&world}; it; ++it) {
        auto& proxy{**it};
#if WITH_EDITOR
        if (!world.IsGameWorld()) {
            proxy.Modify();
        }
#endif
        proxy.set_level_config_asset(&config);
    }
}

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

}

FOnProxyEntitiesBound ATestBatchOrchestrator::on_proxy_entities_bound;

/* **************************************** */
// Lifecycle and simulation control
/* **************************************** */
ATestBatchOrchestrator::ATestBatchOrchestrator() {
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;

    collision_grid_visualization = CreateDefaultSubobject<UCollisionGridVisualizationComponent>(
        TEXT("CollisionGridVisualization"));
    RootComponent = collision_grid_visualization;
    laser_instances_ = CreateDefaultSubobject<USandboxISMCComponent>(TEXT("Lasers"));
    laser_instances_->SetupAttachment(RootComponent);
    spark_renderer_ = CreateDefaultSubobject<USparkRendererComponent>(TEXT("Sparks"));
    spark_renderer_->SetupAttachment(RootComponent);
    capital_instances_ =
        CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("CapitalShips"));
    capital_instances_->SetupAttachment(RootComponent);
    fighter_instances_ = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Fighters"));
    fighter_instances_->SetupAttachment(RootComponent);
    turret_instances_ = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Turrets"));
    turret_instances_->SetupAttachment(RootComponent);
    spinner_instances_ = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Spinners"));
    spinner_instances_->SetupAttachment(RootComponent);
    soft_target_instances_ =
        CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("SoftTargets"));
    soft_target_instances_->SetupAttachment(RootComponent);

    ml::set_actor_component_mobility(*this, EComponentMobility::Static);
    soft_target_instances_->SetMobility(EComponentMobility::Movable);
}

void ATestBatchOrchestrator::PostLoad() {
    Super::PostLoad();

    refresh_collision_grid_visualization();
}

void ATestBatchOrchestrator::BeginPlay() {
    Super::BeginPlay();

    if (start_mode == EOrchestratorStartMode::AuthoredLevel) {
        load_authored_level();
    } else if (should_initialise_in_begin_play()) {
        begin_play();
    } else {
        SetActorTickEnabled(false);
    }
}
void ATestBatchOrchestrator::EndPlay(EEndPlayReason::Type const end_play_reason) {
    if (level_simulation_.IsSet()) {
        level_simulation_->finalize_telemetry_run(ELevelTelemetryRunEndReason::WorldEnd,
                                                  end_play_reason_name(end_play_reason));
    }
    hud_manager.deactivate();
    if (IsValid(player_ship)) {
        player_ship->unbind_simulation();
    }
    persist_finalized_telemetry_run();
    level_presentation_.Reset();
    level_simulation_.Reset();
    level_definition_.Reset();
    world_collision_.restore_collision();
    SetActorTickEnabled(false);
    clear_end_tick_test_hook();

    Super::EndPlay(end_play_reason);
}

void ATestBatchOrchestrator::start_simulation() {
    if (!level_simulation_.IsSet() && !begin_play()) {
        return;
    }
    if (get_state() != EOrchestratorState::Paused) {
        UE_LOG(LogSandbox, Error, TEXT("Cannot start a simulation that is not paused"));
        return;
    }
    level_simulation_->start();
    SetActorTickEnabled(true);
}
void ATestBatchOrchestrator::pause_simulation() {
    if (level_simulation_.IsSet()) {
        level_simulation_->pause();
    }
    SetActorTickEnabled(false);
}
void ATestBatchOrchestrator::reset_for_new_level() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestBatchOrchestrator::reset_for_new_level);

    if (level_simulation_.IsSet()) {
        level_simulation_->finalize_telemetry_run(ELevelTelemetryRunEndReason::OrchestratorReset,
                                                  TEXT("reset"));
    }

    auto* const world{GetWorld()};
    if (!IsValid(world)) {
        UE_LOG(LogSandbox,
               Fatal,
               TEXT("ATestBatchOrchestrator::reset_for_new_level: World is invalid"));
        return;
    }

    SetActorTickEnabled(false);
    clear_end_tick_test_hook();

    hud_manager.deactivate();
    if (IsValid(player_ship)) {
        player_ship->unbind_simulation();
    }

    persist_finalized_telemetry_run();
    level_presentation_.Reset();
    level_simulation_.Reset();
    level_definition_.Reset();

    launched_paused_ = false;
    launch_options_ = {};
    level_source_sha256_.Reset();

    world_collision_.restore_collision();
    collision_grid_visualization->clear_collision_bounds();
    laser_instances_->clear_instances();
    for (auto* component : {capital_instances_.Get(),
                            fighter_instances_.Get(),
                            turret_instances_.Get(),
                            spinner_instances_.Get()}) {
        component->ClearInstances();
    }

    TStaticArray<AActor*, 1> recreated_actors{};
    int32 recreated_actor_count{0};
    auto recreate_actor{[this, world, &recreated_actors, &recreated_actor_count]<typename T>(
                            TObjectPtr<T>& actor) {
        if (!IsValid(actor)) {
            return;
        }

        auto* const old_actor{actor.Get()};
        UClass* const actor_class{old_actor->GetClass()};
        if (!IsValid(actor_class)) {
            UE_LOG(LogSandbox,
                   Fatal,
                   TEXT("ATestBatchOrchestrator::reset_for_new_level: Actor class is invalid"));
            return;
        }

        if (!old_actor->Destroy()) {
            UE_LOG(LogSandbox,
                   Fatal,
                   TEXT("ATestBatchOrchestrator::reset_for_new_level: Failed to destroy %s"),
                   *old_actor->GetName());
            return;
        }

        auto* const replacement{world->SpawnActorDeferred<T>(actor_class, FTransform::Identity)};
        if (!IsValid(replacement)) {
            UE_LOG(LogSandbox,
                   Fatal,
                   TEXT("ATestBatchOrchestrator::reset_for_new_level: Failed to spawn %s"),
                   *actor_class->GetName());
            actor = nullptr;
            return;
        }

        mission_definition.replace_startup_actor(old_actor, *replacement);
        actor = replacement;
        recreated_actors[recreated_actor_count] = replacement;
        ++recreated_actor_count;
    }};

    recreate_actor(player_ship);

    auto is_retained_actor{[this, &recreated_actors](AActor const* const actor) {
        if (actor == this || ml::actor_is_any<AWorldSettings,
                                              AGameModeBase,
                                              AGameStateBase,
                                              APlayerController,
                                              APlayerState,
                                              AHUD,
                                              ALevelScriptActor,
                                              APhysicsVolume>(*actor)) {
            return true;
        }

        for (auto* const recreated_actor : recreated_actors) {
            if (actor == recreated_actor) {
                return true;
            }
        }

        return false;
    }};
    for (TActorIterator<AActor> it{world}; it;) {
        auto* const actor{*it};
        ++it;

        if (!is_retained_actor(actor)) {
            actor->Destroy();
        }
    }

    constexpr auto apply_config{[](auto const actor_ptr, auto const* const actor_config) {
        if (IsValid(actor_ptr)) {
            apply_actor_config(*actor_ptr, actor_config);
        }
    }};

    if (IsValid(level_config)) {
        apply_config(player_ship, &level_config->player_ship);
    }

    for (int32 i{0}; i < recreated_actor_count; ++i) {
        UGameplayStatics::FinishSpawningActor(recreated_actors[i], FTransform::Identity);
    }

    if (should_initialise_in_begin_play()) {
        begin_play();
    }

    on_reset.Broadcast(*this);
}

/* **************************************** */
// Level configuration
/* **************************************** */
void ATestBatchOrchestrator::set_level_config(USpaceGameLevelConfig& config) {
    if (level_simulation_.IsSet()) {
        UE_LOG(LogSandbox,
               Error,
               TEXT("Reset the level simulation before replacing its configuration"));
        return;
    }
    if (!ensureAlwaysMsgf(config.is_valid(presentation_enabled), TEXT("Level config is invalid"))) {
        return;
    }

#if WITH_EDITOR
    if (auto const* const world{GetWorld()}; IsValid(world) && !world->IsGameWorld()) {
        Modify();
    }
#endif

    level_config = &config;
    refresh_collision_grid_visualization();
    if (IsValid(player_ship)) {
        apply_actor_config(*player_ship, &config.player_ship);
    }

    auto* const world{GetWorld()};
    if (IsValid(world)) {
        set_capital_proxy_config_on_all(*world, config);
        set_actor_config_on_all<ATestStaticTurretsProxy>(*world, &config.turrets);
        set_actor_config_on_all<ATestTubeSpinnerProxy>(*world, &config.tube_spinners);
    }
}
void ATestBatchOrchestrator::set_start_mode(EOrchestratorStartMode const mode) {
    if (get_state() != EOrchestratorState::Uninitialised) {
        UE_LOG(LogSandbox,
               Error,
               TEXT("ATestBatchOrchestrator::set_start_mode: Orchestrator is already initialised"));
        return;
    }

    start_mode = mode;
}
void ATestBatchOrchestrator::set_presentation_enabled(bool const enabled) {
    if (get_state() != EOrchestratorState::Uninitialised) {
        UE_LOG(
            LogSandbox, Error, TEXT("Cannot change presentation after simulation initialisation"));
        return;
    }
    presentation_enabled = enabled;
    refresh_collision_grid_visualization();
}

/* **************************************** */
// Player and combat simulations
/* **************************************** */
auto ATestBatchOrchestrator::get_player_ship() const -> ATestSpaceShip const* {
    return player_ship.Get();
}
auto ATestBatchOrchestrator::get_player_ship_simulation() noexcept
    -> ml::test_space_ship::Simulation* {
    return level_simulation_.IsSet() ? level_simulation_->get_player_ship_simulation() : nullptr;
}
auto ATestBatchOrchestrator::get_player_ship_simulation() const noexcept
    -> ml::test_space_ship::Simulation const* {
    return level_simulation_.IsSet() ? level_simulation_->get_player_ship_simulation() : nullptr;
}
void ATestBatchOrchestrator::set_player_ship(ATestSpaceShip& new_player_ship) {
    if (level_simulation_.IsSet()) {
        UE_LOG(LogSandbox, Error, TEXT("Reset the level simulation before replacing its player"));
        return;
    }
    if (IsValid(player_ship)) {
        player_ship->unbind_simulation();
    }
    player_ship = &new_player_ship;
}
void ATestBatchOrchestrator::clear_player_ship() {
    if (IsValid(player_ship)) {
        player_ship->unbind_simulation();
    }
    player_ship = nullptr;
}

/* **************************************** */
// Level initialization
/* **************************************** */
auto ATestBatchOrchestrator::initialise_simulation(ml::FLevelStartErrors& errors) -> bool {
    auto& world{*GetWorld()};
    auto const& config{*level_config};

    TOptional<ml::test_space_ship::FPlayerSpawnData> player;
    if (IsValid(player_ship)) {
        player.Emplace(player_ship->make_spawn_data());
    }

    auto const* const player_collision_mesh{IsValid(player_ship) ? player_ship->get_collision_mesh()
                                                                 : nullptr};

    if (level_definition_.IsSet()) {
        auto result{ml::make_level_simulation_init_data(config,
                                                        simulation_tick_loop,
                                                        level_definition_.GetValue(),
                                                        MoveTemp(player),
                                                        {},
                                                        player_collision_mesh)};
        if (!result) {
            errors = MoveTemp(result.error());
            return false;
        }

        auto const presentation{make_presentation_resources()};
        if (presentation_enabled && !presentation.is_valid()) {
            errors.add(TEXT("Level presentation resources are incomplete"));
            return false;
        }

        result->telemetry_metadata = make_level_telemetry_run_metadata(world, mission_definition);
        result->telemetry_metadata->level_id = level_definition_->metadata.id.value;
        result->telemetry_metadata->level_display_name = level_definition_->metadata.title;
        result->telemetry_metadata->source_sha256 = level_source_sha256_;
        result->telemetry_metadata->requested_duration_seconds =
            launch_options_.simulated_duration_seconds;
        result->telemetry_metadata->detailed_timing = launch_options_.detailed_timing;
        result->telemetry_metadata->stop_when_battle_resolved =
            launch_options_.stop_when_battle_resolved;
        if (auto* const game_subsystem{ml::ioj::UGameSubsystem::get(GetGameInstance())};
            IsValid(game_subsystem)) {
            result->game_memory = &game_subsystem->get_game_memory();
        }

        initial_turret_transforms_ = result->turret_transforms;
        level_simulation_.Emplace(MoveTemp(result.value()));
        validate_entity_handles();
        return true;
    }

    auto result{ml::make_level_simulation_init_data(
        config, simulation_tick_loop, MoveTemp(player), player_collision_mesh)};
    if (!result) {
        errors = MoveTemp(result.error());
        return false;
    }

    auto& data{result.value()};

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
            spawn_data.locations.set(i, FVector3f{proxy_transform.GetLocation()});
            spawn_data.rotations.set(i, FRotator3f{proxy_transform.Rotator()});
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
            spawn_data.set(
                i,
                FVector3f{transform.GetLocation()},
                turret_proxies[i]->get_team(),
                turret_proxies[i]->get_health().Get(level_config->turrets.max_health),
                turret_proxies[i]->get_laser_damage().Get(level_config->turrets.laser.damage));
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

            new_locations.set(i, FVector3f{transform.GetLocation()});
            new_yaws[i] = transform.Rotator().Yaw;
            new_fire_point_indices[i] = proxy->get_initial_active_fire_point();
        }

        data.spinner_locations = MoveTemp(new_locations);
        data.spinner_yaws = MoveTemp(new_yaws);
        data.spinner_fire_points = MoveTemp(new_fire_point_indices);
    }

    auto const presentation{make_presentation_resources()};
    if (presentation_enabled && !presentation.is_valid()) {
        errors.add(TEXT("Level presentation resources are incomplete"));
        return false;
    }

    data.telemetry_metadata = make_level_telemetry_run_metadata(world, mission_definition);
    if (auto* const game_subsystem{ml::ioj::UGameSubsystem::get(GetGameInstance())};
        IsValid(game_subsystem)) {
        data.game_memory = &game_subsystem->get_game_memory();
    }
    ml::validate_world_fighter_spawn_slots(data, errors);
    if (errors.has_errors()) {
        return false;
    }

    initial_turret_transforms_ = data.turret_transforms;
    level_simulation_.Emplace(MoveTemp(data));

    auto const capital_count{capital_proxies.Num()};
    for (int32 i{}; i < capital_count; ++i) {
        capital_proxies[i]->set_entity_handle(get_capital_ships()->get_handle(i));
    }

    auto const turret_count{turret_proxies.Num()};
    for (int32 i{}; i < turret_count; ++i) {
        turret_proxies[i]->set_entity_handle(get_turrets()->get_read_view().entities.handles[i]);
    }

    auto const spinner_count{spinner_proxies.Num()};
    for (int32 i{}; i < spinner_count; ++i) {
        spinner_proxies[i]->set_entity_handle(get_spinners()->get_read_view().entities.handles[i]);
    }

    validate_entity_handles();
    return true;
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

        get_capital_ships()->set_target_handle(identifiers->handle, target_handle);
    }
}
void ATestBatchOrchestrator::bind_and_destroy_proxies() {
    if (level_definition_.IsSet()) {
        return;
    }

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
auto ATestBatchOrchestrator::begin_play() -> bool {
    if (level_simulation_.IsSet()) {
        handle_level_start_failure(TEXT("Level simulation is already initialized"));
        return false;
    }

    auto* const world{GetWorld()};
    if (!IsValid(world)) {
        handle_level_start_failure(TEXT("Cannot start level: world is invalid"));
        return false;
    }

    if (!IsValid(level_config)) {
        handle_level_start_failure(TEXT("Cannot start level: level configuration is invalid"));
        return false;
    }

    ml::FLevelStartErrors config_errors;
    TArray<FString> config_error_messages;
    level_config->get_validation_errors(config_error_messages, presentation_enabled);
    config_errors.append(MoveTemp(config_error_messages));

    if (presentation_enabled && IsValid(player_ship) &&
        !level_config->player_ship.team_visual_data) {
        config_errors.add(TEXT("player_ship.team_visual_data is null"));
    }
    if (!FMath::IsFinite(simulation_tick_loop.tick_rate) || simulation_tick_loop.tick_rate <= 0.0) {
        config_errors.add(TEXT("simulation tick rate must be finite and positive"));
    }
    if (!FMath::IsFinite(simulation_tick_loop.time_scale) ||
        simulation_tick_loop.time_scale <= 0.0) {
        config_errors.add(TEXT("simulation time scale must be finite and positive"));
    }

    if (config_errors.has_errors()) {
        handle_level_start_failure(
            FString::Printf(TEXT("Cannot start level: level configuration failed validation:\n%s"),
                            *config_errors.format()));
        return false;
    }

    if (!presentation_enabled &&
        (IsValid(player_ship) || ml::get_first_actor<ATestSpaceShip>(*world))) {
        handle_level_start_failure(TEXT("Presentation-disabled levels must be playerless"));
        return false;
    }

    set_level_config(*level_config);
    hud_tick_loop.initialise();

    ml::FLevelStartErrors simulation_errors;
    if (!initialise_simulation(simulation_errors)) {
        handle_level_start_failure(
            FString::Printf(TEXT("Cannot start level:\n%s"), *simulation_errors.format()));
        return false;
    }

    if (IsValid(player_ship) && get_player_ship_simulation()) {
        player_ship->bind_simulation(*get_player_ship_simulation());
    }

    bind_and_destroy_proxies();
    world_collision_.initialise_static_geometry(
        *world, level_config->collision_grid, get_spatial_query_manager().get_collision_system());

    external_timings_.Reset();
    telemetry_environment_ = make_level_telemetry_environment(*world);
    level_simulation_->finish_initialisation();

    if (presentation_enabled) {
        level_presentation_.Emplace(make_presentation_resources(),
                                    level_simulation_->get_read_view(),
                                    MoveTemp(initial_turret_transforms_));
    }
    update_collision_bounds_visualization();

    level_simulation_->on_mission_evaluated = [this] { process_mission_result(); };
    level_simulation_->on_end_tick = [this](FLevelSimulation&) {
        end_tick_test_hook.ExecuteIfBound(*this);
        process_mission_result();
        process_battle_run_end();
    };

    if (presentation_enabled) {
        hud_manager.initialise(hud_update_frequencies,
                               get_mission_manager(),
                               get_entity_registry(),
                               hud_tick_loop.tick_rate,
                               get_player_ship_simulation(),
                               level_config->get_visual_config(),
                               level_config->entity_overlay,
                               level_config->radar,
                               soft_target_instances_);
        if (IsValid(player_ship)) {
            if (auto* const controller{
                    Cast<ASpaceGamePlayerController>(player_ship->GetController())};
                IsValid(controller)) {
                controller->activate_ship_control();
            }
        }
    }

    bool const automatic{
        start_mode == EOrchestratorStartMode::Automatic ||
        start_mode == EOrchestratorStartMode::AuthoredLevel ||
        (start_mode == EOrchestratorStartMode::PausedInTest && !GIsAutomationTesting)};
    if (automatic) {
        level_simulation_->start();
    }

    SetActorTickEnabled(automatic);
    return true;
}
void ATestBatchOrchestrator::handle_level_start_failure(FString message) {
    SetActorTickEnabled(false);
    UE_LOG(LogSandbox, Error, TEXT("%s"), *message);

    auto* const world{GetWorld()};
#if WITH_EDITOR
    if (IsValid(world) && world->WorldType == EWorldType::PIE) {
        return;
    }
#endif
    if (GIsAutomationTesting) {
        return;
    }

    auto* const game_instance{IsValid(world) ? world->GetGameInstance() : nullptr};
    auto* const subsystem{ml::ioj::UGameSubsystem::get(game_instance)};
    if (IsValid(subsystem)) {
        subsystem->set_level_launch_error(MoveTemp(message));
        if (subsystem->return_to_level_select()) {
            return;
        }
    }

    if (IsValid(world)) {
        UGameplayStatics::OpenLevel(world, ml::ioj::UGameSubsystem::get_main_menu_level_name());
    }
}
void ATestBatchOrchestrator::load_authored_level() {
    launched_paused_ = false;

    auto* const world{GetWorld()};
    auto* const game_instance{IsValid(world) ? world->GetGameInstance() : nullptr};
    auto* const subsystem{ml::ioj::UGameSubsystem::get(game_instance)};
    if (!IsValid(subsystem)) {
        handle_level_start_failure(TEXT("Cannot load authored level: game subsystem is invalid"));
        return;
    }

    auto pending{subsystem->take_pending_level()};
    if (!pending.IsSet()) {
        auto const error{TEXT("No pending authored level was provided to GameRuntime.")};
        handle_level_start_failure(error);
        return;
    }

    auto const& options{pending->options};
    if (!ml::ioj::level_launch::is_valid_time_scale(options.requested_time_scale)) {
        handle_level_start_failure(
            FString::Printf(TEXT("Cannot load authored level: requested time scale %.17g is "
                                 "outside the supported range (0, %.0f]."),
                            options.requested_time_scale,
                            ml::ioj::level_launch::maximum_time_scale));
        return;
    }

    if (options.presentation_mode == ml::ioj::ELevelPresentationMode::SimulationOnly &&
        !options.simulated_duration_seconds.IsSet()) {
        handle_level_start_failure(
            TEXT("Cannot load authored level: simulation-only runs require a duration."));
        return;
    }
    if (options.simulated_duration_seconds.IsSet() &&
        (!FMath::IsFinite(options.simulated_duration_seconds.GetValue()) ||
         options.simulated_duration_seconds.GetValue() <= 0.0)) {
        handle_level_start_failure(
            TEXT("Cannot load authored level: simulated duration must be finite and positive."));
        return;
    }

    launch_options_ = options;
    level_source_sha256_ = MoveTemp(pending->source_sha256);
    launched_paused_ = options.launch_mode == ml::ioj::ELevelLaunchMode::Paused;
    presentation_enabled = options.presentation_mode == ml::ioj::ELevelPresentationMode::Visual;
    set_time_scale(options.requested_time_scale);

    ml::FLevelLoader loader{*this};
    auto const result{loader.load(pending->definition, options.control_context)};
    if (!result) {
        TArray<FString> messages;
        messages.Reserve(result.validation_errors.Num() + result.errors.Num());
        for (auto const& error : result.validation_errors) {
            messages.Add(error.message);
        }
        for (auto const& error : result.errors) {
            messages.Add(error.message);
        }
        auto const message{FString::Printf(TEXT("Failed to load '%s':\n%s"),
                                           *pending->source_path,
                                           *FString::Join(messages, TEXT("\n")))};
        handle_level_start_failure(message);
        return;
    }

    if (!begin_play()) {
        return;
    }

    if (launched_paused_) {
        pause_simulation();
    }
}
auto ATestBatchOrchestrator::should_initialise_in_begin_play() const noexcept -> bool {
    return start_mode == EOrchestratorStartMode::Automatic ||
           (start_mode == EOrchestratorStartMode::PausedInTest && !GIsAutomationTesting);
}

/* **************************************** */
// Presentation and diagnostics
/* **************************************** */
void ATestBatchOrchestrator::refresh_collision_grid_visualization() {
    if (!IsValid(collision_grid_visualization)) {
        UE_LOG(LogSandbox,
               Error,
               TEXT("Cannot refresh collision-grid visualization: component is invalid"));
        return;
    }

    if (!presentation_enabled || !IsValid(level_config)) {
        collision_grid_visualization->clear();
        return;
    }

    collision_grid_visualization->configure(
        level_config->collision_grid.calculate_grid_dimensions(),
        level_config->collision_grid.cell_size,
        level_config->collision_grid.line_colour,
        level_config->collision_grid.line_thickness,
        level_config->collision_grid.show_grid);
    collision_grid_visualization->configure_collision_bounds(show_collision_bounds,
                                                             collision_bounds_max_draw_distance);
}
void ATestBatchOrchestrator::update_collision_bounds_visualization() {
    if (!IsValid(collision_grid_visualization)) {
        UE_LOG(LogSandbox,
               Error,
               TEXT("Cannot update collision-bounds visualization: component is invalid"));
        return;
    }

    auto const visible{presentation_enabled && show_collision_bounds && level_simulation_.IsSet()};
    collision_grid_visualization->configure_collision_bounds(visible,
                                                             collision_bounds_max_draw_distance);
    if (!visible) {
        collision_grid_visualization->clear_collision_bounds();
        return;
    }

    collision_grid_visualization->update_collision_bounds(
        get_spatial_query_manager().get_collision_system());
}

/* **************************************** */
// Simulation state and timing
/* **************************************** */
void ATestBatchOrchestrator::validate_entity_handles() {
    if (auto const* player{get_player_ship_simulation()}) {
        check(get_entity_registry().is_valid_handle(player->registry_handle));
    }
    get_capital_ships()->validate_entity_handles();
    get_turrets()->validate_entity_handles();
}
void ATestBatchOrchestrator::Tick(float dt) {
    Super::Tick(dt);

    tick(static_cast<time_type>(dt));
}
void ATestBatchOrchestrator::tick(time_type const dt) {
    if (get_state() != EOrchestratorState::Running) {
        return;
    }

    auto const detailed_timing{get_level_telemetry_manager().detailed_timing_enabled()};
    auto const timing_window{get_level_telemetry_manager().get_performance_window_count()};

    level_simulation_->advance(dt);
    update_collision_bounds_visualization();

    if (presentation_enabled) {
        auto const hud_started_at{detailed_timing ? FPlatformTime::Seconds() : 0.0};

        hud_tick_loop.add_time(dt);
        while (hud_tick_loop.try_tick()) {
            hud_manager.tick(1);
        }

        if (detailed_timing) {
            record_external_timing(timing_window,
                                   ELevelTelemetryTimingSystem::Hud,
                                   FPlatformTime::Seconds() - hud_started_at);
        }
    }

    auto const presentation_started_at{detailed_timing ? FPlatformTime::Seconds() : 0.0};
    if (level_presentation_.IsSet()) {
        level_presentation_->tick(dt, level_simulation_->get_read_view());
    }

    if (auto* player{get_player_ship_simulation()};
        player && player->consume_death_notification()) {
        if (IsValid(player_ship)) {
            player_ship->handle_simulation_death();
        }
    }

    if (detailed_timing) {
        record_external_timing(timing_window,
                               ELevelTelemetryTimingSystem::Presentation,
                               FPlatformTime::Seconds() - presentation_started_at);
    }

    persist_finalized_telemetry_run();
}
void ATestBatchOrchestrator::set_time_scale(time_type const scale) noexcept {
    check(scale > time_type{0});
    simulation_tick_loop.time_scale = scale;
    if (level_simulation_.IsSet()) {
        level_simulation_->set_time_scale(scale);
    }
}
auto ATestBatchOrchestrator::frequency_to_tick_period(time_type const frequency) const noexcept
    -> tick_type {
    check(frequency > time_type{0});
    check(simulation_tick_loop.tick_rate > time_type{0});
    return static_cast<tick_type>(FMath::CeilToInt64(simulation_tick_loop.tick_rate / frequency));
}
auto ATestBatchOrchestrator::duration_to_tick_period(time_type const duration) const noexcept
    -> tick_type {
    check(duration >= time_type{0});
    check(simulation_tick_loop.tick_rate > time_type{0});
    return static_cast<tick_type>(FMath::CeilToInt64(duration * simulation_tick_loop.tick_rate));
}

/* **************************************** */
// Testing and level preparation
/* **************************************** */
void ATestBatchOrchestrator::set_end_tick_test_hook(FOrchestratorEndTickTestHook hook) {
    end_tick_test_hook = MoveTemp(hook);
}
void ATestBatchOrchestrator::clear_end_tick_test_hook() {
    end_tick_test_hook.Unbind();
}
void ATestBatchOrchestrator::prepare_level() {
    if (get_state() != EOrchestratorState::Uninitialised) {
        UE_LOG(LogSandbox, Error, TEXT("Cannot prepare an already initialized level"));
        return;
    }
    auto* world{GetWorld()};
    ml::fatal_if_uobject_ptrs_invalid(
        {SANDBOX_NAMED_UOBJECT_PTR(world), SANDBOX_NAMED_UOBJECT_PTR(level_config)});
    if (!IsValid(player_ship)) {
        player_ship = ml::get_first_actor<ATestSpaceShip>(*world);
    }
    set_level_config(*level_config);
}

#if WITH_EDITOR
void ATestBatchOrchestrator::apply_level_config() {
    if (!IsValid(level_config)) {
        UE_LOG(LogSandbox, Error, TEXT("Cannot apply a null level config."));
        return;
    }
    set_level_config(*level_config);
}

void ATestBatchOrchestrator::prepare_level_button() {
    prepare_level();
}
#endif

/* **************************************** */
// Presentation and collision
/* **************************************** */
auto ATestBatchOrchestrator::make_presentation_resources() const -> FLevelPresentationResources {
    return {
        .lasers = laser_instances_,
        .capital_ships = capital_instances_,
        .fighters = fighter_instances_,
        .turrets = turret_instances_,
        .spinners = spinner_instances_,
        .sparks = spark_renderer_,
        .config = level_config->get_visual_config(),
        .player =
            IsValid(player_ship)
                ? TOptional<FPlayerPresentationResources>{player_ship->get_presentation_resources()}
                : NullOpt};
}
auto ATestBatchOrchestrator::add_static_geometry(UPrimitiveComponent& component) -> bool {
    check(level_simulation_.IsSet());
    auto const added{world_collision_.add_static_geometry(
        component, get_spatial_query_manager().get_collision_system())};
    if (added) {
        update_collision_bounds_visualization();
    }
    return added;
}

/* **************************************** */
// Mission and telemetry
/* **************************************** */
void ATestBatchOrchestrator::process_mission_result() {
    auto result{level_simulation_->take_mission_result()};
    if (!result.IsSet()) {
        return;
    }
    auto const par_time_seconds{level_definition_.IsSet()
                                    ? level_definition_->metadata.par_time_seconds
                                    : TOptional<float>{}};
    bool persisted{};
    bool new_best_time{};
    if (result->save_results) {
        auto* game_instance{GetGameInstance()};
        auto* saves{USpaceSaveSubsystem::get(game_instance)};
        if (IsValid(saves)) {
            if (result->state == ETestMissionState::Succeeded) {
                auto const previous_progress{
                    saves->get_level_progress(ml::FLevelId{result->level_id})};
                new_best_time =
                    previous_progress.best_completion_time_seconds < 0.0f ||
                    result->elapsed_seconds < previous_progress.best_completion_time_seconds;
            }

            FScoreRecord const record{
                .date = FDateTime::Now(),
                .level_name = result->level_id,
                .mission_mode = result->mode,
                .end_state = result->state,
                .fail_reason = result->fail_reason,
                .kills = result->kills,
                .time_seconds = result->elapsed_seconds,
                .target_kills = result->target_kills,
                .target_completion_time = result->target_time,
            };
            persisted = saves->save_score_record(record);
            new_best_time = persisted && new_best_time;
        } else {
            UE_LOG(LogSandbox,
                   Error,
                   TEXT("Cannot persist mission result: save subsystem is unavailable"));
        }
    }
    on_mission_completed.Broadcast({.level_id = result->level_id,
                                    .level_display_name = result->level_display_name,
                                    .state = result->state,
                                    .persisted = persisted,
                                    .par_time_seconds = par_time_seconds,
                                    .new_best_time = new_best_time});
}
void ATestBatchOrchestrator::process_battle_run_end() {
    if (!level_simulation_.IsSet() ||
        !level_simulation_->get_level_telemetry_manager().is_run_recording()) {
        return;
    }

    if (launch_options_.stop_when_battle_resolved &&
        !level_simulation_->has_future_authored_spawns()) {
        auto const alive_by_team{get_entity_registry().count_alive_per_team()};
        int32 living_team_count{};
        TOptional<ETestTeam> winner;
        constexpr auto team_count{ml::EnumCountTrait<ETestTeam>::count_value};
        for (int32 team_index{}; team_index < team_count; ++team_index) {
            if (alive_by_team[team_index] > 0) {
                ++living_team_count;
                winner = static_cast<ETestTeam>(team_index);
            }
        }
        if (living_team_count <= 1) {
            level_simulation_->complete_telemetry_run(
                ELevelTelemetryRunEndReason::BattleResolved,
                living_team_count == 1 ? winner : TOptional<ETestTeam>{});
            return;
        }
    }

    if (launch_options_.simulated_duration_seconds.IsSet() &&
        get_simulation_time() >= launch_options_.simulated_duration_seconds.GetValue()) {
        level_simulation_->complete_telemetry_run(ELevelTelemetryRunEndReason::DurationReached);
    }
}
void ATestBatchOrchestrator::handle_telemetry_persisted(FString run_id, FString error) {
    if (launch_options_.results_navigation != ml::ioj::ELevelResultsNavigation::Telemetry) {
        return;
    }
    auto* const game_instance{GetGameInstance()};
    auto* const subsystem{ml::ioj::UGameSubsystem::get(game_instance)};
    if (!IsValid(subsystem)) {
        UE_LOG(LogSandbox, Error, TEXT("Cannot navigate to telemetry: subsystem is unavailable"));
        return;
    }
    if (!subsystem->return_to_telemetry(MoveTemp(run_id), MoveTemp(error))) {
        UE_LOG(LogSandbox, Error, TEXT("Cannot navigate to completed telemetry run"));
    }
}
auto ATestBatchOrchestrator::take_finalized_telemetry_report() -> TOptional<FLevelTelemetryReport> {
    if (!level_simulation_.IsSet()) {
        return {};
    }
    auto record{get_level_telemetry_manager().take_finalized_run()};
    if (!record.IsSet()) {
        return {};
    }
    FLevelTelemetryReport report{MoveTemp(record.GetValue())};
    report.metadata.environment = telemetry_environment_;
    report.metadata.launch_state = launch_options_.launch_mode == ml::ioj::ELevelLaunchMode::Paused
                                     ? TEXT("paused")
                                     : TEXT("running");
    report.metadata.results_navigation =
        launch_options_.results_navigation == ml::ioj::ELevelResultsNavigation::Telemetry
            ? TEXT("telemetry")
            : TEXT("none");
    report.metadata.presentation_enabled = presentation_enabled;
    report.metadata.presentation_mode =
        presentation_enabled ? TEXT("visual") : TEXT("simulation_only");
    append_external_timings(report, external_timings_);
    external_timings_.Reset();
    return report;
}
void ATestBatchOrchestrator::persist_finalized_telemetry_run() {
    if (GIsAutomationTesting) {
        return;
    }
    auto finalized_report{take_finalized_telemetry_report()};
    if (!finalized_report.IsSet()) {
        return;
    }
    auto const& report{finalized_report.GetValue()};
    auto const run_id{report.metadata.run_id};
    auto const path{write_level_telemetry_run(report, level_telemetry_runs_directory())};
    if (path) {
        UE_LOG(LogSandbox, Display, TEXT("Wrote level telemetry run to '%s'"), **path);
        handle_telemetry_persisted(run_id, {});
    } else {
        UE_LOG(LogSandbox, Error, TEXT("Failed to write level telemetry run: %s"), *path.error());
        handle_telemetry_persisted(run_id, path.error());
    }
}
void ATestBatchOrchestrator::record_external_timing(int32 const window_index,
                                                    ELevelTelemetryTimingSystem const system,
                                                    double const seconds) {
    external_timings_.Add({window_index, system, seconds});
}
