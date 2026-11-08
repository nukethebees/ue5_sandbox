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
#include <IDesktopPlatform.h>
#include <Misc/MessageDialog.h>
#include <Misc/Paths.h>
#include <SceneManagement.h>
#include <SceneView.h>
#include <ScopedTransaction.h>
#include <Styling/AppStyle.h>
#include <Subsystems/AssetEditorSubsystem.h>
#include <UnrealClient.h>

#define LOCTEXT_NAMESPACE "US7LevelAuthoringMode"

namespace ioj::editor::s7_level_authoring_mode_detail {
auto select_source_path() -> TOptional<FString> {
    auto* const desktop{FDesktopPlatformModule::Get()};
    if (!desktop) {
        return NullOpt;
    }
    auto const parent{FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr)};
    TArray<FString> paths;
    auto const directory{levels::authoring::default_level_script_directory()};
    auto const selected{desktop->OpenFileDialog(parent,
                                                TEXT("Load S7 Level"),
                                                directory,
                                                TEXT(""),
                                                TEXT("S7 level (*.scm)|*.scm"),
                                                EFileDialogFlags::None,
                                                paths)};
    if (!selected || paths.Num() != 1) {
        return NullOpt;
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
    document_.Reset();
    source_session_.detach();
    Super::Exit();
}

void US7LevelAuthoringMode::CreateToolkit() {
    Toolkit = MakeShared<FS7LevelAuthoringModeToolkit>();
}

void US7LevelAuthoringMode::Tick(FEditorViewportClient* viewport_client, float delta_time) {
    Super::Tick(viewport_client, delta_time);
    refresh_elapsed_seconds_ += delta_time;
    if (refresh_elapsed_seconds_ < 0.5f) {
        return;
    }
    refresh_elapsed_seconds_ = 0.0f;
    refresh_document();
    if (source_session_.is_attached()) {
        auto refreshed{source_session_.refresh_external_conflict()};
        if (!refreshed) {
            set_status(FText::FromString(refreshed.error()));
        }
    }
    changed_.Broadcast();
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
        auto label_text{FText::FromString(binding.id)};
        auto const resolved{ioj::editor::resolve_s7_level_actor(*binding.actor)};
        if (resolved && resolved->spawn_time_seconds > 0.0) {
            label_text = FText::Format(LOCTEXT("DelayedEntityLabel", "{0} [T+{1}s]"),
                                       label_text,
                                       FText::AsNumber(resolved->spawn_time_seconds));
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
    return {.path = source_session_.path(),
            .source_dirty = source_session_.is_dirty(),
            .disk_conflict = source_session_.has_external_conflict(),
            .detached = !source_session_.is_attached()};
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

void US7LevelAuthoringMode::notify_changed(ioj::settings::Settings const* settings) {
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

    auto const& new_id{id};
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
    set_status(FText::Format(LOCTEXT("RenamedEntityId", "Renamed selected entity to '{0}'."),
                             FText::FromString(new_id)));
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
    auto const path{select_source_path()};
    if (!path.IsSet()) {
        return false;
    }
    if (!document_.IsValid()) {
        auto* const level{current_level()};
        if (!IsValid(level)) {
            return false;
        }
        auto created{create_level_authoring_document(*level, *path)};
        if (!created) {
            set_status(FText::FromString(created.error()));
            notify_changed();
            return false;
        }
        document_ = *created;
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
    auto const root_changed{!FPaths::IsSamePath(document_->source_path, source_session_.path())};
    document_->Modify();
    document_->source_path = source_session_.path();
    for (auto const actor : document_->GetLevel()->Actors) {
        if (auto* orchestrator{Cast<ioj::ATestBatchOrchestrator>(actor.Get())};
            IsValid(orchestrator)) {
            orchestrator->Modify();
            orchestrator->set_level_script(document_->source_path);
        }
    }
    auto definition{source_session_.read()};
    if (!definition) {
        set_status(
            FText::FromString(ioj::to_fstring(levels::format_diagnostics(definition.error()))));
        notify_changed();
        return false;
    }
    auto imported{import_s7_level(*document_->GetLevel(), *document_, *definition, root_changed)};
    set_status(imported ? LOCTEXT("Imported",
                                  "Imported editor baseline values for editing and human-owned "
                                  "entities for reference. Play uses the composed level.")
                        : FText::FromString(imported.error()));
    notify_changed();
    return imported.has_value();
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
        set_status(LOCTEXT("SourceReloaded", "Attached the document source from disk."));
    }
    notify_changed();
}

void US7LevelAuthoringMode::set_source_buffer(FString source) {
    auto const revision{source_session_.revision()};
    source_session_.set_buffer(MoveTemp(source));
    if (source_session_.revision() != revision) {
        set_status(LOCTEXT(
            "BufferUpdated",
            "Source buffer updated. Save it or import the buffer to view it in the scene."));
    }
    notify_changed();
}

void US7LevelAuthoringMode::import_source() {
    auto* const level{current_level()};
    if (!IsValid(level) || !document_.IsValid()) {
        return;
    }
    auto definition{source_session_.read()};
    if (!definition) {
        set_status(
            FText::FromString(ioj::to_fstring(levels::format_diagnostics(definition.error()))));
    } else {
        auto imported{import_s7_level(*level, *document_, *definition)};
        set_status(imported ? LOCTEXT("Imported",
                                      "Imported editor baseline values for editing and human-owned "
                                      "entities for reference. Play uses the composed level.")
                            : FText::FromString(imported.error()));
    }
    notify_changed();
}

void US7LevelAuthoringMode::export_scene() {
    auto* const level{current_level()};
    if (!IsValid(level) || !document_.IsValid()) {
        return;
    }
    auto exported{export_s7_editor_config(*level, *document_)};
    set_status(
        exported
            ? LOCTEXT(
                  "Exported",
                  "Saved editor-config.scm. Edits to entities with Export disabled are not saved.")
            : FText::FromString(exported.error()));
    notify_changed();
}

void US7LevelAuthoringMode::save() {
    if (source_session_.buffer().IsEmpty()) {
        set_status(LOCTEXT("NoSourceBuffer", "There is no source buffer to save."));
        notify_changed();
        return;
    }
    auto const saved{source_session_.save()};
    if (!saved) {
        set_status(FText::FromString(saved.error()));
    } else {
        set_status(FText::FromString(FString::Printf(TEXT("Saved the source buffer exactly to %s."),
                                                     *source_session_.path())));
    }
    notify_changed();
}

#undef LOCTEXT_NAMESPACE
