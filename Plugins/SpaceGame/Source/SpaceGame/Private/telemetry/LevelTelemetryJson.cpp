#include <SpaceGame/telemetry/LevelTelemetryJson.h>

#include <ioj/sim/telemetry/report_json.h>

#include <SandboxCoreEngine/strings.h>

#include <HAL/FileManager.h>
#include <Misc/FileHelper.h>
#include <Misc/Paths.h>

auto level_telemetry_runs_directory() -> FString {
    return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Telemetry"), TEXT("Runs"));
}

auto read_level_telemetry_run(FString const& path)
    -> std::expected<ioj::sim::LevelTelemetryReport, FString> {
    FString json;
    if (!FFileHelper::LoadFileToString(json, *path)) {
        return std::unexpected{FString::Printf(TEXT("Could not read telemetry file '%s'"), *path)};
    }
    auto result{ioj::sim::deserialize_level_telemetry_report(ioj::to_string(json))};
    if (!result) {
        return std::unexpected{
            FString::Printf(TEXT("%s: %s"), *path, *ioj::to_fstring(result.error()))};
    }
    return std::move(*result);
}

auto write_level_telemetry_run(ioj::sim::LevelTelemetryReport const& record,
                               FString const& output_directory) -> std::expected<FString, FString> {
    auto const serialized{ioj::sim::serialize_level_telemetry_report(record)};
    if (!serialized) {
        return std::unexpected{ioj::to_fstring(serialized.error())};
    }
    auto const json{ioj::to_fstring(*serialized)};
    if (!IFileManager::Get().MakeDirectory(*output_directory, true)) {
        return std::unexpected{
            FString::Printf(TEXT("Could not create output directory '%s'"), *output_directory)};
    }

    FString timestamp{UTF8_TO_TCHAR(record.metadata.launched_utc.c_str())};
    timestamp.ReplaceCharInline(TEXT(':'), TEXT('-'));
    FString level_name{UTF8_TO_TCHAR(
        (record.metadata.level_id.empty() ? record.metadata.map_name : record.metadata.level_id)
            .c_str())};
    level_name = FPaths::MakeValidFileName(level_name);
    auto const short_run_id{FString{UTF8_TO_TCHAR(record.metadata.run_id.c_str())}.Left(8)};
    auto const filename{
        FString::Printf(TEXT("%s_%s_%s.json"), *timestamp, *level_name, *short_run_id)};
    auto const path{FPaths::Combine(output_directory, filename)};
    auto const temporary_path{path + TEXT(".tmp")};
    if (!FFileHelper::SaveStringToFile(
            json, *temporary_path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)) {
        return std::unexpected{
            FString::Printf(TEXT("Could not write temporary file '%s'"), *temporary_path)};
    }
    if (!IFileManager::Get().Move(*path, *temporary_path)) {
        IFileManager::Get().Delete(*temporary_path, false, true);
        return std::unexpected{FString::Printf(TEXT("Could not replace output file '%s'"), *path)};
    }
    return path;
}
