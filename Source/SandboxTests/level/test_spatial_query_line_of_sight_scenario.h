#pragma once

#include <SandboxTests/support/SimulationTestScenario.h>
#include <SandboxTests/support/TestSimulationDriver.h>

#include <Sandbox/batch_game/ProxyEntityMap.h>

#include <SandboxNative/RegistryEntityHandle.h>

namespace ml {
class FSpatialQueryLineOfSightScenario final : public FSimulationTestScenario {
    static constexpr float distance{30000.f};
    static constexpr float spawn_cooldown{999.f};
  public:
    explicit FSpatialQueryLineOfSightScenario(FSimulationTestContext& context);
    void run() override;
    void tear_down() override;
  private:
    void bind(FProxyEntityMap const& proxies);
    void run_queries();
    void on_end_tick(ATestBatchOrchestrator& orchestrator);

    TOptional<TestSimulationDriver> driver{NullOpt};
    TArray<FName> names{TEXT("North"), TEXT("South"), TEXT("East"), TEXT("West")};
    TArray<FVector3f> locations{
        {0.f, distance, 0.f}, {0.f, -distance, 0.f}, {distance, 0.f, 0.f}, {-distance, 0.f, 0.f}};
    TArray<FRegistryEntityHandle> expected;
};
}
