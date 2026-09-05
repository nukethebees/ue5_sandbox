#pragma once

#include <SandboxTests/support/SimulationTestScenario.h>

#include <SpaceGame/entities/TestTeam.h>

#include <SandboxCore/fixed_array.h>
#include <SandboxCore/time_series_data.h>
#include <SandboxNative/RegistryEntityHandle.h>

namespace ml {
void run_worldless_turret_line_of_sight_blocking(FAutomationTestBase& test,
                                                 FSoftTestAssertions& checks,
                                                 USpaceGameLevelConfig const& config);

class FTurretLineOfSightBlockingScenario final : public FSimulationTestScenario {

    static constexpr time_type initial_enemy_check_time{1.0};
    static constexpr time_type blocker_scheduled_spawn_time{2.0};
    static constexpr time_type blocker_grace_period{0.2};
    static constexpr time_type test_end_time{4.0};
    static constexpr int32 turret_count{2};

    struct FTurretInfo {
        FVector location;
        ETestTeam team;
    };
  public:
    explicit FTurretLineOfSightBlockingScenario(FSimulationTestContext& context);
    void run() override;
  private:
    void spawn_line_of_sight_blocker();
    void sample_laser_count(ATestBatchOrchestrator& orchestrator);
    void on_end_tick(ATestBatchOrchestrator& orchestrator);
    void initial_setup();
    void full_checks();

    FTimespan const timeout{0, 0, 4};
    TFixedArray<FTurretInfo, turret_count> const turret_infos{
        {{-5000.f, 0.f, 0.f}, ETestTeam::Blue},
        {{5000.f, 0.f, 0.f}, ETestTeam::Red},
    };
    TimeSeriesData<int32> laser_counts;
    TimeSeriesData<int32> laser_spawn_counts;
    TimeSeriesData<int32> entity_counts;
    TimeSeriesData<TArray<FRegistryEntityHandle>> target_handles;
    TimeSeriesData<TArray<FVector3f>> registry_locations;
    TOptional<time_type> blocker_spawn_time{NullOpt};
};
}
