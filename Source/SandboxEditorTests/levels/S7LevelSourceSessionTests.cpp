#include <SandboxEditor/levels/S7LevelAuthoringDocument.h>
#include <SandboxEditor/levels/S7LevelAuthoringSession.h>
#include <SandboxEditor/levels/S7LevelSourceSession.h>

#include <CQTest.h>
#include <Engine/World.h>
#include <HAL/FileManager.h>
#include <Misc/FileHelper.h>
#include <Misc/Guid.h>
#include <Misc/Paths.h>
#include <Tests/AutomationEditorCommon.h>

namespace {
struct FTemporarySourceDirectory {
    FString path{FPaths::Combine(FPaths::ProjectSavedDir(),
                                 TEXT("Automation"),
                                 TEXT("S7LevelSourceSession"),
                                 FGuid::NewGuid().ToString())};

    FTemporarySourceDirectory() { IFileManager::Get().MakeDirectory(*path, true); }

    ~FTemporarySourceDirectory() { IFileManager::Get().DeleteDirectory(*path, false, true); }
};

auto spawn_document(UWorld& world, TCHAR const* const label) -> AS7LevelAuthoringDocument* {
    auto* const document{world.SpawnActor<AS7LevelAuthoringDocument>()};
    if (IsValid(document)) {
        document->SetActorLabel(label, true);
    }
    return document;
}

auto write_source(FStringView const source, FString const& path) -> bool {
    return FFileHelper::SaveStringToFile(
        FString{source}, *path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

auto level_source(FStringView const title) -> FString {
    return FString::Printf(
        TEXT("(level :id 'source-session :title \"%s\" :player 'player :entities "
             "(list (entity :id 'player :archetype 'player-fighter :team 'blue :position '(0 0 0) "
             ":rotation '(0 0 0))))"),
        *FString{title});
}
}

TEST_CLASS(S7LevelSourceSession, "Sandbox.UnitTests")
{
    TEST_METHOD(DirtyBufferDetachesWhenTheDocumentChanges)
    {
        FTemporarySourceDirectory directory;
        auto* const world{FAutomationEditorCommonUtils::CreateNewMap()};
        auto* const first{spawn_document(*world, TEXT("First Document"))};
        auto* const second{spawn_document(*world, TEXT("Second Document"))};
        auto const first_path{FPaths::Combine(directory.path, TEXT("first.scm"))};
        auto const second_path{FPaths::Combine(directory.path, TEXT("second.scm"))};
        if (!TestRunner->TestNotNull(TEXT("First document"), first) ||
            !TestRunner->TestNotNull(TEXT("Second document"), second) ||
            !TestRunner->TestTrue(TEXT("First source is written"),
                                  write_source(level_source(TEXT("First")), first_path)) ||
            !TestRunner->TestTrue(TEXT("Second source is written"),
                                  write_source(level_source(TEXT("Second")), second_path))) {
            return;
        }
        first->source_path = first_path;
        second->source_path = second_path;

        ioj::editor::FS7LevelSourceSession session;
        auto const loaded{session.attach(*first)};
        if (!TestRunner->TestTrue(TEXT("First source loads"), loaded.has_value())) {
            TestRunner->AddError(loaded.error());
            return;
        }
        session.set_buffer(TEXT("unsaved buffer"));
        session.state().scene_dirty = true;
        session.state().preview = ioj::levels::authoring::PreviewState::Valid;
        auto const preserved{session.buffer()};

        auto const switched{session.attach(*second)};
        TestRunner->TestFalse(TEXT("Unsafe automatic switch is refused"), switched.has_value());
        TestRunner->TestFalse(TEXT("Dirty buffer is detached"), session.is_attached());
        TestRunner->TestFalse(TEXT("Detachment is distinct from disk conflict"),
                              session.has_external_conflict());
        TestRunner->TestEqual(TEXT("Dirty buffer is preserved"), session.buffer(), preserved);
        TestRunner->TestFalse(TEXT("Switch clears stale scene-dirty state"),
                              session.state().scene_dirty);
        TestRunner->TestTrue(TEXT("Switch clears the previous preview"),
                             session.state().preview == ioj::levels::authoring::PreviewState::None);

        auto const discarded{session.discard_and_attach(*second)};
        if (!TestRunner->TestTrue(TEXT("Explicit discard permits switch"), discarded.has_value())) {
            TestRunner->AddError(discarded.error());
            return;
        }
        TestRunner->TestTrue(TEXT("Second document is attached"), session.document() == second);
        TestRunner->TestTrue(TEXT("Second source replaces discarded buffer"),
                             session.buffer().Contains(TEXT("Second")));
    }
};
