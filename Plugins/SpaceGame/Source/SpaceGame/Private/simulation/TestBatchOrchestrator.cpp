#include "SpaceGame/simulation/TestBatchOrchestrator.h"

#include <SandboxGameShared/utilities/actor_utils.h>
#include <SpaceGame/settings/LevelSettingsFile.h>
#include <SpaceGame/ships/player/SpaceGamePlayerController.h>
#include <SpaceGame/ships/player/TestSpaceShip.h>
#include <SpaceGame/simulation/LevelCollisionHost.h>
#include <SpaceGame/simulation/LevelSimulationBuilder.h>
#include <SpaceGame/simulation/LevelWorldReset.h>
#include <SpaceGame/telemetry/LevelTelemetryIntegration.h>
#include <SpaceGameRendering/SparkRendererComponent.h>
#include <SpaceGameSimulation/support/logging/SandboxLogCategories.h>

#include <SandboxCoreEngine/actor_utils.h>
#include <SandboxCoreEngine/uobject_utils.h>

#include <Components/InstancedStaticMeshComponent.h>
#include <CoreGlobals.h>
#include <Engine/World.h>
#include <Kismet/GameplayStatics.h>
#include <SandboxISMCComponent.h>

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
    fighter_instances_ = CreateDefaultSubobject<USandboxISMCComponent>(TEXT("Fighters"));
    fighter_instances_->SetupAttachment(RootComponent);
    turret_instances_ = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Turrets"));
    turret_instances_->SetupAttachment(RootComponent);
    spinner_instances_ = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Spinners"));
    spinner_instances_->SetupAttachment(RootComponent);
    soft_target_instances_ =
        CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("SoftTargets"));
    soft_target_instances_->SetupAttachment(RootComponent);

    ioj::set_actor_component_mobility(*this, EComponentMobility::Static);
    laser_instances_->SetMobility(EComponentMobility::Movable);
    fighter_instances_->SetMobility(EComponentMobility::Movable);
    soft_target_instances_->SetMobility(EComponentMobility::Movable);
    bind_presentation_components();
}
void ATestBatchOrchestrator::PostLoad() {
    Super::PostLoad();
    bind_presentation_components();
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
    release_level(ioj::sim::LevelTelemetryRunEndReason::WorldEnd);
    Super::EndPlay(end_play_reason);
}
void ATestBatchOrchestrator::release_level(ioj::sim::LevelTelemetryRunEndReason const reason) {
    SetActorTickEnabled(false);
    if (level_simulation_.IsSet()) {
        level_simulation_->finalize_telemetry_run(reason);
    }
    if (IsValid(player_ship)) {
        player_ship->unbind_simulation();
    }
    presentation_host_.reset();
    if (level_simulation_.IsSet()) {
        ioj::persist_finalized_level_telemetry_run(
            *level_simulation_, launch_options_, presentation_enabled, GetGameInstance());
    }

    level_simulation_.Reset();
    prepared_level_.Reset();
}
void ATestBatchOrchestrator::start_simulation() {
    if (!level_simulation_.IsSet() && !begin_play()) {
        return;
    }
    if (get_state() != ioj::sim::OrchestratorState::Paused) {
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

    auto* const world{GetWorld()};
    if (!IsValid(world)) {
        UE_LOG(LogSandbox, Fatal, TEXT("Cannot reset level: world is invalid"));
        return;
    }
    if (level_simulation_.IsSet()) {
        simulation_tick_settings.time_scale = level_simulation_->get_clock().get_time_scale();
    }

    release_level(ioj::sim::LevelTelemetryRunEndReason::OrchestratorReset);
    launch_options_ = {};
    ioj::reset_level_world(*world,
                           *this,
                           player_ship,
                           mission_definition,
                           IsValid(level_config) ? &level_config->player_ship : nullptr);

    if (should_initialise_in_begin_play()) {
        begin_play();
    }
    on_reset.Broadcast(*this);
}

/* **************************************** */
// Level configuration
/* **************************************** */
auto ATestBatchOrchestrator::set_level_config(USpaceGameLevelConfig& config) -> bool {
    if (level_simulation_.IsSet()) {
        UE_LOG(LogSandbox,
               Error,
               TEXT("Reset the level simulation before replacing its configuration"));
        return false;
    }
    if (!ensureAlwaysMsgf(config.is_valid(presentation_enabled), TEXT("Level config is invalid"))) {
        return false;
    }

#if WITH_EDITOR
    if (auto const* const world{GetWorld()}; IsValid(world) && !world->IsGameWorld()) {
        Modify();
    }
#endif

    level_config = &config;
    prepared_level_.Reset();
    refresh_collision_grid_visualization();
    if (auto* const world{GetWorld()}; IsValid(world)) {
        ioj::apply_level_actor_configuration(*world, config, player_ship.Get());
    } else if (IsValid(player_ship)) {
        ioj::configure_player_actor(*player_ship, config);
    }
    return true;
}
void ATestBatchOrchestrator::set_start_mode(EOrchestratorStartMode const mode) {
    if (get_state() != ioj::sim::OrchestratorState::Uninitialised) {
        UE_LOG(LogSandbox, Error, TEXT("Cannot change start mode after simulation initialisation"));
        return;
    }
    start_mode = mode;
}
void ATestBatchOrchestrator::set_presentation_enabled(bool const enabled) {
    if (get_state() != ioj::sim::OrchestratorState::Uninitialised) {
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
auto ATestBatchOrchestrator::get_player_ship_simulation() const noexcept
    -> ioj::sim::player::Sim const* {
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
// Level preparation and startup
/* **************************************** */
auto ATestBatchOrchestrator::prepare_level(ioj::levels::authoring::LevelSource const* const source)
    -> bool {
    check(get_state() == ioj::sim::OrchestratorState::Uninitialised);
    if (prepared_level_.IsSet() && !source) {
        return true;
    }

    auto* const world{GetWorld()};
    ioj::fatal_if_uobject_ptrs_invalid(
        {SANDBOX_NAMED_UOBJECT_PTR(world), SANDBOX_NAMED_UOBJECT_PTR(level_config)});
    auto loaded{read_game_settings(settings_file.FilePath, source, *level_config, this)};
    if (!loaded) {
        handle_level_start_failure(loaded.error());
        return false;
    }
    if (!set_level_config(*loaded->config)) {
        return false;
    }

    simulation_tick_settings = loaded->simulation.clock;
    if (start_mode == EOrchestratorStartMode::AuthoredLevel) {
        simulation_tick_settings.time_scale =
            ioj::PositiveFinite<time_type>::make(launch_options_.requested_time_scale);
    }
    presentation_host_.get_hud_manager().configure(loaded->simulation.hud);
    prepared_level_.Emplace(MoveTemp(*loaded));

    if (!IsValid(player_ship)) {
        player_ship = ioj::get_first_actor<ATestSpaceShip>(*world);
        if (IsValid(player_ship)) {
            ioj::configure_player_actor(*player_ship, *level_config);
        }
    }
    return true;
}
auto ATestBatchOrchestrator::load_level(ioj::levels::authoring::LevelSource const& source,
                                        EPlayerControlContext const playerless_control_context)
    -> std::expected<void, ioj::FLevelLoadErrors> {
    auto const fail{[](ioj::ELevelLoadErrorCode const code,
                       FString message) -> std::expected<void, ioj::FLevelLoadErrors> {
        ioj::FLevelLoadErrors errors;
        errors.errors.Add({.code = code, .message = MoveTemp(message)});
        return std::unexpected{MoveTemp(errors)};
    }};
    if (get_state() != ioj::sim::OrchestratorState::Uninitialised) {
        return fail(ioj::ELevelLoadErrorCode::InvalidOrchestratorState,
                    TEXT("Level loader requires an uninitialised orchestrator"));
    }
    auto* const world{GetWorld()};
    if (!IsValid(world)) {
        return fail(ioj::ELevelLoadErrorCode::InvalidWorld, TEXT("Orchestrator world is invalid"));
    }
    if (!prepare_level(&source)) {
        return fail(ioj::ELevelLoadErrorCode::InvalidLevelConfig,
                    TEXT("Failed to resolve game settings"));
    }

    std::optional<ioj::FLevelVisualLoad> visual;
    if (presentation_enabled) {
        auto* const controller{
            Cast<ASpaceGamePlayerController>(UGameplayStatics::GetPlayerController(world, 0))};
        if (!IsValid(controller)) {
            return fail(ioj::ELevelLoadErrorCode::MissingInfrastructure,
                        TEXT("Authored level requires a space-game player controller"));
        }
        if (!get_presentation_resources().is_valid()) {
            return fail(ioj::ELevelLoadErrorCode::MissingInfrastructure,
                        TEXT("Orchestrator simulation infrastructure is incomplete"));
        }
        visual.emplace(*controller, playerless_control_context);
    }

    return ioj::FLevelLoader::load({.world = *world,
                                    .config = *level_config,
                                    .definition = get_level_definition(),
                                    .player = player_ship,
                                    .visual = visual});
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
    if (!prepared_level_.IsSet() && !prepare_level()) {
        return false;
    }

    auto const authored{prepared_level_->level.has_value()};
    auto build{ioj::prepare_level_simulation({
        .world = *world,
        .config = *level_config,
        .clock_settings = simulation_tick_settings,
        .mission = mission_definition,
        .launch_options = launch_options_,
        .level_source = prepared_level_->level_source,
        .settings_source = prepared_level_->source,
        .definition = authored ? &*prepared_level_->level : nullptr,
        .player = player_ship.Get(),
        .presentation_enabled = presentation_enabled,
    })};
    if (!build) {
        handle_level_start_failure(
            FString::Printf(TEXT("Cannot start level:\n%s"), *build.error().format()));
        return false;
    }

    auto const resources{get_presentation_resources()};
    if (presentation_enabled && !resources.is_valid()) {
        handle_level_start_failure(
            TEXT("Cannot start level: Level presentation resources are incomplete"));
        return false;
    }
    auto* const game_subsystem{ioj::UGameSubsystem::get(GetGameInstance())};
    if (!IsValid(game_subsystem)) {
        handle_level_start_failure(TEXT("Cannot create level simulation without game memory"));
        return false;
    }

    auto const grid_geometry{build->data.collision.grid_geometry};
    level_simulation_.Emplace(game_subsystem->get_game_memory(), MoveTemp(build->data));
    if (IsValid(player_ship) && get_player_ship_simulation()) {
        player_ship->bind_simulation(*level_simulation_->get_player_ship_commands(),
                                     *get_player_ship_simulation());
    }

    if (!authored) {
        auto const proxy_entities{build->bind_proxy_entities(*level_simulation_)};
        on_proxy_entities_bound.Broadcast(proxy_entities);
        build->destroy_proxy_actors();
    }
    auto const static_bounds{ioj::FLevelCollisionHost::extract_static_geometry(
        *world, level_config->collision_grid, grid_geometry)};
    level_simulation_->set_static_collision(static_bounds);
    level_simulation_->finish_initialisation();

    if (presentation_enabled) {
        presentation_host_.initialise(resources.config,
                                      resources.player,
                                      level_simulation_->get_read_access(),
                                      get_mission(),
                                      get_spatial_query_manager(),
                                      MoveTemp(build->initial_turret_transforms));
        if (IsValid(player_ship)) {
            if (auto* const controller{
                    Cast<ASpaceGamePlayerController>(player_ship->GetController())};
                IsValid(controller)) {
                controller->activate_ship_control();
            }
        }
    }
    prepared_level_.Reset();

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
    ioj::FLevelLoader::report_failure(GetWorld(), MoveTemp(message));
}
void ATestBatchOrchestrator::load_authored_level() {
    auto pending{ioj::FLevelLoader::take_pending(GetGameInstance())};
    if (!pending) {
        handle_level_start_failure(pending.error());
        return;
    }
    launch_options_ = pending->options;
    presentation_enabled = launch_options_.presentation_mode == ioj::ELevelPresentationMode::Visual;

    auto const loaded{load_level(pending->source, launch_options_.control_context)};
    if (!loaded) {
        handle_level_start_failure(loaded.error().format(pending->source_path));
        return;
    }
    if (begin_play() && launch_options_.launch_mode == ioj::ELevelLaunchMode::Paused) {
        pause_simulation();
    }
}
auto ATestBatchOrchestrator::should_initialise_in_begin_play() const noexcept -> bool {
    return start_mode == EOrchestratorStartMode::Automatic ||
           (start_mode == EOrchestratorStartMode::PausedInTest && !GIsAutomationTesting);
}

/* **************************************** */
// Presentation integration
/* **************************************** */
void ATestBatchOrchestrator::bind_presentation_components() {
    presentation_host_.bind_components({.lasers = laser_instances_,
                                        .capital_ships = capital_instances_,
                                        .fighters = fighter_instances_,
                                        .turrets = turret_instances_,
                                        .spinners = spinner_instances_,
                                        .sparks = spark_renderer_,
                                        .soft_targets = soft_target_instances_,
                                        .collision_grid = collision_grid_visualization});
}
void ATestBatchOrchestrator::refresh_collision_grid_visualization() {
    TOptional<FCollisionGridVisualizationSettings> settings;
    if (presentation_enabled && IsValid(level_config)) {
        auto const& collision_grid{level_config->collision_grid};
        settings.Emplace(FCollisionGridVisualizationSettings{
            .dimensions = collision_grid.calculate_grid_dimensions(),
            .cell_size = collision_grid.cell_size,
            .line_colour = collision_grid.line_colour,
            .line_thickness = collision_grid.line_thickness,
            .show_grid = collision_grid.show_grid,
        });
    }
    presentation_host_.configure_collision_grid(MoveTemp(settings));
}
auto ATestBatchOrchestrator::get_presentation_resources() const -> FLevelPresentationResources {
    return presentation_host_.make_resources(
        level_config->get_visual_config(),
        IsValid(player_ship)
            ? TOptional<FPlayerPresentationResources>{player_ship->get_presentation_resources()}
            : NullOpt);
}

/* **************************************** */
// Simulation state
/* **************************************** */
void ATestBatchOrchestrator::Tick(float dt) {
    Super::Tick(dt);
    tick(static_cast<time_type>(dt));
}
void ATestBatchOrchestrator::tick(time_type const dt) {
    if (get_state() != ioj::sim::OrchestratorState::Running) {
        return;
    }

    auto const previous_tick{level_simulation_->get_clock().get_completed_ticks()};
    level_simulation_->advance(dt);
    presentation_host_.tick(dt, level_simulation_->get_read_access(), get_spatial_query_manager());

    if (auto player{level_simulation_->get_player_ship_commands()};
        player && player->consume_death_notification()) {
        if (IsValid(player_ship)) {
            player_ship->handle_simulation_death();
        }
    }

    if (level_simulation_->get_clock().get_completed_ticks() != previous_tick) {
        ioj::publish_mission_completion(
            *level_simulation_, GetGameInstance(), on_mission_completed);
        level_simulation_->stop_run_if_requested({
            .stop_when_battle_resolved = launch_options_.stop_when_battle_resolved,
            .duration = launch_options_.simulated_duration_seconds.IsSet()
                          ? std::optional{ioj::Duration::make(
                                launch_options_.simulated_duration_seconds.GetValue())}
                          : std::nullopt,
        });
    }
    ioj::persist_finalized_level_telemetry_run(
        *level_simulation_, launch_options_, presentation_enabled, GetGameInstance());
}
auto ATestBatchOrchestrator::duration_to_tick_period(ioj::Duration const duration) const noexcept
    -> tick_type {
    auto const tick_rate{level_simulation_.IsSet() ? level_simulation_->get_clock().get_tick_rate()
                                                   : simulation_tick_settings.tick_rate};
    return ioj::sim::duration_to_tick_period(tick_rate, duration);
}

/* **************************************** */
// Editor commands
/* **************************************** */
#if WITH_EDITOR
void ATestBatchOrchestrator::apply_level_config() {
    if (!IsValid(level_config)) {
        UE_LOG(LogSandbox, Error, TEXT("Cannot apply a null level config."));
        return;
    }
    set_level_config(*level_config);
}
void ATestBatchOrchestrator::create_level_settings() {
    auto const path{ioj::create_level_settings_file(GetWorld())};
    if (!path.IsSet()) {
        return;
    }

    Modify();
    settings_file.FilePath = path.GetValue();
    prepared_level_.Reset();
    MarkPackageDirty();
    UE_LOG(LogSandbox, Display, TEXT("Assigned level settings '%s'."), *path.GetValue());
}
void ATestBatchOrchestrator::prepare_level_button() {
    prepare_level();
}
#endif
