#include "TestBatchOrchestrator.h"

#include "Sandbox/logging/SandboxLogCategories.h"
#include "Sandbox/misc/learning/TestCapitalShipFighters.h"
#include "Sandbox/misc/learning/TestCapitalShips.h"
#include "Sandbox/misc/learning/TestLasers.h"
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

    tick_counter = 0;

    ml::fatal_if_uobject_ptrs_invalid({
        SANDBOX_NAMED_UOBJECT_PTR(lasers),
        SANDBOX_NAMED_UOBJECT_PTR(capital_ships),
        SANDBOX_NAMED_UOBJECT_PTR(capital_ship_fighters),
        SANDBOX_NAMED_UOBJECT_PTR(turrets),
        SANDBOX_NAMED_UOBJECT_PTR(spinners),
        SANDBOX_NAMED_UOBJECT_PTR(entity_registry),
    });

    ml::invoke_on_all(
        [](AActor* actor) {
            ml::fatal_if_actor_transform_not_identity(*actor);
            ml::fatal_if_actor_root_not_static(*actor);

            UE_LOG(LogSandbox,
                   Display,
                   TEXT("ATestBatchOrchestrator::BeginPlay Disabling tick on: %s"),
                   *ml::get_best_display_name(*actor));
            actor->SetActorTickEnabled(false);
        },
        lasers,
        capital_ships,
        capital_ship_fighters,
        turrets,
        spinners);

    capital_ships->begin_play();

    entity_registry->commit_updates();
    entity_registry->end_frame();
}
void ATestBatchOrchestrator::Tick(float dt) {
    Super::Tick(dt);

    tick(dt);
}
void ATestBatchOrchestrator::tick(float const dt) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestBatchOrchestrator::tick);

    ++tick_counter;

    capital_ships->tick(dt);
    capital_ship_fighters->tick(dt);
    turrets->tick(dt);
    spinners->tick(dt);

    // Lasers must resolve after everything else
    lasers->tick(dt);

    entity_registry->commit_updates();

    capital_ships->sync_from_registry();
    capital_ships->update_visuals();

    entity_registry->end_frame();
}
