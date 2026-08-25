#pragma once

#include <SandboxTests/support/SimulationTestScenario.h>

#include <SpaceGame/entities/ProxyEntityMap.h>

#include <SandboxNative/RegistryEntityHandle.h>

namespace ml {
class FSpatialQueryLineOfSightScenario final : public FSimulationTestScenario {
    static constexpr float distance{30000.f};
    static constexpr float spawn_cooldown{999.f};
  public:
    explicit FSpatialQueryLineOfSightScenario(FSimulationTestContext& context);
    void run() override;
  private:
    void on_tear_down() override;
    void bind(FProxyEntityMap const& proxies);
    void run_queries();
    void on_end_tick(ATestBatchOrchestrator& orchestrator);

    TArray<FName> names{TEXT("North"), TEXT("South"), TEXT("East"), TEXT("West")};
    TArray<FVector3f> locations{
        {0.f, distance, 0.f}, {0.f, -distance, 0.f}, {distance, 0.f, 0.f}, {-distance, 0.f, 0.f}};
    TArray<FRegistryEntityHandle> expected;
};
}
