#include "USFPathValidation.h"

#include "CQTest.h"
#include "MaterialExpressionUSFLoader.h"
#include "Misc/Paths.h"

TEST_CLASS(USFPathValidation, "USFLoader.UnitTests")
{
    TEST_METHOD(AcceptsPluginShaderPath)
    {
        FString const path{TEXT("/Plugin/USFLoader/TestDummy.usf")};
        auto const resolved_path{usf_loader::resolve_shader_path(path)};
        if (!TestRunner->TestTrue(TEXT("Plugin test shader path resolves"),
                                  resolved_path.has_value())) {
            return;
        }
        TestRunner->TestTrue(TEXT("Resolved plugin test shader exists"),
                             FPaths::FileExists(*resolved_path));
    }

    TEST_METHOD(CombinesPrefixAndDeduplicatesPaths)
    {
        TArray<FString> const file_paths{TEXT("/TestDummy.usf"), TEXT("TestDummy.usf")};
        auto const include_paths{
            usf_loader::resolve_include_paths(TEXT("/Plugin/USFLoader/"), file_paths)};
        if (!TestRunner->TestTrue(TEXT("Prefixed include paths resolve"),
                                  include_paths.has_value())) {
            return;
        }
        TestRunner->TestEqual(TEXT("Duplicate include paths are removed"), include_paths->Num(), 1);
        if (include_paths->Num() == 1) {
            TestRunner->TestEqual(TEXT("Virtual path separators are normalized"),
                                  (*include_paths)[0],
                                  FString{TEXT("/Plugin/USFLoader/TestDummy.usf")});
        }
    }

    TEST_METHOD(MaterialExpressionReportsIncludeDependencies)
    {
        auto* const expression{NewObject<UMaterialExpressionUSFLoader>()};
        expression->path_prefix = TEXT("/Plugin/USFLoader");
        expression->usf_file_paths = {TEXT("TestDummy.usf"), TEXT("/TestDummy.usf")};

        TSet<FString> include_paths;
        expression->GetIncludeFilePaths(include_paths);

        TestRunner->TestEqual(
            TEXT("Material expression reports one unique dependency"), include_paths.Num(), 1);
        TestRunner->TestTrue(TEXT("Material expression reports the plugin shader dependency"),
                             include_paths.Contains(TEXT("/Plugin/USFLoader/TestDummy.usf")));
    }

    TEST_METHOD(RejectsNonExistentPath)
    {
        FString const path{TEXT("/NonExistent/Path/Missing.usf")};
        auto const resolved_path{usf_loader::resolve_shader_path(path)};
        TestRunner->TestFalse(TEXT("Non-existent shader path is invalid"),
                              resolved_path.has_value());
    }

    TEST_METHOD(RejectsPathWithoutLeadingSlash)
    {
        FString const path{TEXT("NoLeadingSlash.usf")};
        auto const resolved_path{usf_loader::resolve_shader_path(path)};
        TestRunner->TestFalse(TEXT("Shader path without a leading slash is invalid"),
                              resolved_path.has_value());
    }

    TEST_METHOD(RejectsEmptyIncludeEntry)
    {
        TArray<FString> const file_paths{TEXT("")};
        auto const include_paths{usf_loader::resolve_include_paths({}, file_paths)};
        TestRunner->TestFalse(TEXT("Empty include entry is invalid"), include_paths.has_value());
        if (!include_paths) {
            TestRunner->TestTrue(TEXT("Empty include error identifies its index"),
                                 include_paths.error().Contains(TEXT("index 0")));
        }
    }

    TEST_METHOD(RejectsInvalidVirtualPathSyntax)
    {
        auto const traversal_path{
            usf_loader::resolve_shader_path(TEXT("/Plugin/USFLoader/../TestDummy.usf"))};
        auto const wrong_extension{
            usf_loader::resolve_shader_path(TEXT("/Plugin/USFLoader/TestDummy.txt"))};

        TestRunner->TestFalse(TEXT("Parent path traversal is invalid"), traversal_path.has_value());
        TestRunner->TestFalse(TEXT("Non-shader extension is invalid"), wrong_extension.has_value());
    }
};
