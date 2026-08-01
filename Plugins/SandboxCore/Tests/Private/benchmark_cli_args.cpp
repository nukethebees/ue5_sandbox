#include "benchmark_cli_args.h"

#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

auto get_benchmark_cli_args() -> FBenchmarkCliArgs {
    FBenchmarkCliArgs result;

    int32 benchmark_entities;
    if (FParse::Value(FCommandLine::Get(), TEXT("--benchmark-entities="), benchmark_entities)) {
        result.benchmark_entities = benchmark_entities;
    }

    return result;
}
