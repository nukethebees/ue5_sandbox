#include "SandboxEditor/levels/S7LevelAuthoringMode.h"

#include "SandboxEditor/levels/S7LevelAuthoringDocument.h"
#include "SandboxEditor/levels/S7LevelAuthoringModeToolkit.h"

#include <SpaceGameS7/LevelDefinitionReader.h>
#include <SpaceGameS7/LevelDefinitionWriter.h>
#include <SpaceGameS7/LevelScriptCatalog.h>

#include <CanvasItem.h>
#include <CanvasTypes.h>
#include <DesktopPlatformModule.h>
#include <Editor.h>
#include <Engine/Level.h>
#include <Engine/Selection.h>
#include <Framework/Application/SlateApplication.h>
#include <IDesktopPlatform.h>
#include <Misc/PackageName.h>
#include <SceneManagement.h>
#include <SceneView.h>
#include <ScopedTransaction.h>
#include <Styling/AppStyle.h>
#include <UnrealClient.h>

#define LOCTEXT_NAMESPACE "US7LevelAuthoringMode"

namespace ml::editor::s7_level_authoring_mode_detail {
auto select_source_path(bool const save, FString const& suggested_filename) -> TOptional<FString> {
    auto* const desktop{FDesktopPlatformModule::Get()};
    if (!desktop) {
        return NullOpt;
    }
    auto const parent{FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr)};
    TArray<FString> paths;
    auto const directory{ml::s7::default_level_script_directory()};
    auto const selected{save ? desktop->SaveFileDialog(parent,
                                                       TEXT("Save Explicit S7 Level"),
                                                       directory,
                                                       suggested_filename,
                                                       TEXT("S7 level (*.scm)|*.scm"),
                                                       EFileDialogFlags::None,
                                                       paths)
                             : desktop->OpenFileDialog(parent,
                                                       TEXT("Load S7 Level"),
                                                       directory,
                                                       TEXT(""),
                                                       TEXT("S7 level (*.scm)|*.scm"),
                                                       EFileDialogFlags::None,
                                                       paths)};
    if (!selected || paths.Num() != 1) {
        return NullOpt;
    }
    if (save && FPaths::GetExtension(paths[0]).IsEmpty()) {
        paths[0] += TEXT(".scm");
    }
    return MoveTemp(paths[0]);
}
}

using namespace ml::editor::s7_level_authoring_mode_detail;

FEditorModeID const US7LevelAuthoringMode::mode_id{TEXT("EM_SpaceGameLevelAuthoring")};

US7LevelAuthoringMode::US7LevelAuthoringMode() {
    Info = FEditorModeInfo{mode_id,
                           LOCTEXT("ModeName", "Space Game Level"),
                           FSlateIcon{FAppStyle::GetAppStyleSetName(), TEXT("Icons.Edit")},
                           true,
                           640};
}

void US7LevelAuthoringMode::Enter() {
    Super::Enter();
    refresh_document();
}

void US7LevelAuthoringMode::Exit() {
    preview_.Reset();
    document_.Reset();
    Super::Exit();
}

void US7LevelAuthoringMode::CreateToolkit() {
    Toolkit = MakeShared<FS7LevelAuthoringModeToolkit>();
}

void US7LevelAuthoringMode::Tick(FEditorViewportClient* const viewport_client,
                                 float const delta_time) {
    Super::Tick(viewport_client, delta_time);
    refresh_elapsed_seconds_ += delta_time;
    if (refresh_elapsed_seconds_ < 0.5f) {
        return;
    }
    refresh_elapsed_seconds_ = 0.0f;
    auto* const previous{document_.Get()};
    refresh_document();
    if (previous != document_.Get()) {
        changed_.Broadcast();
        return;
    }
    if (!document_.IsValid()) {
        return;
    }

    auto const disk_refreshed{source_session_.refresh_external_conflict()};
    if (!disk_refreshed || source_session_.has_external_conflict()) {
        auto const stale{
            LOCTEXT("ExternalChange",
                    "The S7 source changed externally. Saving is blocked; an unchanged "
                    "preview can still apply.")};
        if (!status_.EqualTo(stale)) {
            set_status(stale);
            changed_.Broadcast();
        }
    }
    if (preview_.IsSet()) {
        auto* const level{current_level()};
        auto const current{IsValid(level)
                               ? ml::editor::validate_s7_level_authoring_preview(
                                     *level, *document_, source_session_, preview_.GetValue())
                               : std::expected<void, FString>{
                                     std::unexpected{TEXT("The current level is unavailable.")}}};
        if (!current) {
            preview_.Reset();
            set_status(FText::FromString(current.error()));
            changed_.Broadcast();
        }
        return;
    }
    if (!disk_refreshed || source_session_.has_external_conflict()) {
        return;
    }
    if (source_session_.is_dirty()) {
        auto const dirty{LOCTEXT("SourceDirty", "The S7 source buffer has unsaved changes.")};
        if (!status_.EqualTo(dirty)) {
            set_status(dirty);
            changed_.Broadcast();
        }
        return;
    }
    if (!document_->synchronized_scene_hash.IsEmpty()) {
        auto* const level{current_level()};
        auto const definition{IsValid(level)
                                  ? ml::editor::collect_s7_editor_level(*level, *document_)
                                  : std::expected<ml::FLevelDefinition, FString>{std::unexpected{
                                        TEXT("The current level is unavailable.")}}};
        auto const generated{
            definition ? ml::s7::emit_editor_level_source(*definition)
                       : std::expected<FString, FString>{std::unexpected{definition.error()}}};
        if (!generated || document_->synchronized_scene_hash !=
                              ml::editor::FS7LevelSourceSession::source_digest(*generated)) {
            auto const dirty{
                LOCTEXT("SceneDirty", "The scene has unsaved level-authoring changes.")};
            if (!status_.EqualTo(dirty)) {
                set_status(dirty);
                changed_.Broadcast();
            }
        }
    }
}

void US7LevelAuthoringMode::Render(FSceneView const* const view,
                                   FViewport* const viewport,
                                   FPrimitiveDrawInterface* const pdi) {
    Super::Render(view, viewport, pdi);
    if (!pdi || !document_.IsValid()) {
        return;
    }
    auto draw_roles = [pdi](TArray<TObjectPtr<AActor>> const& actors,
                            FLinearColor const color,
                            float const size) {
        for (auto const actor : actors) {
            if (IsValid(actor)) {
                pdi->DrawPoint(actor->GetActorLocation(), color, size, SDPG_Foreground);
            }
        }
    };
    draw_roles(document_->mission.heroes, FLinearColor{0.2f, 0.6f, 1.0f}, 18.0f);
    draw_roles(document_->mission.must_survive, FLinearColor{0.2f, 1.0f, 0.35f}, 22.0f);
    draw_roles(document_->mission.required_kills, FLinearColor{1.0f, 0.15f, 0.1f}, 22.0f);
}

void US7LevelAuthoringMode::DrawHUD(FEditorViewportClient* const viewport_client,
                                    FViewport* const viewport,
                                    FSceneView const* const view,
                                    FCanvas* const canvas) {
    Super::DrawHUD(viewport_client, viewport, view, canvas);
    if (!document_.IsValid() || !viewport || !view || !canvas || !GEngine) {
        return;
    }

    auto const viewport_size{viewport->GetSizeXY()};
    for (auto const& binding : document_->entities) {
        if (!IsValid(binding.actor)) {
            continue;
        }
        auto const projected{view->Project(binding.actor->GetActorLocation())};
        if (projected.W <= 0.0) {
            continue;
        }
        auto color{FLinearColor::White};
        if (document_->mission.required_kills.Contains(binding.actor)) {
            color = FLinearColor{1.0f, 0.15f, 0.1f};
        } else if (document_->mission.must_survive.Contains(binding.actor)) {
            color = FLinearColor{0.2f, 1.0f, 0.35f};
        } else if (document_->mission.heroes.Contains(binding.actor)) {
            color = FLinearColor{0.2f, 0.6f, 1.0f};
        }
        auto label_text{FText::FromName(binding.id)};
        if (binding.spawn_time_seconds > 0.0) {
            label_text = FText::Format(LOCTEXT("DelayedEntityLabel", "{0} [T+{1}s]"),
                                       label_text,
                                       FText::AsNumber(binding.spawn_time_seconds));
            color.A = 0.65f;
        }
        FCanvasTextItem label{
            FVector2D{viewport_size.X * 0.5 + viewport_size.X * 0.5 * projected.X,
                      viewport_size.Y * 0.5 - viewport_size.Y * 0.5 * projected.Y},
            label_text,
            GEngine->GetSmallFont(),
            color};
        label.EnableShadow(FLinearColor::Black);
        canvas->DrawItem(label);
    }
}

auto US7LevelAuthoringMode::document() const -> AS7LevelAuthoringDocument* {
    return document_.Get();
}

auto US7LevelAuthoringMode::source_session() -> ml::editor::FS7LevelSourceSession& {
    return source_session_;
}

auto US7LevelAuthoringMode::status() const -> FText const& {
    return status_;
}

auto US7LevelAuthoringMode::on_changed() -> FOnS7LevelAuthoringChanged& {
    return changed_;
}

auto US7LevelAuthoringMode::current_level() const -> ULevel* {
    auto* const world{GEditor ? GEditor->GetEditorWorldContext().World() : nullptr};
    return IsValid(world) ? world->GetCurrentLevel() : nullptr;
}

void US7LevelAuthoringMode::refresh_document() {
    auto* const level{current_level()};
    if (!IsValid(level)) {
        document_.Reset();
        set_status(LOCTEXT("NoLevel", "The current editor level is unavailable."));
        return;
    }
    auto const found{ml::editor::find_level_authoring_document(*level)};
    if (!found) {
        document_.Reset();
        set_status(FText::FromString(found.error()));
        return;
    }
    document_ = *found;
    if (!document_.IsValid()) {
        create_document();
        return;
    }
    auto const attached{source_session_.attach(*document_)};
    if (!attached) {
        set_status(FText::FromString(attached.error()));
    }
}

void US7LevelAuthoringMode::set_status(FText text) {
    status_ = MoveTemp(text);
}

void US7LevelAuthoringMode::create_document() {
    auto* const level{current_level()};
    if (!IsValid(level)) {
        return;
    }
    auto const had_document{document_.IsValid()};
    auto const created{ml::editor::create_level_authoring_document(*level)};
    if (!created) {
        set_status(FText::FromString(created.error()));
    } else {
        document_ = *created;
        auto const attached{source_session_.attach(*document_)};
        auto const adopted{ml::editor::adopt_unbound_level_entities(*level, *document_)};
        if (!attached) {
            set_status(FText::FromString(attached.error()));
        } else if (!adopted) {
            set_status(FText::FromString(adopted.error()));
        } else if (had_document) {
            set_status(FText::Format(
                LOCTEXT("ExistingDocumentAdopted", "Adopted {0} existing level entities."),
                *adopted));
        } else {
            set_status(FText::Format(
                LOCTEXT(
                    "DocumentCreatedAndAdopted",
                    "Created the S7 authoring document and adopted {0} existing level entities."),
                *adopted));
        }
    }
    changed_.Broadcast();
}

void US7LevelAuthoringMode::adopt_entities() {
    if (!document_.IsValid()) {
        create_document();
    }
    auto* const level{current_level()};
    if (!IsValid(level) || !document_.IsValid()) {
        return;
    }
    auto const adopted{ml::editor::adopt_unbound_level_entities(*level, *document_)};
    set_status(adopted ? FText::Format(LOCTEXT("Adopted", "Adopted {0} entities."), *adopted)
                       : FText::FromString(adopted.error()));
    changed_.Broadcast();
}

void US7LevelAuthoringMode::assign_selected_heroes() {
    assign_selected_objective(1);
}

void US7LevelAuthoringMode::assign_selected_must_survive() {
    assign_selected_objective(2);
}

void US7LevelAuthoringMode::assign_selected_required_kills() {
    assign_selected_objective(3);
}

void US7LevelAuthoringMode::clear_selected_objectives() {
    assign_selected_objective(0);
}

void US7LevelAuthoringMode::assign_selected_objective(int32 const role) {
    if (!document_.IsValid() || !GEditor) {
        return;
    }
    TArray<AActor*> selected;
    for (FSelectionIterator it{*GEditor->GetSelectedActors()}; it; ++it) {
        auto* const actor{Cast<AActor>(*it)};
        if (IsValid(actor) &&
            document_->entities.ContainsByPredicate(
                [actor](FS7LevelEntityBinding const& binding) { return binding.actor == actor; })) {
            selected.Add(actor);
        }
    }
    if (selected.IsEmpty()) {
        set_status(LOCTEXT("NoSelectedEntities", "Select one or more adopted entities first."));
        changed_.Broadcast();
        return;
    }

    FScopedTransaction transaction{
        LOCTEXT("AssignObjectivesTransaction", "Assign Level Objectives")};
    document_->Modify();
    for (auto* const actor : selected) {
        if (role == 1) {
            document_->mission.required_kills.Remove(actor);
            document_->mission.heroes.AddUnique(actor);
        } else if (role == 2) {
            document_->mission.required_kills.Remove(actor);
            document_->mission.must_survive.AddUnique(actor);
        } else if (role == 3) {
            document_->mission.heroes.Remove(actor);
            document_->mission.must_survive.Remove(actor);
            document_->mission.required_kills.AddUnique(actor);
        } else {
            document_->mission.heroes.Remove(actor);
            document_->mission.must_survive.Remove(actor);
            document_->mission.required_kills.Remove(actor);
        }
    }
    set_status(FText::Format(LOCTEXT("AssignedObjectives", "Updated objectives for {0} entities."),
                             selected.Num()));
    changed_.Broadcast();
}

void US7LevelAuthoringMode::load_s7() {
    auto const path{select_source_path(false, TEXT(""))};
    if (!path.IsSet()) {
        return;
    }
    if (!document_.IsValid()) {
        create_document();
    }
    if (!document_.IsValid()) {
        return;
    }
    auto const loaded{source_session_.load(*document_, path.GetValue())};
    if (!loaded) {
        set_status(FText::FromString(loaded.error()));
        changed_.Broadcast();
        return;
    }
    document_->Modify();
    document_->source_path = source_session_.path();
    preview_apply();
}

void US7LevelAuthoringMode::preview_apply() {
    auto* const level{current_level()};
    if (!IsValid(level) || !document_.IsValid() || source_session_.document() != document_.Get()) {
        set_status(LOCTEXT("NoSource", "Attach an S7 source buffer to this level first."));
        changed_.Broadcast();
        return;
    }
    auto const read{source_session_.read()};
    if (!read) {
        set_status(FText::FromString(!read.script_error.IsEmpty()
                                         ? read.script_error
                                         : TEXT("The S7 level did not decode or validate.")));
        preview_.Reset();
        changed_.Broadcast();
        return;
    }
    auto plan{ml::editor::make_s7_level_sync_plan(*level, *document_, read.definition.GetValue())};
    if (!plan) {
        set_status(FText::FromString(plan.error()));
        preview_.Reset();
        changed_.Broadcast();
        return;
    }
    auto preview{ml::editor::make_s7_level_authoring_preview(
        *level, *document_, source_session_, MoveTemp(*plan))};
    if (!preview) {
        set_status(FText::FromString(preview.error()));
        preview_.Reset();
        changed_.Broadcast();
        return;
    }
    preview_ = MoveTemp(*preview);
    if (!preview_->plan.has_changes()) {
        set_status(LOCTEXT("PreviewMatches", "Preview: the source and scene already match."));
    } else {
        TArray<FString> document_changes;
        if (preview_->plan.metadata_changed) {
            document_changes.Add(TEXT("metadata"));
        }
        if (preview_->plan.viewpoint_changed) {
            document_changes.Add(TEXT("viewpoint"));
        }
        if (preview_->plan.mission_changed) {
            document_changes.Add(TEXT("mission"));
        }

        auto const entity_summary{FText::Format(
            LOCTEXT("PreviewEntityChanges", "+{0}, update {1}, replace {2}, remove {3}"),
            preview_->plan.count(ml::editor::ES7LevelSyncAction::Add),
            preview_->plan.count(ml::editor::ES7LevelSyncAction::Update),
            preview_->plan.count(ml::editor::ES7LevelSyncAction::Replace),
            preview_->plan.count(ml::editor::ES7LevelSyncAction::Remove))};
        set_status(
            document_changes.IsEmpty()
                ? FText::Format(LOCTEXT("PreviewEntitiesOnly", "Preview: {0}. Apply is undoable."),
                                entity_summary)
                : FText::Format(LOCTEXT("PreviewWithDocumentChanges",
                                        "Preview: {0}; document: {1}. Apply is undoable."),
                                entity_summary,
                                FText::FromString(FString::Join(document_changes, TEXT(", ")))));
    }
    changed_.Broadcast();
}

void US7LevelAuthoringMode::apply_preview() {
    auto* const level{current_level()};
    if (!IsValid(level) || !document_.IsValid() || !preview_.IsSet()) {
        set_status(LOCTEXT("NoPreview", "Preview the source before applying it."));
        changed_.Broadcast();
        return;
    }
    auto const applied{ml::editor::apply_s7_level_authoring_preview(
        *level, *document_, source_session_, preview_.GetValue())};
    if (!applied) {
        set_status(FText::FromString(applied.error()));
        if (applied.error().StartsWith(TEXT("Preview is stale"))) {
            preview_.Reset();
        }
    } else {
        document_->synchronized_source_hash =
            ml::editor::FS7LevelSourceSession::source_digest(source_session_.buffer());
        auto const definition{ml::editor::collect_s7_editor_level(*level, *document_)};
        auto const generated{
            definition ? ml::s7::emit_editor_level_source(*definition)
                       : std::expected<FString, FString>{std::unexpected{definition.error()}}};
        if (generated) {
            document_->synchronized_scene_hash =
                ml::editor::FS7LevelSourceSession::source_digest(*generated);
        }
        preview_.Reset();
        set_status(LOCTEXT("Applied", "The scene now matches the S7 source."));
    }
    changed_.Broadcast();
}

void US7LevelAuthoringMode::save() {
    if (!source_session_.is_attached() && !source_session_.is_dirty()) {
        set_status(LOCTEXT("NoSourceBuffer", "There is no source buffer to save."));
        changed_.Broadcast();
        return;
    }
    if (source_session_.path().IsEmpty()) {
        save_as();
        return;
    }
    auto const saved{source_session_.save()};
    if (!saved) {
        set_status(FText::FromString(saved.error()));
    } else {
        update_document_source_path();
        set_status(FText::FromString(FString::Printf(TEXT("Saved the source buffer exactly to %s."),
                                                     *source_session_.path())));
    }
    changed_.Broadcast();
}

void US7LevelAuthoringMode::save_as() {
    if (!source_session_.is_attached() && !source_session_.is_dirty()) {
        set_status(LOCTEXT("NoSourceBuffer", "There is no source buffer to save."));
        changed_.Broadcast();
        return;
    }
    auto* const level{current_level()};
    auto const map_name{IsValid(level)
                            ? FPackageName::GetShortName(level->GetOutermost()->GetName())
                            : FString{TEXT("level")}};
    auto const selected{select_source_path(true, map_name + TEXT(".scm"))};
    if (!selected.IsSet()) {
        return;
    }
    auto const saved{source_session_.save_as(
        selected.GetValue(), ml::editor::ES7SourceOverwritePolicy::ReplaceExisting)};
    if (!saved) {
        set_status(FText::FromString(saved.error()));
    } else {
        update_document_source_path();
        set_status(FText::FromString(FString::Printf(TEXT("Saved the source buffer exactly to %s."),
                                                     *source_session_.path())));
    }
    changed_.Broadcast();
}

void US7LevelAuthoringMode::save_canonical_from_scene() {
    auto* const level{current_level()};
    if (!IsValid(level) || !document_.IsValid() || source_session_.document() != document_.Get()) {
        return;
    }
    if (source_session_.has_unapplied_buffer(document_->synchronized_source_hash)) {
        set_status(LOCTEXT("UnappliedSource",
                           "Apply or discard the source buffer before saving canonical scene "
                           "state."));
        changed_.Broadcast();
        return;
    }
    auto const definition{ml::editor::collect_s7_editor_level(*level, *document_)};
    if (!definition) {
        set_status(FText::FromString(definition.error()));
        changed_.Broadcast();
        return;
    }
    auto const source{ml::s7::emit_editor_level_source(*definition)};
    if (!source) {
        set_status(FText::FromString(source.error()));
        changed_.Broadcast();
        return;
    }

    std::expected<void, FString> saved;
    if (source_session_.path().IsEmpty()) {
        auto const map_name{FPackageName::GetShortName(level->GetOutermost()->GetName())};
        auto const selected{select_source_path(true, map_name + TEXT(".scm"))};
        if (!selected.IsSet()) {
            return;
        }
        saved = source_session_.save_replacement_as(
            *source,
            selected.GetValue(),
            document_->synchronized_source_hash,
            ml::editor::ES7SourceOverwritePolicy::ReplaceExisting);
    } else {
        saved = source_session_.save_replacement(*source, document_->synchronized_source_hash);
    }
    if (!saved) {
        set_status(FText::FromString(saved.error()));
        changed_.Broadcast();
        return;
    }
    document_->Modify();
    update_document_source_path();
    auto const digest{ml::editor::FS7LevelSourceSession::source_digest(*source)};
    document_->synchronized_source_hash = digest;
    document_->synchronized_scene_hash = digest;
    set_status(FText::FromString(
        FString::Printf(TEXT("Saved canonical scene state with %d entities to %s."),
                        definition->entities.num(),
                        *source_session_.path())));
    changed_.Broadcast();
}

void US7LevelAuthoringMode::update_document_source_path() {
    if (!document_.IsValid() || source_session_.document() != document_.Get() ||
        document_->source_path == source_session_.path()) {
        return;
    }
    document_->Modify();
    document_->source_path = source_session_.path();
}

#undef LOCTEXT_NAMESPACE
