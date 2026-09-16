#pragma once

#include <SandboxTests/support/SimulationTestScenario.h>

#include <ioj/sim/entity_types.h>
#include <SpaceGame/entities/ProxyEntityMap.h>

namespace ml {
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

    ::ioj::sim::EntityUniqueId default_health_id{};
    ::ioj::sim::EntityUniqueId overridden_health_id{};
    int32 default_health{0};
    int32 overridden_health{0};
    bool proxy_ids_bound{false};
};
}
