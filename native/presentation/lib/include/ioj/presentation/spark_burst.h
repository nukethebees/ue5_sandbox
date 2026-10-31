#pragma once

#include <ioj/presentation/spark_burst_emission.h>
#include <ioj/presentation/spark_burst_style.h>

namespace ioj::presentation {
struct SparkBurst {
    SparkBurstEmission emission;
    SparkBurstStyle style;
};
}
