#pragma once

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace SandboxISMCBenchmark {
inline bool publish_result(FString const& directory,
                           FString const& run_id,
                           FString const& error,
                           TSharedPtr<FJsonObject> const& conditions) {
    if (directory.IsEmpty()) {
        return true;
    }
    auto const result{MakeShared<FJsonObject>()};
    result->SetNumberField(TEXT("schemaVersion"), 1);
    result->SetStringField(TEXT("runId"), run_id);
    result->SetBoolField(TEXT("complete"), error.IsEmpty());
    result->SetStringField(TEXT("error"), error);
    result->SetObjectField(TEXT("conditions"), conditions);
    FString json;
    auto const writer{TJsonWriterFactory<>::Create(&json)};
    if (!FJsonSerializer::Serialize(result, writer)) {
        return false;
    }
    auto const path{FPaths::Combine(directory, TEXT("result.json"))};
    auto const temporary{path + TEXT(".tmp")};
    IFileManager::Get().MakeDirectory(*directory, true);
    return FFileHelper::SaveStringToFile(json, *temporary) &&
           IFileManager::Get().Move(*path, *temporary, true, true);
}

inline bool publish_setup_failure(FString const& error) {
    FString directory;
    FString run_id;
    FParse::Value(FCommandLine::Get(), TEXT("SandboxISMCBenchmarkOutput="), directory);
    FParse::Value(FCommandLine::Get(), TEXT("SandboxISMCBenchmarkRunId="), run_id);
    return publish_result(directory, run_id, error, MakeShared<FJsonObject>());
}

inline bool terminal_result_exists() {
    FString directory;
    FParse::Value(FCommandLine::Get(), TEXT("SandboxISMCBenchmarkOutput="), directory);
    return !directory.IsEmpty() &&
           IFileManager::Get().FileExists(*FPaths::Combine(directory, TEXT("result.json")));
}
} // namespace SandboxISMCBenchmark
