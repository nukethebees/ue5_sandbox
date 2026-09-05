#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"

#include "SandboxUI/EntityOverlay/EntityOverlayTypes.h"

struct SANDBOXUI_API FEntityOverlayBenchmarkResult {
    FString stage;
    int32 candidate_count{0};
    int32 sample_count{0};
    double minimum_microseconds{0.0};
    double median_microseconds{0.0};
    double percentile_95_microseconds{0.0};
    double maximum_microseconds{0.0};
};

struct SANDBOXUI_API FEntityOverlayBenchmarkReport {
    TArray<FEntityOverlayBenchmarkResult> results;

    [[nodiscard]] auto to_csv() const -> FString;
    [[nodiscard]] auto to_text() const -> FString;
};

SANDBOXUI_API auto run_entity_overlay_benchmark(int32 warmup_iterations = 3,
                                                int32 measured_iterations = 20)
    -> FEntityOverlayBenchmarkReport;

SANDBOXUI_API auto write_entity_overlay_debug_image(FEntityOverlayFramePtr frame,
                                                    FEntityOverlayView const& view,
                                                    FString const& output_path) -> bool;
