#include "TestBatchOrchestrator.h"

#include "Sandbox/logging/SandboxLogCategories.h"
#include "Sandbox/misc/learning/TestCapitalShipFighters.h"
#include "Sandbox/misc/learning/TestCapitalShips.h"
#include "Sandbox/misc/learning/TestLasers.h"
#include "Sandbox/misc/learning/TestSpaceShip.h"
#include "Sandbox/misc/learning/TestStaticTurrets.h"
#include "Sandbox/misc/learning/TestTubeSpinners.h"
#include "Sandbox/utilities/actor_utils.h"

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
        SANDBOX_NAMED_UOBJECT_PTR(player_ship),
        SANDBOX_NAMED_UOBJECT_PTR(lasers),
        SANDBOX_NAMED_UOBJECT_PTR(capital_ships),
        SANDBOX_NAMED_UOBJECT_PTR(capital_ship_fighters),
        SANDBOX_NAMED_UOBJECT_PTR(turrets),
        SANDBOX_NAMED_UOBJECT_PTR(spinners),
        SANDBOX_NAMED_UOBJECT_PTR(entity_registry),
    });

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

    ml::invoke_on_all(
        [this](auto actor) { actor->set_owner_id(entity_registry->register_owner(*actor)); },
        player_ship,
        capital_ships,
        capital_ship_fighters,
        turrets,
        spinners);

    ml::invoke_on_all([this](auto actor) { actor->begin_play(); },
                      player_ship,
                      capital_ships,
                      capital_ship_fighters,
                      turrets,
                      spinners,
                      lasers);

    entity_registry->commit_updates();
    entity_registry->end_tick();
}
void ATestBatchOrchestrator::Tick(float dt) {
    Super::Tick(dt);

    tick(dt);
}
void ATestBatchOrchestrator::tick(float const dt) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestBatchOrchestrator::tick);

    ++tick_counter;

    {
        TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestBatchOrchestrator::tick::begin_tick);
        capital_ships->begin_tick();
        capital_ship_fighters->begin_tick();
        turrets->begin_tick();
        spinners->begin_tick();
        lasers->begin_tick();
    }

    { // Process spawns from last tick
        TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestBatchOrchestrator::tick::begin_tick);
        lasers->commit_spawns();
        capital_ships->commit_spawns();
    }

    { // General actor tick
        TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestBatchOrchestrator::tick::tick);
        player_ship->tick(dt);
        capital_ships->tick(dt);
        capital_ship_fighters->tick(dt);
        turrets->tick(dt);
        spinners->tick(dt);
        lasers->tick(dt);
    }

    { // Send updates to the registry
        TRACE_CPUPROFILER_EVENT_SCOPE(
            Sandbox::ATestBatchOrchestrator::tick::update_entity_registry);
        player_ship->update_entity_registry();
        capital_ships->update_entity_registry();
        capital_ship_fighters->update_entity_registry();
        turrets->update_entity_registry();
        spinners->update_entity_registry();
    }

    {
        // Before processing damage events
        // Read damage data and generate explicit entity handles
        TRACE_CPUPROFILER_EVENT_SCOPE(
            Sandbox::ATestBatchOrchestrator::tick::filter_damage_candidates);
        entity_registry->filter_damage_candidates();
    }

    {
        TRACE_CPUPROFILER_EVENT_SCOPE(
            Sandbox::ATestBatchOrchestrator::tick::resolve_damage_targets);
        player_ship->resolve_damage_targets();
        capital_ships->resolve_damage_targets();
        capital_ship_fighters->resolve_damage_targets();
        turrets->resolve_damage_targets();
    }

    { // Resolve events such as damage
        TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestBatchOrchestrator::tick::commit_updates);
        entity_registry->commit_updates();
    }

    { // Apply changes such as damage from the registry
        TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestBatchOrchestrator::tick::sync_from_registry);
        player_ship->sync_from_registry();
        capital_ships->sync_from_registry();
        capital_ship_fighters->sync_from_registry();
        turrets->sync_from_registry();
    }

    { // Update visual state
        TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestBatchOrchestrator::tick::update_visuals);
        player_ship->update_visuals();
        capital_ships->update_visuals();
        capital_ship_fighters->update_visuals();
        turrets->update_visuals();
        spinners->update_visuals();
        lasers->update_visuals();
    }

    {
        TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestBatchOrchestrator::tick::end_tick);
        player_ship->end_tick();
        capital_ships->end_tick();
        capital_ship_fighters->end_tick();
        turrets->end_tick();
        spinners->end_tick();
        lasers->end_tick();
        entity_registry->end_tick();
    }
}
