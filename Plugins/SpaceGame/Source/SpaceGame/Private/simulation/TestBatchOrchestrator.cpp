#include "SpaceGame/simulation/TestBatchOrchestrator.h"

#include <ioj/levels/authoring/definition_reader.h>
#include <ioj/levels/launch_options.h>
#include <ioj/profiles/level_score.h>
#include <SandboxGameShared/utilities/actor_utils.h>
#include <SpaceGame/persistence/SpaceSaveSubsystem.h>
#include <SpaceGame/s7/GameSettingsReader.h>
#include <SpaceGame/ships/player/SpaceGamePlayerController.h>
#include <SpaceGame/ships/player/TestSpaceShip.h>
#include <SpaceGame/simulation/LevelCollisionHost.h>
#include <SpaceGame/simulation/LevelSimulationBuilder.h>
#include <SpaceGame/simulation/LevelWorldReset.h>
#include <SpaceGameRendering/SparkRendererComponent.h>
#include <SpaceGameSimulation/support/logging/SandboxLogCategories.h>

#include <SandboxCoreEngine/strings.h>

#include <Components/InstancedStaticMeshComponent.h>
#include <CoreGlobals.h>
#include <Engine/World.h>
#include <Kismet/GameplayStatics.h>
#include <Misc/Paths.h>
#include <SandboxISMCComponent.h>

namespace ioj {
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

    set_actor_component_mobility(*this, EComponentMobility::Static);
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
    end_level(sim::RunEndReason::WorldEnd, false);
    Super::EndPlay(end_play_reason);
}
void ATestBatchOrchestrator::release_level() {
    SetActorTickEnabled(false);
    if (IsValid(player_ship)) {
        player_ship->unbind_simulation();
    }
    presentation_host_.reset();
    if (level_simulation_.IsSet()) {
        level_simulation_.Reset();
        auto* const game_subsystem{UGameSubsystem::get(GetGameInstance())};
        check(game_subsystem);
        game_subsystem->reset_level_memory();
    }
}
void ATestBatchOrchestrator::start_simulation() {
    if (get_state() != sim::LevelState::Paused) {
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
    end_level(sim::RunEndReason::OrchestratorReset, false);
    launch_options_ = {.presentation_mode = launch_options_.presentation_mode};
    clear_player_ship();
    reset_level_world(*world, *this);

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
    if (!ensureAlwaysMsgf(config.is_valid(is_presentation_enabled()),
                          TEXT("Level config is invalid"))) {
        return false;
    }

#if WITH_EDITOR
    if (auto const* const world{GetWorld()}; IsValid(world) && !world->IsGameWorld()) {
        Modify();
    }
#endif

    level_config = &config;
    refresh_collision_grid_visualization();
    if (IsValid(player_ship)) {
        configure_player_actor(*player_ship, config);
    }
    return true;
}
void ATestBatchOrchestrator::set_level_script(FString path) {
    level_script.FilePath = FPaths::ConvertRelativePathToFull(path);
    FPaths::MakePathRelativeTo(level_script.FilePath, *FPaths::ProjectDir());
}
void ATestBatchOrchestrator::set_start_mode(EOrchestratorStartMode const mode) {
    if (get_state() != sim::LevelState::Uninitialised) {
        UE_LOG(LogSandbox, Error, TEXT("Cannot change start mode after simulation initialisation"));
        return;
    }
    start_mode = mode;
}
void ATestBatchOrchestrator::set_presentation_enabled(bool const enabled) {
    if (get_state() != sim::LevelState::Uninitialised) {
        UE_LOG(
            LogSandbox, Error, TEXT("Cannot change presentation after simulation initialisation"));
        return;
    }
    launch_options_.presentation_mode =
        enabled ? levels::PresentationMode::Visual : levels::PresentationMode::SimulationOnly;
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
auto ATestBatchOrchestrator::load_level(levels::LevelDefinition const& definition,
                                        levels::ControlContext const playerless_control_context)
    -> std::expected<void, FLevelLoadErrors> {
    auto const fail{[](ELevelLoadErrorCode const code,
                       FString message) -> std::expected<void, FLevelLoadErrors> {
        FLevelLoadErrors errors;
        errors.errors.Add({.code = code, .message = MoveTemp(message)});
        return std::unexpected{MoveTemp(errors)};
    }};
    if (get_state() != sim::LevelState::Uninitialised) {
        return fail(ELevelLoadErrorCode::InvalidLevelState,
                    TEXT("Level loader requires an uninitialised orchestrator"));
    }
    auto* const world{GetWorld()};
    if (!IsValid(world)) {
        return fail(ELevelLoadErrorCode::InvalidWorld, TEXT("Orchestrator world is invalid"));
    }
    if (!IsValid(level_config)) {
        return fail(ELevelLoadErrorCode::InvalidLevelConfig,
                    TEXT("Level configuration is invalid"));
    }

    auto resolved{prepare_level_resources(definition.settings(), *level_config, this)};
    if (!resolved) {
        return fail(ELevelLoadErrorCode::InvalidLevelConfig, std::move(resolved.error()));
    }
    if (!set_level_config(**resolved)) {
        return fail(ELevelLoadErrorCode::InvalidLevelConfig,
                    TEXT("Failed to resolve game settings"));
    }

    auto clock_settings{definition.settings().tick};
    if (start_mode == EOrchestratorStartMode::AuthoredLevel) {
        clock_settings.time_scale =
            PositiveFinite<time_type>::make(launch_options_.requested_time_scale);
    }
    presentation_host_.get_hud_manager().configure(definition.settings().hud);

    std::optional<FLevelVisualLoad> visual;
    if (is_presentation_enabled()) {
        auto* const controller{
            Cast<ASpaceGamePlayerController>(UGameplayStatics::GetPlayerController(world, 0))};
        if (!IsValid(controller)) {
            return fail(ELevelLoadErrorCode::MissingInfrastructure,
                        TEXT("Authored level requires a space-game player controller"));
        }
        if (!get_presentation_resources().is_valid()) {
            return fail(ELevelLoadErrorCode::MissingInfrastructure,
                        TEXT("Orchestrator simulation infrastructure is incomplete"));
        }
        visual.emplace(*controller, playerless_control_context);
    }

    auto loaded{FLevelLoader::load({.world = *world,
                                    .config = *level_config,
                                    .definition = definition,
                                    .player = player_ship,
                                    .visual = visual})};
    if (!loaded) {
        return loaded;
    }

    auto const& options{launch_options_};
    auto build{prepare_level_simulation({
        .config = *level_config,
        .clock_settings = clock_settings,
        .launch_options = options,
        .definition = definition,
        .player = player_ship.Get(),
    })};
    if (!build) {
        return fail(ELevelLoadErrorCode::InvalidDefinition, build.error().format());
    }

    auto configured{configure_level_run(*build, options)};
    if (!configured) {
        return fail(ELevelLoadErrorCode::InvalidDefinition, configured.error().format());
    }

    auto const resources{get_presentation_resources()};
    if (is_presentation_enabled() && !resources.is_valid()) {
        return fail(ELevelLoadErrorCode::MissingInfrastructure,
                    TEXT("Level presentation resources are incomplete"));
    }
    auto* const game_subsystem{UGameSubsystem::get(GetGameInstance())};
    if (!IsValid(game_subsystem)) {
        return fail(ELevelLoadErrorCode::MissingInfrastructure,
                    TEXT("Cannot create level simulation without game memory"));
    }

    auto const static_bounds{FLevelCollisionHost::extract_static_geometry(
        *world, level_config->collision_grid, build->collision.grid_geometry)};
    build->collision.static_bounds.reset();
    for (auto const& bounds : static_bounds) {
        build->collision.static_bounds.add(bounds.min, bounds.max);
    }
    level_simulation_.Emplace(MoveTemp((*build)), game_subsystem->get_memory_resource());
    auto const* const player_simulation{level_simulation_->player_ship_simulation()};
    if (IsValid(player_ship) && player_simulation) {
        player_ship->bind_simulation(*level_simulation_->player_ship_commands(),
                                     *player_simulation);
    }

    if (is_presentation_enabled()) {
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
    if (level_simulation_->result()) {
        end_level(sim::RunEndReason::LevelCompleted);
        return {};
    }
    SetActorTickEnabled(true);
    return {};
}
void ATestBatchOrchestrator::handle_level_start_failure(FString message) {
    SetActorTickEnabled(false);
    FLevelLoader::report_failure(GetWorld(), MoveTemp(message));
}
void ATestBatchOrchestrator::load_authored_level() {
    auto* const game{UGameSubsystem::get(GetGameInstance())};
    check(game);
    auto pending{game->take_pending_level()};
    if (!pending && !level_script.FilePath.IsEmpty()) {
        auto const path{
            FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), level_script.FilePath)};
        auto definition{
            levels::authoring::DefinitionReader{}.read_level_file(std::filesystem::path{*path})};
        if (!definition) {
            handle_level_start_failure(to_fstring(levels::format_diagnostics(definition.error())));
            return;
        }
        levels::LaunchOptions options;
        options.control_context = definition->camera() ? levels::ControlContext::Observer
                                                       : levels::ControlContext::Player;
        pending.emplace(session::PendingLevel{std::move(*definition), to_string(path), options});
    }
    if (!pending) {
        handle_level_start_failure(
            TEXT("No Scheme level entry point or prepared level was provided."));
        return;
    }
    launch_options_ = pending->options;

    auto const loaded{load_level(pending->definition, launch_options_.control_context)};
    if (!loaded) {
        handle_level_start_failure(loaded.error().format(to_fstring(pending->source_path)));
        return;
    }
    if (!level_simulation_.IsSet()) {
        return;
    }
    if (!is_presentation_enabled()) {
        auto* const controller{
            Cast<ASpaceGamePlayerController>(UGameplayStatics::GetPlayerController(GetWorld(), 0))};
        if (!IsValid(controller) || !controller->activate_run_controls()) {
            handle_level_start_failure(TEXT("Failed to activate simulation-only run controls"));
            return;
        }
    }
    if (launch_options_.launch_mode == levels::LaunchMode::Running) {
        start_simulation();
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
    if (is_presentation_enabled() && IsValid(level_config)) {
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
    if (state == sim::LevelState::Stopped) {
        end_level(sim::RunEndReason::LevelCompleted);
        return;
    }
    if (state != sim::LevelState::Running && state != sim::LevelState::Paused) {
        return;
    }

    auto const& clk{level_simulation_->clock()};
    auto const previous_tick{clk.get_completed_ticks()};
    level_simulation_->advance(dt);
    if (get_state() == sim::LevelState::Running || clk.get_completed_ticks() != previous_tick) {
        presentation_host_.tick(
            dt, level_simulation_->read_access(), level_simulation_->spatial_query_manager());

        if (auto player{level_simulation_->player_ship_commands()};
            player && player->consume_death_notification()) {
            if (IsValid(player_ship)) {
                player_ship->handle_simulation_death();
            }
        }
    }

    if (level_simulation_->result()) {
        end_level(sim::RunEndReason::LevelCompleted);
        return;
    }
    if (state == sim::LevelState::Running && get_state() == sim::LevelState::Paused) {
        on_simulation_paused.Broadcast();
    }
}
void ATestBatchOrchestrator::end_level(sim::RunEndReason const reason, bool const show_results) {
    if (!level_simulation_.IsSet()) {
        return;
    }
    level_simulation_->stop_run(reason);
    auto completion{make_level_completion(*level_simulation_)};
    auto const terminal_reason{level_simulation_->result()->reason};
    auto const score{
        terminal_reason == sim::RunEndReason::WorldEnd ||
                terminal_reason == sim::RunEndReason::OrchestratorReset
            ? std::optional<profiles::ScoreRecord>{}
            : profiles::make_level_score(*level_simulation_, profiles::current_timestamp())};
    auto* const controller{
        Cast<ASpaceGamePlayerController>(UGameplayStatics::GetPlayerController(GetWorld(), 0))};
    if (IsValid(controller)) {
        controller->finish_level();
    }
    release_level();

    if (score) {
        completion.team_kills = score->kills;
        auto* const saves{USpaceSaveSubsystem::get(GetGameInstance())};
        auto const saved{IsValid(saves) ? saves->profiles().append_score_record(*score)
                                        : profiles::RecordSaveResult{}};
        completion.new_best_time = saved.new_best_time;
        if (!saved.persisted) {
            UE_LOG(LogSandbox, Error, TEXT("Failed to save level score"));
        }
    }
    if (show_results && IsValid(controller)) {
        controller->show_level_completion(completion);
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

} // namespace ioj
