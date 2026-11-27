#include "SpaceGame/simulation/TestBatchOrchestrator.h"

#include "SpaceGame/UnrealMessages.h"
#if defined(UE_LIBRARY_ENABLED) && UE_LIBRARY_ENABLED
#include "settings/GameHost.h"
#endif
#include <ioj/levels/launch_options.h>
#include <ioj/profiles/level_score.h>
#include <SandboxGameShared/utilities/actor_utils.h>
#include <SpaceGame/ships/player/SpaceGamePlayerController.h>
#include <SpaceGame/ships/player/TestSpaceShip.h>
#include <SpaceGamePresentation/audio/GameAudio.h>
#include <SpaceGameRendering/SparkRendererComponent.h>
#include <SpaceGameSimulation/support/logging/SandboxLogCategories.h>

#include <SandboxCoreEngine/strings.h>
#include <SandboxShaders/GpuStarfield/GpuStarfieldActor.h>

#include "Logging/LogVerbosity.h"
#include <Components/InstancedStaticMeshComponent.h>
#include <CoreGlobals.h>
#include <Engine/GameInstance.h>
#include <Engine/World.h>
#include <EngineUtils.h>
#include <Kismet/GameplayStatics.h>
#include <Misc/Paths.h>
#include <SandboxISMCComponent.h>

#include <expected>
#include <optional>

namespace ioj {
/* **************************************** */
// Lifecycle and simulation control
/* **************************************** */
ATestBatchOrchestrator::ATestBatchOrchestrator() {
    PrimaryActorTick.bCanEverTick = false;

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

#if defined(UE_LIBRARY_ENABLED) && UE_LIBRARY_ENABLED
    attach_level_world(*this);
#endif
}
void ATestBatchOrchestrator::EndPlay(EEndPlayReason::Type const end_play_reason) {
    release_level();
#if defined(UE_LIBRARY_ENABLED) && UE_LIBRARY_ENABLED
    detach_level_world(*this);
#endif
    Super::EndPlay(end_play_reason);
}
void ATestBatchOrchestrator::release_level() {
    if (IsValid(player_ship)) {
        player_ship->clear_player_frame();
    }

    presentation_host_.reset();
    status_.reset();
    level_id_ = NAME_None;
}
void ATestBatchOrchestrator::start_simulation() {
    if (get_state() != sim::LevelState::Paused) {
        UE_LOG(LogSandbox, Error, TEXT("Cannot start a simulation that is not paused"));
        return;
    }

    request_simulation_mode(levels::LaunchMode::Running);
}
void ATestBatchOrchestrator::pause_simulation() {
    if (status_) {
        request_simulation_mode(levels::LaunchMode::Paused);
    }
}

/* **************************************** */
// Level configuration
/* **************************************** */
auto ATestBatchOrchestrator::set_level_config(USpaceGameLevelConfig& config) -> bool {
    if (status_) {
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
        if (IsValid(player_ship)) {
            player_ship->Modify();
        }
    }
#endif

    level_config = &config;
    refresh_collision_grid_visualization();
    return true;
}

/* **************************************** */
// Player actor
/* **************************************** */
auto ATestBatchOrchestrator::get_player_ship() const -> ATestSpaceShip const* {
    return player_ship.Get();
}
void ATestBatchOrchestrator::clear_player_ship() {
    if (IsValid(player_ship)) {
        player_ship->clear_player_frame();
    }

    player_ship = nullptr;
}

/* **************************************** */
// Level preparation and startup
/* **************************************** */
auto ATestBatchOrchestrator::initialize_level(presentation::LevelInit const& level,
                                              presentation::LevelFrame const& frame)
    -> FLevelLoadResult {
    presentation_mode_ = level.setup.presentation_mode;
    auto const playerless_control_context{level.setup.control_context};
    auto const fail{[](ELevelLoadErrorCode const code, FString message) -> FLevelLoadResult {
        FLevelLoadErrors errors;
        errors.errors.Add({.code = code, .message = MoveTemp(message)});
        return FLevelLoadResult{std::unexpect, MoveTemp(errors)};
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

    presentation_host_.get_hud_manager().configure(level.setup.hud);

    std::optional<FLevelVisualLoad> visual;
    if (is_presentation_enabled()) {
        auto* const controller{
            Cast<ASpaceGamePlayerController>(UGameplayStatics::GetPlayerController(world, 0))};
        if (!IsValid(controller)) {
            return fail(ELevelLoadErrorCode::MissingInfrastructure,
                        TEXT("Authored level requires a space-game player controller"));
        }

        if (!presentation_host_.components().is_valid()) {
            return fail(ELevelLoadErrorCode::MissingInfrastructure,
                        TEXT("Orchestrator simulation infrastructure is incomplete"));
        }

        visual.emplace(*controller, playerless_control_context);
    }

    auto loaded{FLevelLoader::load({.world = *world,
                                    .config = *level_config,
                                    .player_frame = frame.player,
                                    .camera = level.setup.camera,
                                    .player = player_ship,
                                    .visual = visual})};
    if (!loaded) {
        return loaded;
    }

    TArray<FString> messages;
    level_config->get_validation_errors(
        messages, is_presentation_enabled(), level.setup.presentation);
    if (!messages.IsEmpty()) {
        return fail(ELevelLoadErrorCode::InvalidLevelConfig, FString::Join(messages, TEXT("\n")));
    }

    if (IsValid(player_ship) && is_presentation_enabled() &&
        !level_config->player_ship.team_visual_data) {
        return fail(ELevelLoadErrorCode::InvalidLevelConfig,
                    TEXT("Missing player ship team visual data"));
    }

    if (is_presentation_enabled()) {
        for (TActorIterator<AGpuStarfieldActor> actor{world}; actor; ++actor) {
            actor->set_starfield_settings(level.setup.starfield);
        }
    }

    auto* const audio{GetGameInstance()->GetSubsystem<UGameAudio>()};
    check(audio);
    status_ = frame.status;
    level_id_ = FName{to_fstring(level.id)};
    grid_ = level.grid;
    refresh_collision_grid_visualization();
    if (is_presentation_enabled()) {
        presentation_host_.initialise(
            level_config->get_visual_config(level.setup),
            IsValid(player_ship)
                ? TOptional<FPlayerPresentationResources>{player_ship->get_presentation_resources()}
                : NullOpt,
            audio->facade(),
            level.static_bounds,
            level.radii,
            frame);
        auto* controller{
            CastChecked<ASpaceGamePlayerController>(GetWorld()->GetFirstPlayerController())};
        if (IsValid(player_ship)) {
            controller->Possess(player_ship);
        }
    }

    if (!is_presentation_enabled()) {
        auto* controller{Cast<ASpaceGamePlayerController>(GetWorld()->GetFirstPlayerController())};
        if (!IsValid(controller) || !controller->activate_run_controls()) {
            return fail(ELevelLoadErrorCode::MissingInfrastructure,
                        TEXT("Failed to activate simulation-only run controls"));
        }
    }

    UE_LOG(LogSandbox, Display, TEXT("Level ready: %s"), *to_fstring(level.setup.source_path));
    return {};
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
void ATestBatchOrchestrator::set_collision_overlays(FCollisionOverlayVisibility const visibility) {
    collision_overlays_ = visibility;
    refresh_collision_grid_visualization();
}
void ATestBatchOrchestrator::refresh_collision_grid_visualization() {
    TOptional<FCollisionGridVisualizationSettings> settings;
    if ((collision_overlays_.grid || collision_overlays_.bounds) && is_presentation_enabled() &&
        IsValid(level_config) && status_) {
        auto const grid{grid_};
        auto const& collision_grid{level_config->collision_grid};
        settings.Emplace(FCollisionGridVisualizationSettings{
            .grid = grid,
            .line_colour = collision_grid.line_colour,
            .line_thickness = collision_grid.line_thickness,
            .visibility = collision_overlays_,
        });
    }

    presentation_host_.configure_collision_grid(MoveTemp(settings));
}

/* **************************************** */
// Simulation state
/* **************************************** */
void ATestBatchOrchestrator::present_level(time_type const delta_seconds,
                                           presentation::LevelFrame const& frame) {
    check(status_);
    status_ = frame.status;
    if (IsValid(player_ship) && frame.player) {
        auto const& player{*frame.player};
        player_ship->present_player(player.team, player.status.health, player.flight.ship_velocity);
    }
    presentation_host_.tick(delta_seconds, frame);

    for (auto const victim : frame.events.victims) {
        if (victim.entity_type() == sim::EntityType::PlayerShip && IsValid(player_ship)) {
            player_ship->handle_simulation_death();
            break;
        }
    }
}

void ATestBatchOrchestrator::finish_level(profiles::CompletedLevel const& completed,
                                          bool scored,
                                          bool new_best,
                                          bool const show_results) {
    auto completion{make_level_completion(completed)};
    if (scored) {
        completion.team_kills = completed.player_team_kills;
        completion.new_best_time = new_best;
    }

    auto* const controller{
        Cast<ASpaceGamePlayerController>(UGameplayStatics::GetPlayerController(GetWorld(), 0))};
    if (IsValid(controller)) {
        controller->finish_level();
    }

    release_level();

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
