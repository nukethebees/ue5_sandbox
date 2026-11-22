#include <ioj/files.h>
#include <SpaceGamePresentation/audio/AudioSourceLibrary.h>

#include <SandboxCoreEngine/strings.h>

#include <CQTest.h>
#include <Misc/Paths.h>

TEST_CLASS(AudioSourceLibrary, "Sandbox.UnitTests")
{
    TEST_METHOD(NormalizesUnrealPathsBeforeDiscovery)
    {
        auto const path{FPaths::ProjectDir()};
        auto expected{FPaths::ConvertRelativePathToFull(path)};
        FPaths::NormalizeDirectoryName(expected);
        auto const result{ioj::resolve_audio_source_library(path)};
        TestRunner->TestTrue(TEXT("Existing Unreal project directory resolves"), result.root.valid);
        auto actual{ioj::to_fstring(ioj::path_to_utf8(result.root.path))};
        FPaths::NormalizeDirectoryName(actual);
        TestRunner->TestEqual(TEXT("Adapter preserves normalized Unreal paths"), actual, expected);
    }
};
