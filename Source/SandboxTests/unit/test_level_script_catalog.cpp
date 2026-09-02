#include <SpaceGameS7/LevelScriptCatalog.h>

#include <CQTest.h>
#include <HAL/FileManager.h>
#include <Misc/FileHelper.h>
#include <Misc/Guid.h>
#include <Misc/Paths.h>

namespace {
struct FTemporaryScriptDirectory {
    FString path{FPaths::Combine(FPaths::ProjectSavedDir(),
                                 TEXT("Automation"),
                                 TEXT("LevelScriptCatalog"),
                                 FGuid::NewGuid().ToString())};

    FTemporaryScriptDirectory() { IFileManager::Get().MakeDirectory(*path, true); }

    ~FTemporaryScriptDirectory() { IFileManager::Get().DeleteDirectory(*path, false, true); }
};

auto valid_level_script(FStringView const title) -> FString {
    return FString::Printf(TEXT("(level (title \"%s\") (description \"Catalog test\") "
                                "(teams (team 'blue)) (player 'player) "
                                "(entities (entity 'player 'player-fighter 'blue "
                                "(position 0 0 0) (rotation 0 0 0))))"),
                           *FString{title});
}
}

TEST_CLASS(LevelScriptCatalog, "Sandbox.UnitTests")
{
    TEST_METHOD(DiscoversAndEvaluatesFlatSchemeFiles)
    {
        FTemporaryScriptDirectory directory;
        auto const alpha_path{FPaths::Combine(directory.path, TEXT("alpha.scm"))};
        auto const bravo_path{FPaths::Combine(directory.path, TEXT("Bravo.SCM"))};
        auto const invalid_path{FPaths::Combine(directory.path, TEXT("invalid.scm"))};
        auto const ignored_path{FPaths::Combine(directory.path, TEXT("ignored.txt"))};
        auto const nested_directory{FPaths::Combine(directory.path, TEXT("nested"))};
        IFileManager::Get().MakeDirectory(*nested_directory, true);

        auto const files_written{
            FFileHelper::SaveStringToFile(valid_level_script(TEXT("Alpha Level")), *alpha_path) &&
            FFileHelper::SaveStringToFile(valid_level_script(TEXT("Bravo Level")), *bravo_path) &&
            FFileHelper::SaveStringToFile(TEXT("(level (title \"Broken\"))"), *invalid_path) &&
            FFileHelper::SaveStringToFile(TEXT("not a level"), *ignored_path) &&
            FFileHelper::SaveStringToFile(valid_level_script(TEXT("Nested Level")),
                                          *FPaths::Combine(nested_directory, TEXT("nested.scm")))};
        if (!TestRunner->TestTrue(TEXT("Test scripts are written"), files_written)) {
            return;
        }

        auto const result{ml::s7::discover_level_scripts(directory.path)};
        TestRunner->TestTrue(TEXT("Catalog directory is readable"), result.error.IsEmpty());
        TestRunner->TestEqual(
            TEXT("Only flat Scheme files are discovered"), result.entries.Num(), 3);
        if (result.entries.Num() != 3) {
            return;
        }

        TestRunner->TestEqual(TEXT("Files are sorted case-insensitively"),
                              result.entries[0].filename,
                              FString{TEXT("alpha.scm")});
        TestRunner->TestEqual(TEXT("Valid metadata title is decoded"),
                              result.entries[0].display_title,
                              FString{TEXT("Alpha Level")});
        TestRunner->TestEqual(TEXT("Valid metadata description is decoded"),
                              result.entries[1].description,
                              FString{TEXT("Catalog test")});
        TestRunner->TestTrue(TEXT("Valid entries retain their native definitions"),
                             static_cast<bool>(result.entries[0]));
        TestRunner->TestFalse(TEXT("Malformed entries are retained but invalid"),
                              static_cast<bool>(result.entries[2]));
        TestRunner->TestTrue(TEXT("Malformed entries include a useful error"),
                             !result.entries[2].error.IsEmpty());
    }
};
