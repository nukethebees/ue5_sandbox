#include "SandboxEditor/USFPathValidationSubsystem.h"

BEGIN_DEFINE_SPEC(FUSFPathValidationSpec,
                  "Sandbox.USFPathValidation",
                  EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
// Test data structure
struct FUSFPathTestCase
{
    FString VirtualPath;
    bool bShouldBeValid;
    FString Description;

    FUSFPathTestCase(const FString& InPath, bool bInShouldBeValid, const FString& InDescription)
        : VirtualPath(InPath), bShouldBeValid(bInShouldBeValid), Description(InDescription)
    {
    }
};

TArray<FUSFPathTestCase> TestCases;
END_DEFINE_SPEC(FUSFPathValidationSpec)

void FUSFPathValidationSpec::Define()
{
    // Setup test cases
    TestCases = {
        // Valid engine paths (should exist in standard UE5 installation)
        FUSFPathTestCase(TEXT("/Engine/Private/Common.ush"), true, TEXT("Common engine header")),
        FUSFPathTestCase(TEXT("/Engine/Public/Platform.ush"), true, TEXT("Platform definitions")),

        // Our test dummy shader
        FUSFPathTestCase(TEXT("/Project/Shaders/TestDummy.usf"), true, TEXT("Project test shader")),

        // Invalid paths
        FUSFPathTestCase(TEXT("/NonExistent/Path/Missing.usf"), false, TEXT("Non-existent path")),
        FUSFPathTestCase(TEXT("NoLeadingSlash.usf"), false, TEXT("Path without leading slash")),
        FUSFPathTestCase(TEXT("/Engine/Private/DoesNotExist.usf"), false, TEXT("Non-existent engine file")),
        FUSFPathTestCase(TEXT(""), false, TEXT("Empty path")),

        // Edge cases
        FUSFPathTestCase(TEXT("/Engine/Private/../Common.ush"), true, TEXT("Path with relative components")),
        FUSFPathTestCase(TEXT("/Engine/Private/Common.txt"), false, TEXT("Wrong file extension")),
    };

    Describe("USFPathValidationSubsystem", [this]()
    {
        It("should be available in editor context", [this]()
        {
            // Verify subsystem is accessible
            UUSFPathValidationSubsystem* Subsystem = GEditor ?
                GEditor->GetEditorSubsystem<UUSFPathValidationSubsystem>() : nullptr;
            TestNotNull("USF validation subsystem", Subsystem);
        });

        Describe("ValidateUSFPath", [this]()
        {
            for (const FUSFPathTestCase& TestCase : TestCases)
            {
                It(*FString::Printf(TEXT("should validate '%s' correctly (%s)"),
                                    *TestCase.VirtualPath, *TestCase.Description),
                   [this, TestCase]()
                {
                    bool bActualValid = UUSFPathValidationSubsystem::ValidateUSFPath(TestCase.VirtualPath);
                    TestEqual(*FString::Printf(TEXT("Validation of '%s'"), *TestCase.VirtualPath),
                             bActualValid, TestCase.bShouldBeValid);
                });
            }
        });

        Describe("ResolveUSFPath", [this]()
        {
            It("should resolve valid engine paths", [this]()
            {
                auto ResolvedPath = UUSFPathValidationSubsystem::ResolveUSFPath(TEXT("/Engine/Private/Common.ush"));
                TestTrue("Should resolve valid engine path", ResolvedPath.IsSet());

                if (ResolvedPath.IsSet())
                {
                    TestTrue("Resolved path should be absolute",
                             FPaths::IsRelative(ResolvedPath.GetValue()) == false);
                    TestTrue("Resolved path should contain 'Common.ush'",
                             ResolvedPath.GetValue().Contains(TEXT("Common.ush")));
                }
            });

            It("should not resolve invalid paths", [this]()
            {
                auto ResolvedPath = UUSFPathValidationSubsystem::ResolveUSFPath(TEXT("/Invalid/Path.usf"));
                TestFalse("Should not resolve invalid path", ResolvedPath.IsSet());
            });
        });

        Describe("Caching behavior", [this]()
        {
            It("should cache results for performance", [this]()
            {
                // Clear cache first
                UUSFPathValidationSubsystem::ClearCache();
                TestEqual("Cache should be empty after clear", UUSFPathValidationSubsystem::GetCacheSize(), 0);

                // First validation should populate cache
                bool bValid1 = UUSFPathValidationSubsystem::ValidateUSFPath(TEXT("/Engine/Private/Common.ush"));
                TestEqual("Cache should have one entry", UUSFPathValidationSubsystem::GetCacheSize(), 1);

                // Second validation should use cache
                bool bValid2 = UUSFPathValidationSubsystem::ValidateUSFPath(TEXT("/Engine/Private/Common.ush"));
                TestEqual("Cache should still have one entry", UUSFPathValidationSubsystem::GetCacheSize(), 1);
                TestEqual("Cached result should match", bValid1, bValid2);
            });
        });
    });
}