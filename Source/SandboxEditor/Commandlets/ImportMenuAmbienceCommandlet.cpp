#include "SandboxEditor/Commandlets/ImportMenuAmbienceCommandlet.h"

#include <SpaceGamePresentation/audio/BoomAudioLibrary.h>
#include <SpaceGamePresentation/support/logging/PresentationLogCategories.h>

#include <AssetImportTask.h>
#include <AssetToolsModule.h>
#include <Factories/SoundFactory.h>
#include <Misc/PackageName.h>
#include <Sound/SoundWave.h>
#include <UObject/Package.h>
#include <UObject/SavePackage.h>
#include <UObject/StrongObjectPtr.h>

namespace import_menu_ambience {
inline constexpr TCHAR destination_path[]{TEXT("/SpaceGame/Audio/Generated")};
inline constexpr TCHAR asset_name[]{TEXT("A_MenuAmbience_ComputerRoomLow02")};
inline constexpr float asset_volume{0.2f};

auto import_sound_wave(FString const& source_path) -> USoundWave* {
    TStrongObjectPtr<USoundFactory> factory{NewObject<USoundFactory>()};
    factory->SuppressImportDialogs();
    factory->bAutoCreateCue = false;

    TStrongObjectPtr<UAssetImportTask> task{NewObject<UAssetImportTask>()};
    task->Filename = source_path;
    task->DestinationPath = destination_path;
    task->DestinationName = asset_name;
    task->Factory = factory.Get();
    task->bAutomated = true;
    task->bReplaceExisting = true;
    task->bReplaceExistingSettings = true;
    task->bSave = false;
    task->bAsync = false;

    FAssetToolsModule::GetModule().Get().ImportAssetTasks({task.Get()});
    for (auto* const object : task->GetObjects()) {
        if (auto* const sound_wave{Cast<USoundWave>(object)}; IsValid(sound_wave)) {
            return sound_wave;
        }
    }
    return nullptr;
}

auto configure_and_save(USoundWave& sound_wave) -> bool {
    sound_wave.Modify();
    sound_wave.bLooping = true;
    sound_wave.Volume = asset_volume;
    sound_wave.PostEditChange();

    auto* const package{sound_wave.GetOutermost()};
    package->MarkPackageDirty();
    auto const package_path{FPackageName::LongPackageNameToFilename(
        package->GetName(), FPackageName::GetAssetPackageExtension())};
    FSavePackageArgs save_arguments{};
    save_arguments.TopLevelFlags = RF_Public | RF_Standalone;
    return UPackage::SavePackage(package, &sound_wave, *package_path, save_arguments);
}
}

UImportMenuAmbienceCommandlet::UImportMenuAmbienceCommandlet() {
    IsClient = false;
    IsEditor = true;
    IsServer = false;
    LogToConsole = true;
}

int32 UImportMenuAmbienceCommandlet::Main(FString const&) {
    auto const resolution{
        ml::ioj::resolve_boom_audio_library(ml::ioj::get_configured_boom_audio_root())};
    if (!resolution.menu_ambience_source.valid) {
        if (resolution.menu_ambience_source.path.IsEmpty()) {
            UE_LOG(LogSandboxAudio,
                   Warning,
                   TEXT("Menu ambience was not imported because BEE_AUDIO_ROOT is unset or "
                        "invalid."));
        } else {
            UE_LOG(LogSandboxAudio,
                   Warning,
                   TEXT("Menu ambience was not imported because its source file is unavailable: "
                        "\"%s\"."),
                   *resolution.menu_ambience_source.path);
        }
        return 0;
    }

    auto* const sound_wave{
        import_menu_ambience::import_sound_wave(resolution.menu_ambience_source.path)};
    if (!IsValid(sound_wave)) {
        UE_LOG(LogSandboxAudio,
               Error,
               TEXT("Failed to import menu ambience source: \"%s\"."),
               *resolution.menu_ambience_source.path);
        return 1;
    }
    if (!import_menu_ambience::configure_and_save(*sound_wave)) {
        UE_LOG(LogSandboxAudio,
               Error,
               TEXT("Failed to save imported menu ambience asset: \"%s\"."),
               *sound_wave->GetPathName());
        return 1;
    }

    UE_LOG(LogSandboxAudio,
           Display,
           TEXT("Imported optional menu ambience asset: \"%s\"."),
           *sound_wave->GetPathName());
    return 0;
}
