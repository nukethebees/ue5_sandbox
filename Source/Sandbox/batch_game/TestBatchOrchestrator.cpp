#include "TestBatchOrchestrator.h"

#include <Sandbox/batch_game/test_entity_registry/TestEntityRegistry.h>
#include <Sandbox/batch_game/TestCapitalShipFighters.h>
#include <Sandbox/batch_game/TestCapitalShips.h>
#include <Sandbox/batch_game/TestLasers.h>
#include <Sandbox/batch_game/TestMissionManager.h>
#include <Sandbox/batch_game/TestSpaceShip.h>
#include <Sandbox/batch_game/TestStaticTurrets.h>
#include <Sandbox/batch_game/TestTubeSpinners.h>
#include <Sandbox/environment/effects/DelayedNiagaraSpawner.h>
#include <Sandbox/logging/SandboxLogCategories.h>
#include <Sandbox/utilities/actor_utils.h>

#include <SandboxCore/invoke.h>
#include <SandboxCore/uobject_utils.h>

#include <DrawDebugHelpers.h>

ATestBatchOrchestrator::ATestBatchOrchestrator() {
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;

    ml::set_actor_component_mobility(*this, EComponentMobility::Static);
}

void ATestBatchOrchestrator::BeginPlay() {
    Super::BeginPlay();

    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestBatchOrchestrator::begin_play);

    tick_counter = 0;

    ml::fatal_if_uobject_ptrs_invalid({
        SANDBOX_NAMED_UOBJECT_PTR(lasers),
        SANDBOX_NAMED_UOBJECT_PTR(capital_ships),
        SANDBOX_NAMED_UOBJECT_PTR(capital_ship_fighters),
        SANDBOX_NAMED_UOBJECT_PTR(turrets),
        SANDBOX_NAMED_UOBJECT_PTR(spinners),
        SANDBOX_NAMED_UOBJECT_PTR(entity_registry),
        SANDBOX_NAMED_UOBJECT_PTR(mission_manager),
        SANDBOX_NAMED_UOBJECT_PTR(niagara_spawner),
    });

    route_actor_references();

    entity_registry->reset();

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

    ml::invoke_on_all([this](auto actor) { actor->clear_runtime_state(); },
                      lasers,
                      capital_ships,
                      capital_ship_fighters,
                      turrets,
                      spinners);

    auto register_owner{
        [this](auto actor) { actor->set_owner_id(entity_registry->register_owner(*actor)); }};

    if (player_ship) { register_owner(player_ship); }
    ml::invoke_on_all(register_owner, capital_ships, capital_ship_fighters, turrets, spinners);

    if (player_ship) { player_ship->begin_play(); }
    ml::invoke_on_all([this](auto actor) { actor->begin_play(); },
                      capital_ships,
                      capital_ship_fighters,
                      turrets,
                      spinners,
                      lasers);

    check_proxy_handles();
    ml::invoke_on_all([this](auto actor) { actor->resolve_initial_targets(); }, capital_ships);

    if (player_ship) {
        mission_manager->update_player_handles();
        check(entity_registry->is_valid_unique_id(mission_manager->get_player_id()));
    }

    entity_registry->commit_updates();
    entity_registry->end_tick();

    if (player_ship) { mission_manager->begin_play(); }
}
void ATestBatchOrchestrator::check_proxy_handles() {}

void ATestBatchOrchestrator::Tick(float dt) {
    Super::Tick(dt);

    tick(dt);
}
void ATestBatchOrchestrator::tick(float const dt) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestBatchOrchestrator::tick);

    ++tick_counter;

    // ---------------------------------------------------------------------------------------------
    // Setup phase
    // ---------------------------------------------------------------------------------------------
    {
        // Clear transient data
        // Assume registry data is stable here
        TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestBatchOrchestrator::tick::begin_tick);

        if (player_ship) { player_ship->begin_tick(); }

        capital_ships->begin_tick();
        capital_ship_fighters->begin_tick();
        turrets->begin_tick();
        spinners->begin_tick();
        lasers->begin_tick();
    }

    {
        // Process spawns from last tick
        TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestBatchOrchestrator::tick::commit_spawns);

        lasers->commit_spawns();
        capital_ships->commit_spawns();
    }

    // ---------------------------------------------------------------------------------------------
    // Actor decision phase
    // ---------------------------------------------------------------------------------------------
    // Query target data from registry
    // Queue projectile spawns

    {
        TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestBatchOrchestrator::tick::update_timers);

        if (player_ship) { player_ship->update_timers(dt); }
        capital_ship_fighters->update_timers(dt);
        capital_ships->update_timers(dt);
        turrets->update_timers(dt);
        spinners->update_timers(dt);
    }

    {
        TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestBatchOrchestrator::tick::make_decisions);
        turrets->make_decisions();
        capital_ships->make_decisions();
        capital_ship_fighters->make_decisions();
    }

    // ---------------------------------------------------------------------------------------------
    // Simulation phase
    // ---------------------------------------------------------------------------------------------
    {
        // Movement
        TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestBatchOrchestrator::tick::movement);

        if (player_ship) { player_ship->move(dt); }

        capital_ship_fighters->move(dt);
        spinners->move(dt);
    }

    {
        // Queue commands
        // e.g. spawning lasers for the next frame
        TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestBatchOrchestrator::tick::queue_commands);

        if (player_ship) { player_ship->queue_commands(); }

        capital_ship_fighters->queue_commands();
        turrets->queue_commands();
        spinners->queue_commands();
    }

    {
        // Entity collision
        TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestBatchOrchestrator::tick::entity_collision);
    }

    {
        // Projectile simulation
        TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestBatchOrchestrator::tick::projectile_simulation);

        lasers->simulate(dt);
    }

    // ---------------------------------------------------------------------------------------------
    // Resolution phase
    // ---------------------------------------------------------------------------------------------
    {
        // Resolve hit events
        TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestBatchOrchestrator::tick::resolve_hit_events);

        if (player_ship) { player_ship->resolve_hit_events(); }

        capital_ships->resolve_hit_events();
        capital_ship_fighters->resolve_hit_events();
        turrets->resolve_hit_events();
    }

    {
        // Send updates to the registry
        TRACE_CPUPROFILER_EVENT_SCOPE(
            Sandbox::ATestBatchOrchestrator::tick::update_entity_registry);

        if (player_ship) { player_ship->update_entity_registry(); }

        capital_ships->update_entity_registry();
        capital_ship_fighters->update_entity_registry();
        turrets->update_entity_registry();
        spinners->update_entity_registry();
    }

    {
        TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestBatchOrchestrator::tick::commit_updates);

        entity_registry->commit_updates();
    }

    {
        // Apply changes from the registry e.g. destroyed targets
        TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestBatchOrchestrator::tick::sync_from_registry);

        if (player_ship) { player_ship->sync_from_registry(); }

        capital_ships->sync_from_registry();
        capital_ship_fighters->sync_from_registry();
        turrets->sync_from_registry();
    }

    if (player_ship) { mission_manager->mission_tick(dt); }

    // ---------------------------------------------------------------------------------------------
    // End phase
    // ---------------------------------------------------------------------------------------------
    {
        // Update visual state
        TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestBatchOrchestrator::tick::update_visuals);

        if (player_ship) { player_ship->update_visuals(); }

        capital_ships->update_visuals();
        capital_ship_fighters->update_visuals();
        turrets->update_visuals();
        spinners->update_visuals();
        lasers->update_visuals();

        niagara_spawner->update_spawns(dt);
    }

    {
        TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestBatchOrchestrator::tick::end_tick);

        if (player_ship) { player_ship->end_tick(); }

        capital_ships->end_tick();
        capital_ship_fighters->end_tick();
        turrets->end_tick();
        spinners->end_tick();
        lasers->end_tick();
        entity_registry->end_tick();
    }
}

void ATestBatchOrchestrator::route_actor_references() {
    capital_ships->set_niagara_spawner(*niagara_spawner);

    if (player_ship) { mission_manager->set_player_ship(*player_ship); }

    ml::invoke_on_all([&](auto actor) { actor->set_entity_registry(*entity_registry); },
                      lasers,
                      capital_ships,
                      capital_ship_fighters,
                      turrets,
                      spinners,
                      mission_manager);

    ml::invoke_on_all([&](auto actor) { actor->set_laser_actor(*lasers); },
                      capital_ship_fighters,
                      turrets,
                      spinners);
}
void ATestBatchOrchestrator::spawn_missing_actors() {
    auto* world{GetWorld()};

    auto spawn{[&](auto c) {
        if (!IsValid(c)) {
            UE_LOG(LogSandbox,
                   Warning,
                   TEXT("ATestBatchOrchestrator::spawn_missing_actors: Null actor class"));
            return;
        }
        ml::ensure_actor_exists(*world, c);
    }};

    spawn(lasers_class);
    spawn(capital_ships_class);
    spawn(capital_ship_fighters_class);
    spawn(turrets_class);
    spawn(spinners_class);

    spawn(entity_registry_class);
    spawn(mission_manager_class);
    spawn(niagara_spawner_class);
}
