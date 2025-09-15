#include "USFPathValidationSubsystem.h"

#include "Editor.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/Paths.h"
#include "ShaderCore.h"

// Static convenience methods - handle subsystem access automatically

bool UUSFPathValidationSubsystem::ValidateUSFPath(FString const& VirtualPath) {
    if (UUSFPathValidationSubsystem* Subsystem = GetSubsystem()) {
        return Subsystem->ValidateUSFPath_Internal(VirtualPath);
    }
    return false; // Subsystem not available
}

TOptional<FString> UUSFPathValidationSubsystem::ResolveUSFPath(FString const& VirtualPath) {
    if (UUSFPathValidationSubsystem* Subsystem = GetSubsystem()) {
        return Subsystem->ResolveUSFPath_Internal(VirtualPath);
    }
    return {}; // Subsystem not available
}

void UUSFPathValidationSubsystem::ClearCache() {
    if (UUSFPathValidationSubsystem* Subsystem = GetSubsystem()) {
        Subsystem->ClearCache_Internal();
    }
}

int32 UUSFPathValidationSubsystem::GetCacheSize() {
    if (UUSFPathValidationSubsystem* Subsystem = GetSubsystem()) {
        return Subsystem->GetCacheSize_Internal();
    }
    return 0;
}

// Instance implementation methods

bool UUSFPathValidationSubsystem::ValidateUSFPath_Internal(FString const& VirtualPath) {
    // Check cache first - O(1) lookup
    if (int32* IndexPtr = PathToIndex.Find(VirtualPath)) {
        FCachedPathResult const& Cached = CacheEntries[*IndexPtr];
        return Cached.bExists;
    }

    // Not cached - resolve and cache the result
    FString ResolvedPath = ResolveVirtualPath(VirtualPath);
    bool bExists = false;

    if (!ResolvedPath.IsEmpty()) {
        // Check if the resolved file actually exists
        bExists = FPaths::FileExists(ResolvedPath);
    }

    // Add to cache - permanent storage since USF files don't appear/disappear at runtime
    int32 NewIndex = CacheEntries.Add(FCachedPathResult(VirtualPath, ResolvedPath, bExists));
    PathToIndex.Add(VirtualPath, NewIndex);

    return bExists;
}

TOptional<FString>
    UUSFPathValidationSubsystem::ResolveUSFPath_Internal(FString const& VirtualPath) {
    // Check cache first
    if (int32* IndexPtr = PathToIndex.Find(VirtualPath)) {
        FCachedPathResult const& Cached = CacheEntries[*IndexPtr];
        if (Cached.bExists) {
            return Cached.ResolvedPath;
        }
        return {}; // Cached as not existing
    }

    // Not cached - resolve path
    FString ResolvedPath = ResolveVirtualPath(VirtualPath);
    bool bExists = false;

    if (!ResolvedPath.IsEmpty()) {
        bExists = FPaths::FileExists(ResolvedPath);
    }

    // Cache the result
    int32 NewIndex = CacheEntries.Add(FCachedPathResult(VirtualPath, ResolvedPath, bExists));
    PathToIndex.Add(VirtualPath, NewIndex);

    // Return resolved path only if file exists
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

    // Get all shader source directory mappings from UE5
    TMap<FString, FString> const& ShaderDirectoryMappings = AllShaderSourceDirectoryMappings();

    // Iterate through mappings to find matching prefix
    for (auto const& Mapping : ShaderDirectoryMappings) {
        FString const& VirtualPrefix = Mapping.Key;   // e.g., "/Engine/Private"
        FString const& RealDirectory = Mapping.Value; // e.g., "C:/UE5/Engine/Shaders/Private"

        // Check if virtual path starts with this prefix
        if (VirtualPath.StartsWith(VirtualPrefix)) {
            // Calculate remaining path after prefix
            FString RemainingPath = VirtualPath.Mid(VirtualPrefix.Len());

            // Ensure we don't have double slashes
            if (!RemainingPath.StartsWith(TEXT("/")) && !RealDirectory.EndsWith(TEXT("/")) &&
                !RealDirectory.EndsWith(TEXT("\\"))) {
                RemainingPath = TEXT("/") + RemainingPath;
            }

            // Build full resolved path
            FString ResolvedPath = RealDirectory + RemainingPath;

            // Normalize path separators for current platform
            FPaths::NormalizeFilename(ResolvedPath);

            return ResolvedPath;
        }
    }

    // No mapping found
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
