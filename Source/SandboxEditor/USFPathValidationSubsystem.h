#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "Misc/Optional.h"

#include "USFPathValidationSubsystem.generated.h"

/**
 * Editor subsystem for validating and resolving USF (Unreal Shader File) virtual paths.
 * Provides permanent caching of path resolution to avoid expensive directory mapping iteration.
 * Cache never expires since UE5 requires project regeneration to recognize new USF files.
 */
UCLASS()
class UUSFPathValidationSubsystem : public UEditorSubsystem {
    GENERATED_BODY()
  public:
    // Static convenience API - handles subsystem access automatically

    /**
     * Validates whether a virtual USF path can be resolved and exists on disk.
     * Results are permanently cached for performance.
     *
     * @param VirtualPath Virtual shader path like "/Engine/Private/MyShader.usf"
     * @return true if path exists, false if not found or invalid
     */
    static bool ValidateUSFPath(FString const& VirtualPath);

    /**
     * Resolves a virtual USF path to an absolute filesystem path.
     * Results are permanently cached for performance.
     *
     * @param VirtualPath Virtual shader path like "/Engine/Private/MyShader.usf"
     * @return Resolved absolute path if successful, empty optional if failed
     */
    static TOptional<FString> ResolveUSFPath(FString const& VirtualPath);

    /**
     * Clears the path resolution cache. Use when adding new USF files to the project.
     */
    static void ClearCache();

    /**
     * Returns the number of cached path resolutions for debugging.
     */
    static int32 GetCacheSize();
  private:
    // Internal cache storage
    struct FCachedPathResult {
        FString VirtualPath;  // Original virtual path for debugging
        FString ResolvedPath; // Computed absolute filesystem path
        bool bExists{false};  // Whether file exists on disk

        FCachedPathResult() = default;
        FCachedPathResult(FString const& InVirtualPath,
                          FString const& InResolvedPath,
                          bool bInExists)
            : VirtualPath(InVirtualPath)
            , ResolvedPath(InResolvedPath)
            , bExists(bInExists) {}
    };

    // Memory-optimized storage: array for cache locality + map for O(1) lookup
    TArray<FCachedPathResult> CacheEntries;
    TMap<FString, int32> PathToIndex;

    // Instance methods (called by static API)
    bool ValidateUSFPath_Internal(FString const& VirtualPath);
    TOptional<FString> ResolveUSFPath_Internal(FString const& VirtualPath);
    void ClearCache_Internal();
    int32 GetCacheSize_Internal() const;

    /**
     * Resolves a virtual USF path to absolute filesystem path by iterating shader directory
     * mappings. This is the expensive operation we cache to avoid repeated prefix matching.
     *
     * @param VirtualPath Virtual path starting with / (e.g. "/Engine/Private/Common.ush")
     * @return Resolved absolute path or empty string if no mapping found
     */
    FString ResolveVirtualPath(FString const& VirtualPath) const;

    /**
     * Gets the subsystem instance, returning nullptr if not available.
     */
    static UUSFPathValidationSubsystem* GetSubsystem();
};
