#include <SpaceGame/settings/LevelSettingsFile.h>

#if WITH_EDITOR
#include <SpaceGameSimulation/support/logging/SandboxLogCategories.h>

#include <Engine/World.h>
#include <HAL/FileManager.h>
#include <Misc/FileHelper.h>
#include <Misc/Paths.h>

namespace ioj {
auto create_level_settings_file(UWorld const* const world) -> TOptional<FString> {
    if (!IsValid(world) || world->IsGameWorld()) {
        UE_LOG(LogSandbox, Warning, TEXT("Create level settings requires an editor world."));
        return {};
    }

    auto const filename{FPaths::MakeValidFileName(world->GetName()) + TEXT(".scm")};
    auto const relative_path{FPaths::Combine(TEXT("LevelScripts/Settings"), filename)};
    auto const path{FPaths::Combine(FPaths::ProjectDir(), relative_path)};
    auto& files{IFileManager::Get()};
    if (!files.FileExists(*path)) {
        files.MakeDirectory(*FPaths::GetPath(path), true);
        FString const source{
            TEXT("(settings-layers\n  (load-script \"Settings/base.scm\")\n  (settings))\n")};
        if (!FFileHelper::SaveStringToFile(source,
                                           *path,
                                           FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM,
                                           &files,
                                           FILEWRITE_NoReplaceExisting)) {
            UE_LOG(LogSandbox, Error, TEXT("Cannot create settings file '%s'."), *path);
            return {};
        }
    }

    return relative_path;
}
}
#endif
