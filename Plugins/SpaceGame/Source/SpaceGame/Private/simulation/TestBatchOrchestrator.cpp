#include "SpaceGame/simulation/TestBatchOrchestrator.h"

#include <ioj/levels/authoring/definition_reader.h>
#include <ioj/levels/launch_options.h>
#include <SandboxGameShared/utilities/actor_utils.h>
#include <SpaceGame/ships/player/SpaceGamePlayerController.h>
#include <SpaceGame/ships/player/TestSpaceShip.h>
#include <SpaceGame/simulation/LevelCollisionHost.h>
#include <SpaceGame/simulation/LevelSimulationBuilder.h>
#include <SpaceGame/simulation/LevelWorldReset.h>
#include <SpaceGame/telemetry/LevelTelemetryIntegration.h>
#include <SpaceGameRendering/SparkRendererComponent.h>
#include <SpaceGameSimulation/support/logging/SandboxLogCategories.h>

#include <SandboxCoreEngine/actor_utils.h>
#include <SandboxCoreEngine/strings.h>
#include <SandboxCoreEngine/uobject_utils.h>

#include <Components/InstancedStaticMeshComponent.h>
#include <CoreGlobals.h>
#include <Engine/World.h>
#include <Kismet/GameplayStatics.h>
#include <Misc/Paths.h>
#include <SandboxISMCComponent.h>

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

    if (start_mode == EOrchestratorStartMode::AuthoredLevel || should_initialise_in_begin_play()) {
        load_authored_level();
    } else {
        SetActorTickEnabled(false);
    }
}
void ATestBatchOrchestrator::EndPlay(EEndPlayReason::Type const end_play_reason) {
    release_level(ioj::sim::telemetry::RunEndReason::WorldEnd);
    Super::EndPlay(end_play_reason);
}
void ATestBatchOrchestrator::release_level(ioj::sim::telemetry::RunEndReason const reason) {
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
        level_simulation_.Reset();
        auto* const game_subsystem{ioj::UGameSubsystem::get(GetGameInstance())};
        check(game_subsystem);
        game_subsystem->reset_level_memory();
    }

    prepared_level_.Reset();
    initialisation_data_.reset();
}
void ATestBatchOrchestrator::start_simulation() {
    if (!level_simulation_.IsSet() && !begin_play()) {
        return;
    }
    if (get_state() != ioj::sim::LevelState::Paused) {
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
}
void ATestBatchOrchestrator::reset_for_new_level() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestBatchOrchestrator::reset_for_new_level);

    auto* const world{GetWorld()};
    if (!IsValid(world)) {
        UE_LOG(LogSandbox, Fatal, TEXT("Cannot reset level: world is invalid"));
        return;
    }
    if (level_simulation_.IsSet()) {
        simulation_tick_settings.time_scale = level_simulation_->clock().time_scale();
    }

    release_level(ioj::sim::telemetry::RunEndReason::OrchestratorReset);
    launch_options_ = {};
    clear_player_ship();
    ioj::reset_level_world(*world, *this);

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
    if (IsValid(player_ship)) {
        ioj::configure_player_actor(*player_ship, config);
    }
    return true;
}
void ATestBatchOrchestrator::set_level_script(FString path) {
    level_script.FilePath = FPaths::ConvertRelativePathToFull(path);
    FPaths::MakePathRelativeTo(level_script.FilePath, *FPaths::ProjectDir());
}
void ATestBatchOrchestrator::set_start_mode(EOrchestratorStartMode const mode) {
    if (get_state() != ioj::sim::LevelState::Uninitialised) {
        UE_LOG(LogSandbox, Error, TEXT("Cannot change start mode after simulation initialisation"));
        return;
    }
    start_mode = mode;
}
void ATestBatchOrchestrator::set_presentation_enabled(bool const enabled) {
    if (get_state() != ioj::sim::LevelState::Uninitialised) {
        UE_LOG(
            LogSandbox, Error, TEXT("Cannot change presentation after simulation initialisation"));
        return;
    }
    presentation_enabled = enabled;
    refresh_collision_grid_visualization();
}

/* **************************************** */
// Player actor
/* **************************************** */
auto ATestBatchOrchestrator::get_player_ship() const -> ATestSpaceShip const* {
    return player_ship.Get();
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
auto ATestBatchOrchestrator::prepare_level(ioj::levels::LevelDefinition definition) -> bool {
    check(get_state() == ioj::sim::LevelState::Uninitialised);
    auto* const world{GetWorld()};
    ioj::fatal_if_uobject_ptrs_invalid(
        {SANDBOX_NAMED_UOBJECT_PTR(world), SANDBOX_NAMED_UOBJECT_PTR(level_config)});
    auto loaded{prepare_level_resources(definition.settings(), *level_config, this)};
    if (!loaded) {
        handle_level_start_failure(loaded.error());
        return false;
    }
    if (!set_level_config(**loaded)) {
        return false;
    }

    simulation_tick_settings = definition.settings().tick;
    if (start_mode == EOrchestratorStartMode::AuthoredLevel) {
        simulation_tick_settings.time_scale =
            ioj::PositiveFinite<time_type>::make(launch_options_.requested_time_scale);
    }
    presentation_host_.get_hud_manager().configure(definition.settings().hud);
    prepared_level_.Emplace(std::move(definition));

    return true;
}
auto ATestBatchOrchestrator::load_level(
    ioj::levels::LevelDefinition definition,
    ::ioj::levels::ControlContext const playerless_control_context)
    -> std::expected<void, ioj::FLevelLoadErrors> {
    auto const fail{[](ioj::ELevelLoadErrorCode const code,
                       FString message) -> std::expected<void, ioj::FLevelLoadErrors> {
        ioj::FLevelLoadErrors errors;
        errors.errors.Add({.code = code, .message = MoveTemp(message)});
        return std::unexpected{MoveTemp(errors)};
    }};
    if (get_state() != ioj::sim::LevelState::Uninitialised) {
        return fail(ioj::ELevelLoadErrorCode::InvalidLevelState,
                    TEXT("Level loader requires an uninitialised orchestrator"));
    }
    auto* const world{GetWorld()};
    if (!IsValid(world)) {
        return fail(ioj::ELevelLoadErrorCode::InvalidWorld, TEXT("Orchestrator world is invalid"));
    }
    if (!prepare_level(std::move(definition))) {
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
    if (!prepared_level_.IsSet()) {
        handle_level_start_failure(
            TEXT("Load a Scheme level definition before starting the simulation."));
        return false;
    }

    auto options{launch_options_};
    options.presentation_mode = presentation_enabled
                                  ? ioj::levels::PresentationMode::Visual
                                  : ioj::levels::PresentationMode::SimulationOnly;
    auto build{initialisation_data_
                   ? ioj::FLevelSimBuildResult{std::in_place, std::move(*initialisation_data_)}
                   : ioj::prepare_level_simulation({
                         .config = *level_config,
                         .clock_settings = simulation_tick_settings,
                         .launch_options = options,
                         .definition = *prepared_level_,
                         .player = player_ship.Get(),
                     })};
    if (!build) {
        handle_level_start_failure(
            FString::Printf(TEXT("Cannot start level:\n%s"), *build.error().format()));
        return false;
    }

    auto configured{ioj::configure_level_run(*build, *world, get_level_definition(), options)};
    if (!configured) {
        handle_level_start_failure(configured.error().format());
        return false;
    }
    build->clock_settings = simulation_tick_settings;

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

    auto const grid_geometry{(*build).collision.grid_geometry};
    level_simulation_.Emplace(MoveTemp((*build)), game_subsystem->get_memory_resource());
    auto const* const player_simulation{level_simulation_->player_ship_simulation()};
    if (IsValid(player_ship) && player_simulation) {
        player_ship->bind_simulation(*level_simulation_->player_ship_commands(),
                                     *player_simulation);
    }

    initialisation_data_.reset();
    auto const static_bounds{ioj::FLevelCollisionHost::extract_static_geometry(
        *world, level_config->collision_grid, grid_geometry)};
    level_simulation_->set_static_collision(static_bounds);
    level_simulation_->finish_initialisation();

    if (presentation_enabled) {
        presentation_host_.initialise(resources.config,
                                      resources.player,
                                      level_simulation_->read_access(),
                                      level_simulation_->spatial_query_manager());
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
    if (level_simulation_->result()) {
        ioj::publish_level_completion(*level_simulation_, GetGameInstance(), on_level_completed);
        ioj::persist_finalized_level_telemetry_run(
            *level_simulation_, launch_options_, presentation_enabled, GetGameInstance());
    }
    if (automatic && launch_options_.launch_mode == ioj::levels::LaunchMode::Running &&
        level_simulation_->state() == ioj::sim::LevelState::Paused) {
        level_simulation_->start();
    }
    SetActorTickEnabled(true);
    return true;
}
void ATestBatchOrchestrator::handle_level_start_failure(FString message) {
    SetActorTickEnabled(false);
    ioj::FLevelLoader::report_failure(GetWorld(), MoveTemp(message));
}
void ATestBatchOrchestrator::load_authored_level() {
    auto* const game{ioj::UGameSubsystem::get(GetGameInstance())};
    check(game);
    auto pending{game->take_pending_level()};
    if (!pending && !level_script.FilePath.IsEmpty()) {
        auto const path{
            FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), level_script.FilePath)};
        auto definition{ioj::levels::authoring::DefinitionReader{}.read_level_file(
            std::filesystem::path{*path})};
        if (!definition) {
            handle_level_start_failure(
                ioj::to_fstring(ioj::levels::format_diagnostics(definition.error())));
            return;
        }
        ioj::levels::LaunchOptions options;
        options.control_context = definition->camera() ? ioj::levels::ControlContext::Observer
                                                       : ioj::levels::ControlContext::Player;
        pending.emplace(
            ioj::session::PendingLevel{std::move(*definition), ioj::to_string(path), options});
    }
    if (!pending) {
        handle_level_start_failure(
            TEXT("No Scheme level entry point or prepared level was provided."));
        return;
    }
    launch_options_ = pending->options;
    presentation_enabled =
        launch_options_.presentation_mode == ioj::levels::PresentationMode::Visual;

    auto const loaded{load_level(std::move(pending->definition), launch_options_.control_context)};
    if (!loaded) {
        handle_level_start_failure(loaded.error().format(ioj::to_fstring(pending->source_path)));
        return;
    }
    if (!presentation_enabled) {
        auto* const controller{
            Cast<ASpaceGamePlayerController>(UGameplayStatics::GetPlayerController(GetWorld(), 0))};
        if (!IsValid(controller) || !controller->activate_run_controls()) {
            handle_level_start_failure(TEXT("Failed to activate simulation-only run controls"));
            return;
        }
    }
    begin_play();
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
    auto const state{get_state()};
    if (state != ioj::sim::LevelState::Running && state != ioj::sim::LevelState::Paused) {
        return;
    }

    auto const& clk{level_simulation_->clock()};
    auto const previous_tick{clk.get_completed_ticks()};
    level_simulation_->advance(dt);
    if (get_state() == ioj::sim::LevelState::Running ||
        clk.get_completed_ticks() != previous_tick) {
        presentation_host_.tick(
            dt, level_simulation_->read_access(), level_simulation_->spatial_query_manager());

        if (auto player{level_simulation_->player_ship_commands()};
            player && player->consume_death_notification()) {
            if (IsValid(player_ship)) {
                player_ship->handle_simulation_death();
            }
        }

        if (auto const result{level_simulation_->result()};
            result && result->terminal.tick > previous_tick) {
            ioj::publish_level_completion(
                *level_simulation_, GetGameInstance(), on_level_completed);
        }
        ioj::persist_finalized_level_telemetry_run(
            *level_simulation_, launch_options_, presentation_enabled, GetGameInstance());
    }

    if (state == ioj::sim::LevelState::Running && get_state() == ioj::sim::LevelState::Paused) {
        on_simulation_paused.Broadcast();
    }
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
#endif
