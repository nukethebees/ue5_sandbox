#include <ioj/files.h>
#include <SpaceGamePresentation/audio/BoomAudioLibrary.h>

#include <SandboxCoreEngine/strings.h>

#include <CQTest.h>
#include <Misc/Paths.h>

TEST_CLASS(BoomAudioLibrary, "Sandbox.UnitTests")
{
    TEST_METHOD(NormalizesUnrealPathsBeforeDiscovery)
    {
        auto const path{FPaths::ProjectDir()};
        auto expected{FPaths::ConvertRelativePathToFull(path)};
        FPaths::NormalizeDirectoryName(expected);
        auto const result{ioj::resolve_boom_audio_library(path)};
        TestRunner->TestTrue(TEXT("Existing Unreal project directory resolves"), result.root.valid);
        auto actual{ioj::to_fstring(ioj::path_to_utf8(result.root.path))};
        FPaths::NormalizeDirectoryName(actual);
        TestRunner->TestEqual(TEXT("Adapter preserves normalized Unreal paths"), actual, expected);
    }
};
