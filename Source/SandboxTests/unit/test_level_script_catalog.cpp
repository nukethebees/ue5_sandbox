#include <SpaceGameS7/level_script_catalog.h>

#include <CQTest.h>

TEST_CLASS(LevelScriptCatalog, "Sandbox.UnitTests")
{

    TEST_METHOD(LoadsCanonicalResourcesAndProjectScriptPaths)
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
};
