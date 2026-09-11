#include "SandboxEditor/Commandlets/ImportGameAudioCommandlet.h"

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

namespace import_game_audio {
inline constexpr TCHAR destination_path[]{TEXT("/SpaceGame/Audio/Generated")};
inline constexpr TCHAR ambience_asset_name[]{TEXT("A_MenuAmbience_ComputerRoomLow02")};
inline constexpr TCHAR button_asset_name[]{TEXT("A_MenuButtonPressed")};
inline constexpr float ambience_volume{0.2f};
inline constexpr float button_volume{0.25f};

auto import_sound_wave(FString const& source_path, TCHAR const* const asset_name) -> USoundWave* {
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

auto configure_and_save(USoundWave& sound_wave, bool const looping, float const volume) -> bool {
    sound_wave.Modify();
    sound_wave.bLooping = looping;
    sound_wave.Volume = volume;
    sound_wave.PostEditChange();

    auto* const package{sound_wave.GetOutermost()};
    package->MarkPackageDirty();
    auto const package_path{FPackageName::LongPackageNameToFilename(
        package->GetName(), FPackageName::GetAssetPackageExtension())};
    FSavePackageArgs save_arguments{};
    save_arguments.TopLevelFlags = RF_Public | RF_Standalone;
    return UPackage::SavePackage(package, &sound_wave, *package_path, save_arguments);
}

auto import_optional_sound(ml::ioj::FAudioSourceFile const& source,
                           TCHAR const* const description,
                           TCHAR const* const asset_name,
                           bool const looping,
                           float const volume) -> bool {
    if (!source.valid) {
        if (source.path.IsEmpty()) {
            UE_LOG(LogSandboxAudio,
                   Warning,
                   TEXT("%s was not imported because BEE_AUDIO_ROOT is unset or invalid."),
                   description);
        } else {
            UE_LOG(LogSandboxAudio,
                   Warning,
                   TEXT("%s was not imported because its source file is unavailable: \"%s\"."),
                   description,
                   *source.path);
        }
        return true;
    }

    auto* const sound_wave{import_sound_wave(source.path, asset_name)};
    if (!IsValid(sound_wave)) {
        UE_LOG(LogSandboxAudio,
               Error,
               TEXT("Failed to import %s source: \"%s\"."),
               description,
               *source.path);
        return false;
    }
    if (!configure_and_save(*sound_wave, looping, volume)) {
        UE_LOG(LogSandboxAudio,
               Error,
               TEXT("Failed to save imported %s asset: \"%s\"."),
               description,
               *sound_wave->GetPathName());
        return false;
    }

    UE_LOG(LogSandboxAudio,
           Display,
           TEXT("Imported optional %s asset: \"%s\"."),
           description,
           *sound_wave->GetPathName());
    return true;
}
}

UImportGameAudioCommandlet::UImportGameAudioCommandlet() {
    IsClient = false;
    IsEditor = true;
    IsServer = false;
    LogToConsole = true;
}

int32 UImportGameAudioCommandlet::Main(FString const&) {
    auto const resolution{
        ml::ioj::resolve_boom_audio_library(ml::ioj::get_configured_boom_audio_root())};

    auto const ambience_imported{
        import_game_audio::import_optional_sound(resolution.menu_ambience_source,
                                                 TEXT("menu ambience"),
                                                 import_game_audio::ambience_asset_name,
                                                 true,
                                                 import_game_audio::ambience_volume)};
    auto const button_imported{
        import_game_audio::import_optional_sound(resolution.menu_button_pressed_source,
                                                 TEXT("menu button audio"),
                                                 import_game_audio::button_asset_name,
                                                 false,
                                                 import_game_audio::button_volume)};
    return ambience_imported && button_imported ? 0 : 1;
}
