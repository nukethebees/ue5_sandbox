#include "USFPathValidationSubsystem.h"

BEGIN_DEFINE_SPEC(FUSFPathValidationSpec,
                  "Sandbox.USFPathValidation",
                  EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FUSFPathValidationSpec)

void FUSFPathValidationSpec::Define() {
    It("should be available in editor context", [this]() {
        UUSFPathValidationSubsystem* Subsystem =
            GEditor ? GEditor->GetEditorSubsystem<UUSFPathValidationSubsystem>() : nullptr;
        TestNotNull("USF validation subsystem", Subsystem);
    });

    It("should validate project test shader", [this]() {
        FString const path = TEXT("/Project/TestDummy.usf");
        bool const valid = UUSFPathValidationSubsystem::ValidateUSFPath(path);
        TestTrue(*FString::Printf(TEXT("'%s' should be valid"), *path), valid);
    });

    It("should reject non-existent path", [this]() {
        FString const path = TEXT("/NonExistent/Path/Missing.usf");
        bool const valid = UUSFPathValidationSubsystem::ValidateUSFPath(path);
        TestFalse(*FString::Printf(TEXT("'%s' should be invalid"), *path), valid);
    });

    It("should reject path without leading slash", [this]() {
        FString const path = TEXT("NoLeadingSlash.usf");
        bool const valid = UUSFPathValidationSubsystem::ValidateUSFPath(path);
        TestFalse(*FString::Printf(TEXT("'%s' should be invalid"), *path), valid);
    });

    It("should reject empty path", [this]() {
        FString const path = TEXT("");
        bool const valid = UUSFPathValidationSubsystem::ValidateUSFPath(path);
        TestFalse(*FString::Printf(TEXT("'%s' (empty path) should be invalid"), *path), valid);
    });
}
