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
    if (auto* index_ptr = PathToIndex.Find(VirtualPath)) {
        auto const& cached_entry = CacheEntries[*index_ptr];
        return cached_entry.bExists;
    }

    auto resolved_path{ResolveVirtualPath(VirtualPath)};
    bool file_exists{false};

    if (!resolved_path.IsEmpty()) {
        file_exists = FPaths::FileExists(resolved_path);
    }

    auto new_index{CacheEntries.Add(FCachedPathResult(VirtualPath, resolved_path, file_exists))};
    PathToIndex.Add(VirtualPath, new_index);

    return file_exists;
}

TOptional<FString>
    UUSFPathValidationSubsystem::ResolveUSFPath_Internal(FString const& VirtualPath) {
    if (auto* index_ptr{PathToIndex.Find(VirtualPath)}) {
        auto const& cached_entry{CacheEntries[*index_ptr]};
        if (cached_entry.bExists) {
            return cached_entry.ResolvedPath;
        }
        return {};
    }

    auto resolved_path{ResolveVirtualPath(VirtualPath)};
    bool file_exists{false};

    if (!resolved_path.IsEmpty()) {
        file_exists = FPaths::FileExists(resolved_path);
    }

    auto new_index{CacheEntries.Add(FCachedPathResult(VirtualPath, resolved_path, file_exists))};
    PathToIndex.Add(VirtualPath, new_index);

    return file_exists ? TOptional<FString>(resolved_path) : TOptional<FString>{};
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

    auto const& shader_directory_mappings{AllShaderSourceDirectoryMappings()};

    for (auto const& mapping : shader_directory_mappings) {
        auto const& virtual_prefix{mapping.Key};
        auto const& real_directory{mapping.Value};

        if (VirtualPath.StartsWith(virtual_prefix)) {
            auto remaining_path{VirtualPath.Mid(virtual_prefix.Len())};

            if (!remaining_path.StartsWith(TEXT("/")) && !real_directory.EndsWith(TEXT("/")) &&
                !real_directory.EndsWith(TEXT("\\"))) {
                remaining_path = TEXT("/") + remaining_path;
            }

            auto resolved_path{real_directory + remaining_path};

            FPaths::NormalizeFilename(resolved_path);
            return resolved_path;
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
