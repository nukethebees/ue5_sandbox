#pragma once

#include <ioj/sim/testing/level_sim_test_access.h>
#include <SpaceGame/simulation/TestBatchOrchestrator.h>
#include <SpaceGame/telemetry/LevelTelemetryIntegration.h>

struct FTestBatchOrchestratorTestAccess {
    static void queue_direct_damage_events(ioj::ATestBatchOrchestrator& orchestrator,
                                           ::ioj::sim::DirectDamageEvents::ConstView events) {
        check(orchestrator.level_simulation_.IsSet());
        ::ioj::sim::LevelSimTestAccess::queue_direct_damage_events(
            orchestrator.level_simulation_.GetValue(), events);
    }
    static void stop_run(ioj::ATestBatchOrchestrator& orchestrator,
                         ::ioj::sim::telemetry::RunEndReason reason) {
        check(orchestrator.level_simulation_.IsSet());
        orchestrator.level_simulation_->stop_run(reason);
    }
    static auto take_finalized_telemetry_report(ioj::ATestBatchOrchestrator& orchestrator)
        -> TOptional<::ioj::sim::telemetry::Report> {
        check(orchestrator.level_simulation_.IsSet());
        return ::ioj::take_finalized_level_telemetry_report(
            orchestrator.level_simulation_.GetValue(),
            orchestrator.launch_options_,
            orchestrator.is_presentation_enabled());
    }
};
