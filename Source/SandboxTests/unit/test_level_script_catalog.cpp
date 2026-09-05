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

auto valid_level_script(FStringView const id,
                        FStringView const title,
                        FStringView const unlock = FStringView{}) -> FString {
    return FString::Printf(TEXT("(level (id '%s) (title \"%s\") "
                                "(description \"Catalog test\") "
                                "%s "
                                "(teams (team 'blue)) (player 'player) "
                                "(entities (entity 'player 'player-fighter 'blue "
                                "(position 0 0 0) (rotation 0 0 0))))"),
                           *FString{id},
                           *FString{title},
                           *FString{unlock});
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
            FFileHelper::SaveStringToFile(valid_level_script(TEXT("alpha"), TEXT("Alpha Level")),
                                          *alpha_path) &&
            FFileHelper::SaveStringToFile(valid_level_script(TEXT("bravo"), TEXT("Bravo Level")),
                                          *bravo_path) &&
            FFileHelper::SaveStringToFile(TEXT("(level (title \"Broken\"))"), *invalid_path) &&
            FFileHelper::SaveStringToFile(TEXT("not a level"), *ignored_path) &&
            FFileHelper::SaveStringToFile(valid_level_script(TEXT("nested"), TEXT("Nested Level")),
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

    TEST_METHOD(RejectsDuplicateLevelIds)
    {
        FTemporaryScriptDirectory directory;
        auto const alpha_path{FPaths::Combine(directory.path, TEXT("alpha.scm"))};
        auto const bravo_path{FPaths::Combine(directory.path, TEXT("bravo.scm"))};
        auto const files_written{
            FFileHelper::SaveStringToFile(valid_level_script(TEXT("shared"), TEXT("Alpha")),
                                          *alpha_path) &&
            FFileHelper::SaveStringToFile(valid_level_script(TEXT("shared"), TEXT("Bravo")),
                                          *bravo_path)};
        if (!TestRunner->TestTrue(TEXT("Test scripts are written"), files_written)) {
            return;
        }

        auto const result{ml::s7::discover_level_scripts(directory.path)};
        TestRunner->TestEqual(TEXT("Both scripts remain visible"), result.entries.Num(), 2);
        if (result.entries.Num() != 2) {
            return;
        }

        TestRunner->TestFalse(TEXT("First duplicate is invalid"),
                              static_cast<bool>(result.entries[0]));
        TestRunner->TestFalse(TEXT("Second duplicate is invalid"),
                              static_cast<bool>(result.entries[1]));
        TestRunner->TestTrue(TEXT("Conflict identifies the first file"),
                             result.entries[0].error.Contains(TEXT("alpha.scm")));
        TestRunner->TestTrue(TEXT("Conflict identifies the second file"),
                             result.entries[0].error.Contains(TEXT("bravo.scm")));
    }

    TEST_METHOD(ParsesCampaignOrderingAndReferences)
    {
        FTemporaryScriptDirectory directory;
        auto const campaign_directory{FPaths::Combine(directory.path, TEXT("Campaigns"))};
        IFileManager::Get().MakeDirectory(*campaign_directory, true);
        auto const files_written{
            FFileHelper::SaveStringToFile(valid_level_script(TEXT("alpha"), TEXT("Alpha")),
                                          *FPaths::Combine(directory.path, TEXT("alpha.scm"))) &&
            FFileHelper::SaveStringToFile(valid_level_script(TEXT("bravo"), TEXT("Bravo")),
                                          *FPaths::Combine(directory.path, TEXT("bravo.scm"))) &&
            FFileHelper::SaveStringToFile(TEXT("(campaign (id 'main) (title \"Main Campaign\") "
                                               "(levels 'bravo 'alpha))"),
                                          *FPaths::Combine(campaign_directory, TEXT("main.scm")))};
        if (!TestRunner->TestTrue(TEXT("Campaign fixtures are written"), files_written)) {
            return;
        }

        auto const result{ml::s7::discover_level_scripts(directory.path)};
        TestRunner->TestTrue(TEXT("Catalog is valid"), result.error.IsEmpty());
        if (!TestRunner->TestEqual(TEXT("One campaign is discovered"), result.campaigns.Num(), 1) ||
            !TestRunner->TestTrue(TEXT("Campaign is valid"),
                                  static_cast<bool>(result.campaigns[0]))) {
            return;
        }
        auto const& campaign{result.campaigns[0].definition.GetValue()};
        TestRunner->TestEqual(TEXT("Campaign order is retained"), campaign.level_ids.Num(), 2);
        TestRunner->TestTrue(TEXT("First declared level remains first"),
                             campaign.level_ids[0] == ml::FLevelId{FName{TEXT("bravo")}});
        TestRunner->TestTrue(TEXT("Second declared level remains second"),
                             campaign.level_ids[1] == ml::FLevelId{FName{TEXT("alpha")}});
    }

    TEST_METHOD(RejectsMissingCampaignAndUnlockReferences)
    {
        FTemporaryScriptDirectory directory;
        auto const campaign_directory{FPaths::Combine(directory.path, TEXT("Campaigns"))};
        IFileManager::Get().MakeDirectory(*campaign_directory, true);
        auto const files_written{
            FFileHelper::SaveStringToFile(
                valid_level_script(
                    TEXT("locked"), TEXT("Locked"), TEXT("(unlock (level-completed 'missing))")),
                *FPaths::Combine(directory.path, TEXT("locked.scm"))) &&
            FFileHelper::SaveStringToFile(TEXT("(campaign (id 'main) (title \"Main\") "
                                               "(levels 'missing))"),
                                          *FPaths::Combine(campaign_directory, TEXT("main.scm")))};
        if (!TestRunner->TestTrue(TEXT("Invalid fixtures are written"), files_written)) {
            return;
        }

        auto const result{ml::s7::discover_level_scripts(directory.path)};
        TestRunner->TestFalse(TEXT("Missing references are reported"), result.error.IsEmpty());
        TestRunner->TestFalse(TEXT("Level with missing prerequisite is invalid"),
                              static_cast<bool>(result.entries[0]));
        TestRunner->TestFalse(TEXT("Campaign with missing level is invalid"),
                              static_cast<bool>(result.campaigns[0]));
    }

    TEST_METHOD(RejectsUnlockDependencyCycles)
    {
        FTemporaryScriptDirectory directory;
        auto const files_written{
            FFileHelper::SaveStringToFile(
                valid_level_script(
                    TEXT("alpha"), TEXT("Alpha"), TEXT("(unlock (level-completed 'bravo))")),
                *FPaths::Combine(directory.path, TEXT("alpha.scm"))) &&
            FFileHelper::SaveStringToFile(
                valid_level_script(
                    TEXT("bravo"), TEXT("Bravo"), TEXT("(unlock (level-completed 'alpha))")),
                *FPaths::Combine(directory.path, TEXT("bravo.scm")))};
        if (!TestRunner->TestTrue(TEXT("Cycle fixtures are written"), files_written)) {
            return;
        }

        auto const result{ml::s7::discover_level_scripts(directory.path)};
        TestRunner->TestFalse(TEXT("Cycle is reported"), result.error.IsEmpty());
        TestRunner->TestTrue(TEXT("Cycle path is readable"), result.error.Contains(TEXT("->")));
        TestRunner->TestFalse(TEXT("First cycle member is invalid"),
                              static_cast<bool>(result.entries[0]));
        TestRunner->TestFalse(TEXT("Second cycle member is invalid"),
                              static_cast<bool>(result.entries[1]));
    }
};
