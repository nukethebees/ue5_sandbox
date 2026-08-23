#include "TestBatchOrchestrator.h"

#include <Sandbox/batch_game/test_entity_registry/TestEntityRegistry.h>
#include <Sandbox/batch_game/TestCapitalShipFighters.h>
#include <Sandbox/batch_game/TestCapitalShipProxy.h>
#include <Sandbox/batch_game/TestCapitalShips.h>
#include <Sandbox/batch_game/TestEntity.h>
#include <Sandbox/batch_game/TestLasers.h>
#include <Sandbox/batch_game/TestMissionManager.h>
#include <Sandbox/batch_game/TestSimulationConfig.h>
#include <Sandbox/batch_game/TestSpaceShip.h>
#include <Sandbox/batch_game/TestStaticTurrets.h>
#include <Sandbox/batch_game/TestStaticTurretsProxy.h>
#include <Sandbox/batch_game/TestTubeSpinnerProxy.h>
#include <Sandbox/batch_game/TestTubeSpinners.h>
#include <Sandbox/environment/effects/DelayedNiagaraSpawner.h>
#include <Sandbox/logging/SandboxLogCategories.h>
#include <Sandbox/ui/HUDManager.h>
#include <SandboxGameShared/utilities/actor_utils.h>

#include <SandboxCore/invoke.h>
#include <SandboxCoreEngine/actor_utils.h>
#include <SandboxCoreEngine/uobject_utils.h>

#include <CoreGlobals.h>
#include <Engine/LevelScriptActor.h>
#include <EngineUtils.h>
#include <HAL/PlatformMisc.h>
#include <GameFramework/GameModeBase.h>
#include <GameFramework/GameStateBase.h>
#include <GameFramework/HUD.h>
#include <GameFramework/PhysicsVolume.h>
#include <GameFramework/PlayerController.h>
#include <GameFramework/PlayerState.h>
#include <GameFramework/WorldSettings.h>
#include <Kismet/GameplayStatics.h>
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

template <typename... TProxies>
void bind_and_destroy_proxy_actors(UWorld& world,
                                   FTestEntityRegistry const& entity_registry,
                                   FTestMissionManager& mission_manager) {
    FProxyEntityMap proxy_entities;
    (add_proxy_handles<TProxies>(world, entity_registry, proxy_entities), ...);

    mission_manager.on_proxy_entities_bound(proxy_entities);
    ATestBatchOrchestrator::on_proxy_entities_bound.Broadcast(proxy_entities);

    (destroy_proxy_actors<TProxies>(world), ...);
}
}

FOnProxyEntitiesBound ATestBatchOrchestrator::on_proxy_entities_bound;

ATestBatchOrchestrator::ATestBatchOrchestrator()
    : query_manager{entity_registry} {
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;

    ml::set_actor_component_mobility(*this, EComponentMobility::Static);
}

void ATestBatchOrchestrator::BeginPlay() {
    Super::BeginPlay();

    state = EOrchestratorState::Uninitialised;
    if (should_initialise_in_begin_play()) {
        begin_play();
    } else {
        SetActorTickEnabled(false);
    }
}
void ATestBatchOrchestrator::EndPlay(EEndPlayReason::Type const end_play_reason) {
    hud_manager.deactivate();
    state = EOrchestratorState::Stopped;
    SetActorTickEnabled(false);
    stop_visual_logging();

    Super::EndPlay(end_play_reason);
}

void ATestBatchOrchestrator::start_simulation() {
    if (state == EOrchestratorState::Uninitialised) {
        begin_play();
    }

    if (state != EOrchestratorState::Paused) {
        UE_LOG(LogSandbox,
               Error,
               TEXT("ATestBatchOrchestrator::start_simulation: Orchestrator is not paused"));
        return;
    }

    state = EOrchestratorState::Running;
    SetActorTickEnabled(true);
    start_visual_logging();
}
void ATestBatchOrchestrator::pause_simulation() {
    state = EOrchestratorState::Paused;
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

    TStaticArray<AActor*, 7> recreated_actors{};
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

        mission_manager.replace_startup_actor(old_actor, *replacement);
        actor = replacement;
        recreated_actors[recreated_actor_count] = replacement;
        ++recreated_actor_count;
    }};

    recreate_actor(player_ship);
    recreate_actor(lasers);
    recreate_actor(capital_ships);
    recreate_actor(capital_ship_fighters);
    recreate_actor(turrets);
    recreate_actor(spinners);
    recreate_actor(niagara_spawner);

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

    constexpr auto apply_config{[](auto const actor_ptr, auto const actor_config) {
        if (IsValid(actor_ptr)) {
            apply_actor_config(*actor_ptr, actor_config.Get());
        }
    }};

    if (IsValid(simulation_config)) {
        apply_config(player_ship, simulation_config->player_ship_config);
        apply_config(lasers, simulation_config->lasers_config);
        apply_config(capital_ships, simulation_config->capital_ships_config);
        apply_config(capital_ship_fighters, simulation_config->capital_ship_fighters_config);
        apply_config(turrets, simulation_config->static_turrets_config);
        apply_config(spinners, simulation_config->tube_spinners_config);
    }

    for (int32 i{0}; i < recreated_actor_count; ++i) {
        UGameplayStatics::FinishSpawningActor(recreated_actors[i], FTransform::Identity);
    }

    mission_manager.reset_runtime_state();
    state = EOrchestratorState::Uninitialised;
    if (should_initialise_in_begin_play()) {
        begin_play();
    }

    on_reset.Broadcast(*this);
}

void ATestBatchOrchestrator::set_test_config(UTestSimulationConfig const& config) {
    if (!ensureAlwaysMsgf(config.is_valid(), TEXT("Test simulation config is invalid"))) {
        return;
    }

    actor_classes = config.actor_classes;
    set_assets(config.simulation_config.Get(),
               ESimulationAssetActorScope::OrchestratorActors,
               ESimulationAssetProxyMode::Include);
}
void ATestBatchOrchestrator::set_start_mode(EOrchestratorStartMode const mode) {
    if (state != EOrchestratorState::Uninitialised) {
        UE_LOG(LogSandbox,
               Error,
               TEXT("ATestBatchOrchestrator::set_start_mode: Orchestrator is already initialised"));
        return;
    }

    start_mode = mode;
}
void ATestBatchOrchestrator::set_assets(USimulationConfig* const assets,
                                        ESimulationAssetActorScope const actor_scope,
                                        ESimulationAssetProxyMode const proxy_mode) {
    if (!ensureAlwaysMsgf(IsValid(assets), TEXT("Simulation config is invalid"))) {
        return;
    }
    if (!ensureAlwaysMsgf(assets->is_valid(),
                          TEXT("Simulation config contains invalid actor configs"))) {
        return;
    }

    auto* const world{GetWorld()};
    auto const requires_world{actor_scope == ESimulationAssetActorScope::AllActorsInLevel ||
                              proxy_mode == ESimulationAssetProxyMode::Include};
    if (requires_world &&
        !ensureAlwaysMsgf(IsValid(world), TEXT("Cannot apply simulation config without a world"))) {
        return;
    }

#if WITH_EDITOR
    if (IsValid(world) && !world->IsGameWorld()) {
        Modify();
    }
#endif
    simulation_config = assets;
#if WITH_EDITORONLY_DATA
    simulation_asset_actor_scope = actor_scope;
    simulation_asset_proxy_mode = proxy_mode;
#endif

    if (actor_scope == ESimulationAssetActorScope::OrchestratorActors) {
        if (IsValid(player_ship)) {
            apply_actor_config(*player_ship, assets->player_ship_config.Get());
        }
        if (IsValid(lasers)) {
            apply_actor_config(*lasers, assets->lasers_config.Get());
        }
        if (IsValid(capital_ships)) {
            apply_actor_config(*capital_ships, assets->capital_ships_config.Get());
        }
        if (IsValid(capital_ship_fighters)) {
            apply_actor_config(*capital_ship_fighters, assets->capital_ship_fighters_config.Get());
        }
        if (IsValid(turrets)) {
            apply_actor_config(*turrets, assets->static_turrets_config.Get());
        }
        if (IsValid(spinners)) {
            apply_actor_config(*spinners, assets->tube_spinners_config.Get());
        }
    } else {
        check(world);
        set_actor_config_on_all<ATestSpaceShip>(*world, assets->player_ship_config.Get());
        set_actor_config_on_all<ATestLasers>(*world, assets->lasers_config.Get());
        set_actor_config_on_all<ATestCapitalShips>(*world, assets->capital_ships_config.Get());
        set_actor_config_on_all<ATestCapitalShipFighters>(
            *world, assets->capital_ship_fighters_config.Get());
        set_actor_config_on_all<ATestStaticTurrets>(*world, assets->static_turrets_config.Get());
        set_actor_config_on_all<ATestTubeSpinners>(*world, assets->tube_spinners_config.Get());
    }

    if (proxy_mode == ESimulationAssetProxyMode::Include) {
        check(world);
        set_actor_config_on_all<ATestCapitalShips::Proxy>(*world,
                                                          assets->capital_ships_config.Get());
        set_actor_config_on_all<ATestStaticTurrets::Proxy>(*world,
                                                           assets->static_turrets_config.Get());
        set_actor_config_on_all<ATestTubeSpinners::Proxy>(*world,
                                                          assets->tube_spinners_config.Get());
    }
}

auto ATestBatchOrchestrator::get_player_ship() const -> ATestSpaceShip const* {
    return player_ship.Get();
}
void ATestBatchOrchestrator::set_player_ship(ATestSpaceShip& new_player_ship) {
    player_ship = &new_player_ship;
}

void ATestBatchOrchestrator::begin_play() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestBatchOrchestrator::begin_play);

    completed_ticks = 0;
    simulation_tick_loop.initialise();
    hud_tick_loop.initialise();
    mission_manager.reset_runtime_state();

    auto* world{GetWorld()};

#if WITH_EDITOR
    if (log_ticks) {
        UE_LOG(LogSandbox, Display, TEXT("ATestBatchOrchestrator: begin_play start"));
    }
#endif

    ml::fatal_if_uobject_ptrs_invalid({
        SANDBOX_NAMED_UOBJECT_PTR(lasers),
        SANDBOX_NAMED_UOBJECT_PTR(capital_ships),
        SANDBOX_NAMED_UOBJECT_PTR(capital_ship_fighters),
        SANDBOX_NAMED_UOBJECT_PTR(turrets),
        SANDBOX_NAMED_UOBJECT_PTR(spinners),
        SANDBOX_NAMED_UOBJECT_PTR(niagara_spawner),
        SANDBOX_NAMED_UOBJECT_PTR(world),
    });

    bind_simulation_dependencies();
    mission_manager.set_world(*world);

    entity_registry.reset();

    ml::invoke_on_all(
        [](AActor* actor) {
            ml::fatal_if_actor_transform_not_identity(*actor);
            ml::fatal_if_actor_root_not_static(*actor);
            actor->SetActorTickEnabled(false);
        },
        lasers,
        capital_ships,
        capital_ship_fighters,
        turrets,
        spinners);

    lasers_phase.clear_runtime_state();
    capital_ships_phase.clear_runtime_state();
    capital_ship_fighters_phase.clear_runtime_state();
    turrets_phase.clear_runtime_state();
    spinners_phase.clear_runtime_state();

    if (IsValid(player_ship)) {
        player_ship_phase.begin_play();
    }
    capital_ships_phase.begin_play();
    capital_ship_fighters_phase.begin_play();
    turrets_phase.begin_play();
    spinners_phase.begin_play();
    lasers_phase.begin_play();

    query_manager.initialise(
        *world, player_ship.Get(), *capital_ships, *capital_ship_fighters, *turrets, *spinners);
    query_manager.reserve_thread_buffers(
        FMath::Max(1, FPlatformMisc::NumberOfCoresIncludingHyperthreads()));

    validate_proxy_handles();

    bind_and_destroy_proxy_actors<ATestCapitalShipProxy,
                                  ATestStaticTurretsProxy,
                                  ATestTubeSpinnerProxy>(*world, entity_registry, mission_manager);

    entity_registry.commit_updates();
    entity_registry.end_tick();

    mission_manager.begin_play();

    hud_manager.initialise(hud_update_frequencies,
                           mission_manager,
                           entity_registry,
                           hud_tick_loop.tick_rate,
                           player_ship.Get());

#if WITH_EDITOR
    if (log_ticks) {
        UE_LOG(LogSandbox, Display, TEXT("ATestBatchOrchestrator: begin_play end"));
    }
#endif

    switch (start_mode) {
        case EOrchestratorStartMode::Paused: {
            state = EOrchestratorState::Paused;
            SetActorTickEnabled(false);
            break;
        }
        case EOrchestratorStartMode::PausedInTest: {
            if (GIsAutomationTesting) {
                state = EOrchestratorState::Paused;
                SetActorTickEnabled(false);
            } else {
                state = EOrchestratorState::Running;
                SetActorTickEnabled(true);
            }
            break;
        }
        case EOrchestratorStartMode::Automatic: {
            state = EOrchestratorState::Running;
            SetActorTickEnabled(true);
            break;
        }
    }

    if (state == EOrchestratorState::Running) {
        start_visual_logging();
    }
}
auto ATestBatchOrchestrator::should_initialise_in_begin_play() const noexcept -> bool {
    return start_mode == EOrchestratorStartMode::Automatic ||
           (start_mode == EOrchestratorStartMode::PausedInTest && !GIsAutomationTesting);
}

void ATestBatchOrchestrator::start_visual_logging() {
#if ENABLE_VISUAL_LOG
    if (!enable_visual_logging) {
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

void ATestBatchOrchestrator::validate_proxy_handles() {
    if (IsValid(player_ship)) {
        if (!entity_registry.is_valid_handle(player_ship->get_entity_registry_handle())) {
            UE_LOG(LogSandbox, Fatal, TEXT("Player ship handle is invalid"));
        }
    }
    ml::invoke_on_all(
        [this](auto actor) { actor->validate_proxy_handles(); }, capital_ships, turrets);
}

void ATestBatchOrchestrator::Tick(float dt) {
    Super::Tick(dt);

    tick(static_cast<time_type>(dt));
}
void ATestBatchOrchestrator::tick(time_type const dt) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestBatchOrchestrator::tick);

    if (state != EOrchestratorState::Running) {
        return;
    }

    simulation_tick_loop.add_time(dt);

    while (simulation_tick_loop.try_tick()) {
#if WITH_EDITOR
        if (log_ticks) {
            UE_LOG(LogSandbox,
                   Display,
                   TEXT("ATestBatchOrchestrator: Tick %d start"),
                   completed_ticks);
        }
#endif

        /* -------------------------------------------------------------------------------- */
        // Setup phase
        /* -------------------------------------------------------------------------------- */
        {
            // Clear transient data
            // Assume registry data is stable here
            TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestBatchOrchestrator::tick::begin_tick);

            if (IsValid(player_ship)) {
                player_ship_phase.begin_tick();
            }

            capital_ships_phase.begin_tick();
            capital_ship_fighters_phase.begin_tick();
            turrets_phase.begin_tick();
            spinners_phase.begin_tick();
            lasers_phase.begin_tick();
        }

        /* -------------------------------------------------------------------------------- */
        // Actor decision phase
        /* -------------------------------------------------------------------------------- */
        // Query target data from registry
        // Queue projectile spawns

        {
            TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestBatchOrchestrator::tick::update_timers);

            if (IsValid(player_ship)) {
                player_ship_phase.update_timers(simulation_tick_loop.tick_period);
            }
            capital_ship_fighters_phase.update_timers(simulation_tick_loop.tick_period);
            capital_ships_phase.update_timers(simulation_tick_loop.tick_period);
            turrets_phase.update_timers(simulation_tick_loop.tick_period);
            spinners_phase.update_timers(simulation_tick_loop.tick_period);
        }

        {
            TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestBatchOrchestrator::tick::make_decisions);
            turrets_phase.make_decisions();
            capital_ships_phase.make_decisions();
            capital_ship_fighters_phase.make_decisions();
        }

        /* -------------------------------------------------------------------------------- */
        // Simulation phase
        /* -------------------------------------------------------------------------------- */
        {
            // Movement
            TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestBatchOrchestrator::tick::movement);

            if (IsValid(player_ship)) {
                player_ship_phase.move(simulation_tick_loop.tick_period);
            }

            capital_ship_fighters_phase.move(simulation_tick_loop.tick_period);
            spinners_phase.move(simulation_tick_loop.tick_period);
        }

        {
            // Queue commands
            // e.g. spawning lasers for the next frame
            TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestBatchOrchestrator::tick::queue_commands);

            if (IsValid(player_ship)) {
                player_ship_phase.queue_commands();
            }

            capital_ship_fighters_phase.queue_commands();
            turrets_phase.queue_commands();
            spinners_phase.queue_commands();
        }

        {
            // Entity collision
            TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestBatchOrchestrator::tick::entity_collision);
        }

        {
            // Projectile simulation
            TRACE_CPUPROFILER_EVENT_SCOPE(
                Sandbox::ATestBatchOrchestrator::tick::projectile_simulation);

            lasers_phase.simulate(simulation_tick_loop.tick_period);
            lasers_phase.commit_spawns();
        }

        /* -------------------------------------------------------------------------------- */
        // Resolution phase
        /* -------------------------------------------------------------------------------- */
        {
            // Resolve hit events
            TRACE_CPUPROFILER_EVENT_SCOPE(
                Sandbox::ATestBatchOrchestrator::tick::resolve_damage_events);

            if (IsValid(player_ship)) {
                player_ship_phase.resolve_damage_events();
            }

            capital_ships_phase.resolve_damage_events();
            capital_ship_fighters_phase.resolve_damage_events();
            turrets_phase.resolve_damage_events();
        }

        {
            // Send updates to the registry
            TRACE_CPUPROFILER_EVENT_SCOPE(
                Sandbox::ATestBatchOrchestrator::tick::update_entity_registry);

            if (IsValid(player_ship)) {
                player_ship_phase.update_entity_registry();
            }

            capital_ships_phase.update_entity_registry();
            capital_ship_fighters_phase.update_entity_registry();
            turrets_phase.update_entity_registry();
            spinners_phase.update_entity_registry();
        }

        {
            TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestBatchOrchestrator::tick::commit_updates);

            entity_registry.commit_updates();
        }

        {
            // Apply changes from the registry e.g. destroyed targets
            TRACE_CPUPROFILER_EVENT_SCOPE(
                Sandbox::ATestBatchOrchestrator::tick::sync_from_registry);

            if (IsValid(player_ship)) {
                player_ship_phase.sync_from_registry();
            }

            capital_ships_phase.sync_from_registry();
            capital_ship_fighters_phase.sync_from_registry();
            turrets_phase.sync_from_registry();
        }

        mission_manager.mission_tick();

        {
            TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestBatchOrchestrator::update_visual_data);

            if (IsValid(player_ship)) {
                player_ship_phase.update_visual_data();
            }

            capital_ships_phase.update_visual_data();
            capital_ship_fighters_phase.update_visual_data();
            turrets_phase.update_visual_data();
            spinners_phase.update_visual_data();
            lasers_phase.update_visual_data();
        }

        /* -------------------------------------------------------------------------------- */
        // End phase
        /* -------------------------------------------------------------------------------- */
        {
            TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestBatchOrchestrator::tick::end_tick);

            if (IsValid(player_ship)) {
                player_ship_phase.end_tick();
            }

            capital_ships_phase.end_tick();
            capital_ship_fighters_phase.end_tick();
            turrets_phase.end_tick();
            spinners_phase.end_tick();
            lasers_phase.end_tick();
            entity_registry.end_tick();
        }

#if WITH_EDITOR
        if (log_ticks) {
            UE_LOG(
                LogSandbox, Display, TEXT("ATestBatchOrchestrator: Tick %d end"), completed_ticks);
        }
#endif

        ++completed_ticks;
        end_tick_test_hook.ExecuteIfBound(*this);
    }

    hud_tick_loop.add_time(dt);
    while (hud_tick_loop.try_tick()) {
        hud_manager.tick(1);
    }

    /* -------------------------------------------------------------------------------- */
    // Rendering
    /* -------------------------------------------------------------------------------- */
    {
        TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestBatchOrchestrator::commit_visual_data);

        if (IsValid(player_ship)) {
            player_ship_phase.commit_visual_data();
        }

        capital_ships_phase.commit_visual_data();
        capital_ship_fighters_phase.commit_visual_data();
        turrets_phase.commit_visual_data();
        spinners_phase.commit_visual_data();
        lasers_phase.commit_visual_data();

        niagara_spawner->update_spawns(dt);
    }
}

void ATestBatchOrchestrator::set_time_scale(time_type const scale) noexcept {
    check(scale > time_type{0});
    simulation_tick_loop.time_scale = scale;
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

void ATestBatchOrchestrator::bind_simulation_dependencies() {
    UE_LOG(LogSandbox, Display, TEXT("ATestBatchOrchestrator::bind_simulation_dependencies"));

    if (IsValid(player_ship)) {
        player_ship_phase.bind(*player_ship);
    }
    check(IsValid(lasers));
    check(IsValid(capital_ships));
    check(IsValid(capital_ship_fighters));
    check(IsValid(turrets));
    check(IsValid(spinners));
    lasers_phase.bind(*lasers);
    capital_ships_phase.bind(*capital_ships);
    capital_ship_fighters_phase.bind(*capital_ship_fighters);
    turrets_phase.bind(*turrets);
    spinners_phase.bind(*spinners);

    capital_ships->set_niagara_spawner(*niagara_spawner);
    capital_ships->bind_fighters(*capital_ship_fighters);

    auto const bind_simulation_clock{[this](auto actor) { actor->bind_simulation_clock(*this); }};
    if (IsValid(player_ship)) {
        bind_simulation_clock(player_ship);
    }
    ml::invoke_on_all(bind_simulation_clock, capital_ship_fighters, turrets, spinners, lasers);
    mission_manager.bind_simulation_clock(*this);

    if (IsValid(player_ship)) {
        player_ship->set_entity_registry(&entity_registry);
        player_ship->set_laser_actor(lasers);
    }

    ml::invoke_on_all([&](auto actor) { actor->set_entity_registry(entity_registry); },
                      lasers,
                      capital_ships,
                      capital_ship_fighters,
                      turrets,
                      spinners);
    mission_manager.set_entity_registry(entity_registry);

    lasers->set_spatial_query_manager(query_manager);
    capital_ships->set_spatial_query_manager(query_manager);
    capital_ship_fighters->set_spatial_query_manager(query_manager);
    turrets->set_spatial_query_manager(query_manager);

    ml::invoke_on_all([&](auto actor) { actor->set_laser_actor(*lasers); },
                      capital_ship_fighters,
                      turrets,
                      spinners);
}

void ATestBatchOrchestrator::set_end_tick_test_hook(FOrchestratorEndTickTestHook hook) {
    end_tick_test_hook = MoveTemp(hook);
}
void ATestBatchOrchestrator::clear_end_tick_test_hook() {
    end_tick_test_hook.Unbind();
}

void ATestBatchOrchestrator::spawn_missing_actors() {
    if (state != EOrchestratorState::Uninitialised) {
        UE_LOG(LogSandbox,
               Error,
               TEXT("ATestBatchOrchestrator::spawn_missing_actors: Orchestrator is already "
                    "initialised"));
        return;
    }

    auto* world{GetWorld()};
    if (!ensureAlwaysMsgf(IsValid(world), TEXT("Cannot spawn simulation actors without a world"))) {
        return;
    }

    TArray<AActor*> spawned_actors;
    auto spawn{[&]<typename T>(TSubclassOf<T> const actor_class) -> T* {
        if (!IsValid(actor_class)) {
            UE_LOG(LogSandbox,
                   Warning,
                   TEXT("ATestBatchOrchestrator::spawn_missing_actors: Null actor class"));
            return nullptr;
        }

        if (auto* const actor{ml::get_first_actor<T>(*world)}) {
            return actor;
        }

        auto* const actor{world->SpawnActorDeferred<T>(actor_class, FTransform::Identity)};
        if (IsValid(actor)) {
            spawned_actors.Add(actor);
        } else {
            UE_LOG(LogSandbox,
                   Error,
                   TEXT("ATestBatchOrchestrator::spawn_missing_actors: Failed to spawn %s"),
                   *actor_class->GetName());
        }

        return actor;
    }};

    if (!IsValid(player_ship)) {
        player_ship = ml::get_first_actor<ATestSpaceShip>(*world);
    }

    lasers = spawn(actor_classes.lasers_class);
    capital_ships = spawn(actor_classes.capital_ships_class);
    capital_ship_fighters = spawn(actor_classes.capital_ship_fighters_class);
    turrets = spawn(actor_classes.turrets_class);
    spinners = spawn(actor_classes.spinners_class);

    niagara_spawner = spawn(actor_classes.niagara_spawner_class);

    if (IsValid(simulation_config)) {
        set_assets(simulation_config.Get(),
                   ESimulationAssetActorScope::OrchestratorActors,
                   ESimulationAssetProxyMode::Include);
    }

    for (auto* const actor : spawned_actors) {
        actor->FinishSpawning(FTransform::Identity);
        UE_LOG(LogSandbox, Display, TEXT("Spawned missing %s"), *actor->GetClass()->GetName());
    }
}

#if WITH_EDITOR
void ATestBatchOrchestrator::apply_simulation_config() {
    set_assets(simulation_config.Get(), simulation_asset_actor_scope, simulation_asset_proxy_mode);
}

void ATestBatchOrchestrator::spawn_missing_actors_button() {
    spawn_missing_actors();
}
#endif
