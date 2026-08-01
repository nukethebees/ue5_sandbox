#pragma once

#include "CoreTypes.h"

#include <optional>

struct FBenchmarkCliArgs {
    // A scale value, not always a true absolute
    // Different tests may want different counts
    // e.g. --benchmark_entities=1000
    // One test might have a base value of 16 -> 16k values
    // Another might use 100 -> 100k samples
    std::optional<int32> benchmark_entities;
};

[[nodiscard]] auto get_benchmark_cli_args() -> FBenchmarkCliArgs;
