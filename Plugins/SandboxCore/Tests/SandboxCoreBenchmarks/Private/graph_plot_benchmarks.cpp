#include <SandboxCore/graph_plot.h>

#include <catch2/benchmark/catch_benchmark.hpp>
#include "CoreMinimal.h"
#include "TestHarness.h"

TEST_CASE("SandboxCore.GraphPlot.Cache benchmarks", "[benchmark]") {
    auto benchmark_count{[](int32 const sample_count, char const* const name) {
        TArray<float> x;
        TArray<float> y;
        x.SetNumUninitialized(sample_count);
        y.SetNumUninitialized(sample_count);
        for (int32 i{0}; i < sample_count; ++i) {
            x[i] = static_cast<float>(i) * 0.01f;
            y[i] = FMath::Sin(static_cast<float>(i) * 0.01f);
        }
        if (sample_count > 10) {
            y[sample_count / 2] = 100.0f;
        }

        FGraphSeriesView const series{.name = FText::FromString(TEXT("benchmark")), .x = x, .y = y};
        FGraphRenderCache cache;
        uint64 revision{0};

        BENCHMARK(name) {
            ++revision;
            auto const changed{cache.set_series(TConstArrayView<FGraphSeriesView>{&series, 1}, revision)};
            auto const rebuilt{cache.update({1500.0f, 400.0f})};
            return changed && rebuilt ? cache.get_stats().emitted_point_count : 0;
        };

        REQUIRE(cache.get_stats().emitted_point_count <= 3002);

        if (sample_count == 1000000) {
            BENCHMARK("unchanged one-million cache key") {
                return cache.update({1500.0f, 400.0f});
            };
        }
    }};

    benchmark_count(100, "graph cache rebuild 100 samples");
    benchmark_count(1000, "graph cache rebuild 1,000 samples");
    benchmark_count(10000, "graph cache rebuild 10,000 samples");
    benchmark_count(100000, "graph cache rebuild 100,000 samples");
    benchmark_count(1000000, "graph cache rebuild 1,000,000 samples");
}
