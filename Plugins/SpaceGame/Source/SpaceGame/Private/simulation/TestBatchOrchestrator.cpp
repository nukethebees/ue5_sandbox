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
#include <SpaceGame/support/logging/SandboxLogCategories.h>

#include <SandboxCore/invoke.h>
#include <SandboxCoreEngine/actor_utils.h>
#include <SandboxCoreEngine/uobject_utils.h>
#include <SandboxISMCComponent.h>

#include <CoreGlobals.h>
#include <Engine/GameInstance.h>
#include <Engine/LevelScriptActor.h>
#include <EngineUtils.h>
#include <GameFramework/GameModeBase.h>
#include <GameFramework/GameStateBase.h>
#include <GameFramework/HUD.h>
#include <GameFramework/PhysicsVolume.h>
#include <GameFramework/PlayerController.h>
#include <GameFramework/PlayerState.h>
#include <GameFramework/WorldSettings.h>
#include <HAL/PlatformMisc.h>
#include <Kismet/GameplayStatics.h>
#include <SpaceGame/persistence/SpaceSaveGame.h>
#include <SpaceGame/persistence/SpaceSaveSubsystem.h>
#include <VisualLogger/VisualLogger.h>

namespace {
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
    hud_manager.deactivate();
    if (IsValid(player_ship)) {
        player_ship->unbind_simulation();
    }
    level_simulation_.Reset();
    world_collision_.restore_collision();
    SetActorTickEnabled(false);
    stop_visual_logging();
    clear_end_tick_test_hook();

    Super::EndPlay(end_play_reason);
}

void ATestBatchOrchestrator::start_simulation() {
    if (!level_simulation_.IsSet()) {
        begin_play();
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
    world_collision_.restore_collision();
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

void ATestBatchOrchestrator::begin_play() {
    if (level_simulation_.IsSet()) {
        UE_LOG(LogSandbox, Error, TEXT("Level simulation is already initialized"));
        return;
    }
    auto* world{GetWorld()};
    ml::fatal_if_uobject_ptrs_invalid(
        {SANDBOX_NAMED_UOBJECT_PTR(world), SANDBOX_NAMED_UOBJECT_PTR(level_config)});
    if (!presentation_enabled &&
        (IsValid(player_ship) || ml::get_first_actor<ATestSpaceShip>(*world))) {
        UE_LOG(LogSandbox, Error, TEXT("Presentation-disabled levels must be playerless"));
        SetActorTickEnabled(false);
        return;
    }
    set_level_config(*level_config);
    hud_tick_loop.initialise();
    initialise_simulation();
    bind_and_destroy_proxies();
    world_collision_.initialise_static_geometry(
        *world, level_config->collision_grid, get_spatial_query_manager().get_collision_system());
    level_simulation_->finish_initialisation();
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
                               player_ship.Get());
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
}

void ATestBatchOrchestrator::load_authored_level() {
    auto* const world{GetWorld()};
    auto* const game_instance{IsValid(world) ? world->GetGameInstance() : nullptr};
    auto* const subsystem{
        IsValid(game_instance) ? game_instance->GetSubsystem<ml::ioj::UGameSubsystem>() : nullptr};
    if (!IsValid(subsystem)) {
        UE_LOG(LogSandbox,
               Error,
               TEXT("ATestBatchOrchestrator::load_authored_level: Game subsystem is invalid"));
        UGameplayStatics::OpenLevel(this, ml::ioj::UGameSubsystem::get_main_menu_level_name());
        return;
    }

    auto pending{subsystem->take_pending_level()};
    if (!pending.IsSet()) {
        auto const error{TEXT("No pending authored level was provided to GameRuntime.")};
        UE_LOG(LogSandbox, Error, TEXT("%s"), error);
        subsystem->set_level_launch_error(error);
        if (!subsystem->return_to_level_select()) {
            UE_LOG(LogSandbox,
                   Error,
                   TEXT("ATestBatchOrchestrator::load_authored_level: Failed to return to level "
                        "select."));
        }
        return;
    }

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
        UE_LOG(LogSandbox, Error, TEXT("%s"), *message);
        subsystem->set_level_launch_error(message);
        if (!subsystem->return_to_level_select()) {
            UE_LOG(LogSandbox,
                   Error,
                   TEXT("ATestBatchOrchestrator::load_authored_level: Failed to return to level "
                        "select."));
        }
        return;
    }

    begin_play();
    if (pending->launch_mode == ml::ioj::ELevelLaunchMode::Paused) {
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
    return world_collision_.add_static_geometry(component,
                                                get_spatial_query_manager().get_collision_system());
}
void ATestBatchOrchestrator::process_mission_result() {
    auto result{get_mission_manager().take_result()};
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
    if (result->state == ETestMissionState::Succeeded) {
        on_mission_completed.Broadcast({.level_id = result->level_id,
                                        .level_display_name = result->level_display_name,
                                        .persisted = persisted});
    }
}
