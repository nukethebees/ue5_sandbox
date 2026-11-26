#include "SpaceGame/simulation/TestBatchOrchestrator.h"

#include "SpaceGame/UnrealMessages.h"
#if defined(UE_LIBRARY_ENABLED) && UE_LIBRARY_ENABLED
#include "settings/GameHost.h"
#include <ioj/host/mission_startup.h>
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

#include <utility>

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
    FWorldDelegates::OnWorldPostActorTick.AddUObject(this, &ThisClass::record_world_delta);
#endif
}
void ATestBatchOrchestrator::EndPlay(EEndPlayReason::Type const end_play_reason) {
    FWorldDelegates::OnWorldPostActorTick.RemoveAll(this);
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
    mission_id_ = NAME_None;
    frame_delta_seconds_.reset();
    reset_level_input();
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
auto ATestBatchOrchestrator::prepare_level(presentation::MissionInit const& mission)
    -> FLevelLoadResult {
    presentation_mode_ = mission.presentation_mode;
    auto const playerless_control_context{mission.control_context};
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

    presentation_host_.get_hud_manager().configure(mission.hud);

    std::optional<FLevelVisualLoad> visual;
    if (is_presentation_enabled()) {
        auto* const controller{
            Cast<ASpaceGamePlayerController>(UGameplayStatics::GetPlayerController(world, 0))};
        if (!IsValid(controller)) {
            return fail(ELevelLoadErrorCode::MissingInfrastructure,
                        TEXT("Authored level requires a space-game player controller"));
        }

        if (!get_presentation_resources(mission).is_valid()) {
            return fail(ELevelLoadErrorCode::MissingInfrastructure,
                        TEXT("Orchestrator simulation infrastructure is incomplete"));
        }

        visual.emplace(*controller, playerless_control_context);
    }

    auto loaded{FLevelLoader::load({.world = *world,
                                    .config = *level_config,
                                    .mission = mission,
                                    .player = player_ship,
                                    .visual = visual})};
    if (!loaded) {
        return loaded;
    }

    TArray<FString> messages;
    level_config->get_validation_errors(messages, is_presentation_enabled(), mission.presentation);
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
            actor->set_starfield_settings(mission.starfield);
        }
    }

    auto const resources{get_presentation_resources(mission)};
    if (is_presentation_enabled() && !resources.is_valid()) {
        return fail(ELevelLoadErrorCode::MissingInfrastructure,
                    TEXT("Level presentation resources are incomplete"));
    }

    return {};
}

auto ATestBatchOrchestrator::bind_level(presentation::MissionInit const& mission,
                                        presentation::MissionFrame const& frame, sim::LevelSim const& level) -> bool {
    check(!status_);
    auto const resources{get_presentation_resources(mission)};
    auto* const audio{GetGameInstance()->GetSubsystem<UGameAudio>()};
    check(audio);
    status_ = frame.status;
    mission_id_ = FName{to_fstring(mission.id)};
    grid_ = mission.grid;
    refresh_collision_grid_visualization();
    if (IsValid(player_ship) && frame.player) {
        player_ship->present_player(*frame.player);
    }

    if (is_presentation_enabled()) {
        presentation_host_.initialise(
            resources.config, resources.player, audio->facade(), mission, frame, level.read_access());
        auto* controller{
            CastChecked<ASpaceGamePlayerController>(GetWorld()->GetFirstPlayerController())};
        if (IsValid(player_ship)) {
            controller->activate_ship_control();
        }
        if (mission.launch_mode == levels::LaunchMode::Paused) {
            controller->show_initial_pause_menu();
        }
    }

    if (!is_presentation_enabled()) {
        auto* controller{Cast<ASpaceGamePlayerController>(GetWorld()->GetFirstPlayerController())};
        if (!IsValid(controller) || !controller->activate_run_controls()) {
            handle_level_start_failure(TEXT("Failed to activate simulation-only run controls"));
            return false;
        }
    }

    UE_LOG(LogSandbox, Display, TEXT("Mission ready: %s"), *to_fstring(mission.source_path));
    frame_delta_seconds_.reset();
#if defined(UE_LIBRARY_ENABLED) && UE_LIBRARY_ENABLED
    report_mission_startup(MissionStartup::Ready);
#endif
    return true;
}
void ATestBatchOrchestrator::handle_level_start_failure(FString message) {
#if defined(UE_LIBRARY_ENABLED) && UE_LIBRARY_ENABLED
    report_mission_startup(MissionStartup::Failed);
#endif
    release_level();
    FLevelLoader::report_failure(GetWorld(), MoveTemp(message));
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
auto ATestBatchOrchestrator::get_presentation_resources(
    presentation::MissionInit const& mission) const -> FLevelPresentationResources {
    return presentation_host_.make_resources(
        level_config->get_visual_config(mission),
        IsValid(player_ship)
            ? TOptional<FPlayerPresentationResources>{player_ship->get_presentation_resources()}
            : NullOpt);
}

/* **************************************** */
// Simulation state
/* **************************************** */
void ATestBatchOrchestrator::record_world_delta(UWorld* world, ELevelTick, float delta_seconds) {
    if (status_ && world == GetWorld()) {
        frame_delta_seconds_ = delta_seconds;
    }
}
auto ATestBatchOrchestrator::take_frame_delta_seconds() -> std::optional<double> {
    return std::exchange(frame_delta_seconds_, std::nullopt);
}
void ATestBatchOrchestrator::present_level(time_type const delta_seconds,
                                           presentation::MissionFrame const& frame, sim::LevelSim const& level) {
    check(status_);
    status_ = frame.status;
    if (IsValid(player_ship) && frame.player) {
        player_ship->present_player(*frame.player);
    }
    presentation_host_.tick(delta_seconds, frame, level.read_access());

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
