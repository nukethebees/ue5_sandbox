#pragma once

#include <ioj/sim/entities/entity_id.h>
#include <SandboxTests/support/SimulationTestScenario.h>
#include <SpaceGame/entities/ProxyEntityMap.h>

namespace ioj {
class FTestCapitalShipProxyScenario final : public FSimulationTestScenario {
    inline static FName const default_health_capital_name{TEXT("default_health_capital")};
    inline static FName const overridden_health_capital_name{TEXT("overridden_health_capital")};
  public:
    explicit FTestCapitalShipProxyScenario(FSimulationTestContext& context);
    void run() override;
  private:
    void on_tear_down() override;
    void spawn_proxies(UWorld& world, USpaceGameLevelConfig const& config);
    void resolve_proxy_ids(FProxyEntityMap const& proxy_entities);
    void check_proxy_healths();

    sim::EntityId default_health_id{};
    sim::EntityId overridden_health_id{};
    int32 default_health{0};
    int32 overridden_health{0};
    bool proxy_ids_bound{false};
};
}
