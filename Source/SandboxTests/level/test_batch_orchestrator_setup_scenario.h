#pragma once

#include <SandboxTests/support/SimulationTestScenario.h>

#include <SandboxCore/time_series_data.h>

namespace ml {
enum class EOrchestratorSetupScenario : uint8 {
    SpawnMissingActors,
    SimulationClockConversions,
    LevelTelemetry,
};

class FTestBatchOrchestratorSetupScenario final : public FSimulationTestScenario {
    struct FTelemetryObservation {
        uint64 completed_ticks{0};
        int32 telemetry_sample_count{0};
        uint64 last_telemetry_tick{0};
        int32 telemetry_entity_count{0};
        int32 registry_entity_count{0};
        int32 composition_sample_count{0};
        uint64 last_composition_tick{0};
        int32 telemetry_player_ship_count{0};
        int32 registry_player_ship_count{0};
        int32 kill_sample_count{0};
        uint64 last_kill_tick{0};
        int32 cumulative_kill_count{0};
        int32 slot_sample_count{0};
        int32 registry_slot_count{0};
        int32 issued_unique_id_sample_count{0};
        int32 issued_unique_id_count{0};
    };
  public:
    FTestBatchOrchestratorSetupScenario(FSimulationTestContext& context,
                                        EOrchestratorSetupScenario scenario);
    void run() override;
  private:
    void spawn_missing_actors();
    void simulation_clock_conversions();
    void level_telemetry();
    void begin_level_telemetry();
    void kill_telemetry_test_entity();
    void on_level_telemetry_end_tick(ATestBatchOrchestrator& orchestrator);
    void check_level_telemetry();

    EOrchestratorSetupScenario scenario_;
    TimeSeriesData<FTelemetryObservation> telemetry_observations;
    int32 telemetry_player_team_index{0};
    int32 initial_active_entity_count{0};
    int32 telemetry_samples_before_change{0};
    int32 composition_samples_before_change{0};
    int32 kill_samples_before_change{0};
    int32 initial_registry_slot_count{0};
    int32 initial_issued_unique_id_count{0};
};
}
