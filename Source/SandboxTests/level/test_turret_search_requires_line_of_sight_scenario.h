#pragma once

#include <SandboxTests/support/SimulationTestScenario.h>
#include <SandboxTests/support/TestSimulationDriver.h>

#include <SpaceGame/entities/ProxyEntityMap.h>

#include <SandboxCore/time_series_data.h>
#include <SandboxNative/RegistryEntityHandle.h>

namespace ml {
class FTurretSearchRequiresLineOfSightScenario final : public FSimulationTestScenario {
    using time_type = TestSimulationDriver::time_type;

    static constexpr time_type test_end_time{1.0};
  public:
    explicit FTurretSearchRequiresLineOfSightScenario(FSimulationTestContext& context);
    void run() override;
    void tear_down() override;
  private:
    void bind(FProxyEntityMap const& proxy_entities);
    void on_end_tick(ATestBatchOrchestrator& orchestrator);
    void initial_setup();
    void full_checks();

    FTimespan const timeout{0, 0, 2};
    FName const blue_turret_name{TEXT("BlueTurret")};
    FName const blocked_enemy_name{TEXT("BlockedEnemy")};
    FName const visible_enemy_name{TEXT("VisibleEnemy")};
    TOptional<TestSimulationDriver> test_driver{NullOpt};
    TimeSeriesData<TArray<FRegistryEntityHandle>> target_handles;
    FRegistryEntityHandle blocked_enemy_handle;
    FRegistryEntityHandle visible_enemy_handle;
};
}
