#include <SpaceGameS7/level_script_catalog.h>

#include <SandboxCoreEngine/strings.h>

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

    auto write(TCHAR const* const filename, FString const& source) const -> bool {
        auto const filename_path{FPaths::Combine(path, filename)};
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(filename_path), true);
        return FFileHelper::SaveStringToFile(source, *filename_path);
    }
};

auto write_test_root(FString const& directory) -> FString {
    FString source{TEXT("(catalog :levels (list\n")};
    for (auto const* subdirectory : {TEXT(""), TEXT("Campaigns")}) {
        TArray<FString> filenames;
        IFileManager::Get().FindFiles(
            filenames, *FPaths::Combine(directory, subdirectory, TEXT("*.scm")), true, false);
        filenames.Sort([](FString const& lhs, FString const& rhs) {
            return lhs.Compare(rhs, ESearchCase::IgnoreCase) < 0;
        });
        for (auto const& filename : filenames) {
            source += FString::Printf(TEXT("\"%s\"\n"), *FPaths::Combine(subdirectory, filename));
        }
        source += *subdirectory == 0 ? TEXT(") :campaigns (list\n") : TEXT("))");
    }
    auto const path{FPaths::Combine(directory, TEXT("catalog.scm"))};
    check(FFileHelper::SaveStringToFile(source, *path));
    return path;
}

auto valid_level_script(FStringView const id,
                        FStringView const title,
                        FStringView const unlock = FStringView{}) -> FString {
    return FString::Printf(
        TEXT("(level :id '%s :title \"%s\" :description \"Catalog test\" %s :player "
             "'player :entities (list (entity :id 'player :archetype 'player-fighter :team 'blue "
             ":position '(0 0 0) :rotation '(0 0 0))))"),
        *FString{id},
        *FString{title},
        *FString{unlock});
}

auto valid_camera_level_script(FStringView const id, FStringView const title) -> FString {
    return FString::Printf(
        TEXT("(level :id '%s :title \"%s\" :description \"Catalog test\" :camera "
             "(camera :look-at '(capital) :distance 10000 :offset-direction '(-1 0 0)) :entities "
             "(list (entity :id 'capital :archetype 'capital-ship :team 'blue :position '(0 0 0) "
             ":rotation '(0 0 0))))"),
        *FString{id},
        *FString{title});
}
}

TEST_CLASS(LevelScriptCatalog, "Sandbox.UnitTests")
{

    TEST_METHOD(LoadsCheckedInLevelsAndCampaigns)
    {
        auto const result{::ioj::levels::authoring::load_level_script_catalog()};
        TestRunner->TestTrue(TEXT("Checked-in script catalog has no errors"),
                             result.error.IsEmpty());
        TestRunner->TestTrue(TEXT("Checked-in script catalog contains levels"),
                             !result.entries.IsEmpty());
        TestRunner->TestTrue(TEXT("Checked-in script catalog contains campaigns"),
                             !result.campaigns.IsEmpty());

        for (auto const& entry : result.entries) {
            TestRunner->TestTrue(*FString::Printf(TEXT("Level '%s' loads"), *entry.filename),
                                 static_cast<bool>(entry));
        }
        for (auto const& campaign : result.campaigns) {
            TestRunner->TestTrue(*FString::Printf(TEXT("Campaign '%s' loads"), *campaign.filename),
                                 static_cast<bool>(campaign));
        }
    }

    TEST_METHOD(LoadsOnlyDefinitionsSelectedByRoot)
    {
        FTemporaryScriptDirectory directory;

        auto const files_written{
            directory.write(TEXT("alpha.scm"),
                            valid_level_script(TEXT("alpha"), TEXT("Alpha Level"))) &&
            directory.write(TEXT("Bravo.SCM"),
                            valid_camera_level_script(TEXT("bravo"), TEXT("Bravo Level"))) &&
            directory.write(TEXT("invalid.scm"), TEXT("(level :title \"Broken\")")) &&
            directory.write(TEXT("ignored.txt"), TEXT("not a level")) &&
            directory.write(TEXT("nested/nested.scm"),
                            valid_level_script(TEXT("nested"), TEXT("Nested Level")))};
        if (!TestRunner->TestTrue(TEXT("Test scripts are written"), files_written)) {
            return;
        }

        auto const root{write_test_root(directory.path)};
        directory.write(TEXT("unselected.scm"), TEXT("(undefined-unselected-file)"));
        auto const result{::ioj::levels::authoring::load_level_script_catalog(root)};
        TestRunner->TestTrue(TEXT("Catalog directory is readable"), result.error.IsEmpty());
        TestRunner->TestEqual(
            TEXT("Only files selected by the root are loaded"), result.entries.Num(), 3);
        if (result.entries.Num() != 3) {
            return;
        }

        TestRunner->TestEqual(TEXT("Root selection order is retained"),
                              result.entries[0].filename,
                              FString{TEXT("alpha.scm")});
        TestRunner->TestEqual(TEXT("Valid metadata title is decoded"),
                              result.entries[0].display_title,
                              FString{TEXT("Alpha Level")});
        TestRunner->TestEqual(TEXT("Valid metadata description is decoded"),
                              result.entries[1].description,
                              FString{TEXT("Catalog test")});
        if (!TestRunner->TestTrue(
                *FString::Printf(TEXT("Alpha is valid: %s"), *result.entries[0].error),
                static_cast<bool>(result.entries[0])) ||
            !TestRunner->TestTrue(
                *FString::Printf(TEXT("Bravo is valid: %s"), *result.entries[1].error),
                static_cast<bool>(result.entries[1]))) {
            return;
        }
        TestRunner->TestTrue(
            TEXT("Player levels are catalogued as missions"),
            ::ioj::levels::authoring::catalog_category(result.entries[0].definition.GetValue()) ==
                ::ioj::levels::authoring::ELevelCatalogCategory::Mission);
        TestRunner->TestTrue(
            TEXT("Playerless levels are catalogued for Battle Viewer"),
            ::ioj::levels::authoring::catalog_category(result.entries[1].definition.GetValue()) ==
                ::ioj::levels::authoring::ELevelCatalogCategory::BattleViewer);
        TestRunner->TestFalse(TEXT("Malformed entries are retained but invalid"),
                              static_cast<bool>(result.entries[2]));
        TestRunner->TestTrue(TEXT("Malformed entries include a useful error"),
                             !result.entries[2].error.IsEmpty());
    }

    TEST_METHOD(RejectsDuplicateLevelIds)
    {
        FTemporaryScriptDirectory directory;
        auto const files_written{
            directory.write(TEXT("alpha.scm"), valid_level_script(TEXT("shared"), TEXT("Alpha"))) &&
            directory.write(TEXT("bravo.scm"), valid_level_script(TEXT("shared"), TEXT("Bravo")))};
        if (!TestRunner->TestTrue(TEXT("Test scripts are written"), files_written)) {
            return;
        }

        auto const result{
            ::ioj::levels::authoring::load_level_script_catalog(write_test_root(directory.path))};
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
        auto const files_written{
            directory.write(TEXT("alpha.scm"), valid_level_script(TEXT("alpha"), TEXT("Alpha"))) &&
            directory.write(TEXT("bravo.scm"), valid_level_script(TEXT("bravo"), TEXT("Bravo"))) &&
            directory.write(
                TEXT("Campaigns/main.scm"),
                TEXT("(campaign :id 'main :title \"Main Campaign\" :levels '(bravo alpha))"))};
        if (!TestRunner->TestTrue(TEXT("Campaign fixtures are written"), files_written)) {
            return;
        }

        auto const result{
            ::ioj::levels::authoring::load_level_script_catalog(write_test_root(directory.path))};
        TestRunner->TestTrue(TEXT("Catalog is valid"), result.error.IsEmpty());
        if (!TestRunner->TestEqual(TEXT("One campaign is discovered"), result.campaigns.Num(), 1) ||
            !TestRunner->TestTrue(TEXT("Campaign is valid"),
                                  static_cast<bool>(result.campaigns[0]))) {
            return;
        }
        auto const& campaign{result.campaigns[0].definition.GetValue()};
        TestRunner->TestEqual(TEXT("Campaign order is retained"), campaign.level_ids.Num(), 2);
        TestRunner->TestTrue(TEXT("First declared level remains first"),
                             campaign.level_ids[0] == ioj::FLevelId{FName{TEXT("bravo")}});
        TestRunner->TestTrue(TEXT("Second declared level remains second"),
                             campaign.level_ids[1] == ioj::FLevelId{FName{TEXT("alpha")}});
    }

    TEST_METHOD(RejectsMissingCampaignAndUnlockReferences)
    {
        FTemporaryScriptDirectory directory;
        auto const files_written{
            directory.write(
                TEXT("locked.scm"),
                valid_level_script(TEXT("locked"),
                                   TEXT("Locked"),
                                   TEXT(":unlock (list (level-completed 'missing))"))) &&
            directory.write(TEXT("Campaigns/main.scm"),
                            TEXT("(campaign :id 'main :title \"Main\" :levels '(missing))"))};
        if (!TestRunner->TestTrue(TEXT("Invalid fixtures are written"), files_written)) {
            return;
        }

        auto const result{
            ::ioj::levels::authoring::load_level_script_catalog(write_test_root(directory.path))};
        TestRunner->TestFalse(TEXT("Missing references are reported"), result.error.IsEmpty());
        TestRunner->TestFalse(TEXT("Level with missing prerequisite is invalid"),
                              static_cast<bool>(result.entries[0]));
        TestRunner->TestFalse(TEXT("Campaign with missing level is invalid"),
                              static_cast<bool>(result.campaigns[0]));
    }

    TEST_METHOD(RejectsDuplicateCampaignIds)
    {
        FTemporaryScriptDirectory directory;
        auto const files_written{
            directory.write(TEXT("alpha.scm"), valid_level_script(TEXT("alpha"), TEXT("Alpha"))) &&
            directory.write(TEXT("Campaigns/first.scm"),
                            TEXT("(campaign :id 'shared :title \"First\" :levels '(alpha))")) &&
            directory.write(TEXT("Campaigns/second.scm"),
                            TEXT("(campaign :id 'shared :title \"Second\" :levels '(alpha))"))};
        if (!TestRunner->TestTrue(TEXT("Duplicate campaign fixtures are written"), files_written)) {
            return;
        }

        auto const result{
            ::ioj::levels::authoring::load_level_script_catalog(write_test_root(directory.path))};
        if (!TestRunner->TestEqual(
                TEXT("Both campaigns remain visible"), result.campaigns.Num(), 2)) {
            return;
        }
        TestRunner->TestFalse(TEXT("First duplicate campaign is invalid"),
                              static_cast<bool>(result.campaigns[0]));
        TestRunner->TestFalse(TEXT("Second duplicate campaign is invalid"),
                              static_cast<bool>(result.campaigns[1]));
        TestRunner->TestTrue(TEXT("Duplicate error identifies both campaign files"),
                             result.error.Contains(TEXT("first.scm")) &&
                                 result.error.Contains(TEXT("second.scm")));
    }
};
