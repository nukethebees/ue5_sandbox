#include "USFPathValidationSubsystem.h"

#include "Editor.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/Paths.h"
#include "ShaderCore.h"

// Static convenience methods - handle subsystem access automatically

bool UUSFPathValidationSubsystem::ValidateUSFPath(FString const& VirtualPath) {
    if (auto* Subsystem = GetSubsystem()) {
        return Subsystem->ValidateUSFPath_Internal(VirtualPath);
    }
    return false;
}

TOptional<FString> UUSFPathValidationSubsystem::ResolveUSFPath(FString const& VirtualPath) {
    if (auto* Subsystem = GetSubsystem()) {
        return Subsystem->ResolveUSFPath_Internal(VirtualPath);
    }
    return {};
}

void UUSFPathValidationSubsystem::ClearCache() {
    if (auto* Subsystem = GetSubsystem()) {
        Subsystem->ClearCache_Internal();
    }
}

int32 UUSFPathValidationSubsystem::GetCacheSize() {
    if (auto* Subsystem = GetSubsystem()) {
        return Subsystem->GetCacheSize_Internal();
    }
    return 0;
}

// Instance implementation methods

bool UUSFPathValidationSubsystem::ValidateUSFPath_Internal(FString const& VirtualPath) {
    if (auto* IndexPtr = PathToIndex.Find(VirtualPath)) {
        auto const& Cached = CacheEntries[*IndexPtr];
        return Cached.bExists;
    }

    auto ResolvedPath{ResolveVirtualPath(VirtualPath)};
    bool bExists{false};

    if (!ResolvedPath.IsEmpty()) {
        bExists = FPaths::FileExists(ResolvedPath);
    }

    auto NewIndex{CacheEntries.Add(FCachedPathResult(VirtualPath, ResolvedPath, bExists))};
    PathToIndex.Add(VirtualPath, NewIndex);

    return bExists;
}

TOptional<FString>
    UUSFPathValidationSubsystem::ResolveUSFPath_Internal(FString const& VirtualPath) {
    if (auto* IndexPtr = PathToIndex.Find(VirtualPath)) {
        auto const& Cached = CacheEntries[*IndexPtr];
        if (Cached.bExists) {
            return Cached.ResolvedPath;
        }
        return {};
    }

    auto ResolvedPath{ResolveVirtualPath(VirtualPath)};
    bool bExists{false};

    if (!ResolvedPath.IsEmpty()) {
        bExists = FPaths::FileExists(ResolvedPath);
    }

    auto NewIndex{CacheEntries.Add(FCachedPathResult(VirtualPath, ResolvedPath, bExists))};
    PathToIndex.Add(VirtualPath, NewIndex);

    return bExists ? TOptional<FString>(ResolvedPath) : TOptional<FString>{};
}

void UUSFPathValidationSubsystem::ClearCache_Internal() {
    CacheEntries.Empty();
    PathToIndex.Empty();
    UE_LOG(LogTemp, Log, TEXT("USF path validation cache cleared"));
}

int32 UUSFPathValidationSubsystem::GetCacheSize_Internal() const {
    return CacheEntries.Num();
}

FString UUSFPathValidationSubsystem::ResolveVirtualPath(FString const& VirtualPath) const {
    // Early exit for non-virtual paths
    if (!VirtualPath.StartsWith(TEXT("/"))) {
        return VirtualPath; // Assume it's already a real path
    }

    auto const& ShaderDirectoryMappings = AllShaderSourceDirectoryMappings();

    for (auto const& Mapping : ShaderDirectoryMappings) {
        auto const& VirtualPrefix = Mapping.Key;
        auto const& RealDirectory = Mapping.Value;

        if (VirtualPath.StartsWith(VirtualPrefix)) {
            auto RemainingPath{VirtualPath.Mid(VirtualPrefix.Len())};

            if (!RemainingPath.StartsWith(TEXT("/")) && !RealDirectory.EndsWith(TEXT("/")) &&
                !RealDirectory.EndsWith(TEXT("\\"))) {
                RemainingPath = TEXT("/") + RemainingPath;
            }

            auto ResolvedPath{RealDirectory + RemainingPath};

            FPaths::NormalizeFilename(ResolvedPath);
            return ResolvedPath;
        }
    }

    UE_LOG(LogTemp,
           Warning,
           TEXT("USF path validation: No directory mapping found for virtual path: %s"),
           *VirtualPath);
    return FString{};
}

UUSFPathValidationSubsystem* UUSFPathValidationSubsystem::GetSubsystem() {
    if (GEditor) {
        return GEditor->GetEditorSubsystem<UUSFPathValidationSubsystem>();
    }
    return nullptr;
}
