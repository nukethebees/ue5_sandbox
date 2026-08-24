#pragma once

#include <SandboxTests/support/SimulationTestScenario.h>
#include <SandboxTests/support/TestSimulationDriver.h>

#include <SpaceGame/missions/TestMissionFailReason.h>
#include <SpaceGame/missions/TestMissionState.h>

#include <SandboxCore/time_series_data.h>

struct FTestMissionManager;

namespace ml {
enum class EMissionManagerScenario : uint8 {
    SurviveTime,
    KillEnemies,
    KillEnemiesWithinTime,
    DefenceObjective,
    RequiredKillsObjective,
    RequiredKillsTimeElapsed,
};

class FTestMissionManagerScenario final : public FSimulationTestScenario {
    using EScenario = EMissionManagerScenario;

    struct FSimulationSample {
        ETestMissionState mission_state{ETestMissionState::NotStarted};
        ETestMissionFailReason mission_fail_reason{ETestMissionFailReason::None};
        int32 mission_kills{0};
        TArray<int32> surviving_entity_health;
        TArray<int32> required_kill_entity_health;
    };

    static constexpr float short_mission_time{0.1f};
    static constexpr float long_mission_time{10.f};
    inline static FTimespan const timeout{0, 0, 2};
  public:
    FTestMissionManagerScenario(FSimulationTestContext& context, EScenario scenario);
    void run() override;
    void tear_down() override;
  private:
    static void configure_level(UWorld& world,
                                UTestSimulationConfig const& config,
                                FSoftTestAssertions& checks,
                                EScenario scenario);
    static void configure_mission_manager(UWorld& world,
                                          ATestBatchOrchestrator& orchestrator,
                                          EScenario scenario);
    void setup_scenario(EScenario new_scenario);
    void start_scenario();
    void on_end_tick(ATestBatchOrchestrator& orchestrator);
    void queue_enemy_kill(FTestMissionManager const& manager);
    void queue_defended_entity_kill(FTestMissionManager const& manager);
    void queue_required_entity_kill(FTestMissionManager const& manager);
    auto mission_has_ended() const -> bool;
    void check_scenario_result();

    TOptional<TestSimulationDriver> test_driver{NullOpt};
    EScenario scenario;
    TimeSeriesData<FSimulationSample> samples;
};
}
