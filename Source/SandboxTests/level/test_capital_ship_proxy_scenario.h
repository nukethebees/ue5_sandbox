#pragma once

#include <SandboxTests/support/SimulationTestScenario.h>

#include <SpaceGame/entities/ProxyEntityMap.h>
#include <SpaceGame/entities/TestEntityUniqueId.h>

#include <SandboxNative/RegistryEntityHandle.h>

namespace ml {
class FTestCapitalShipProxyScenario final : public FSimulationTestScenario {
    inline static FName const default_health_capital_name{TEXT("default_health_capital")};
    inline static FName const overridden_health_capital_name{TEXT("overridden_health_capital")};
  public:
    explicit FTestCapitalShipProxyScenario(FSimulationTestContext& context);
    void run() override;
    void tear_down() override;
  private:
    void spawn_proxies(UWorld& world, UTestSimulationConfig const& config);
    void resolve_proxy_handles(FProxyEntityMap const& proxy_entities);
    void check_proxy_healths();

    FRegistryEntityHandle default_health_handle;
    FRegistryEntityHandle overridden_health_handle;
    TestEntityUniqueId default_health_unique_id;
    TestEntityUniqueId overridden_health_unique_id;
    int32 default_health{0};
    int32 overridden_health{0};
    bool proxy_handles_bound{false};
};
}
