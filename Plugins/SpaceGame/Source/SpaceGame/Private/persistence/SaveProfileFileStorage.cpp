#include "SpaceGame/persistence/SaveProfileFileStorage.h"

#include "SpaceGame/persistence/SpaceSaveGame.h"

#include <HAL/CriticalSection.h>
#include <HAL/PlatformFileManager.h>
#include <HAL/PlatformProcess.h>
#include <Kismet/GameplayStatics.h>
#include <Misc/CommandLine.h>
#include <Misc/Crc.h>
#include <Misc/FileHelper.h>
#include <Misc/Guid.h>
#include <Misc/Parse.h>
#include <Misc/Paths.h>
#include <Misc/ScopeLock.h>
#include <Serialization/MemoryReader.h>
#include <Serialization/MemoryWriter.h>

#if PLATFORM_WINDOWS
#include <Windows/WindowsHWrapper.h>
#endif

namespace ml::ioj {
namespace save_profile_file_storage {
inline constexpr uint32 envelope_magic{0x534A4F49};
inline constexpr uint32 envelope_version{1};
inline FTimespan const lock_timeout{FTimespan::FromSeconds(5.0)};

struct FEnvelopeHeader {
    uint32 magic{envelope_magic};
    uint32 version{envelope_version};
    uint64 generation{};
    uint64 payload_size{};
    uint32 payload_crc{};
};

auto operator<<(FArchive& archive, FEnvelopeHeader& header) -> FArchive& {
    archive << header.magic;
    archive << header.version;
    archive << header.generation;
    archive << header.payload_size;
    archive << header.payload_crc;
    return archive;
}

auto index_filename() -> FString {
    return TEXT("SpaceProfileIndex.sav");
}
auto results_filename(FString const& profile_id) -> FString {
    return FString::Printf(TEXT("SpaceProfile_%s_Results.sav"), *profile_id);
}

bool valid_identifier(FString const& value, int32 const maximum_length) {
    if (value.IsEmpty() || value.Len() > maximum_length) {
        return false;
    }
    for (auto const character : value) {
        if (!FChar::IsAlnum(character) && character != TEXT('_') && character != TEXT('-')) {
            return false;
        }
    }
    return true;
}

auto read_payload(FString const& path, TArray<uint8>& payload, uint64& generation) -> bool {
    TArray<uint8> bytes{};
    if (!FFileHelper::LoadFileToArray(bytes, *path)) {
        return false;
    }

    FMemoryReader reader{bytes, true};
    FEnvelopeHeader header{};
    reader << header;
    auto const remaining{reader.TotalSize() - reader.Tell()};
    if (reader.IsError() || header.magic != envelope_magic || header.version != envelope_version ||
        header.payload_size != static_cast<uint64>(remaining) ||
        header.payload_size > static_cast<uint64>(MAX_int32)) {
        return false;
    }

    payload.SetNumUninitialized(static_cast<int32>(header.payload_size));
    reader.Serialize(payload.GetData(), payload.Num());
    if (reader.IsError() ||
        FCrc::MemCrc32(payload.GetData(), payload.Num()) != header.payload_crc) {
        payload.Reset();
        return false;
    }
    generation = header.generation;
    return true;
}

bool write_all(FString const& path, TArray<uint8> const& bytes) {
    auto& platform_file{FPlatformFileManager::Get().GetPlatformFile()};
    TUniquePtr<IFileHandle> file{platform_file.OpenWrite(*path, false, false)};
    return file && file->Write(bytes.GetData(), bytes.Num()) && file->Flush(true);
}

bool replace_file(FString const& destination, FString const& temporary, FString const& backup) {
#if PLATFORM_WINDOWS
    auto const absolute_destination{FPaths::ConvertRelativePathToFull(destination)};
    auto const absolute_temporary{FPaths::ConvertRelativePathToFull(temporary)};
    auto const absolute_backup{FPaths::ConvertRelativePathToFull(backup)};
    if (IFileManager::Get().FileExists(*absolute_destination)) {
        if (!CopyFileW(*absolute_destination, *absolute_backup, false)) {
            return false;
        }
        return MoveFileExW(*absolute_temporary,
                           *absolute_destination,
                           MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
    }
    return MoveFileExW(*absolute_temporary, *absolute_destination, MOVEFILE_WRITE_THROUGH) != 0;
#else
    if (IFileManager::Get().FileExists(*destination) &&
        !IFileManager::Get().Move(*backup, *destination, true, true, false, true)) {
        return false;
    }
    return IFileManager::Get().Move(*destination, *temporary, false, true, false, true);
#endif
}

bool save_payload(FString const& path, TArray<uint8> const& payload) {
    uint64 generation{};
    TArray<uint8> ignored{};
    if (IFileManager::Get().FileExists(*path)) {
        read_payload(path, ignored, generation);
    }

    FEnvelopeHeader header{.generation = generation + 1,
                           .payload_size = static_cast<uint64>(payload.Num()),
                           .payload_crc = FCrc::MemCrc32(payload.GetData(), payload.Num())};
    TArray<uint8> bytes{};
    FMemoryWriter writer{bytes, true};
    writer << header;
    writer.Serialize(const_cast<uint8*>(payload.GetData()), payload.Num());
    if (writer.IsError()) {
        return false;
    }

    auto const temporary{path + TEXT(".tmp.") + FGuid::NewGuid().ToString(EGuidFormats::Digits)};
    if (!write_all(temporary, bytes)) {
        IFileManager::Get().Delete(*temporary, false, true);
        return false;
    }
    if (!replace_file(path, temporary, path + TEXT(".bak"))) {
        IFileManager::Get().Delete(*temporary, false, true);
        return false;
    }
    return true;
}

template <typename SaveType, typename DataType>
auto load_data(FString const& path, DataType& data) -> ESaveProfileLoadResult {
    auto const backup{path + TEXT(".bak")};
    if (!IFileManager::Get().FileExists(*path) && !IFileManager::Get().FileExists(*backup)) {
        return ESaveProfileLoadResult::not_found;
    }

    TArray<uint8> payload{};
    uint64 generation{};
    auto loaded{IFileManager::Get().FileExists(*path) && read_payload(path, payload, generation)};
    if (!loaded && IFileManager::Get().FileExists(*backup)) {
        loaded = read_payload(backup, payload, generation);
    }
    if (!loaded) {
        return ESaveProfileLoadResult::failed;
    }

    auto* const save{Cast<SaveType>(UGameplayStatics::LoadGameFromMemory(payload))};
    if (!IsValid(save)) {
        return ESaveProfileLoadResult::failed;
    }
    data = save->data;
    return ESaveProfileLoadResult::succeeded;
}

template <typename SaveType, typename DataType>
bool save_data(FString const& path, DataType const& data) {
    auto* const save{NewObject<SaveType>()};
    if (!IsValid(save)) {
        return false;
    }
    save->data = data;
    TArray<uint8> payload{};
    return UGameplayStatics::SaveGameToMemory(save, payload) && save_payload(path, payload);
}

struct FStorageState : TSharedFromThis<FStorageState> {
    FString root{};
    FString lock_name{};
    FCriticalSection process_lock{};
};
}

auto development_save_base_root() -> FString {
    auto const settings_root{FPlatformProcess::GetApplicationSettingsDir(
        {FPlatformProcess::ApplicationSettingsContext::Context::LocalUser, false})};
    return FPaths::Combine(settings_root, TEXT("IsleOfJebbers"), TEXT("DevelopmentSaves"));
}

auto resolve_save_profile_storage(FString const& command_line,
                                  bool const editor,
                                  bool const unattended,
                                  bool const automation,
                                  FSaveProfileStorageSelection& selection,
                                  FString& error) -> bool {
    selection = {};
    selection.mode = editor ? ESaveProfileStorageMode::shared : ESaveProfileStorageMode::platform;
    if (editor && (unattended || automation)) {
        selection.mode = ESaveProfileStorageMode::local;
    }
    if (!editor) {
        return true;
    }

    auto const local{FParse::Param(*command_line, TEXT("SpaceSaveLocal"))};
    auto const shared{FParse::Param(*command_line, TEXT("SpaceSaveShared"))};
    FString experiment{};
    auto const experimental{
        FParse::Value(*command_line, TEXT("SpaceSaveExperiment="), experiment, false)};
    if (static_cast<int32>(local) + static_cast<int32>(shared) + static_cast<int32>(experimental) >
        1) {
        error = TEXT("Conflicting SpaceSave storage selectors.");
        return false;
    }

    if (local) {
        selection.mode = ESaveProfileStorageMode::local;
    } else if (shared) {
        selection.mode = ESaveProfileStorageMode::shared;
    } else if (experimental) {
        experiment.ToLowerInline();
        if (!save_profile_file_storage::valid_identifier(experiment, 48)) {
            error = TEXT("SpaceSaveExperiment must contain only letters, digits, '_' or '-'.");
            return false;
        }
        selection.mode = ESaveProfileStorageMode::experimental;
        selection.experiment_name = experiment;
    }

    selection.import_local = FParse::Param(*command_line, TEXT("SpaceSaveImportLocal"));
    selection.start_fresh = FParse::Param(*command_line, TEXT("SpaceSaveStartFresh"));
    selection.clone_shared = FParse::Param(*command_line, TEXT("SpaceSaveCloneShared"));
    if (selection.import_local && selection.start_fresh) {
        error = TEXT("SpaceSaveImportLocal and SpaceSaveStartFresh are mutually exclusive.");
        return false;
    }
    if ((selection.import_local || selection.start_fresh) &&
        selection.mode != ESaveProfileStorageMode::shared) {
        error = TEXT("SpaceSaveImportLocal and SpaceSaveStartFresh require Shared storage.");
        return false;
    }
    if (selection.clone_shared && selection.mode != ESaveProfileStorageMode::experimental) {
        error = TEXT("SpaceSaveCloneShared requires SpaceSaveExperiment.");
        return false;
    }

    auto const base_root{development_save_base_root()};
    switch (selection.mode) {
        case ESaveProfileStorageMode::shared:
            selection.root = FPaths::Combine(base_root, TEXT("Shared"));
            break;
        case ESaveProfileStorageMode::local:
            selection.root = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SaveGames"));
            break;
        case ESaveProfileStorageMode::experimental:
            selection.root =
                FPaths::Combine(base_root, TEXT("Experiments"), selection.experiment_name);
            break;
        case ESaveProfileStorageMode::platform:
            break;
    }
    return true;
}

auto make_file_profile_storage(
    FString root, TFunction<ESaveProfileLoadResult(TArray<FScoreRecord>&)> load_legacy_results)
    -> FSaveProfileStorage {
    using namespace save_profile_file_storage;
    IFileManager::Get().MakeDirectory(*root, true);
    auto state{MakeShared<FStorageState>()};
    state->root = MoveTemp(root);
    state->lock_name =
        FString::Printf(TEXT("IsleOfJebbersSave_%08x"),
                        FCrc::StrCrc32(*FPaths::ConvertRelativePathToFull(state->root)));

    return {
        .with_exclusive_access =
            [state](TFunctionRef<bool()> operation) {
                FScopeLock const process_scope{&state->process_lock};
                FSystemWideCriticalSection const system_lock{state->lock_name, lock_timeout};
                return system_lock.IsValid() && operation();
            },
        .load_index =
            [state](FSaveProfileIndexData& data) {
                return load_data<USpaceSaveProfileIndexSaveGame>(
                    FPaths::Combine(state->root, index_filename()), data);
            },
        .save_index =
            [state](FSaveProfileIndexData const& data) {
                return save_data<USpaceSaveProfileIndexSaveGame>(
                    FPaths::Combine(state->root, index_filename()), data);
            },
        .load_results =
            [state](FString const& profile_id, FSaveProfileResultsData& data) {
                if (!valid_identifier(profile_id, 64)) {
                    return ESaveProfileLoadResult::failed;
                }
                return load_data<USpaceSaveProfileResultsSaveGame>(
                    FPaths::Combine(state->root, results_filename(profile_id)), data);
            },
        .save_results =
            [state](FString const& profile_id, FSaveProfileResultsData const& data) {
                return valid_identifier(profile_id, 64) &&
                       save_data<USpaceSaveProfileResultsSaveGame>(
                           FPaths::Combine(state->root, results_filename(profile_id)), data);
            },
        .load_legacy_results = MoveTemp(load_legacy_results),
    };
}

auto clone_save_profile_files(FString const& source_root,
                              FString const& destination_root,
                              FString& error) -> bool {
    using namespace save_profile_file_storage;
    if (IFileManager::Get().FileExists(*FPaths::Combine(destination_root, index_filename())) ||
        IFileManager::Get().FileExists(
            *FPaths::Combine(destination_root, index_filename() + TEXT(".bak")))) {
        return true;
    }
    if (IFileManager::Get().DirectoryExists(*destination_root) &&
        !IFileManager::Get().DeleteDirectory(*destination_root, true, false)) {
        error = TEXT("The experiment root exists but is not initialized.");
        return false;
    }
    if (!IFileManager::Get().DirectoryExists(*source_root)) {
        error = TEXT("The Shared save root does not exist.");
        return false;
    }

    auto const staging{destination_root + TEXT(".creating.") +
                       FGuid::NewGuid().ToString(EGuidFormats::Digits)};
    auto no_legacy = [](TArray<FScoreRecord>&) { return ESaveProfileLoadResult::not_found; };
    auto source_storage{make_file_profile_storage(source_root, MoveTemp(no_legacy))};
    auto const copied{
        source_storage.with_exclusive_access([&source_storage, &source_root, &staging] {
            FSaveProfileIndexData index{};
            if (source_storage.load_index(index) != ESaveProfileLoadResult::succeeded ||
                index.save_version < 1 ||
                index.save_version > FSaveProfileIndexData::current_save_version) {
                return false;
            }
            for (auto const& profile : index.profiles) {
                FSaveProfileResultsData results{};
                if (source_storage.load_results(profile.profile_id, results) !=
                        ESaveProfileLoadResult::succeeded ||
                    results.save_version != FSaveProfileResultsData::current_save_version) {
                    return false;
                }
            }

            TArray<FString> files{};
            IFileManager::Get().FindFiles(
                files, *FPaths::Combine(source_root, TEXT("*.sav")), true, false);
            if (files.IsEmpty() || !IFileManager::Get().MakeDirectory(*staging, true)) {
                return false;
            }
            for (auto const& filename : files) {
                if (IFileManager::Get().Copy(*FPaths::Combine(staging, filename),
                                             *FPaths::Combine(source_root, filename),
                                             false,
                                             true) != COPY_OK) {
                    return false;
                }
            }
            return true;
        })};
    if (!copied) {
        error = TEXT("Failed to copy Shared saves into the experiment staging root.");
        IFileManager::Get().DeleteDirectory(*staging, false, true);
        return false;
    }
    if (!IFileManager::Get().Move(*destination_root, *staging, false, true, false, true)) {
        error = TEXT("Failed to publish the experiment save root.");
        IFileManager::Get().DeleteDirectory(*staging, false, true);
        return false;
    }
    return true;
}

auto import_local_save_profiles(
    FString const& destination_root,
    TFunction<ESaveProfileLoadResult(TArray<FScoreRecord>&)> load_legacy_results,
    FString& error) -> bool {
    using namespace save_profile_file_storage;
    auto const destination_index{FPaths::Combine(destination_root, index_filename())};
    if (IFileManager::Get().FileExists(*destination_index) ||
        IFileManager::Get().FileExists(*(destination_index + TEXT(".bak")))) {
        error = TEXT("The Shared save root already exists.");
        return false;
    }
    if (IFileManager::Get().DirectoryExists(*destination_root) &&
        !IFileManager::Get().DeleteDirectory(*destination_root, true, false)) {
        error = TEXT("The uninitialized Shared save root is not empty.");
        return false;
    }

    auto const staging{destination_root + TEXT(".importing.") +
                       FGuid::NewGuid().ToString(EGuidFormats::Digits)};
    auto storage{make_file_profile_storage(staging, load_legacy_results)};
    auto const index_slot{FString{TEXT("SpaceProfileIndex")}};
    auto imported{false};
    if (!UGameplayStatics::DoesSaveGameExist(index_slot, 0)) {
        FSaveProfileManager manager{storage};
        imported = manager.initialise();
    } else {
        imported = storage.with_exclusive_access([&storage, &index_slot] {
            auto* const index_save{Cast<USpaceSaveProfileIndexSaveGame>(
                UGameplayStatics::LoadGameFromSlot(index_slot, 0))};
            if (!IsValid(index_save) || index_save->data.save_version < 1 ||
                index_save->data.save_version > FSaveProfileIndexData::current_save_version ||
                !storage.save_index(index_save->data)) {
                return false;
            }
            for (auto const& profile : index_save->data.profiles) {
                if (!valid_identifier(profile.profile_id, 64)) {
                    return false;
                }
                auto const slot{
                    FString::Printf(TEXT("SpaceProfile_%s_Results"), *profile.profile_id)};
                auto* const results_save{Cast<USpaceSaveProfileResultsSaveGame>(
                    UGameplayStatics::LoadGameFromSlot(slot, 0))};
                if (!IsValid(results_save) ||
                    results_save->data.save_version !=
                        FSaveProfileResultsData::current_save_version ||
                    !storage.save_results(profile.profile_id, results_save->data)) {
                    return false;
                }
            }
            return true;
        });
    }

    if (imported) {
        auto no_legacy = [](TArray<FScoreRecord>&) { return ESaveProfileLoadResult::not_found; };
        FSaveProfileManager validator{make_file_profile_storage(staging, MoveTemp(no_legacy))};
        imported = validator.initialise();
    }
    if (!imported) {
        error = TEXT("Local saves are missing, malformed, incomplete, or unsupported.");
        IFileManager::Get().DeleteDirectory(*staging, false, true);
        return false;
    }
    if (!IFileManager::Get().Move(*destination_root, *staging, false, true, false, true)) {
        error = TEXT("Failed to publish the imported Shared save root.");
        IFileManager::Get().DeleteDirectory(*staging, false, true);
        return false;
    }
    return true;
}
}
