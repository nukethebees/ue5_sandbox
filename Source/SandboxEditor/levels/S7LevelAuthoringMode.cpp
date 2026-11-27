#include "SandboxEditor/levels/S7LevelAuthoringMode.h"

#include "SandboxEditor/levels/S7LevelAuthoringActors.h"
#include "SandboxEditor/levels/S7LevelAuthoringDocument.h"
#include "SandboxEditor/levels/S7LevelAuthoringModeToolkit.h"
#include "SandboxEditor/levels/S7LevelObserverCamera.h"
#include "SandboxEditor/levels/S7SceneActors.h"
#include "SandboxEditor/logging/SandboxEditorLogCategories.h"
#include <ioj/levels/authoring/level_definition_writer.h>
#include <ioj/levels/collision_grid_geometry.h>
#include <ioj/rotator_math.h>
#include <ioj/sim/collision/entity_world_bounds.h>
#include <ioj/sim/levels/simulation_settings.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>
#include <SpaceGame/simulation/TestBatchOrchestrator.h>
#include <SpaceGameS7/level_script_catalog.h>

#include <SandboxCoreEngine/strings.h>
#include <SandboxCoreEngine/VectorConversion.h>

#include <CanvasItem.h>
#include <CanvasTypes.h>
#include <DesktopPlatformModule.h>
#include <Editor.h>
#include <Engine/Level.h>
#include <Engine/Selection.h>
#include <EngineUtils.h>
#include <Framework/Application/SlateApplication.h>
#include <HAL/PlatformProcess.h>
#include <IDesktopPlatform.h>
#include <Misc/MessageDialog.h>
#include <Misc/Paths.h>
#include <PrimitiveDrawingUtils.h>
#include <SceneInterface.h>
#include <SceneManagement.h>
#include <SceneView.h>
#include <ScopedTransaction.h>
#include <Styling/AppStyle.h>
#include <UnrealClient.h>
#include <UObject/ObjectSaveContext.h>

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

}

namespace levels = ioj::levels;

using namespace ioj::editor;
using namespace ioj::editor::s7_level_authoring_mode_detail;

ioj::FCollisionOverlayVisibility US7LevelAuthoringMode::collision_overlays_{};

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
    FEditorDelegates::PostSaveWorldWithContext.AddWeakLambda(
        this, [this](UWorld* world, FObjectPostSaveContext) {
            if (document_.IsValid() && document_->GetWorld() == world) {
                notify_changed();
            }
        });
    refresh_document();
    refresh_collision_preview();
    notify_changed();
}

void US7LevelAuthoringMode::Exit() {
    FEditorDelegates::PostSaveWorldWithContext.RemoveAll(this);
    document_.Reset();
    grid_lines_.Reset();
    entity_bounds_.Reset();
    Super::Exit();
}

void US7LevelAuthoringMode::ActorSelectionChangeNotify() {
    Super::ActorSelectionChangeNotify();
    notify_changed();
}

void US7LevelAuthoringMode::PostUndo() {
    Super::PostUndo();
    notify_changed();
}

void US7LevelAuthoringMode::CreateToolkit() {
    Toolkit = MakeShared<FS7LevelAuthoringModeToolkit>();
}

void US7LevelAuthoringMode::Tick(FEditorViewportClient* viewport_client, float delta_time) {
    Super::Tick(viewport_client, delta_time);
    if (current_level() != displayed_level_.Get() || document_.IsStale() ||
        (current_level() && current_level()->Actors.Num() != actor_count_)) {
        auto const previous_document{document_};
        auto const previous_level{displayed_level_};
        refresh_document();
        if (previous_document != document_ || previous_level != displayed_level_) {
            refresh_collision_preview();
        }
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
    if (view && view->Family->Scene == level->GetWorld()->Scene) {
        draw_collision_preview(*pdi, *level);
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
        auto label_text{FText::FromString(binding.actor->GetActorLabel())};
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

auto US7LevelAuthoringMode::current_level() const -> ULevel* {
    auto* const world{GEditor ? GEditor->GetEditorWorldContext().World() : nullptr};
    return IsValid(world) ? world->GetCurrentLevel() : nullptr;
}

void US7LevelAuthoringMode::refresh_document() {
    auto* const level{current_level()};
    displayed_level_ = level;
    actor_count_ = level ? level->Actors.Num() : 0;
    if (!IsValid(level)) {
        document_.Reset();
        return;
    }
    auto const previous_document{document_};
    auto const found{find_level_authoring_document(*level)};
    document_ = found ? *found : nullptr;
    if (!found) {
        UE_LOG(LogS7LevelAuthoring, Warning, TEXT("%s"), *found.error());
    } else if (document_.IsValid() && document_ != previous_document &&
               !FPaths::FileExists(s7_level_path(*document_))) {
        UE_LOG(LogS7LevelAuthoring,
               Warning,
               TEXT("Missing %s. Use Link Existing Script to locate it."),
               *document_->source_path);
    }
}

auto US7LevelAuthoringMode::collision_overlays() -> ioj::FCollisionOverlayVisibility {
    return collision_overlays_;
}
void US7LevelAuthoringMode::set_collision_overlays(
    ioj::FCollisionOverlayVisibility const visibility) {
    collision_overlays_ = visibility;
    refresh_collision_preview();
    for (auto const& context : GEditor->GetWorldContexts()) {
        if (context.WorldType == EWorldType::PIE && IsValid(context.World())) {
            for (TActorIterator<ioj::ATestBatchOrchestrator> it{context.World()}; it; ++it) {
                it->set_collision_overlays(visibility);
            }
        }
    }
    GEditor->RedrawLevelEditingViewports();
    notify_changed();
}

void US7LevelAuthoringMode::refresh_collision_preview() {
    grid_lines_.Reset();
    entity_bounds_.Reset();
    if ((!collision_overlays_.grid && !collision_overlays_.bounds) || !document_.IsValid()) {
        return;
    }

    auto definition{read_s7_authored_level(*document_)};
    if (!definition) {
        UE_LOG(LogS7LevelAuthoring, Warning, TEXT("Collision preview: %s"), *definition.error());
        return;
    }
    apply_collision_preview(definition->settings());
}

void US7LevelAuthoringMode::apply_collision_preview(ioj::settings::Settings const& settings) {
    auto const grid{ioj::sim::make_grid_spec(settings.collision_grid)};
    if (!ioj::sim::collision::is_configured(grid)) {
        UE_LOG(LogS7LevelAuthoring,
               Warning,
               TEXT("Invalid S7 collision grid dimensions or cell size."));
        return;
    }
    if (collision_overlays_.grid) {
        grid_lines_ = ioj::make_collision_grid_lines(grid);
    }
    if (collision_overlays_.bounds) {
        entity_bounds_.Emplace(ioj::sim::make_entity_bounds(settings.entity_geometry));
    }
}

void US7LevelAuthoringMode::draw_collision_preview(FPrimitiveDrawInterface& pdi,
                                                   ULevel const& level) const {
    auto const* config{document_->level_config.Get()};
    if (!IsValid(config)) {
        return;
    }
    auto const& style{config->collision_grid};
    ioj::draw_collision_grid_lines(
        pdi, grid_lines_, style.line_colour, style.line_thickness, SDPG_World);

    if (!entity_bounds_) {
        return;
    }
    for (auto const& actor : level.Actors) {
        if (!IsValid(actor)) {
            continue;
        }
        auto const resolved{resolve_s7_level_actor(*actor)};
        if (!resolved) {
            continue;
        }
        using enum ioj::levels::EntityArchetype;
        using ioj::sim::EntityType;
        EntityType type{};
        ioj::Rotator3f rotation{actor->GetActorRotation()};
        switch (resolved->archetype) {
            case PlayerFighter:
                type = EntityType::PlayerShip;
                break;
            case CapitalShip:
                type = EntityType::CapitalShip;
                break;
            case StaticTurret:
                type = EntityType::Turret;
                break;
            case Fighter:
                type = EntityType::Fighter;
                rotation.roll = 0.f;
                break;
            case TubeSpinner:
                type = EntityType::TubeSpinner;
                rotation.pitch = 0.f;
                rotation.roll = 0.f;
                break;
        }
        auto const box{
            ioj::sim::collision::make_entity_world_bounds(*entity_bounds_,
                                                          type,
                                                          ioj::Vector3f{actor->GetActorLocation()},
                                                          ioj::to_quaternion(rotation))};
        DrawWireBox(&pdi,
                    FBox{FVector{ioj::to_unreal(box.min)}, FVector{ioj::to_unreal(box.max)}},
                    FLinearColor::Green,
                    SDPG_World,
                    style.line_thickness,
                    0.f,
                    true);
    }
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
        UE_LOG(LogS7LevelAuthoring,
               Warning,
               TEXT("This map already links to %s."),
               *document_->source_path);
        return;
    }
    auto created{set_up_s7_level(*level, LevelSetup::Create)};
    if (!created) {
        UE_LOG(LogS7LevelAuthoring, Error, TEXT("Create Level Script: %s"), *created.error());
        return;
    }
    document_ = *created;
    UE_LOG(LogS7LevelAuthoring, Display, TEXT("Created %s."), *document_->source_path);
    refresh_collision_preview();
    notify_changed();
}

auto US7LevelAuthoringMode::selected_binding() const -> FS7LevelEntityBinding* {
    if (!document_.IsValid() || !GEditor) {
        return nullptr;
    }
    auto* const selection{GEditor->GetSelectedActors()};
    if (selection->Num() != 1) {
        return nullptr;
    }
    auto* const actor{Cast<AActor>(selection->GetSelectedObject(0))};
    return IsValid(actor)
             ? document_->entities.FindByPredicate(
                   [actor](FS7LevelEntityBinding const& binding) { return binding.actor == actor; })
             : nullptr;
}
auto US7LevelAuthoringMode::selected_script_id() const -> FString {
    auto const* binding{selected_binding()};
    return binding ? s7_actor_script_id(*binding->actor, &binding->id) : FString{};
}
void US7LevelAuthoringMode::rename_selected_entity(FString const id) {
    auto* const binding{selected_binding()};
    if (!binding) {
        UE_LOG(LogS7LevelAuthoring, Warning, TEXT("Select one bound actor to edit its Script ID."));
        notify_changed();
        return;
    }

    if (!is_canonical_s7_level_entity_id(FStringView{id})) {
        UE_LOG(LogS7LevelAuthoring,
               Warning,
               TEXT("Entity IDs must be lowercase S7 symbols beginning with a letter."));
        notify_changed();
        return;
    }
    if (selected_script_id() == id) {
        return;
    }
    if (document_->entities.ContainsByPredicate(
            [binding, &id](FS7LevelEntityBinding const& candidate) {
                return &candidate != binding && IsValid(candidate.actor) &&
                       s7_actor_script_id(*candidate.actor, &candidate.id) == id;
            })) {
        UE_LOG(LogS7LevelAuthoring, Warning, TEXT("Script ID '%s' is already in use."), *id);
        notify_changed();
        return;
    }

    FScopedTransaction transaction{LOCTEXT("RenameEntityId", "Rename S7 Entity ID")};
    document_->Modify();
    SceneActors::visit_scene_actor(*binding->actor, [&](auto& actor, auto) {
        if (actor.export_to_script) {
            actor.Modify();
            auto const label{s7_actor_label_for_id(id)};
            if constexpr (requires { actor.custom_name; }) {
                actor.custom_name = FName{*label};
            }
            actor.SetActorLabel(label);
        } else {
            binding->id = id;
        }
    });
    document_->set_needs_export(true);
    UE_LOG(LogS7LevelAuthoring, Display, TEXT("Renamed selected entity to '%s'."), *id);
    notify_changed();
}

void US7LevelAuthoringMode::link_script() {
    auto* level{current_level()};
    auto path{select_source_path()};
    if (!level || !path) {
        return;
    }
    auto linked{set_up_s7_level(*level, LevelSetup::Link, *path)};
    if (!linked) {
        UE_LOG(LogS7LevelAuthoring, Error, TEXT("Link Existing Script: %s"), *linked.error());
        return;
    }
    document_ = *linked;
    UE_LOG(LogS7LevelAuthoring, Display, TEXT("Linked %s."), *document_->source_path);
    refresh_collision_preview();
    notify_changed();
}

void US7LevelAuthoringMode::open_script_editor() {
    if (document_.IsValid()) {
        auto const path{s7_level_path(*document_)};
        FPlatformProcess::LaunchFileInDefaultExternalApplication(*path);
    }
}

void US7LevelAuthoringMode::rebuild_scene() {
    rebuild_scene(RebuildPolicy::PreserveEdits);
}
void US7LevelAuthoringMode::force_rebuild_scene() {
    rebuild_scene(RebuildPolicy::DiscardEdits);
}
void US7LevelAuthoringMode::rebuild_scene(RebuildPolicy policy) {
    auto* const level{current_level()};
    if (!IsValid(level) || !document_.IsValid()) {
        return;
    }
    if (policy == RebuildPolicy::DiscardEdits) {
        auto const targets{s7_rebuild_targets(*level, *document_)};
        auto const message{FText::Format(
            LOCTEXT("ConfirmForceRebuild",
                    "Discard scene edits and rebuild from the saved script? This replaces {0} "
                    "bound actors, removes {1} unexported placements and restores the viewpoint."),
            targets.bound.Num(),
            targets.unexported.Num())};
        if (FMessageDialog::Open(EAppMsgType::YesNo, EAppReturnType::No, message) !=
            EAppReturnType::Yes) {
            return;
        }
    }

    auto imported{import_s7_level(*level, *document_, policy)};
    if (imported) {
        UE_LOG(
            LogS7LevelAuthoring, Display, TEXT("Rebuilt scene from %s."), *document_->source_path);
    } else {
        UE_LOG(LogS7LevelAuthoring, Warning, TEXT("Rebuild Scene: %s"), *imported.error());
    }
    notify_changed();
}

void US7LevelAuthoringMode::check_scene() {
    auto* const level{current_level()};
    if (!IsValid(level) || !document_.IsValid()) {
        UE_LOG(LogS7LevelAuthoring,
               Warning,
               TEXT("Create or link a level script before checking the scene."));
    } else if (auto checked{check_s7_scene(*level, *document_)}) {
        apply_collision_preview(checked->definition.settings());
        UE_LOG(LogS7LevelAuthoring, Display, TEXT("Scene passes export checks."));
        for (auto const& warning : checked->warnings) {
            UE_LOG(LogS7LevelAuthoring, Warning, TEXT("Shared Level Assets: %s"), *warning);
        }
    } else {
        refresh_collision_preview();
        UE_LOG(LogS7LevelAuthoring, Error, TEXT("Check Scene: %s"), *checked.error());
    }
}

void US7LevelAuthoringMode::export_scene() {
    auto* const level{current_level()};
    if (!IsValid(level) || !document_.IsValid()) {
        return;
    }
    auto exported{export_s7_editor_config(*level, *document_)};
    if (exported) {
        UE_LOG(LogS7LevelAuthoring,
               Display,
               TEXT("Exported scene to %s."),
               *s7_editor_config_path(*document_));
    } else {
        UE_LOG(LogS7LevelAuthoring, Error, TEXT("Export Scene: %s"), *exported.error());
    }
    notify_changed();
}

#undef LOCTEXT_NAMESPACE
