#pragma once

#include <CoreMinimal.h>

class ATestCapitalShipProxy;
class UTestSimulationConfig;
class UWorld;

namespace ml {
auto spawn_capital_proxy(UWorld& world,
                         UTestSimulationConfig const& config,
                         FVector const& location) -> ATestCapitalShipProxy&;
}
