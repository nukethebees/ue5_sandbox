#pragma once

#include <CoreMinimal.h>

class ATestCapitalShipProxy;
class UTestSimulationConfig;
class UWorld;

namespace ml {
struct FSoftTestAssertions;

auto spawn_capital_proxy(UWorld& world,
                         UTestSimulationConfig const& config,
                         FSoftTestAssertions& checks,
                         FVector const& location) -> ATestCapitalShipProxy*;
}
