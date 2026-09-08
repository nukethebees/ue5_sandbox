#pragma once

#include <Containers/Array.h>
#include "benchmark_cli_args.h"
#include "TestHarness.h"

namespace ml::single_allocation_benchmarks {
inline auto counts() -> TArray<int32> {
    auto const args{get_benchmark_cli_args()};
    if (args.benchmark_entities) {
        REQUIRE(*args.benchmark_entities > 0);
        REQUIRE(*args.benchmark_entities <= 1048576);
        return {*args.benchmark_entities};
    }
    return {4096, 65536, 1048576};
}

}
