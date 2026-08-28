#include "Benchmarks/Scatter3D/Scatter3DBenchmarkCommandlet.h"

#include "Benchmarks/Scatter3D/Scatter3DBenchmark.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "RHIGlobals.h"

DEFINE_LOG_CATEGORY_STATIC(LogScatter3DBenchmark, Log, All);

namespace {
void parse_scatter_point_counts(FString const& params, FScatter3DBenchmarkOptions& options) {
    FString const key{TEXT("PointCounts=")};
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
    auto values{params.Mid(value_start, value_end - value_start)};
    values.TrimQuotesInline();

    TArray<FString> entries;
    values.ParseIntoArray(entries, TEXT(","), true);
    TArray<int32> parsed;
    for (auto const& entry : entries) {
        auto const point_count{FCString::Atoi(*entry)};
        if (point_count > 0 && point_count <= 65536) {
            parsed.AddUnique(point_count);
        }
    }
    if (!parsed.IsEmpty()) {
        options.point_counts = MoveTemp(parsed);
    }
}
} // namespace

UScatter3DBenchmarkCommandlet::UScatter3DBenchmarkCommandlet() {
    IsClient = true;
    IsEditor = true;
    LogToConsole = true;
    ShowErrorCount = true;
}

int32 UScatter3DBenchmarkCommandlet::Main(FString const& params) {
    if (GUsingNullRHI) {
        UE_LOG(LogScatter3DBenchmark,
               Error,
               TEXT("Scatter3DBenchmark requires a real RHI. Use -RenderOffscreen, not -NullRHI."));
        return 1;
    }

    FScatter3DBenchmarkOptions options;
    FParse::Value(*params, TEXT("Warmup="), options.warmup_iterations);
    FParse::Value(*params, TEXT("Iterations="), options.measured_iterations);
    parse_scatter_point_counts(params, options);
    if (options.warmup_iterations < 0 || options.measured_iterations <= 0) {
        UE_LOG(
            LogScatter3DBenchmark, Error, TEXT("Warmup must be >= 0 and Iterations must be > 0."));
        return 1;
    }

    FString output_path;
    if (!FParse::Value(*params, TEXT("Output="), output_path)) {
        output_path =
            FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Benchmarks/Scatter3DBenchmark.csv"));
    } else if (FPaths::IsRelative(output_path)) {
        output_path = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), output_path);
    }

    auto const report{run_scatter_3d_benchmark(options)};
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(output_path), true);
    if (!FFileHelper::SaveStringToFile(report.to_csv(), *output_path)) {
        UE_LOG(LogScatter3DBenchmark, Error, TEXT("Failed to write %s."), *output_path);
        return 1;
    }

    UE_LOG(LogScatter3DBenchmark, Display, TEXT("\n%s"), *report.to_text());
    UE_LOG(LogScatter3DBenchmark, Display, TEXT("Wrote %s"), *output_path);
    return 0;
}
