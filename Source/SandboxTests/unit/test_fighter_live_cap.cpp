#include <CQTest.h>
#include <SandboxGameShared/core/SandboxDeveloperSettings.h>
#include <UObject/UObjectGlobals.h>

TEST_CLASS(FighterLiveCap, "Sandbox.UnitTests")
{
    TEST_METHOD(SettingsClamp)
    {
        auto* settings{NewObject<USandboxDeveloperSettings>()};
        TestRunner->TestEqual(TEXT("Default live-fighter cap"), settings->max_live_fighters, 2000);
        settings->max_live_fighters = 1;
        TestRunner->TestEqual(TEXT("Effective live-fighter cap applies hard minimum"),
                              settings->get_effective_max_live_fighters(),
                              USandboxDeveloperSettings::minimum_max_live_fighters);
    }
};
