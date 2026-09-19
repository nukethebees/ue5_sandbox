#pragma once

#include <CoreMinimal.h>

#include <ioj/sim/fixed_tick_loop.h>

#include "FixedTickLoopConfig.generated.h"

USTRUCT()
struct SPACEGAMESIMULATION_API FFixedTickLoopConfig {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Sandbox")
    double tick_rate{60.0};

    UPROPERTY(EditAnywhere, Category = "Sandbox")
    double time_scale{1.0};
};

namespace ml {
[[nodiscard]] inline auto make_fixed_tick_loop(FFixedTickLoopConfig const& config)
    -> ::ioj::sim::FixedTickLoop {
    return {.tick_rate = config.tick_rate, .time_scale = config.time_scale};
}
}
