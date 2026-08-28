#include "Benchmarks/VolumeHeatmap3D/VolumeHeatmap3DBenchmarkCommandlet.h"

#include "Benchmarks/VolumeHeatmap3D/VolumeHeatmap3DBenchmark.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "RHIGlobals.h"

DEFINE_LOG_CATEGORY_STATIC(LogVolumeHeatmap3DBenchmark, Log, All);

namespace {
void parse_list(FString const& params,
                TCHAR const* const key_text,
                int32 const minimum,
                int32 const maximum,
                TArray<int32>& output) {
    FString const key{key_text};
    auto const key_index{params.Find(key, ESearchCase::IgnoreCase)};
    if (key_index == INDEX_NONE) {
        return;
    }
    int32 const value_start{key_index + key.Len()};
    auto value_end{
        params.Find(TEXT(" "), ESearchCase::CaseSensitive, ESearchDir::FromStart, value_start)};
    if (value_end == INDEX_NONE) {
        value_end = params.Len();
    }
    auto values{params.Mid(value_start, value_end - value_start)};
    values.TrimQuotesInline();
    TArray<FString> entries;
    values.ParseIntoArray(entries, TEXT(","), true);
    TArray<int32> parsed;
    for (auto const& entry : entries) {
        int32 const value{FCString::Atoi(*entry)};
        if (value >= minimum && value <= maximum) {
            parsed.AddUnique(value);
        }
    }
    if (!parsed.IsEmpty()) {
        output = MoveTemp(parsed);
    }
}
} // namespace

UVolumeHeatmap3DBenchmarkCommandlet::UVolumeHeatmap3DBenchmarkCommandlet() {
    IsClient = true;
    IsEditor = true;
    LogToConsole = true;
    ShowErrorCount = true;
}

int32 UVolumeHeatmap3DBenchmarkCommandlet::Main(FString const& params) {
    if (GUsingNullRHI) {
        UE_LOG(LogVolumeHeatmap3DBenchmark,
               Error,
               TEXT("VolumeHeatmap3DBenchmark requires a real RHI."));
        return 1;
    }
    FVolumeHeatmap3DBenchmarkOptions options;
    FParse::Value(*params, TEXT("Warmup="), options.warmup_iterations);
    FParse::Value(*params, TEXT("Iterations="), options.measured_iterations);
    FParse::Value(*params, TEXT("FixedGrid="), options.fixed_grid_dimension);
    FParse::Value(*params, TEXT("FixedSlices="), options.fixed_slice_count);
    parse_list(params, TEXT("GridSizes="), 1, 128, options.grid_dimensions);
    parse_list(params, TEXT("SliceCounts="), 8, 256, options.slice_counts);
    if (options.warmup_iterations < 0 || options.measured_iterations <= 0 ||
        options.fixed_grid_dimension <= 0 || options.fixed_grid_dimension > 128 ||
        options.fixed_slice_count < 8 || options.fixed_slice_count > 256) {
        UE_LOG(LogVolumeHeatmap3DBenchmark, Error, TEXT("Invalid benchmark arguments."));
        return 1;
    }
    FString output_path;
    if (!FParse::Value(*params, TEXT("Output="), output_path)) {
        output_path = FPaths::Combine(FPaths::ProjectSavedDir(),
                                      TEXT("Benchmarks/VolumeHeatmap3DBenchmark.csv"));
    } else if (FPaths::IsRelative(output_path)) {
        output_path = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), output_path);
    }
    auto const report{run_volume_heatmap_3d_benchmark(options)};
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(output_path), true);
    if (!FFileHelper::SaveStringToFile(report.to_csv(), *output_path)) {
        UE_LOG(LogVolumeHeatmap3DBenchmark, Error, TEXT("Failed to write %s."), *output_path);
        return 1;
    }
    UE_LOG(LogVolumeHeatmap3DBenchmark, Display, TEXT("\n%s"), *report.to_text());
    UE_LOG(LogVolumeHeatmap3DBenchmark, Display, TEXT("Wrote %s"), *output_path);
    return 0;
}
