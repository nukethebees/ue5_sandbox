#pragma once

#include <ioj/sim/testing/level_sim_test_access.h>
#include <SpaceGame/simulation/TestBatchOrchestrator.h>

struct FTestBatchOrchestratorTestAccess {
    static void use_settings_file(ATestBatchOrchestrator& orchestrator, FString path) {
        check(!orchestrator.level_simulation_.IsSet());
        orchestrator.settings_file.FilePath = MoveTemp(path);
        orchestrator.settings_prepared_ = false;
    }

    static void queue_direct_damage_events(ATestBatchOrchestrator& orchestrator,
                                           ::ioj::sim::DirectDamageEvents::ConstView events) {
        check(orchestrator.level_simulation_.IsSet());
        ::ioj::sim::LevelSimTestAccess::queue_direct_damage_events(
            orchestrator.level_simulation_.GetValue(), events);
    }
    static void set_telemetry_metadata(ATestBatchOrchestrator& orchestrator,
                                       ::ioj::sim::LevelTelemetryRunMetadata metadata) {
        check(orchestrator.level_simulation_.IsSet());
        orchestrator.level_simulation_->set_telemetry_metadata(std::move(metadata));
    }
    static void complete_telemetry_run(ATestBatchOrchestrator& orchestrator,
                                       ::ioj::sim::LevelTelemetryRunEndReason reason) {
        check(orchestrator.level_simulation_.IsSet());
        orchestrator.level_simulation_->complete_telemetry_run(reason);
    }
};
