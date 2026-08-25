#pragma once

#include <SandboxTests/support/SimulationTestScenario.h>
#include <SandboxTests/support/TestSimulationDriver.h>

#include <SpaceGame/entities/ProxyEntityMap.h>

#include <SandboxCore/time_series_data.h>

namespace ml {
enum class ELaserLifecycleScenario : uint8 { Hit, SimultaneousLethalHits, Miss, WorldBlocker };

class FLaserLifecycleScenario final : public FSimulationTestScenario {
    struct FSample {
        int32 active_lasers{0};
        int32 total_spawned{0};
        int32 target_health{0};
        int32 alive_entities{0};
        int32 kills{0};
    };
  public:
    FLaserLifecycleScenario(FSimulationTestContext& context, ELaserLifecycleScenario scenario);
    void run() override;
  private:
    void on_tear_down() override;
    void spawn_fixture();
    void bind_fixture(FProxyEntityMap const& proxy_entities);
    void begin_scenario();
    void queue_projectiles();
    void on_end_tick(ATestBatchOrchestrator& orchestrator);
    void check_scenario();

    ELaserLifecycleScenario scenario_;
    TOptional<TestSimulationDriver> driver{NullOpt};
    TimeSeriesData<FSample> samples;
    FRegistryEntityHandle shooter_handle;
    FRegistryEntityHandle target_handle;
};
}
