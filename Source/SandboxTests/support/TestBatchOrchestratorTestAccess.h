#pragma once

#include <ioj/sim/testing/level_sim_test_access.h>
#include <SpaceGame/simulation/TestBatchOrchestrator.h>
#include <SpaceGame/telemetry/LevelTelemetryIntegration.h>

struct FTestBatchOrchestratorTestAccess {
    static void use_settings_file(ATestBatchOrchestrator& orchestrator, FString path) {
        check(!orchestrator.level_simulation_.IsSet());
        orchestrator.settings_file.FilePath = MoveTemp(path);
        orchestrator.prepared_level_.Reset();
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
    static void stop_run(ATestBatchOrchestrator& orchestrator,
                         ::ioj::sim::LevelTelemetryRunEndReason reason) {
        check(orchestrator.level_simulation_.IsSet());
        orchestrator.level_simulation_->stop_run(reason);
    }
    static auto take_finalized_telemetry_report(ATestBatchOrchestrator& orchestrator)
        -> TOptional<::ioj::sim::LevelTelemetryReport> {
        check(orchestrator.level_simulation_.IsSet());
        return ::ioj::take_finalized_level_telemetry_report(
            orchestrator.level_simulation_.GetValue(),
            orchestrator.launch_options_,
            orchestrator.presentation_enabled);
    }
};
