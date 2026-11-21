#include "SandboxEditor/levels/S7LevelAuthoringMode.h"

#include "SandboxEditor/levels/S7LevelAuthoringActors.h"
#include "SandboxEditor/levels/S7LevelAuthoringDocument.h"
#include "SandboxEditor/levels/S7LevelAuthoringModeToolkit.h"
#include "SandboxEditor/levels/S7LevelObserverCamera.h"
#include "SandboxEditor/levels/S7LevelPlayableSetup.h"
#include <ioj/levels/authoring/level_definition_writer.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>
#include <SpaceGame/simulation/TestBatchOrchestrator.h>
#include <SpaceGameS7/level_script_catalog.h>

#include <SandboxCoreEngine/strings.h>

#include <Algo/Count.h>
#include <CanvasItem.h>
#include <CanvasTypes.h>
#include <DesktopPlatformModule.h>
#include <Editor.h>
#include <Engine/Level.h>
#include <Engine/Selection.h>
#include <Framework/Application/SlateApplication.h>
#include <HAL/PlatformProcess.h>
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
                                                TEXT("Link Existing Level Script"),
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

auto selected_bound_entity(AS7LevelAuthoringDocument& document)
    -> S7AuthoringResult<FS7LevelEntityBinding*> {
    using Result = S7AuthoringResult<FS7LevelEntityBinding*>;

    if (!GEditor) {
        return Result{std::unexpect, TEXT("The editor selection is unavailable.")};
    }

    TArray<AActor*> selected;
    for (FSelectionIterator it{*GEditor->GetSelectedActors()}; it; ++it) {
        auto* const actor{Cast<AActor>(*it)};
        if (IsValid(actor)) {
            selected.AddUnique(actor);
        }
    }
    if (selected.Num() != 1) {
        return Result{std::unexpect, TEXT("Select exactly one bound entity first.")};
    }

    auto* const binding{document.entities.FindByPredicate(
        [actor = selected[0]](FS7LevelEntityBinding const& candidate) {
            return candidate.actor == actor;
        })};
    if (!binding) {
        return Result{std::unexpect,
                      TEXT("The selected actor is not an bound S7 entity. Rename an "
                           "unbound actor in the Outliner before its first export.")};
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
    notify_changed();
}

void US7LevelAuthoringMode::Exit() {
    document_.Reset();
    Super::Exit();
}

void US7LevelAuthoringMode::CreateToolkit() {
    Toolkit = MakeShared<FS7LevelAuthoringModeToolkit>();
}

void US7LevelAuthoringMode::Tick(FEditorViewportClient* viewport_client, float delta_time) {
    Super::Tick(viewport_client, delta_time);
    if (current_level() != displayed_level_.Get() || document_.IsStale() ||
        (current_level() && current_level()->Actors.Num() != actor_count_)) {
        refresh_document();
        notify_changed();
    }
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

auto US7LevelAuthoringMode::status() const -> FText const& {
    return status_;
}

auto US7LevelAuthoringMode::current_level() const -> ULevel* {
    auto* const world{GEditor ? GEditor->GetEditorWorldContext().World() : nullptr};
    return IsValid(world) ? world->GetCurrentLevel() : nullptr;
}

void US7LevelAuthoringMode::refresh_document() {
    auto* const level{current_level()};
    displayed_level_ = level;
    actor_count_ = level ? level->Actors.Num() : 0;
    auto const found{IsValid(level)
                         ? find_level_authoring_document(*level)
                         : S7AuthoringResult<AS7LevelAuthoringDocument*>{
                               std::unexpect, TEXT("The current editor level is unavailable.")}};
    document_ = found ? *found : nullptr;
    if (!found) {
        set_status(FText::FromString(found.error()));
        return;
    }
    if (!document_.IsValid()) {
        set_status(LOCTEXT("NoLinkedScript", "Create a level script or link an existing one."));
    } else if (!FPaths::FileExists(s7_mission_path(*document_))) {
        set_status(FText::Format(
            LOCTEXT("MissingScript", "Missing {0}. Use Link Existing Script to locate it."),
            FText::FromString(document_->source_path)));
    } else {
        set_status(FText::FromString(document_->source_path));
    }
}

void US7LevelAuthoringMode::set_status(FText text) {
    status_ = MoveTemp(text);
}

void US7LevelAuthoringMode::notify_changed() {
    if (Toolkit.IsValid()) {
        StaticCastSharedPtr<FS7LevelAuthoringModeToolkit>(Toolkit)->refresh();
    }
}

void US7LevelAuthoringMode::create_document() {
    auto* const level{current_level()};
    if (!IsValid(level)) {
        return;
    }
    if (document_.IsValid()) {
        set_status(LOCTEXT("AlreadyLinked", "This map already has a linked script."));
    } else {
        auto created{set_up_s7_mission(*level, MissionSetup::Create)};
        if (created) {
            document_ = *created;
        }
        set_status(FText::FromString(created ? document_->source_path : created.error()));
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
    document_->set_needs_export(true);
    set_status(FText::Format(LOCTEXT("RenamedEntityId", "Renamed selected entity to '{0}'."),
                             FText::FromString(new_id)));
    notify_changed();
}

void US7LevelAuthoringMode::open_level_config() {
    auto* const config{document_.IsValid() ? document_->level_config.Get() : nullptr};
    auto* const assets{GEditor ? GEditor->GetEditorSubsystem<UAssetEditorSubsystem>() : nullptr};
    if (!IsValid(config) || !IsValid(assets) || !assets->OpenEditorForAsset(config)) {
        set_status(
            LOCTEXT("OpenLevelConfigFailed", "Could not open the linked level's shared asset."));
    } else {
        set_status(LOCTEXT(
            "OpenLevelConfig",
            "This asset supplies meshes, effects and team palettes for all linked levels."));
    }
    notify_changed();
}

void US7LevelAuthoringMode::validate_playable_level() {
    if (!document_.IsValid()) {
        set_status(LOCTEXT("NoLevelForValidation", "Open an S7 level before validating it."));
    } else {
        auto const result{validate_playable_s7_level(*document_)};
        set_status(FText::FromString(result ? *result : result.error()));
    }
    notify_changed();
}

void US7LevelAuthoringMode::link_script() {
    auto* level{current_level()};
    auto path{select_source_path()};
    if (!level || !path) {
        return;
    }
    auto linked{set_up_s7_mission(*level, MissionSetup::Link, *path)};
    if (linked) {
        document_ = *linked;
    }
    set_status(FText::FromString(linked ? document_->source_path : linked.error()));
    notify_changed();
}

void US7LevelAuthoringMode::open_script_editor() {
    if (document_.IsValid()) {
        auto const path{s7_mission_path(*document_)};
        FPlatformProcess::LaunchFileInDefaultExternalApplication(*path);
    }
}

void US7LevelAuthoringMode::rebuild_scene() {
    auto* const level{current_level()};
    if (!IsValid(level) || !document_.IsValid()) {
        return;
    }
    auto const count{Algo::CountIf(document_->entities,
                                   [](auto const& binding) { return IsValid(binding.actor); })};
    if (count && FMessageDialog::Open(
                     EAppMsgType::YesNo,
                     FText::Format(LOCTEXT("ConfirmRebuild",
                                           "Replace {0} bound actors from the saved level script? "
                                           "Unexported edits to these actors will be lost."),
                                   count)) != EAppReturnType::Yes) {
        return;
    }
    auto imported{import_s7_level(*level, *document_)};
    set_status(imported
                   ? LOCTEXT("Imported",
                             "Restored editor placements and script-defined actors. "
                             "Move exported actors and save the map to update their placements.")
                   : FText::FromString(imported.error()));
    notify_changed();
}

void US7LevelAuthoringMode::check_export() {
    auto* level{current_level()};
    if (!level || !document_.IsValid()) {
        return;
    }
    auto result{check_s7_export(*level, *document_)};
    set_status(result ? LOCTEXT("ExportChecked", "Scene passes export checks.")
                      : FText::FromString(result.error()));
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

#undef LOCTEXT_NAMESPACE
