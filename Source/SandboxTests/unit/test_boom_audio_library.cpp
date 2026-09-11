#include <SpaceGamePresentation/audio/BoomAudioLibrary.h>

#include <CQTest.h>
#include <HAL/FileManager.h>
#include <Misc/FileHelper.h>
#include <Misc/Guid.h>
#include <Misc/Paths.h>

namespace {
struct FTemporaryBoomAudioDirectory {
    FString path{FPaths::Combine(FPaths::ProjectSavedDir(),
                                 TEXT("Automation"),
                                 TEXT("BoomAudioLibrary"),
                                 FGuid::NewGuid().ToString())};

    FTemporaryBoomAudioDirectory() { IFileManager::Get().MakeDirectory(*path, true); }

    ~FTemporaryBoomAudioDirectory() { IFileManager::Get().DeleteDirectory(*path, false, true); }
};

auto normalized_full_path(FString path) -> FString {
    path = FPaths::ConvertRelativePathToFull(path);
    FPaths::NormalizeDirectoryName(path);
    return path;
}
}

TEST_CLASS(BoomAudioLibrary, "Sandbox.UnitTests")
{
    TEST_METHOD(ReportsUnsetAndMissingRootsWithoutPackChecks)
    {
        auto const unset{ml::ioj::resolve_boom_audio_library(FStringView{})};
        TestRunner->TestTrue(TEXT("An unset root has an empty path"), unset.path.IsEmpty());
        TestRunner->TestFalse(TEXT("An unset root is invalid"), unset.valid);
        TestRunner->TestTrue(TEXT("An unset root has no directories"), unset.directories.IsEmpty());

        FTemporaryBoomAudioDirectory directory;
        auto const missing_path{FPaths::Combine(directory.path, TEXT("missing"))};
        auto const missing{ml::ioj::resolve_boom_audio_library(missing_path)};
        TestRunner->TestFalse(TEXT("A nonexistent root is invalid"), missing.valid);
        TestRunner->TestEqual(TEXT("The missing root is normalized"),
                              missing.path,
                              normalized_full_path(missing_path));
        TestRunner->TestTrue(TEXT("A missing root has no directories"),
                             missing.directories.IsEmpty());
    }

    TEST_METHOD(ResolvesAvailablePackDirectoriesRelativeToTheRoot)
    {
        FTemporaryBoomAudioDirectory directory;
        TArray<FString> const expected_relative_directories{
            TEXT("sci-fi_ds_2220mb"),
        };
        for (auto const& relative_directory : expected_relative_directories) {
            IFileManager::Get().MakeDirectory(*FPaths::Combine(directory.path, relative_directory),
                                              true);
        }

        auto const resolution{ml::ioj::resolve_boom_audio_library(directory.path)};
        TestRunner->TestTrue(TEXT("The root is valid"), resolution.valid);
        TestRunner->TestEqual(TEXT("Every expected pack is resolved"),
                              resolution.directories.Num(),
                              expected_relative_directories.Num());
        if (resolution.directories.Num() != expected_relative_directories.Num()) {
            return;
        }

        auto const expected_root{normalized_full_path(directory.path)};
        for (int32 index{}; index < expected_relative_directories.Num(); ++index) {
            auto const& resolved_directory{resolution.directories[index]};
            TestRunner->TestEqual(TEXT("The relative directory is preserved"),
                                  resolved_directory.relative_directory,
                                  expected_relative_directories[index]);
            TestRunner->TestEqual(
                TEXT("The pack path is joined beneath the root"),
                resolved_directory.path,
                FPaths::Combine(expected_root, expected_relative_directories[index]));
            TestRunner->TestTrue(TEXT("The pack directory is valid"), resolved_directory.valid);
        }
    }

    TEST_METHOD(ReportsNonDirectoryPack)
    {
        FTemporaryBoomAudioDirectory directory;
        auto const pack_path{FPaths::Combine(directory.path, TEXT("sci-fi_ds_2220mb"))};
        TestRunner->TestTrue(TEXT("The non-directory fixture is written"),
                             FFileHelper::SaveStringToFile(TEXT("not a directory"), *pack_path));

        auto const resolution{ml::ioj::resolve_boom_audio_library(directory.path)};
        TestRunner->TestEqual(
            TEXT("The expected pack is reported"), resolution.directories.Num(), 1);
        if (resolution.directories.Num() != 1) {
            return;
        }

        TestRunner->TestFalse(TEXT("A file is not an available pack directory"),
                              resolution.directories[0].valid);
    }

    TEST_METHOD(ReportsMissingPack)
    {
        FTemporaryBoomAudioDirectory directory;

        auto const resolution{ml::ioj::resolve_boom_audio_library(directory.path)};
        TestRunner->TestEqual(
            TEXT("The expected pack is reported"), resolution.directories.Num(), 1);
        if (resolution.directories.Num() != 1) {
            return;
        }

        TestRunner->TestFalse(TEXT("A missing pack is unavailable"),
                              resolution.directories[0].valid);
    }
};
