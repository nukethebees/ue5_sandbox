#include <SandboxEditor/levels/S7LevelAuthoringDocument.h>
#include <SandboxEditor/levels/S7LevelAuthoringSession.h>
#include <SandboxEditor/levels/S7LevelSourceSession.h>

#include <SandboxCoreEngine/strings.h>

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
    TEST_METHOD(LoadEditSaveAndReloadTrackStateAndRevision)
    {
        FTemporarySourceDirectory directory;
        auto* const world{FAutomationEditorCommonUtils::CreateNewMap()};
        auto* const document{spawn_document(*world, TEXT("Source Document"))};
        auto const source_path{FPaths::Combine(directory.path, TEXT("level.scm"))};
        auto const initial{level_source(TEXT("Initial"))};
        if (!TestRunner->TestNotNull(TEXT("Document"), document) ||
            !TestRunner->TestTrue(TEXT("Initial source is written"),
                                  write_source(initial, source_path))) {
            return;
        }
        document->source_path = source_path;

        ioj::editor::FS7LevelSourceSession session;
        auto const loaded{session.load(*document, source_path)};
        if (!TestRunner->TestTrue(TEXT("Source loads"), loaded.has_value())) {
            TestRunner->AddError(loaded.error());
            return;
        }
        auto const loaded_revision{session.revision()};
        TestRunner->TestEqual(TEXT("Loaded text is retained"), session.buffer(), initial);
        TestRunner->TestFalse(TEXT("Loaded source is clean"), session.is_dirty());

        session.set_buffer(initial);
        TestRunner->TestEqual(
            TEXT("No-op edit preserves revision"), session.revision(), loaded_revision);
        auto const edited{initial + TEXT("\r\n;; exact £ text\n")};
        session.set_buffer(edited);
        TestRunner->TestTrue(TEXT("Edit makes source dirty"), session.is_dirty());
        TestRunner->TestEqual(
            TEXT("Edit advances revision"), session.revision(), loaded_revision + 1);

        auto const saved{session.save()};
        if (!TestRunner->TestTrue(TEXT("Edited source saves"), saved.has_value())) {
            TestRunner->AddError(saved.error());
            return;
        }
        FString disk_source;
        TArray<uint8> disk_bytes;
        TestRunner->TestTrue(TEXT("Saved source reads"),
                             FFileHelper::LoadFileToString(disk_source, *source_path));
        TestRunner->TestEqual(TEXT("Save preserves exact text"), disk_source, edited);
        TestRunner->TestTrue(TEXT("Saved bytes read"),
                             FFileHelper::LoadFileToArray(disk_bytes, *source_path));
        TestRunner->TestFalse(TEXT("Save omits UTF-8 BOM"),
                              disk_bytes.Num() >= 3 && disk_bytes[0] == 0xef &&
                                  disk_bytes[1] == 0xbb && disk_bytes[2] == 0xbf);
        TestRunner->TestFalse(TEXT("Saved source is clean"), session.is_dirty());

        auto const replacement{level_source(TEXT("Reloaded"))};
        TestRunner->TestTrue(TEXT("Replacement source is written"),
                             write_source(replacement, source_path));
        auto const reloaded{session.reload()};
        if (!TestRunner->TestTrue(TEXT("Source reloads"), reloaded.has_value())) {
            TestRunner->AddError(reloaded.error());
            return;
        }
        TestRunner->TestEqual(TEXT("Reload replaces the buffer"), session.buffer(), replacement);
        TestRunner->TestTrue(TEXT("Reload advances revision"),
                             session.revision() > loaded_revision + 1);
        TestRunner->TestFalse(TEXT("Reloaded source is clean"), session.is_dirty());
    }

    TEST_METHOD(BufferParsingUsesSiblingLibraryDirectory)
    {
        FTemporarySourceDirectory directory;
        auto* const world{FAutomationEditorCommonUtils::CreateNewMap()};
        auto* const document{spawn_document(*world, TEXT("Library Document"))};
        auto const library_directory{FPaths::Combine(directory.path, TEXT("Libraries"))};
        auto const source_path{FPaths::Combine(directory.path, TEXT("level.scm"))};
        IFileManager::Get().MakeDirectory(*library_directory, true);
        FString const source{
            TEXT("(load-script \"metadata.scm\") (level :id 'library-level :title shared-title "
                 " :settings (settings :collision-grid (collision-grid :cell-size shared-cells) "
                 " :presentation (presentation :entity-overlay (entity-overlay :enabled #f))) "
                 " :player 'player :entities (list (entity :id 'player :archetype "
                 "'player-fighter :team 'blue :position '(0 0 0) :rotation '(0 0 0))))")};
        if (!TestRunner->TestNotNull(TEXT("Document"), document) ||
            !TestRunner->TestTrue(
                TEXT("Library is written"),
                write_source(TEXT("(define shared-title \"From Sibling Library\") (define "
                                  "shared-cells '(6000 6000 20000))"),
                             FPaths::Combine(library_directory, TEXT("metadata.scm")))) ||
            !TestRunner->TestTrue(TEXT("Level is written"), write_source(source, source_path))) {
            return;
        }
        document->source_path = source_path;

        ioj::editor::FS7LevelSourceSession session;
        auto const loaded{session.attach(*document)};
        if (!TestRunner->TestTrue(TEXT("Source loads"), loaded.has_value())) {
            TestRunner->AddError(loaded.error());
            return;
        }
        auto const read{session.read()};
        if (!TestRunner->TestTrue(TEXT("Buffer parses"), static_cast<bool>(read))) {
            TestRunner->AddError(ioj::to_fstring(ioj::levels::format_diagnostics(read.error())));
            return;
        }
        TestRunner->TestEqual(TEXT("Sibling library supplies title"),
                              ioj::to_fstring(read->metadata().title),
                              FString{TEXT("From Sibling Library")});

        session.set_buffer(source.Replace(TEXT("shared-title"), TEXT("\"From Buffer\"")));
        auto const edited_read{session.read()};
        if (!TestRunner->TestTrue(TEXT("Edited buffer parses without saving"),
                                  static_cast<bool>(edited_read))) {
            TestRunner->AddError(
                ioj::to_fstring(ioj::levels::format_diagnostics(edited_read.error())));
            return;
        }
        TestRunner->TestEqual(TEXT("Preview input comes from the buffer"),
                              ioj::to_fstring(edited_read->metadata().title),
                              FString{TEXT("From Buffer")});
        FString unchanged_disk_source;
        FFileHelper::LoadFileToString(unchanged_disk_source, *source_path);
        TestRunner->TestEqual(TEXT("Buffer parsing does not write the source file"),
                              unchanged_disk_source,
                              FString{source});

        auto const replay{session.read_replay({})};
        if (!TestRunner->TestTrue(TEXT("Replay captures evaluated inputs"), replay.has_value())) {
            return;
        }
        document->evaluated_level_source = replay->evaluated_source;
        if (!TestRunner->TestTrue(
                TEXT("Library changes on disk"),
                write_source(TEXT("(define shared-title \"Changed Library\") (define shared-cells "
                                  "'(7000 7000 20000))"),
                             FPaths::Combine(library_directory, TEXT("metadata.scm"))))) {
            return;
        }
        auto const applied_settings{ioj::editor::read_s7_document_settings(*document)};
        TestRunner->TestTrue(TEXT("Applied settings retain their evaluated library input"),
                             applied_settings.has_value() &&
                                 *applied_settings == replay->definition.settings());
        auto const next_replay{session.read_replay({})};
        TestRunner->TestTrue(TEXT("Explicit replay sees updated library input"),
                             next_replay.has_value() &&
                                 next_replay->definition.settings().collision_grid.cell_size ==
                                     ioj::Vector3f{7000.f, 7000.f, 20000.f});
    }

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

        auto const recovery_path{FPaths::Combine(directory.path, TEXT("recovered.scm"))};
        TestRunner->TestTrue(
            TEXT("Detached buffer can be recovered to a new file"),
            session.save_as(recovery_path, ioj::editor::ES7SourceOverwritePolicy::RefuseExisting)
                .has_value());
        TestRunner->TestFalse(TEXT("Recovery does not attach to the new document"),
                              session.is_attached());
        TestRunner->TestFalse(
            TEXT("Automatic attachment still requires explicit recovery resolution"),
            session.attach(*second).has_value());
        FString recovered;
        FFileHelper::LoadFileToString(recovered, *recovery_path);
        TestRunner->TestEqual(TEXT("Recovery preserves exact text"), recovered, preserved);

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
