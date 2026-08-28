#pragma once

#include <SandboxTests/support/SimulationTestScenario.h>

#include <SpaceGame/ships/fighters/TestCapitalShipFighters.h>

#include <SandboxCore/time_series_data.h>
#include <SandboxNative/RegistryEntityHandle.h>

namespace ml {
class FFightersStandbyTransitionScenario final : public FSimulationTestScenario {
    using Task = ATestCapitalShipFighters::Task;

    struct FSimulationSample {
        int32 capital_count{0};
        TArray<FRegistryEntityHandle> fighter_handles;
        TArray<Task> fighter_tasks;
        TArray<FVector3f> fighter_velocities;
    };

    static constexpr time_type pre_kill_wait{8.0};
    static constexpr time_type post_kill_wait{0.1};
    inline static FTimespan const timeout{0, 0, 2};
  public:
    explicit FFightersStandbyTransitionScenario(FSimulationTestContext& context);
    void run() override;
  private:
    void spawn_capitals(UWorld& world, USpaceGameLevelConfig const& config);
    void initial_setup();
    void on_end_tick(ATestBatchOrchestrator& orchestrator);
    void sample_values();
    void check_pre_kill_state(FSimulationSample const& sample);
    void check_post_kill_state(FSimulationSample const& sample);
    void full_checks();

    FRegistryEntityHandle enemy_capital{};
    TimeSeriesData<FSimulationSample> samples;
    TOptional<time_type> pre_kill_time{NullOpt};
    TOptional<time_type> post_kill_time{NullOpt};
};
}
