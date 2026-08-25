#include "Benchmarks/Heatmap/HeatmapBenchmarkCommandlet.h"

#include "Benchmarks/Heatmap/HeatmapBenchmark.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "RHIGlobals.h"

DEFINE_LOG_CATEGORY_STATIC(LogHeatmapBenchmark, Log, All);

namespace {
void parse_resolutions(FString const& params, FHeatmapBenchmarkOptions& options) {
    FString const key{TEXT("Resolutions=")};
    auto const key_index{params.Find(key, ESearchCase::IgnoreCase)};
    if (key_index == INDEX_NONE) {
        return;
    }

    auto const value_start{key_index + key.Len()};
    auto value_end{
        params.Find(TEXT(" "), ESearchCase::CaseSensitive, ESearchDir::FromStart, value_start)};
    if (value_end == INDEX_NONE) {
        value_end = params.Len();
    }
    auto resolutions{params.Mid(value_start, value_end - value_start)};
    resolutions.TrimQuotesInline();

    TArray<FString> entries;
    resolutions.ParseIntoArray(entries, TEXT(","), true);
    TArray<int32> parsed;
    for (auto const& entry : entries) {
        auto const resolution{FCString::Atoi(*entry)};
        if (resolution > 0 && resolution <= 512) {
            parsed.AddUnique(resolution);
        }
    }
    if (!parsed.IsEmpty()) {
        options.resolutions = MoveTemp(parsed);
    }
}
}

UHeatmapBenchmarkCommandlet::UHeatmapBenchmarkCommandlet() {
    IsClient = true;
    IsEditor = true;
    LogToConsole = true;
    ShowErrorCount = true;
}

int32 UHeatmapBenchmarkCommandlet::Main(FString const& params) {
    if (GUsingNullRHI) {
        UE_LOG(LogHeatmapBenchmark,
               Error,
               TEXT("HeatmapBenchmark requires a real RHI. Use -RenderOffscreen, not -NullRHI."));
        return 1;
    }

    FHeatmapBenchmarkOptions options;
    FParse::Value(*params, TEXT("Warmup="), options.warmup_iterations);
    FParse::Value(*params, TEXT("Iterations="), options.measured_iterations);
    parse_resolutions(params, options);
    if (options.warmup_iterations < 0 || options.measured_iterations <= 0) {
        UE_LOG(LogHeatmapBenchmark, Error, TEXT("Warmup must be >= 0 and Iterations must be > 0."));
        return 1;
    }

    FString output_path;
    if (!FParse::Value(*params, TEXT("Output="), output_path)) {
        output_path =
            FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Benchmarks/HeatmapBenchmark.csv"));
    } else if (FPaths::IsRelative(output_path)) {
        output_path = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), output_path);
    }

    auto const report{run_heatmap_benchmark(options)};
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(output_path), true);
    if (!FFileHelper::SaveStringToFile(report.to_csv(), *output_path)) {
        UE_LOG(LogHeatmapBenchmark, Error, TEXT("Failed to write %s."), *output_path);
        return 1;
    }

    UE_LOG(LogHeatmapBenchmark, Display, TEXT("\n%s"), *report.to_text());
    UE_LOG(LogHeatmapBenchmark, Display, TEXT("Wrote %s"), *output_path);
    return 0;
}
