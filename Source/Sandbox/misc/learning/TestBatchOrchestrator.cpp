#include "TestBatchOrchestrator.h"

#include "Sandbox/logging/SandboxLogCategories.h"
#include "Sandbox/utilities/actor_utils.h"
#include "TestCapitalShipFighters.h"
#include "TestCapitalShips.h"
#include "TestLasers.h"
#include "TestStaticTurrets.h"
#include "TestTubeSpinners.h"

#include <SandboxCore/invoke.h>
#include <SandboxCore/uobject_utils.h>

ATestBatchOrchestrator::ATestBatchOrchestrator() {
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;
}

void ATestBatchOrchestrator::BeginPlay() {
    Super::BeginPlay();

    ml::fatal_if_uobject_ptrs_invalid({
        SANDBOX_NAMED_UOBJECT_PTR(lasers),
        SANDBOX_NAMED_UOBJECT_PTR(capital_ships),
        SANDBOX_NAMED_UOBJECT_PTR(capital_ship_fighters),
        SANDBOX_NAMED_UOBJECT_PTR(turrets),
        SANDBOX_NAMED_UOBJECT_PTR(spinners),
        SANDBOX_NAMED_UOBJECT_PTR(entity_registry),
    });

    ml::invoke_on_all(
        [](AActor* ptr) {
            UE_LOG(LogSandbox,
                   Display,
                   TEXT("ATestBatchOrchestrator::BeginPlay Disabling tick on: %s"),
                   *ml::get_best_display_name(*ptr));
            ptr->SetActorTickEnabled(false);
        },
        lasers,
        capital_ships,
        capital_ship_fighters,
        turrets,
        spinners);
}
void ATestBatchOrchestrator::Tick(float dt) {
    Super::Tick(dt);

    tick(dt);
}
void ATestBatchOrchestrator::tick(float const dt) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestBatchOrchestrator::tick);

    lasers->tick(dt);
    capital_ships->tick(dt);
    capital_ship_fighters->tick(dt);
    turrets->tick(dt);
    spinners->tick(dt);
}
