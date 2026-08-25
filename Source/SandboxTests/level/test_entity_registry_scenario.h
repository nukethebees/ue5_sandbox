#pragma once

#include <SandboxTests/support/SimulationTestScenario.h>

#include <Sandbox/batch_game/test_entity_registry/TestEntityRegistry.h>

#include <SandboxCore/time_series_data.h>

namespace ml {
enum class EEntityRegistryScenario : uint8 { TeamCounts, OnePlayerKill, TwoPlayerKills };

class FEntityRegistryScenario final : public FSimulationTestScenario {

    struct FVariableKillSample {
        int32 player_kills{0};
        int32 total_kills{0};
        int32 alive_count{0};
    };

    static constexpr time_type team_count_test_time{0.1};
    static constexpr time_type before_kill_time{0.05};
    static constexpr time_type kill_time{0.1};
    static constexpr time_type after_kill_time{0.3};
    static constexpr time_type variable_kill_test_duration{0.35};
  public:
    FEntityRegistryScenario(FSimulationTestContext& context, EEntityRegistryScenario scenario);
    void run() override;
  private:
    void spawn_fixture();
    void spawn_player();
    void begin_team_count_scenario();
    void sample_team_counts();
    void on_team_count_end_tick(ATestBatchOrchestrator& orchestrator);
    void check_team_counts();
    void begin_variable_kill_scenario();
    void on_variable_kill_end_tick(ATestBatchOrchestrator& orchestrator);
    void check_variable_kill_results();

    EEntityRegistryScenario scenario_;
    TimeSeriesData<FTestEntityRegistry::TeamCounts> alive_per_team;
    TimeSeriesData<FTestEntityRegistry::EntityCounts> alive_per_team_and_type;
    TimeSeriesData<FVariableKillSample> kill_samples;
    TestEntityUniqueId player_id;
    int32 expected_kills{0};
    int32 initial_alive_count{0};
};
}
