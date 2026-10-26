#include "SandboxEditor/levels/S7LevelAuthoringMode.h"

#include "SandboxEditor/levels/S7LevelAuthoringActors.h"
#include "SandboxEditor/levels/S7LevelAuthoringDocument.h"
#include "SandboxEditor/levels/S7LevelAuthoringModeToolkit.h"
#include "SandboxEditor/levels/S7LevelObserverCamera.h"
#include "SandboxEditor/levels/S7LevelPlayableSetup.h"
#include <ioj/levels/authoring/level_definition_writer.h>
#include <SpaceGame/s7/GameSettingsReader.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>
#include <SpaceGame/simulation/TestBatchOrchestrator.h>
#include <SpaceGameS7/level_script_catalog.h>

#include <SandboxCoreEngine/strings.h>

#include <CanvasItem.h>
#include <CanvasTypes.h>
#include <DesktopPlatformModule.h>
#include <Editor.h>
#include <Engine/Level.h>
#include <Engine/Selection.h>
#include <Framework/Application/SlateApplication.h>
#include <HAL/FileManager.h>
#include <IDesktopPlatform.h>
#include <Misc/MessageDialog.h>
#include <Misc/PackageName.h>
#include <SceneManagement.h>
#include <SceneView.h>
#include <ScopedTransaction.h>
#include <Styling/AppStyle.h>
#include <Subsystems/AssetEditorSubsystem.h>
#include <UnrealClient.h>

#define LOCTEXT_NAMESPACE "US7LevelAuthoringMode"

namespace ioj::editor::s7_level_authoring_mode_detail {
auto select_source_path(bool const save, FString const& suggested_filename) -> TOptional<FString> {
    auto* const desktop{FDesktopPlatformModule::Get()};
    if (!desktop) {
        return NullOpt;
    }
    auto const parent{FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr)};
    TArray<FString> paths;
    auto const directory{levels::authoring::default_level_script_directory()};
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

auto confirm_discard_source_buffer() -> bool {
    return FMessageDialog::Open(
               EAppMsgType::YesNo,
               LOCTEXT("DiscardSourceBuffer",
                       "Discard the unsaved S7 source buffer and reload the file from disk?")) ==
           EAppReturnType::Yes;
}

auto confirm_replace_source(FStringView const path) -> bool {
    return FMessageDialog::Open(
               EAppMsgType::YesNo,
               FText::Format(
                   LOCTEXT("ReplaceSource", "The S7 source file '{0}' already exists. Replace it?"),
                   FText::FromString(FString{path}))) == EAppReturnType::Yes;
}

struct SourceSaveTarget {
    FString path;
    ES7SourceOverwritePolicy overwrite;
};

auto select_save_target(ULevel const* const level) -> TOptional<SourceSaveTarget> {
    auto const map_name{IsValid(level)
                            ? FPackageName::GetShortName(level->GetOutermost()->GetName())
                            : FString{TEXT("level")}};
    auto selected{select_source_path(true, map_name + TEXT(".scm"))};
    if (!selected.IsSet()) {
        return NullOpt;
    }
    auto overwrite{ES7SourceOverwritePolicy::RefuseExisting};
    if (IFileManager::Get().FileExists(**selected)) {
        if (!confirm_replace_source(*selected)) {
            return NullOpt;
        }
        overwrite = ES7SourceOverwritePolicy::ReplaceExisting;
    }
    return SourceSaveTarget{MoveTemp(*selected), overwrite};
}

auto scene_has_unsaved_changes(AS7LevelAuthoringDocument const& document, FString const& source)
    -> bool {
    return source.IsEmpty() || (document.synchronized_scene_source.IsEmpty()
                                    ? !document.entities.IsEmpty()
                                    : source != document.synchronized_scene_source);
}

auto selected_bound_entity(AS7LevelAuthoringDocument& document)
    -> std::expected<FS7LevelEntityBinding*, FString> {
    if (!GEditor) {
        return std::unexpected{TEXT("The editor selection is unavailable.")};
    }

    TArray<AActor*> selected;
    for (FSelectionIterator it{*GEditor->GetSelectedActors()}; it; ++it) {
        auto* const actor{Cast<AActor>(*it)};
        if (IsValid(actor)) {
            selected.AddUnique(actor);
        }
    }
    if (selected.Num() != 1) {
        return std::unexpected{TEXT("Select exactly one adopted entity first.")};
    }

    auto* const binding{document.entities.FindByPredicate(
        [actor = selected[0]](FS7LevelEntityBinding const& candidate) {
            return candidate.actor == actor;
        })};
    if (!binding) {
        return std::unexpected{TEXT("The selected actor is not an adopted S7 entity.")};
    }
    return binding;
}
}

namespace levels = ioj::levels;

using namespace ioj::editor;
using namespace ioj::editor::s7_level_authoring_mode_detail;
using levels::authoring::PreviewState;

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
    clear_preview();
    source_session_.state().scene_dirty = false;
    document_.Reset();
    source_session_.detach();
    observed_scene_.Reset();
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
        notify_changed();
        return;
    }
    if (!document_.IsValid()) {
        return;
    }

    auto* const level{current_level()};
    if (!IsValid(level)) {
        return;
    }
    auto& state{source_session_.state()};
    auto const was_scene_dirty{state.scene_dirty};
    auto const was_disk_conflict{state.disk_conflict};
    auto const disk_refreshed{source_session_.refresh_external_conflict()};
    auto const scene{collect_s7_scene(*level, *document_)};
    if (!scene || !observed_scene_.IsSet() || *scene != *observed_scene_ ||
        observed_actor_count_ != level->Actors.Num()) {
        auto const source{collect_s7_scene_source(*level, *document_, scene)};
        observed_scene_source_ = source.value_or(FString{});
        observed_scene_ = scene ? TOptional{*scene} : NullOpt;
        observed_actor_count_ = level->Actors.Num();
    }
    state.scene_dirty = scene_has_unsaved_changes(*document_, observed_scene_source_);
    if (preview_.IsSet() && state.preview == PreviewState::Valid) {
        auto const current{validate_s7_level_authoring_preview(
            *level, *document_, source_session_, *preview_, scene)};
        if (!current) {
            clear_preview(PreviewState::Stale);
            set_status(FText::FromString(current.error().message));
            notify_changed(scene ? &scene->definition.settings : nullptr);
            return;
        }
    }
    if (!disk_refreshed || state.disk_conflict) {
        set_status(LOCTEXT("ExternalChange",
                           "The S7 source changed externally. Reload it before saving; "
                           "replay will use the saved file."));
    } else if (state.preview == PreviewState::Valid) {
        if (was_disk_conflict) {
            set_status(LOCTEXT("ConflictCleared",
                               "The disk conflict cleared. The preview remains valid."));
        }
    } else if (!source_session_.is_attached()) {
        set_status(LOCTEXT(
            "DetachedSource",
            "Source is detached. Save Buffer As to recover it, or use Discard and Attach."));
    } else if (source_session_.is_dirty()) {
        set_status(LOCTEXT("SourceDirty", "The S7 source buffer has unsaved changes."));
    } else if (state.scene_dirty) {
        set_status(LOCTEXT("SceneDirty", "The scene has unsaved level-authoring changes."));
    } else if (was_scene_dirty || was_disk_conflict) {
        set_status(LOCTEXT("StateClean", "There are no scene changes or disk conflicts."));
    }
    notify_changed(scene ? &scene->definition.settings : nullptr);
}

void US7LevelAuthoringMode::Render(FSceneView const* const view,
                                   FViewport* const viewport,
                                   FPrimitiveDrawInterface* const pdi) {
    Super::Render(view, viewport, pdi);
    if (!pdi || !document_.IsValid()) {
        return;
    }
    auto* const level{current_level()};
    if (!IsValid(level)) {
        return;
    }
    auto const observer_camera{make_s7_observer_camera_transform(*level, *document_)};
    if (!observer_camera || !observer_camera->IsSet()) {
        return;
    }

    auto const& camera{observer_camera->GetValue()};
    auto const camera_color{FLinearColor{1.0f, 0.8f, 0.2f}};
    auto const focus_color{FLinearColor{0.75f, 0.35f, 1.0f}};
    for (auto const target : document_->camera.targets) {
        pdi->DrawLine(target->GetActorLocation(), camera.focus, focus_color, SDPG_Foreground, 2.0f);
    }
    pdi->DrawPoint(camera.focus, focus_color, 28.0f, SDPG_Foreground);
    pdi->DrawPoint(camera.transform.GetLocation(), camera_color, 28.0f, SDPG_Foreground);
    pdi->DrawLine(
        camera.transform.GetLocation(), camera.focus, camera_color, SDPG_Foreground, 2.5f);

    auto const direction{(camera.focus - camera.transform.GetLocation()).GetSafeNormal()};
    auto const indicator_length{FMath::Clamp(document_->camera.distance * 0.05, 1000.0, 10000.0)};
    pdi->DrawLine(camera.transform.GetLocation(),
                  camera.transform.GetLocation() + direction * indicator_length,
                  camera_color,
                  SDPG_Foreground,
                  4.0f);
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

    auto* const level{current_level()};
    if (!IsValid(level)) {
        return;
    }
    auto const observer_camera{make_s7_observer_camera_transform(*level, *document_)};
    if (!observer_camera || !observer_camera->IsSet()) {
        return;
    }
    auto const projected{view->Project(observer_camera->GetValue().transform.GetLocation())};
    if (projected.W <= 0.0) {
        return;
    }
    FCanvasTextItem camera_label{
        FVector2D{viewport_size.X * 0.5 + viewport_size.X * 0.5 * projected.X,
                  viewport_size.Y * 0.5 - viewport_size.Y * 0.5 * projected.Y},
        LOCTEXT("ObserverCameraLabel", "Observer Camera"),
        GEngine->GetSmallFont(),
        FLinearColor{1.0f, 0.8f, 0.2f}};
    camera_label.EnableShadow(FLinearColor::Black);
    canvas->DrawItem(camera_label);
}

auto US7LevelAuthoringMode::document() const -> AS7LevelAuthoringDocument* {
    return document_.Get();
}

auto US7LevelAuthoringMode::source_session() -> FS7LevelSourceSession& {
    return source_session_;
}

auto US7LevelAuthoringMode::source_session() const -> FS7LevelSourceSession const& {
    return source_session_;
}

auto US7LevelAuthoringMode::script_editor_state() const -> FS7LevelScriptEditorState {
    auto const has_document{document_.IsValid()};
    auto const attached{source_session_.is_attached() &&
                        source_session_.document() == document_.Get()};
    return {
        .path = source_session_.path(),
        .source_dirty = source_session_.is_dirty(),
        .unapplied_buffer =
            has_document && source_session_.has_unapplied_buffer(document_->synchronized_source),
        .scene_dirty = source_session_.state().scene_dirty,
        .disk_conflict = source_session_.has_external_conflict(),
        .preview_valid = preview_.IsSet() && source_session_.state().preview == PreviewState::Valid,
        .preview_stale = source_session_.state().preview == PreviewState::Stale,
        .detached = !attached};
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
    auto const found{IsValid(level)
                         ? find_level_authoring_document(*level)
                         : std::expected<AS7LevelAuthoringDocument*, FString>{
                               std::unexpected{TEXT("The current editor level is unavailable.")}}};
    auto* const next{found ? *found : nullptr};
    if (document_.Get() != next || !IsValid(next)) {
        clear_preview();
        observed_scene_.Reset();
        source_session_.detach();
        document_ = next;
    }
    if (!found) {
        set_status(FText::FromString(found.error()));
        return;
    }
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

void US7LevelAuthoringMode::clear_preview(PreviewState const state) {
    preview_.Reset();
    source_session_.state().preview = state;
}

void US7LevelAuthoringMode::notify_changed(ioj::settings::Settings const* const settings) {
    if (source_session_.state().preview != PreviewState::Valid) {
        preview_.Reset();
    }
    if (Toolkit.IsValid()) {
        StaticCastSharedPtr<FS7LevelAuthoringModeToolkit>(Toolkit)->refresh(settings);
    }
    changed_.Broadcast();
}

void US7LevelAuthoringMode::create_document() {
    auto* const level{current_level()};
    if (!IsValid(level)) {
        return;
    }
    auto const had_document{document_.IsValid()};
    auto const created{create_level_authoring_document(*level)};
    if (!created) {
        set_status(FText::FromString(created.error()));
    } else {
        document_ = *created;
        auto const attached{source_session_.attach(*document_)};
        auto const adopted{adopt_unbound_level_entities(*level, *document_)};
        if (adopted > 0) {
            source_session_.state().invalidate_preview();
        }
        if (!attached) {
            set_status(FText::FromString(attached.error()));
        } else if (had_document) {
            set_status(FText::Format(
                LOCTEXT("ExistingDocumentAdopted", "Adopted {0} existing level entities."),
                adopted));
        } else {
            set_status(FText::Format(
                LOCTEXT(
                    "DocumentCreatedAndAdopted",
                    "Created the S7 authoring document and adopted {0} existing level entities."),
                adopted));
        }
    }
    notify_changed();
}

void US7LevelAuthoringMode::adopt_entities() {
    if (!document_.IsValid()) {
        create_document();
    }
    auto* const level{current_level()};
    if (!IsValid(level) || !document_.IsValid()) {
        return;
    }
    auto const adopted{adopt_unbound_level_entities(*level, *document_)};
    set_status(FText::Format(LOCTEXT("Adopted", "Adopted {0} entities."), adopted));
    if (adopted > 0) {
        source_session_.state().invalidate_preview();
    }
    notify_changed();
}

void US7LevelAuthoringMode::repair_and_adopt_entities() {
    if (!document_.IsValid()) {
        create_document();
    }
    auto* const level{current_level()};
    if (!IsValid(level) || !document_.IsValid()) {
        return;
    }

    auto const repaired{repair_s7_level_bindings(*level, *document_)};
    if (!repaired.has_changes()) {
        set_status(
            LOCTEXT("BindingsAlreadyRepaired", "Bindings and references already need no repair."));
    } else {
        source_session_.state().invalidate_preview();
        set_status(FText::Format(
            LOCTEXT("BindingsRepaired",
                    "Repaired bindings: removed {0}; references: removed {1}; adopted {2}."),
            repaired.removed_bindings,
            repaired.removed_references,
            repaired.adopted_entities));
    }
    notify_changed();
}

void US7LevelAuthoringMode::rename_selected_entity(FString const id) {
    if (!document_.IsValid()) {
        set_status(LOCTEXT("NoDocumentForRename", "Create an S7 authoring document first."));
        notify_changed();
        return;
    }

    auto const binding{selected_bound_entity(*document_)};
    if (!binding) {
        set_status(FText::FromString(binding.error()));
        notify_changed();
        return;
    }

    FName const new_id{id};
    if (!is_canonical_s7_level_entity_id(FStringView{id})) {
        set_status(LOCTEXT("InvalidEntityId",
                           "Entity IDs must be lowercase S7 symbols beginning with a letter."));
        notify_changed();
        return;
    }
    if ((*binding)->id == new_id) {
        set_status(LOCTEXT("UnchangedEntityId", "The selected entity already has that ID."));
        notify_changed();
        return;
    }
    if (document_->entities.ContainsByPredicate(
            [binding = *binding, new_id](FS7LevelEntityBinding const& candidate) {
                return &candidate != binding && candidate.id == new_id;
            })) {
        set_status(LOCTEXT("DuplicateEntityId", "That S7 entity ID is already in use."));
        notify_changed();
        return;
    }

    FScopedTransaction transaction{LOCTEXT("RenameEntityId", "Rename S7 Entity ID")};
    document_->Modify();
    (*binding)->id = new_id;
    source_session_.state().invalidate_preview();
    set_status(FText::Format(LOCTEXT("RenamedEntityId", "Renamed selected entity to '{0}'."),
                             FText::FromName(new_id)));
    notify_changed();
}

void US7LevelAuthoringMode::open_level_config() {
    auto* const config{document_.IsValid() ? document_->level_config.Get() : nullptr};
    auto* const assets{GEditor ? GEditor->GetEditorSubsystem<UAssetEditorSubsystem>() : nullptr};
    if (!IsValid(config) || !IsValid(assets) || !assets->OpenEditorForAsset(config)) {
        set_status(
            LOCTEXT("OpenLevelConfigFailed",
                    "Could not open Edit Resources. Check the document's shared resource asset."));
    } else {
        set_status(LOCTEXT("OpenLevelConfig",
                           "Edit shared resource assets here. Configure the collision grid in "
                           "the S7 source. Resources may be shared by other levels."));
    }
    notify_changed();
}

void US7LevelAuthoringMode::validate_playable_level() {
    auto* const level{current_level()};
    if (!IsValid(level) || !document_.IsValid()) {
        set_status(LOCTEXT("NoLevelForValidation", "Open an S7 level before validating it."));
    } else {
        auto const result{validate_playable_s7_level(*level, *document_)};
        set_status(FText::FromString(result ? *result : result.error()));
    }
    notify_changed();
}

void US7LevelAuthoringMode::set_up_playable_level() {
    auto* const level{current_level()};
    if (!IsValid(level) || !document_.IsValid()) {
        set_status(LOCTEXT("NoLevelForPlayableSetup", "Open an S7 level before setting up play."));
        notify_changed();
        return;
    }
    auto const setup{set_up_playable_s7_level(*level, *document_)};
    set_status(FText::FromString(setup ? *setup : setup.error()));
    notify_changed();
}

auto US7LevelAuthoringMode::load_s7() -> bool {
    auto const path{select_source_path(false, TEXT(""))};
    if (!path.IsSet()) {
        return false;
    }
    if (!document_.IsValid()) {
        create_document();
    }
    if (!document_.IsValid()) {
        return false;
    }
    auto const loaded{source_session_.load(*document_, path.GetValue())};
    if (!loaded) {
        set_status(FText::FromString(loaded.error()));
        notify_changed();
        return false;
    }
    document_->Modify();
    document_->source_path = source_session_.path();
    preview_apply();
    return preview_.IsSet() && source_session_.state().preview == PreviewState::Valid;
}

void US7LevelAuthoringMode::reload_s7() {
    if (!document_.IsValid()) {
        return;
    }
    if ((source_session_.is_dirty() || source_session_.state().recovery_required) &&
        !confirm_discard_source_buffer()) {
        return;
    }
    auto const reloaded{source_session_.document() == document_.Get()
                            ? source_session_.reload()
                            : source_session_.discard_and_attach(*document_)};
    if (!reloaded) {
        set_status(FText::FromString(reloaded.error()));
    } else {
        clear_preview();
        set_status(LOCTEXT("SourceReloaded", "Attached the document source from disk."));
    }
    notify_changed();
}

void US7LevelAuthoringMode::set_source_buffer(FString source) {
    auto const revision{source_session_.revision()};
    source_session_.set_buffer(MoveTemp(source));
    if (source_session_.revision() != revision) {
        set_status(LOCTEXT("BufferUpdated", "Source buffer updated. Preview before applying."));
    }
    notify_changed();
}

void US7LevelAuthoringMode::preview_apply() {
    clear_preview();
    auto* const level{current_level()};
    if (!IsValid(level) || !document_.IsValid() || source_session_.document() != document_.Get()) {
        set_status(LOCTEXT("NoSource", "Attach an S7 source buffer to this level first."));
        notify_changed();
        return;
    }
    if (!prepare_source_replay()) {
        notify_changed();
        return;
    }
    if (!IsValid(document_->level_config)) {
        set_status(LOCTEXT("MissingReplayResources", "The level resources are unavailable."));
        notify_changed();
        return;
    }
    auto base{read_game_settings_source(s7_settings_path(*level), *document_->level_config)};
    if (!base) {
        set_status(FText::FromString(base.error()));
        notify_changed();
        return;
    }
    auto read{source_session_.read_replay(std::move(base->settings))};
    if (!read) {
        set_status(FText::FromString(ioj::to_fstring(levels::format_diagnostics(read.error()))));
        notify_changed();
        return;
    }
    auto plan{make_s7_level_sync_plan(*level,
                                      *document_,
                                      MoveTemp(read->definition),
                                      FS7LevelReplaySources{source_session_.buffer(),
                                                            MoveTemp(base->source),
                                                            MoveTemp(read->evaluated_source),
                                                            MoveTemp(base->evaluated_source)})};
    if (!plan) {
        set_status(FText::FromString(plan.error()));
        notify_changed();
        return;
    }
    auto preview{
        make_s7_level_authoring_preview(*level, *document_, source_session_, MoveTemp(*plan))};
    if (!preview) {
        set_status(FText::FromString(preview.error()));
        notify_changed();
        return;
    }
    preview_ = MoveTemp(*preview);
    source_session_.state().preview = PreviewState::Valid;
    if (!preview_->plan.has_changes()) {
        set_status(LOCTEXT("PreviewMatches", "Preview: the source and scene already match."));
    } else {
        TArray<FString> document_changes;
        if (preview_->plan.metadata_changed) {
            document_changes.Add(TEXT("metadata"));
        }
        if (preview_->plan.settings_changed) {
            document_changes.Add(TEXT("settings"));
        }
        if (preview_->plan.viewpoint_changed) {
            document_changes.Add(TEXT("viewpoint"));
        }
        if (preview_->plan.mission_changed) {
            document_changes.Add(TEXT("mission"));
        }

        auto const entity_summary{FText::Format(
            LOCTEXT("PreviewEntityChanges", "+{0}, update {1}, replace {2}, remove {3}"),
            preview_->plan.count(ES7LevelSyncAction::Add),
            preview_->plan.count(ES7LevelSyncAction::Update),
            preview_->plan.count(ES7LevelSyncAction::Replace),
            preview_->plan.count(ES7LevelSyncAction::Remove))};
        set_status(
            document_changes.IsEmpty()
                ? FText::Format(LOCTEXT("PreviewEntitiesOnly",
                                        "Preview: {0}. Click Apply Preview to update the scene; "
                                        "this is undoable."),
                                entity_summary)
                : FText::Format(LOCTEXT("PreviewWithDocumentChanges",
                                        "Preview: {0}; document: {1}. Click Apply Preview to "
                                        "update the scene; this is undoable."),
                                entity_summary,
                                FText::FromString(FString::Join(document_changes, TEXT(", ")))));
    }
    notify_changed();
}

void US7LevelAuthoringMode::apply_preview() {
    static_cast<void>(apply_current_source(true));
}

auto US7LevelAuthoringMode::prepare_for_play() -> bool {
    if (!document_.IsValid()) {
        return true;
    }
    return apply_current_source(false);
}

auto US7LevelAuthoringMode::prepare_source_replay() -> bool {
    auto* const level{current_level()};
    if (!IsValid(level) || !document_.IsValid()) {
        return false;
    }
    auto const scene_source{collect_s7_scene_source(*level, *document_)};
    auto const scene_changed{
        scene_has_unsaved_changes(*document_, scene_source.value_or(FString{}))};
    source_session_.state().scene_dirty = scene_changed;
    if (source_session_.is_dirty() || scene_changed) {
        auto const choice{FMessageDialog::Open(
            EAppMsgType::YesNoCancel,
            LOCTEXT("SaveBeforeReplay",
                    "There are unsaved level changes. Save and continue?\n\n"
                    "Yes: save and continue.\nNo: discard and continue.\nCancel: keep editing."))};
        if (choice == EAppReturnType::Cancel) {
            return false;
        }
        if (choice == EAppReturnType::Yes) {
            if (scene_changed &&
                source_session_.has_unapplied_buffer(document_->synchronized_source)) {
                set_status(
                    LOCTEXT("ConflictingUnsavedChanges",
                            "Both the scene and source have changes. Save the source separately or "
                            "resolve the scene changes before replaying."));
                return false;
            }
            if (scene_changed) {
                save_canonical_from_scene();
                if (source_session_.state().scene_dirty) {
                    return false;
                }
            } else {
                save();
            }
            if (source_session_.is_dirty() || source_session_.path().IsEmpty()) {
                return false;
            }
        }
    }
    if (source_session_.path().IsEmpty()) {
        set_status(LOCTEXT("SaveBeforeReplayPath", "Save the level source before replaying it."));
        return false;
    }
    auto const loaded{source_session_.reload()};
    if (!loaded) {
        set_status(FText::FromString(loaded.error()));
        return false;
    }
    notify_changed();
    return true;
}

auto US7LevelAuthoringMode::apply_current_source(bool const require_review) -> bool {
    auto previous{preview_};
    if (require_review && previous.IsSet()) {
        auto* const level{current_level()};
        if (!IsValid(level) || !document_.IsValid()) {
            return false;
        }
        auto const current{
            validate_s7_level_authoring_preview(*level, *document_, source_session_, *previous)};
        if (!current) {
            clear_preview(PreviewState::Stale);
            set_status(FText::FromString(current.error().message));
            notify_changed();
            return false;
        }
    }
    preview_apply();
    auto* const level{current_level()};
    if (!IsValid(level) || !document_.IsValid() || !preview_.IsSet()) {
        return false;
    }
    auto const changed_inputs{
        !previous.IsSet() || !previous->plan.sources.IsSet() ||
        previous->plan.sources->evaluated_source != preview_->plan.sources->evaluated_source ||
        previous->plan.sources->evaluated_base_settings_source !=
            preview_->plan.sources->evaluated_base_settings_source ||
        previous->plan.definition.settings() != preview_->plan.definition.settings()};
    if (require_review && changed_inputs) {
        set_status(LOCTEXT(
            "ReplayChangedPreview",
            "The saved source inputs changed. Review the refreshed preview, then apply it."));
        notify_changed();
        return false;
    }
    if (!require_review) {
        auto const candidate{
            validate_playable_s7_definition(preview_->plan.definition, *document_->level_config)};
        if (!candidate) {
            set_status(FText::FromString(candidate.error()));
            notify_changed();
            return false;
        }
    }
    auto const applied{
        apply_s7_level_authoring_preview(*level, *document_, source_session_, preview_.GetValue())};
    bool playable{false};
    if (!applied) {
        set_status(FText::FromString(applied.error().message));
        if (applied.error().code == ES7LevelPreviewError::Stale) {
            clear_preview(PreviewState::Stale);
        }
    } else {
        clear_preview();
        source_session_.state().scene_dirty = false;
        auto const validation{validate_playable_s7_level(*level, *document_)};
        playable = validation.has_value();
        set_status(FText::FromString(
            validation ? FString::Printf(TEXT("The scene matches the S7 source. %s"), **validation)
                       : FString::Printf(TEXT("The scene matches the S7 source. Play validation: "
                                              "%s"),
                                         *validation.error())));
    }
    notify_changed();
    return applied.has_value() && (require_review || playable);
}

void US7LevelAuthoringMode::save() {
    save_source_buffer(false);
}

void US7LevelAuthoringMode::save_as() {
    save_source_buffer(true);
}

void US7LevelAuthoringMode::save_source_buffer(bool const select_path) {
    if (source_session_.buffer().IsEmpty()) {
        set_status(LOCTEXT("NoSourceBuffer", "There is no source buffer to save."));
        notify_changed();
        return;
    }
    std::expected<void, FString> saved;
    if (select_path || !source_session_.is_attached() || source_session_.path().IsEmpty()) {
        auto const selected{select_save_target(current_level())};
        if (!selected.IsSet()) {
            return;
        }
        saved = source_session_.save_as(selected->path, selected->overwrite);
    } else {
        saved = source_session_.save();
    }
    if (!saved) {
        set_status(FText::FromString(saved.error()));
    } else {
        update_document_source_path();
        set_status(FText::FromString(FString::Printf(TEXT("Saved the source buffer exactly to %s."),
                                                     *source_session_.path())));
    }
    notify_changed();
}

void US7LevelAuthoringMode::save_canonical_from_scene() {
    auto* const level{current_level()};
    if (!IsValid(level) || !document_.IsValid() || source_session_.document() != document_.Get()) {
        return;
    }
    if (source_session_.has_unapplied_buffer(document_->synchronized_source)) {
        set_status(LOCTEXT("UnappliedSource",
                           "Apply or discard the source buffer before saving canonical scene "
                           "state."));
        notify_changed();
        return;
    }
    auto const emitted{collect_s7_scene_source(*level, *document_)};
    if (!emitted) {
        set_status(FText::FromString(emitted.error()));
        notify_changed();
        return;
    }
    auto const source{*emitted};

    std::expected<void, FString> saved;
    if (source_session_.path().IsEmpty()) {
        auto const selected{select_save_target(level)};
        if (!selected.IsSet()) {
            return;
        }
        saved = source_session_.save_replacement_as(
            source, selected->path, document_->synchronized_source, selected->overwrite);
    } else {
        saved = source_session_.save_replacement(source, document_->synchronized_source);
    }
    if (!saved) {
        set_status(FText::FromString(saved.error()));
        notify_changed();
        return;
    }
    document_->Modify();
    update_document_source_path();
    document_->synchronized_source = source;
    document_->synchronized_scene_source = source;
    document_->evaluated_level_source = source;
    source_session_.state().scene_dirty = false;
    set_status(FText::FromString(
        FString::Printf(TEXT("Saved canonical scene state with %d entities to %s."),
                        document_->entities.Num(),
                        *source_session_.path())));
    notify_changed();
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
