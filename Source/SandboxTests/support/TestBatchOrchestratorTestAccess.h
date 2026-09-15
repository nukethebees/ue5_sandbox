#pragma once

#include <ioj/sim/testing/level_sim_test_access.h>
#include <SpaceGame/simulation/TestBatchOrchestrator.h>

struct FTestBatchOrchestratorTestAccess {
    static void queue_direct_damage_events(ATestBatchOrchestrator& orchestrator,
                                           ::ioj::sim::DirectDamageEventsConstView events) {
        check(orchestrator.level_simulation_.IsSet());
        ::ioj::sim::LevelSimTestAccess::queue_direct_damage_events(
            orchestrator.level_simulation_.GetValue(), events);
    }
    static void begin_telemetry_run(ATestBatchOrchestrator& orchestrator,
                                    ::ioj::sim::LevelTelemetryRunMetadata metadata) {
        check(orchestrator.level_simulation_.IsSet());
        ::ioj::sim::LevelSimTestAccess::begin_telemetry_run(
            orchestrator.level_simulation_.GetValue(), std::move(metadata));
    }
    static void capture_realtime_telemetry_sample(ATestBatchOrchestrator& orchestrator) {
        check(orchestrator.level_simulation_.IsSet());
        ::ioj::sim::LevelSimTestAccess::capture_realtime_telemetry_sample(
            orchestrator.level_simulation_.GetValue());
    }
    static void complete_telemetry_run(ATestBatchOrchestrator& orchestrator,
                                       ::ioj::sim::LevelTelemetryRunEndReason reason) {
        check(orchestrator.level_simulation_.IsSet());
        orchestrator.level_simulation_->complete_telemetry_run(reason);
    }
};
