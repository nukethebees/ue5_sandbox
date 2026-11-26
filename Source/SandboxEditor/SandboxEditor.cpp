#include "SandboxEditor/SandboxEditor.h"

#include "SandboxEditor/levels/S7EntityGeometryExport.h"
#include "SandboxEditor/levels/S7LevelAuthoringActors.h"
#include "SandboxEditor/levels/S7LevelAuthoringDocument.h"
#include "SandboxEditor/levels/S7LevelAuthoringMode.h"
#include "SandboxEditor/levels/S7LevelAuthoringSession.h"
#include "SandboxEditor/levels/S7LevelPlayableSetup.h"
#include "SandboxEditor/logging/SandboxEditorLogCategories.h"
#include "SandboxGameShared/utilities/macros/null_checks.hpp"
#include <SpaceGame/simulation/LevelCollisionHost.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>

#include "Containers/UnrealString.h"
#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "Editor/UnrealEdEngine.h"
#include "EditorModeManager.h"
#include "GenericPlatform/GenericPlatformMisc.h"
#include "HAL/Platform.h"
#include "HAL/PlatformProcess.h"
#include "Internationalization/Internationalization.h"
#include "Logging/LogMacros.h"
#include "Logging/LogVerbosity.h"
#include "Misc/FileHelper.h"
#include "Misc/MessageDialog.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "ToolMenus.h"
#include "UnrealEdGlobals.h"
#include <Components/SceneComponent.h>
#include <Engine/Level.h>
#include <Engine/World.h>
#include <Misc/CoreDelegates.h>
#include <Misc/TransactionObjectEvent.h>
#include <UObject/ObjectSaveContext.h>
#define LOCTEXT_NAMESPACE "FSandboxEditorModule"

namespace ioj::editor {
static auto current_mission_document() -> AS7LevelAuthoringDocument* {
    auto* const world{GEditor ? GEditor->GetEditorWorldContext().World() : nullptr};
    auto* const level{world ? world->GetCurrentLevel() : nullptr};
    if (!level) {
        return nullptr;
    }

    auto const document{find_level_authoring_document(*level)};
    return document ? *document : nullptr;
}

auto prepare_staged_mission(AS7LevelAuthoringDocument& document) -> S7AuthoringResult<FString> {
    auto stale{s7_scene_needs_export(document)};
    if (!stale) {
        return std::unexpected{stale.error()};
    }

    if (document.needs_export && !*stale) {
        return std::unexpected{FString{
            TEXT("The mission does not import the edited scene. Restore its editor-config.scm "
                 "import or rebuild the scene from the mission before playing.")}};
    }

    if (*stale) {
        auto result{export_s7_editor_config(*document.GetLevel(), document)};
        if (!result) {
            return std::unexpected{result.error()};
        }
    }

    return s7_mission_path(document);
}
auto refresh_staged_player() -> FProcHandle {
    auto const project{FPaths::ConvertRelativePathToFull(FPaths::ProjectDir())};
    return FPlatformProcess::CreateProc(TEXT("cmake.exe"),
                                        TEXT("--workflow --preset development-staged-game"),
                                        false,
                                        true,
                                        true,
                                        nullptr,
                                        0,
                                        *project,
                                        nullptr);
}
auto launch_staged_mission(FString const& mission, bool exit_when_ready)
    -> S7AuthoringResult<FProcHandle> {
    FString executable;
    auto const marker{FPaths::ProjectSavedDir() / TEXT("IojPlayer.path")};
    FFileHelper::LoadFileToString(executable, *marker);
    executable.TrimStartAndEndInline();
    if (!FPaths::FileExists(executable)) {
        return std::unexpected{
            FString::Printf(TEXT("Build the Development stage to launch ioj.exe (%s)."), *marker)};
    }

    auto const scripts{
        FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("LevelScripts"))};
    auto args{
        FString::Printf(TEXT("--display-mode windowed --editor-scripts \"%s\" --mission \"%s\""),
                        *scripts,
                        *mission)};
    if (exit_when_ready) {
        args += TEXT(" --exit-when-ready");
    }

    auto const directory{FPaths::GetPath(executable)};

    auto process{FPlatformProcess::CreateProc(
        *executable, *args, true, false, false, nullptr, 0, *directory, nullptr)};
    if (!process.IsValid()) {
        return std::unexpected{FString::Printf(TEXT("Could not launch %s"), *executable)};
    }

    return process;
}

static void launch_mission(FString const& mission) {
    auto process{launch_staged_mission(mission)};
    if (!process) {
        FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(process.error()));
        return;
    }

    FPlatformProcess::CloseProc(*process);
}
}

void FSandboxEditorModule::StartupModule() {
    if (GEditor) {
        register_actor_events();
    } else {
        FCoreDelegates::GetOnPostEngineInit().AddRaw(this,
                                                     &FSandboxEditorModule::register_actor_events);
    }

    FCoreUObjectDelegates::OnObjectPropertyChanged.AddRaw(
        this, &FSandboxEditorModule::on_property_changed);
    FCoreUObjectDelegates::OnObjectTransacted.AddRaw(this, &FSandboxEditorModule::on_transacted);
    FEditorDelegates::PreSaveWorldWithContext.AddRaw(this,
                                                     &FSandboxEditorModule::on_pre_save_world);
#if UE_BUILD_DEVELOPMENT
    UE_LOG(LogTemp, Verbose, TEXT("FSandboxEditorModule starting up"));
#endif
    UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(
        this, &FSandboxEditorModule::register_menu_extensions));
}
void FSandboxEditorModule::ShutdownModule() {
    FCoreDelegates::GetOnPostEngineInit().RemoveAll(this);
    FEditorDelegates::OnNewActorsPlaced.RemoveAll(this);
    FEditorDelegates::OnMapOpened.RemoveAll(this);
    if (GUnrealEd) {
        GUnrealEd->OnPasteActorsEnd().RemoveAll(this);
    }

    if (GEditor) {
        GEditor->OnLevelActorAdded().RemoveAll(this);
        GEditor->OnLevelActorDeleted().RemoveAll(this);
        GEditor->OnEndObjectMovement().RemoveAll(this);
    }

    FCoreUObjectDelegates::OnObjectPropertyChanged.RemoveAll(this);
    FCoreUObjectDelegates::OnObjectTransacted.RemoveAll(this);
    FEditorDelegates::PreSaveWorldWithContext.RemoveAll(this);
    FTSTicker::GetCoreTicker().RemoveTicker(refresh_ticker_);
    if (refresh_process_.IsValid()) {
        FPlatformProcess::CloseProc(refresh_process_);
    }

    UToolMenus::UnRegisterStartupCallback(this);
    UToolMenus::UnregisterOwner(this);
}

namespace {
auto editor_proxy_levels(TArray<AActor*> const& actors) {
    TMap<ULevel*, TSet<AActor*>> levels;
    for (auto* actor : actors) {
        if (IsValid(actor) && !actor->IsTemplate() && actor->GetWorld() &&
            actor->GetWorld()->WorldType == EWorldType::Editor &&
            !ioj::editor::is_rebuilding_s7_level(*actor->GetLevel()) &&
            ioj::editor::resolve_s7_level_actor(*actor)) {
            levels.FindOrAdd(actor->GetLevel()).Add(actor);
        }
    }

    return levels;
}
void mark_s7_edit(UObject* object, FName property = NAME_None) {
    if (!object || object->IsTemplate() || object->HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad)) {
        return;
    }

    auto* actor{Cast<AActor>(object)};
    if (auto* component{Cast<USceneComponent>(object)}) {
        actor = component->GetOwner();
    }

    auto* world{actor ? actor->GetWorld() : nullptr};
    if (!world || world->WorldType != EWorldType::Editor || world->bIsTearingDown) {
        return;
    }

    if ((property == TEXT("team") || property == TEXT("custom_name")) &&
        !ioj::editor::is_rebuilding_s7_level(*actor->GetLevel())) {
        ioj::editor::name_placed_s7_proxies(*actor->GetLevel(), {actor});
    }

    if (auto* document{Cast<AS7LevelAuthoringDocument>(actor)}) {
        document->set_needs_export(true);
        return;
    }

    auto document{ioj::editor::find_level_authoring_document(*actor->GetLevel())};
    if (!document || !*document) {
        return;
    }

    auto const* config{(*document)->level_config.Get()};
    if (config && ioj::FLevelCollisionHost::exports_actor(*actor, config->collision_grid)) {
        (*document)->set_needs_export(true);
        return;
    }

    if (object != actor && object != actor->GetRootComponent()) {
        return;
    }

    auto const resolved{ioj::editor::resolve_s7_level_actor(*actor)};
    if (!resolved || (!resolved->export_to_script && property != TEXT("export_to_script"))) {
        return;
    }

    if (property == TEXT("ActorLabel") &&
        (*document)->entities.ContainsByPredicate(
            [actor](auto const& binding) { return binding.actor == actor; })) {
        return;
    }
    (*document)->set_needs_export(true);
}
}
void FSandboxEditorModule::register_actor_events() {
    if (GEditor) {
        GEditor->OnLevelActorAdded().AddRaw(this, &FSandboxEditorModule::on_actor_added);
        FEditorDelegates::OnNewActorsPlaced.AddRaw(this, &FSandboxEditorModule::on_actors_placed);
        FEditorDelegates::OnMapOpened.AddRaw(this, &FSandboxEditorModule::on_map_opened);
        if (GUnrealEd) {
            GUnrealEd->OnPasteActorsEnd().AddRaw(this, &FSandboxEditorModule::on_actors_pasted);
        }
        GEditor->OnLevelActorDeleted().AddRaw(this, &FSandboxEditorModule::on_actor_changed);
        GEditor->OnEndObjectMovement().AddRaw(this, &FSandboxEditorModule::on_movement_finished);
    }
}
void FSandboxEditorModule::on_actor_added(AActor* actor) {
    if (IsValid(actor) && !actor->HasAnyFlags(RF_WasLoaded)) {
        on_actors_placed(nullptr, {actor});
    }

    on_actor_changed(actor);
}
void FSandboxEditorModule::on_actors_placed(UObject*, TArray<AActor*> const& actors) {
    for (auto const& group : editor_proxy_levels(actors)) {
        ioj::editor::name_placed_s7_proxies(*group.Key, group.Value);
    }
}
void FSandboxEditorModule::on_actors_pasted(TArray<AActor*> const& actors) {
    for (auto const& group : editor_proxy_levels(actors)) {
        ioj::editor::name_placed_s7_proxies(*group.Key, group.Value);
    }
}
void FSandboxEditorModule::on_map_opened(FString const&, bool) {
    auto* world{GEditor->GetEditorWorldContext().World()};
    if (IsValid(world)) {
        for (auto* level : world->GetLevels()) {
            ioj::editor::refresh_s7_proxy_labels(*level);
        }
    }
}
void FSandboxEditorModule::on_actor_changed(AActor* actor) {
    if (!Cast<AS7LevelAuthoringDocument>(actor)) {
        mark_s7_edit(actor);
    }
}
void FSandboxEditorModule::on_movement_finished(UObject& object) {
    mark_s7_edit(&object);
}
void FSandboxEditorModule::on_property_changed(UObject* object, FPropertyChangedEvent& change) {
    if (!(change.ChangeType & EPropertyChangeType::Interactive)) {
        mark_s7_edit(object, change.GetPropertyName());
    }
}
void FSandboxEditorModule::on_transacted(UObject* object, FTransactionObjectEvent const& change) {
    if (change.GetEventType() == ETransactionObjectEventType::UndoRedo) {
        auto const& properties{change.GetChangedProperties()};
        if (properties.IsEmpty()) {
            mark_s7_edit(object);
        } else {
            for (auto const property : properties) {
                mark_s7_edit(object, property);
            }
        }
    }
}
void FSandboxEditorModule::on_pre_save_world(UWorld* world, FObjectPreSaveContext context) {
    if (!world || world->WorldType != EWorldType::Editor || context.IsCooking() ||
        context.IsProceduralSave() || context.IsFromAutoSave()) {
        return;
    }

    auto& level{*world->PersistentLevel};
    auto document{ioj::editor::find_level_authoring_document(level)};
    if (!document) {
        UE_LOG(LogS7LevelAuthoring, Warning, TEXT("%s"), *document.error());
        return;
    }

    if (!*document) {
        return;
    }

    auto stale{ioj::editor::s7_scene_needs_export(**document)};
    if (!stale) {
        UE_LOG(LogS7LevelAuthoring, Warning, TEXT("Scene export failed: %s"), *stale.error());
    } else if (*stale) {
        auto exported{ioj::editor::export_s7_editor_config(level, **document)};
        if (!exported) {
            UE_LOG(
                LogS7LevelAuthoring, Warning, TEXT("Scene export failed: %s"), *exported.error());
        }
    }
}
bool FSandboxEditorModule::can_play_mission() const {
    return !refresh_process_.IsValid() && ioj::editor::current_mission_document();
}

void FSandboxEditorModule::play_mission(bool refresh) {
    auto* document{ioj::editor::current_mission_document()};
    if (!document || refresh_process_.IsValid()) {
        return;
    }

    auto mission{ioj::editor::prepare_staged_mission(*document)};
    if (!mission) {
        FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(mission.error()));
        return;
    }

    if (!refresh) {
        ioj::editor::launch_mission(*mission);
        return;
    }

    refresh_process_ = ioj::editor::refresh_staged_player();
    if (!refresh_process_.IsValid()) {
        FMessageDialog::Open(EAppMsgType::Ok,
                             LOCTEXT("RefreshStartFailed",
                                     "Could not start the Development cook and stage workflow."));
        return;
    }

    refresh_mission_ = MoveTemp(*mission);
    refresh_ticker_ = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateRaw(this, &FSandboxEditorModule::poll_refresh), 0.5f);
}

bool FSandboxEditorModule::poll_refresh(float) {
    if (FPlatformProcess::IsProcRunning(refresh_process_)) {
        return true;
    }

    int32 code{};
    auto const success{FPlatformProcess::GetProcReturnCode(refresh_process_, &code) && code == 0};
    FPlatformProcess::CloseProc(refresh_process_);
    refresh_process_.Reset();
    if (success) {
        ioj::editor::launch_mission(refresh_mission_);
    } else {
        FMessageDialog::Open(
            EAppMsgType::Ok,
            FText::Format(LOCTEXT("RefreshFailed", "Development cook and stage failed (exit {0})."),
                          code));
    }

    refresh_mission_.Reset();
    return false;
}

// Menu Extensions
void FSandboxEditorModule::register_menu_extensions() {

    FToolMenuOwnerScoped owner{this};
    TRY_INIT_PTR(tool_menus, UToolMenus::Get());

    auto* const play_menu{tool_menus->ExtendMenu("LevelEditor.LevelEditorToolBar.PlayToolBar")};
    play_menu->FindOrAddSection("Play").AddEntry(FToolMenuEntry::InitToolBarButton(
        "SandboxPlayExternalMission",
        FUIAction{FExecuteAction::CreateRaw(this, &FSandboxEditorModule::play_mission, false),
                  FCanExecuteAction::CreateRaw(this, &FSandboxEditorModule::can_play_mission)},
        LOCTEXT("PlayMission", "Play Mission"),
        LOCTEXT("PlayMissionTip",
                "Export scene changes and run this mission with the staged Development player"),
        FSlateIcon(FAppStyle::GetAppStyleSetName(), "PlayWorld.PlayInViewport")));

    play_menu->FindOrAddSection("Play").AddEntry(FToolMenuEntry::InitToolBarButton(
        "SandboxRefreshPlayMission",
        FUIAction{FExecuteAction::CreateRaw(this, &FSandboxEditorModule::play_mission, true),
                  FCanExecuteAction::CreateRaw(this, &FSandboxEditorModule::can_play_mission)},
        LOCTEXT("RefreshPlay", "Refresh & Play"),
        LOCTEXT("RefreshPlayTip",
                "Cook saved asset changes, stage the Development player, and run this mission"),
        FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Refresh")));

    TRY_INIT_PTR(menu, tool_menus->ExtendMenu("LevelEditor.LevelEditorToolBar.User"));
    FToolMenuSection& section{menu->AddSection("SandboxTools", FText::FromString("Sandbox Tools"))};

    section.AddEntry(FToolMenuEntry::InitToolBarButton(
        "SpaceGameLevelAuthoring",
        FUIAction(FExecuteAction::CreateLambda(
            []() { GLevelEditorModeTools().ActivateMode(US7LevelAuthoringMode::mode_id); })),
        FText::FromString("Space Game Level"),
        FText::FromString("Open the S7 level-authoring mode"),
        FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Edit")));

    auto& resource_tools{tool_menus->ExtendMenu("LevelEditor.MainMenu.Tools")
                             ->FindOrAddSection("SandboxTools", LOCTEXT("Tools", "Space Game"))};
    resource_tools.AddMenuEntry(
        "ExportEntityGeometry",
        LOCTEXT("Geometry", "Export Entity Geometry"),
        LOCTEXT("GeometryTip", "Export shared mesh bounds and attachment points."),
        FSlateIcon{},
        FUIAction{FExecuteAction::CreateLambda([] {
            auto const result{
                ioj::editor::export_entity_geometry(ioj::editor::entity_geometry_path())};
            FMessageDialog::Open(EAppMsgType::Ok,
                                 result ? LOCTEXT("SavedGeometry", "Saved shared entity geometry.")
                                        : FText::FromString(result.error()));
        })});
#if UE_BUILD_DEVELOPMENT
    UE_LOG(LogTemp, Log, TEXT("FSandboxEditorModule registered menu extensions"));
#endif
}
#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FSandboxEditorModule, SandboxEditor)
