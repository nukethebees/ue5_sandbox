#pragma once

#include <CoreMinimal.h>

#include <sandbox/simulation/fixed_tick.h>

#include "FixedTickLoop.generated.h"

USTRUCT()
struct SPACEGAMESIMULATION_API FFixedTickLoop {
    GENERATED_BODY()

    void initialise() {
        auto const succeeded{
            ml::simulation::initialise_tick_loop(tick_rate, time_scale, tick_period, accumulator)};
        check(succeeded);
    }

    void add_time(double const dt) {
        ml::simulation::add_tick_loop_time(dt, time_scale, accumulator);
    }

    auto try_tick() -> bool { return ml::simulation::try_consume_tick(tick_period, accumulator); }

    UPROPERTY(EditAnywhere, Category = "Sandbox")
    double tick_rate{60.0};

    UPROPERTY(EditAnywhere, Category = "Sandbox")
    double time_scale{1.0};

    double tick_period{0.0};
    double accumulator{0.0};
};
