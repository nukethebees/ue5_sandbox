#include "SpaceGame/simulation/TestBatchOrchestrator.h"

#include "SpaceGame/levels/LevelLoader.h"
#include "SpaceGame/system/GameSubsystem.h"

#include <SandboxGameShared/utilities/actor_utils.h>
#include <SpaceGame/defences/spinners/TestTubeSpinnerProxy.h>
#include <SpaceGame/defences/turrets/TestStaticTurretsProxy.h>
#include <SpaceGame/entities/TestEntity.h>
#include <SpaceGame/entities/TestEntityRegistry.h>
#include <SpaceGame/missions/TestMissionManager.h>
#include <SpaceGame/presentation/HUDManager.h>
#include <SpaceGame/ships/capital/TestCapitalShipProxy.h>
#include <SpaceGame/ships/player/TestSpaceShip.h>
#include <SpaceGame/simulation/CollisionGridVisualizationComponent.h>
#include <SpaceGame/simulation/LevelSimulationBuilder.h>
#include <SpaceGame/support/logging/SandboxLogCategories.h>
#include <SpaceGame/support/mesh.h>
#include <SpaceGame/telemetry/LevelTelemetryMetadata.h>

#include <SandboxCore/array_utils.h>
#include <SandboxCore/invoke.h>
#include <SandboxCore/soa_rotator_utils.h>
#include <SandboxCore/soa_vector_utils.h>
#include <SandboxCoreEngine/actor_utils.h>
#include <SandboxCoreEngine/uobject_utils.h>
#include <SandboxISMCComponent.h>

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
#include <Kismet/GameplayStatics.h>
#include <Misc/DateTime.h>
#include <SpaceGame/persistence/SpaceSaveGame.h>
#include <SpaceGame/persistence/SpaceSaveSubsystem.h>
#include <VisualLogger/VisualLogger.h>

#if WITH_EDITOR
#include <Editor.h>
#endif

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

ATestBatchOrchestrator::ATestBatchOrchestrator() {
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;

    collision_grid_visualization = CreateDefaultSubobject<UCollisionGridVisualizationComponent>(
        TEXT("CollisionGridVisualization"));
    RootComponent = collision_grid_visualization;
    laser_instances_ = CreateDefaultSubobject<USandboxISMCComponent>(TEXT("Lasers"));
    laser_instances_->SetupAttachment(RootComponent);
    capital_instances_ =
        CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("CapitalShips"));
    capital_instances_->SetupAttachment(RootComponent);
    fighter_instances_ = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Fighters"));
    fighter_instances_->SetupAttachment(RootComponent);
    turret_instances_ = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Turrets"));
    turret_instances_->SetupAttachment(RootComponent);
    spinner_instances_ = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Spinners"));
    spinner_instances_->SetupAttachment(RootComponent);

    ml::set_actor_component_mobility(*this, EComponentMobility::Static);
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
    level_simulation_.Reset();
    level_definition_.Reset();
    world_collision_.restore_collision();
    SetActorTickEnabled(false);
    stop_visual_logging();
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
    start_visual_logging();
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
    stop_visual_logging();
    clear_end_tick_test_hook();
    hud_manager.deactivate();
    if (IsValid(player_ship)) {
        player_ship->unbind_simulation();
    }
    level_simulation_.Reset();
    level_definition_.Reset();
    launched_paused_ = false;
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
        set_actor_config_on_all<ATestCapitalShipProxy>(*world, &config.capital_ships);
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
        result->telemetry_metadata =
            make_level_telemetry_run_metadata(world, mission_definition, presentation_enabled);
        level_simulation_.Emplace(MoveTemp(result.value()),
                                  presentation_enabled ? &presentation : nullptr);
        validate_proxy_handles();
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

            ml::assign(new_locations, i, transform.GetLocation());
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
    data.telemetry_metadata =
        make_level_telemetry_run_metadata(world, mission_definition, presentation_enabled);
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
    bind_and_destroy_proxies();
    world_collision_.initialise_static_geometry(
        *world, level_config->collision_grid, get_spatial_query_manager().get_collision_system());
    level_simulation_->finish_initialisation();
    update_collision_bounds_visualization();
    level_simulation_->on_mission_evaluated = [this] { process_mission_result(); };
    level_simulation_->on_end_tick = [this](FLevelSimulation&) {
        end_tick_test_hook.ExecuteIfBound(*this);
        process_mission_result();
    };
    if (presentation_enabled) {
        hud_manager.initialise(hud_update_frequencies,
                               get_mission_manager(),
                               get_entity_registry(),
                               hud_tick_loop.tick_rate,
                               get_player_ship_simulation(),
                               *level_config,
                               presentation_settings.entity_overlay);
    }
    bool const automatic{
        start_mode == EOrchestratorStartMode::Automatic ||
        start_mode == EOrchestratorStartMode::AuthoredLevel ||
        (start_mode == EOrchestratorStartMode::PausedInTest && !GIsAutomationTesting)};
    if (automatic) {
        level_simulation_->start();
        start_visual_logging();
    }
    SetActorTickEnabled(automatic);
    return true;
}

void ATestBatchOrchestrator::handle_level_start_failure(FString message) {
    SetActorTickEnabled(false);
    stop_visual_logging();
    UE_LOG(LogSandbox, Error, TEXT("%s"), *message);

    auto* const world{GetWorld()};
#if WITH_EDITOR
    if (!GIsAutomationTesting && IsValid(world) && world->WorldType == EWorldType::PIE && GEditor) {
        GEditor->RequestEndPlayMap();
        return;
    }
#endif
    if (GIsAutomationTesting) {
        return;
    }

    auto* const game_instance{IsValid(world) ? world->GetGameInstance() : nullptr};
    auto* const subsystem{
        IsValid(game_instance) ? game_instance->GetSubsystem<ml::ioj::UGameSubsystem>() : nullptr};
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
    auto* const subsystem{
        IsValid(game_instance) ? game_instance->GetSubsystem<ml::ioj::UGameSubsystem>() : nullptr};
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

    launched_paused_ = pending->launch_mode == ml::ioj::ELevelLaunchMode::Paused;

    ml::FLevelLoader loader{*this};
    auto const result{loader.load(pending->definition)};
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

void ATestBatchOrchestrator::start_visual_logging() {
#if ENABLE_VISUAL_LOG
    if (!presentation_enabled || !enable_visual_logging) {
        return;
    }

    auto& visual_logger{FVisualLogger::Get()};
    visual_logger.SetGetTimeStampFunc([this](UObject const*) { return get_simulation_time(); });
    visual_logger.SetIsRecording(true);
#endif
}
void ATestBatchOrchestrator::stop_visual_logging() {
#if ENABLE_VISUAL_LOG
    if (!enable_visual_logging) {
        return;
    }

    auto& visual_logger{FVisualLogger::Get()};
    if (FVisualLogger::IsRecording()) {
        visual_logger.SetIsRecording(false);
    }
    visual_logger.SetGetTimeStampFunc(TFunction<double(UObject const*)>{});
#endif
}

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

    collision_grid_visualization->configure(level_config->collision_grid);
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
        get_entity_registry(), get_spatial_query_manager().get_collision_system());
}

void ATestBatchOrchestrator::validate_proxy_handles() {
    if (auto const* player{get_player_ship_simulation()}) {
        check(get_entity_registry().is_valid_handle(player->registry_handle));
    }
    get_capital_ships()->validate_proxy_handles();
    get_turrets()->validate_proxy_handles();
}

void ATestBatchOrchestrator::Tick(float dt) {
    Super::Tick(dt);

    tick(static_cast<time_type>(dt));
}
void ATestBatchOrchestrator::tick(time_type const dt) {
    if (get_state() != EOrchestratorState::Running) {
        return;
    }
    level_simulation_->advance(dt);
    update_collision_bounds_visualization();
    if (presentation_enabled) {
        hud_tick_loop.add_time(dt);
        while (hud_tick_loop.try_tick()) {
            hud_manager.tick(1);
        }
    }
    level_simulation_->commit_presentation(dt);
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

auto ATestBatchOrchestrator::make_presentation_resources() const -> FLevelPresentationResources {
    return {.lasers = laser_instances_,
            .capital_ships = capital_instances_,
            .fighters = fighter_instances_,
            .turrets = turret_instances_,
            .spinners = spinner_instances_,
            .config = level_config,
            .player = player_ship,
            .settings = presentation_settings};
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
void ATestBatchOrchestrator::process_mission_result() {
    auto result{level_simulation_->take_mission_result()};
    if (!result.IsSet()) {
        return;
    }
    bool persisted{};
    if (result->save_results) {
        auto* game_instance{GetGameInstance()};
        auto* saves{IsValid(game_instance) ? game_instance->GetSubsystem<USpaceSaveSubsystem>()
                                           : nullptr};
        if (IsValid(saves)) {
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
        } else {
            UE_LOG(LogSandbox,
                   Error,
                   TEXT("Cannot persist mission result: save subsystem is unavailable"));
        }
    }
    on_mission_completed.Broadcast({.level_id = result->level_id,
                                    .level_display_name = result->level_display_name,
                                    .state = result->state,
                                    .persisted = persisted});
}
