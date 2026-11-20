#pragma once
#include "SimulationTestAssets.h"
#include <ioj/sim/testing/level_sim_test_access.h>
#include <SpaceGame/simulation/TestBatchOrchestrator.h>

struct FTestBatchOrchestratorTestAccess {
    static auto level_config(ioj::ATestBatchOrchestrator& orchestrator)
        -> ioj::USpaceGameLevelConfig* {
        return orchestrator.level_config.Get();
    }
    static auto level_presentation(ioj::ATestBatchOrchestrator const& orchestrator)
        -> ioj::FLevelPresentation const* {
        return orchestrator.presentation_host_.get_presentation();
    }
    static auto presentation_resources(ioj::ATestBatchOrchestrator const& orchestrator)
        -> ioj::FLevelPresentationResources {
        return orchestrator.get_presentation_resources(ioj::default_test_settings());
    }

    static void
        queue_direct_damage_events(ioj::ATestBatchOrchestrator& orchestrator,
                                   ::ioj::sim::DirectDamageEvents::CompactConstView events) {
        check(orchestrator.level_simulation_.IsSet());
        ::ioj::sim::LevelSimTestAccess::queue_direct_damage_events(
            orchestrator.level_simulation_.GetValue(), events);
    }
    static void stop_run(ioj::ATestBatchOrchestrator& orchestrator,
                         ::ioj::sim::RunEndReason reason) {
        check(orchestrator.level_simulation_.IsSet());
        orchestrator.level_simulation_->stop_run(reason);
    }
};
