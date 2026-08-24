#pragma once

#include <CoreMinimal.h>

#include "FixedTickLoop.generated.h"

USTRUCT()
struct SPACEGAME_API FFixedTickLoop {
    GENERATED_BODY()

    void initialise() {
        check(tick_rate > 0.0);
        check(time_scale > 0.0);

        tick_period = 1.0 / tick_rate;
        accumulator = 0.0;
    }

    void add_time(double const dt) { accumulator += dt * time_scale; }

    auto try_tick() -> bool {
        if (accumulator < tick_period) {
            return false;
        }

        accumulator -= tick_period;
        return true;
    }

    UPROPERTY(EditAnywhere, Category = "Sandbox")
    double tick_rate{60.0};

    UPROPERTY(EditAnywhere, Category = "Sandbox")
    double time_scale{1.0};

    double tick_period{0.0};
    double accumulator{0.0};
};
