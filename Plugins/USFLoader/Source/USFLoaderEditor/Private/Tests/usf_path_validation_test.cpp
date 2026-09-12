#include "USFPathValidationSubsystem.h"

#include "CQTest.h"
#include "Editor.h"

TEST_CLASS(USFPathValidation, "USFLoader.UnitTests")
{
    TEST_METHOD(EditorSubsystemIsAvailable)
    {
        auto* const subsystem{GEditor != nullptr
                                  ? GEditor->GetEditorSubsystem<UUSFPathValidationSubsystem>()
                                  : nullptr};
        TestRunner->TestNotNull(TEXT("USF validation subsystem"), subsystem);
    }

    TEST_METHOD(AcceptsPluginShaderPath)
    {
        FString const path{TEXT("/Plugin/USFLoader/TestDummy.usf")};
        bool const valid{UUSFPathValidationSubsystem::ValidateUSFPath(path)};
        TestRunner->TestTrue(TEXT("Plugin test shader path is valid"), valid);
    }

    TEST_METHOD(RejectsNonExistentPath)
    {
        FString const path{TEXT("/NonExistent/Path/Missing.usf")};
        bool const valid{UUSFPathValidationSubsystem::ValidateUSFPath(path)};
        TestRunner->TestFalse(TEXT("Non-existent shader path is invalid"), valid);
    }

    TEST_METHOD(RejectsPathWithoutLeadingSlash)
    {
        FString const path{TEXT("NoLeadingSlash.usf")};
        bool const valid{UUSFPathValidationSubsystem::ValidateUSFPath(path)};
        TestRunner->TestFalse(TEXT("Shader path without a leading slash is invalid"), valid);
    }

    TEST_METHOD(RejectsEmptyPath)
    {
        FString const path{};
        bool const valid{UUSFPathValidationSubsystem::ValidateUSFPath(path)};
        TestRunner->TestFalse(TEXT("Empty shader path is invalid"), valid);
    }
};
