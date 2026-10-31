#include "SandboxEditor/Commandlets/MergeSaveProfilesCommandlet.h"

#include <ioj/profiles/file_profile_storage.h>

#include <SandboxCoreEngine/strings.h>

#include <Misc/Parse.h>
#include <Misc/Paths.h>

UMergeSaveProfilesCommandlet::UMergeSaveProfilesCommandlet() {
    IsClient = false;
    IsEditor = true;
    LogToConsole = true;
}
int32 UMergeSaveProfilesCommandlet::Main(FString const& parameters) {
    FString roots_argument;
    FString destination;
    FString excluded_argument;
    if (!FParse::Value(*parameters, TEXT("SourceRoots="), roots_argument, false) ||
        !FParse::Value(*parameters, TEXT("DestinationRoot="), destination, false)) {
        UE_LOG(LogTemp, Error, TEXT("MergeSaveProfiles requires SourceRoots and DestinationRoot."));
        return 1;
    }
    FParse::Value(*parameters, TEXT("ExcludeProfiles="), excluded_argument, false);
    TArray<FString> roots;
    TArray<FString> excluded;
    roots_argument.ParseIntoArray(roots, TEXT(";"), true);
    excluded_argument.ParseIntoArray(excluded, TEXT(","), true);
    std::vector<std::filesystem::path> native_roots;
    std::vector<std::string> native_excluded;
    for (auto const& root : roots) {
        native_roots.emplace_back(*FPaths::ConvertRelativePathToFull(root));
    }
    for (auto const& id : excluded) {
        native_excluded.emplace_back(TCHAR_TO_UTF8(*id));
    }
    auto const result{ioj::profiles::merge_profile_files(
        native_roots,
        native_excluded,
        std::filesystem::path{*FPaths::ConvertRelativePathToFull(destination)})};
    if (!result) {
        UE_LOG(LogTemp, Error, TEXT("MergeSaveProfiles: %s"), *ioj::to_fstring(result.error()));
        return 1;
    }
    UE_LOG(LogTemp,
           Display,
           TEXT("MERGE_SAVE_PROFILES_RESULT\nstatus=success\nsources=%d\nsource_profiles=%"
                "llu\nprofiles=%llu\nsource_records=%llu\nunique_records=%llu\ndestination=%s"),
           roots.Num(),
           static_cast<unsigned long long>(result->source_profile_count),
           static_cast<unsigned long long>(result->profile_count),
           static_cast<unsigned long long>(result->source_record_count),
           static_cast<unsigned long long>(result->unique_record_count),
           *destination);
    return 0;
}
