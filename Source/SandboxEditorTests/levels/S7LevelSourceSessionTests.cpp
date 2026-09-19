#include <SandboxEditor/levels/S7LevelAuthoringDocument.h>
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
    return FString::Printf(TEXT("(level (id 'source-session) (title \"%s\") "
                                "(teams (team 'blue)) (player 'player) "
                                "(entities (entity 'player 'player-fighter 'blue "
                                "(position 0 0 0) (rotation 0 0 0))))"),
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

        ml::editor::FS7LevelSourceSession session;
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
        FString const source{TEXT("(load-script \"metadata.scm\") "
                                  "(level (id 'library-level) (title shared-title) "
                                  "(teams (team 'blue)) (player 'player) "
                                  "(entities (entity 'player 'player-fighter 'blue "
                                  "(position 0 0 0) (rotation 0 0 0))))")};
        if (!TestRunner->TestNotNull(TEXT("Document"), document) ||
            !TestRunner->TestTrue(
                TEXT("Library is written"),
                write_source(TEXT("(define shared-title \"From Sibling Library\")"),
                             FPaths::Combine(library_directory, TEXT("metadata.scm")))) ||
            !TestRunner->TestTrue(TEXT("Level is written"), write_source(source, source_path))) {
            return;
        }
        document->source_path = source_path;

        ml::editor::FS7LevelSourceSession session;
        auto const loaded{session.attach(*document)};
        if (!TestRunner->TestTrue(TEXT("Source loads"), loaded.has_value())) {
            TestRunner->AddError(loaded.error());
            return;
        }
        auto const read{session.read()};
        if (!TestRunner->TestTrue(TEXT("Buffer parses"), static_cast<bool>(read))) {
            TestRunner->AddError(read.script_error);
            return;
        }
        TestRunner->TestEqual(TEXT("Sibling library supplies title"),
                              read.definition->metadata.title,
                              FString{TEXT("From Sibling Library")});

        session.set_buffer(source.Replace(TEXT("shared-title"), TEXT("\"From Buffer\"")));
        auto const edited_read{session.read()};
        if (!TestRunner->TestTrue(TEXT("Edited buffer parses without saving"),
                                  static_cast<bool>(edited_read))) {
            TestRunner->AddError(edited_read.script_error);
            return;
        }
        TestRunner->TestEqual(TEXT("Preview input comes from the buffer"),
                              edited_read.definition->metadata.title,
                              FString{TEXT("From Buffer")});
        FString unchanged_disk_source;
        FFileHelper::LoadFileToString(unchanged_disk_source, *source_path);
        TestRunner->TestEqual(TEXT("Buffer parsing does not write the source file"),
                              unchanged_disk_source,
                              FString{source});
    }

    TEST_METHOD(SaveAsRequiresExplicitReplacementAndAdvancesPathRevision)
    {
        FTemporarySourceDirectory directory;
        auto* const world{FAutomationEditorCommonUtils::CreateNewMap()};
        auto* const document{spawn_document(*world, TEXT("Save As Document"))};
        if (!TestRunner->TestNotNull(TEXT("Document"), document)) {
            return;
        }

        ml::editor::FS7LevelSourceSession session;
        auto const attached{session.attach(*document)};
        if (!TestRunner->TestTrue(TEXT("Unsaved buffer attaches"), attached.has_value())) {
            TestRunner->AddError(attached.error());
            return;
        }
        auto const source{level_source(TEXT("Save As"))};
        session.set_buffer(source);
        auto const target{FPaths::Combine(directory.path, TEXT("target.scm"))};
        TestRunner->TestTrue(TEXT("Unrelated target is written"),
                             write_source(TEXT("unrelated"), target));

        auto const refused{
            session.save_as(target, ml::editor::ES7SourceOverwritePolicy::RefuseExisting)};
        TestRunner->TestFalse(TEXT("Existing target is refused"), refused.has_value());
        FString retained;
        FFileHelper::LoadFileToString(retained, *target);
        TestRunner->TestEqual(
            TEXT("Refusal preserves target"), retained, FString{TEXT("unrelated")});

        auto const prior_revision{session.revision()};
        auto const replaced{
            session.save_as(target, ml::editor::ES7SourceOverwritePolicy::ReplaceExisting)};
        if (!TestRunner->TestTrue(TEXT("Confirmed replacement saves"), replaced.has_value())) {
            TestRunner->AddError(replaced.error());
            return;
        }
        FFileHelper::LoadFileToString(retained, *target);
        TestRunner->TestEqual(TEXT("Confirmed replacement writes buffer"), retained, source);
        TestRunner->TestTrue(TEXT("Path change advances revision"),
                             session.revision() > prior_revision);
    }

    TEST_METHOD(ExternalModificationAndDeletionBlockSaves)
    {
        FTemporarySourceDirectory directory;
        auto* const world{FAutomationEditorCommonUtils::CreateNewMap()};
        auto* const document{spawn_document(*world, TEXT("Conflict Document"))};
        auto const source_path{FPaths::Combine(directory.path, TEXT("level.scm"))};
        auto const initial{level_source(TEXT("Initial"))};
        if (!TestRunner->TestNotNull(TEXT("Document"), document) ||
            !TestRunner->TestTrue(TEXT("Initial source is written"),
                                  write_source(initial, source_path))) {
            return;
        }
        document->source_path = source_path;

        ml::editor::FS7LevelSourceSession session;
        auto const loaded{session.attach(*document)};
        if (!TestRunner->TestTrue(TEXT("Source loads"), loaded.has_value())) {
            TestRunner->AddError(loaded.error());
            return;
        }
        session.set_buffer(level_source(TEXT("Edited")));
        TestRunner->TestTrue(TEXT("External edit is written"),
                             write_source(TEXT("external"), source_path));
        TestRunner->TestFalse(TEXT("External edit blocks raw save"), session.save().has_value());
        TestRunner->TestTrue(TEXT("External edit sets conflict"), session.has_external_conflict());

        auto const reloaded{session.reload()};
        if (!TestRunner->TestTrue(TEXT("Conflict can be reloaded"), reloaded.has_value())) {
            TestRunner->AddError(reloaded.error());
            return;
        }
        session.set_buffer(TEXT("edited after reload"));
        TestRunner->TestTrue(TEXT("Source is deleted"), IFileManager::Get().Delete(*source_path));
        TestRunner->TestFalse(TEXT("Deletion blocks raw save"), session.save().has_value());
        TestRunner->TestTrue(TEXT("Deletion remains a conflict"), session.has_external_conflict());

        TestRunner->TestTrue(TEXT("Source can be restored for canonical conflict"),
                             write_source(initial, source_path));
        auto const canonical_reload{session.reload()};
        if (!TestRunner->TestTrue(TEXT("Restored source reloads"), canonical_reload.has_value())) {
            TestRunner->AddError(canonical_reload.error());
            return;
        }
        auto const synchronized_digest{
            ml::editor::FS7LevelSourceSession::source_digest(session.buffer())};
        TestRunner->TestTrue(TEXT("Second external edit is written"),
                             write_source(TEXT("external again"), source_path));
        TestRunner->TestFalse(
            TEXT("External edit blocks canonical save"),
            session.save_replacement(level_source(TEXT("Canonical")), synchronized_digest)
                .has_value());
    }

    TEST_METHOD(RawSaveDoesNotMakeSourceApplied)
    {
        FTemporarySourceDirectory directory;
        auto* const world{FAutomationEditorCommonUtils::CreateNewMap()};
        auto* const document{spawn_document(*world, TEXT("Synchronization Document"))};
        auto const source_path{FPaths::Combine(directory.path, TEXT("level.scm"))};
        auto const initial{level_source(TEXT("Initial"))};
        if (!TestRunner->TestNotNull(TEXT("Document"), document) ||
            !TestRunner->TestTrue(TEXT("Initial source is written"),
                                  write_source(initial, source_path))) {
            return;
        }
        document->source_path = source_path;
        document->synchronized_source_hash =
            ml::editor::FS7LevelSourceSession::source_digest(initial);
        document->synchronized_scene_hash = TEXT("scene-baseline");

        ml::editor::FS7LevelSourceSession session;
        auto const loaded{session.attach(*document)};
        if (!TestRunner->TestTrue(TEXT("Source loads"), loaded.has_value())) {
            TestRunner->AddError(loaded.error());
            return;
        }
        session.set_buffer(level_source(TEXT("Edited")));
        auto const raw_saved{session.save()};
        if (!TestRunner->TestTrue(TEXT("Raw buffer saves"), raw_saved.has_value())) {
            TestRunner->AddError(raw_saved.error());
            return;
        }
        TestRunner->TestEqual(TEXT("Raw save preserves applied-source synchronization"),
                              document->synchronized_source_hash,
                              ml::editor::FS7LevelSourceSession::source_digest(initial));
        TestRunner->TestEqual(TEXT("Raw save preserves scene synchronization"),
                              document->synchronized_scene_hash,
                              FString{TEXT("scene-baseline")});
        TestRunner->TestTrue(TEXT("Saved edit is still unapplied"),
                             session.has_unapplied_buffer(document->synchronized_source_hash));
        auto const canonical{session.save_replacement(level_source(TEXT("Canonical Scene")),
                                                      document->synchronized_source_hash)};
        TestRunner->TestFalse(TEXT("Canonical save refuses unapplied source"),
                              canonical.has_value());
        FString retained;
        FFileHelper::LoadFileToString(retained, *source_path);
        TestRunner->TestEqual(
            TEXT("Canonical refusal preserves raw source"), retained, session.buffer());
    }

    TEST_METHOD(UnexpectedFileCreationBlocksSave)
    {
        FTemporarySourceDirectory directory;
        auto* const world{FAutomationEditorCommonUtils::CreateNewMap()};
        auto* const document{spawn_document(*world, TEXT("New Source Document"))};
        auto const source_path{FPaths::Combine(directory.path, TEXT("new-level.scm"))};
        if (!TestRunner->TestNotNull(TEXT("Document"), document)) {
            return;
        }
        document->source_path = source_path;

        ml::editor::FS7LevelSourceSession session;
        auto const attached{session.attach(*document)};
        if (!TestRunner->TestTrue(TEXT("Absent source baseline attaches"), attached.has_value())) {
            TestRunner->AddError(attached.error());
            return;
        }
        session.set_buffer(level_source(TEXT("New Buffer")));
        TestRunner->TestTrue(TEXT("Unexpected source is created"),
                             write_source(TEXT("unrelated"), source_path));

        auto const saved{session.save()};
        TestRunner->TestFalse(TEXT("Unexpected creation blocks save"), saved.has_value());
        TestRunner->TestTrue(TEXT("Unexpected creation sets conflict"),
                             session.has_external_conflict());
        FString retained;
        FFileHelper::LoadFileToString(retained, *source_path);
        TestRunner->TestEqual(
            TEXT("Unexpected file is preserved"), retained, FString{TEXT("unrelated")});
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

        ml::editor::FS7LevelSourceSession session;
        auto const loaded{session.attach(*first)};
        if (!TestRunner->TestTrue(TEXT("First source loads"), loaded.has_value())) {
            TestRunner->AddError(loaded.error());
            return;
        }
        session.set_buffer(TEXT("unsaved buffer"));
        auto const preserved{session.buffer()};

        auto const switched{session.attach(*second)};
        TestRunner->TestFalse(TEXT("Unsafe automatic switch is refused"), switched.has_value());
        TestRunner->TestFalse(TEXT("Dirty buffer is detached"), session.is_attached());
        TestRunner->TestTrue(TEXT("Detached buffer is conflicted"),
                             session.has_external_conflict());
        TestRunner->TestEqual(TEXT("Dirty buffer is preserved"), session.buffer(), preserved);

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
