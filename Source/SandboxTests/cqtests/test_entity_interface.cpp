#include <Sandbox/batch_game/test_entity_registry/TestEntityRegistry.h>
#include <Sandbox/batch_game/TestBatchOrchestrator.h>
#include <Sandbox/batch_game/TestCapitalShipFighters.h>
#include <Sandbox/batch_game/TestCapitalShips.h>
#include <Sandbox/core/SandboxDeveloperSettings.h>

#include <SandboxTests/cqtests/TestSimulationDriver.h>

#include <Components/MapTestSpawner.h>
#include <Containers/Set.h>
#include <CQTest.h>
#include <EngineUtils.h>
#include <Misc/Optional.h>

TEST_CLASS(EntityInterfaceTest, "Sandbox.FunctionalTests")
{
    ATestBatchOrchestrator const* orchestrator{nullptr};

    TEST_METHOD(MainTest)
    {}
};
