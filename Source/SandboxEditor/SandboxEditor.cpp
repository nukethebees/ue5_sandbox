#include "SandboxEditor/SandboxEditor.h"

#include "GenericPlatform/GenericPlatformMisc.h"
#include "SandboxEditor/levels/S7EntityGeometryExport.h"
#include "SandboxEditor/levels/S7LevelAuthoringActors.h"
#include "SandboxEditor/levels/S7LevelAuthoringDocument.h"
#include "SandboxEditor/levels/S7LevelAuthoringMode.h"
#include "SandboxEditor/levels/S7LevelAuthoringSession.h"
#include "SandboxEditor/levels/S7LevelPlayableSetup.h"
#include "SandboxEditor/logging/SandboxEditorLogCategories.h"
#include "SandboxGameShared/utilities/macros/null_checks.hpp"

#include "Containers/UnrealString.h"
#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "Editor/UnrealEdEngine.h"
#include "EditorModeManager.h"
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

static void play_external_mission() {
    auto* const document{current_mission_document()};
    if (!document) {
        UE_LOG(LogS7LevelAuthoring, Warning, TEXT("No mission document in the current level."));
        return;
    }

    FString executable;
    auto const marker{FPaths::ProjectSavedDir() / TEXT("SandboxHost.path")};
    FFileHelper::LoadFileToString(executable, *marker);
    executable.TrimStartAndEndInline();
    if (!FPaths::FileExists(executable)) {
        FMessageDialog::Open(
            EAppMsgType::Ok,
            FText::Format(LOCTEXT("MissingHost",
                                  "Build the stage CMake target to launch SandboxHost. "
                                  "Host location: {0}"),
                          FText::FromString(executable.IsEmpty() ? marker : executable)));
        return;
    }

    auto const stale{s7_scene_needs_export(*document)};
    if (!stale) {
        UE_LOG(LogS7LevelAuthoring, Warning, TEXT("%s"), *stale.error());
    } else if (*stale) {
        UE_LOG(LogS7LevelAuthoring,
               Warning,
               TEXT("Scene edits need export. Playing the saved level script. Use Export Scene."));
    }

    auto const scripts{
        FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("LevelScripts"))};
    auto const args{
        FString::Printf(TEXT("--display-mode windowed --editor-scripts \"%s\" --mission \"%s\""),
                        *scripts,
                        *s7_mission_path(*document))};
    auto const directory{FPaths::GetPath(executable)};

    auto process{FPlatformProcess::CreateProc(
        *executable, *args, true, false, false, nullptr, 0, *directory, nullptr)};
    if (!process.IsValid()) {
        FMessageDialog::Open(EAppMsgType::Ok,
                             FText::Format(LOCTEXT("HostLaunchFailed", "Could not launch {0}"),
                                           FText::FromString(executable)));
        return;
    }
    FPlatformProcess::CloseProc(process);
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
    FEditorDelegates::PreBeginPIE.AddRaw(this, &FSandboxEditorModule::on_pre_begin_pie);
    s7_pie_handle_ = FWorldDelegates::OnWorldInitializedActors.AddLambda(
        [](FActorsInitializedParams const& params) {
            if (params.World->WorldType == EWorldType::PIE) {
                ioj::editor::prepare_s7_scene_for_pie(*params.World);
            }
        });
    constexpr auto logger{NestedLogger<"StartupModule">()};
    logger.log_verbose(TEXT("Module starting up!"));
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
    FEditorDelegates::PreBeginPIE.RemoveAll(this);
    FWorldDelegates::OnWorldInitializedActors.Remove(s7_pie_handle_);
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
        if (!actor || component != actor->GetRootComponent()) {
            return;
        }
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
    auto const resolved{ioj::editor::resolve_s7_level_actor(*actor)};
    if (!resolved || (!resolved->export_to_script && property != TEXT("export_to_script"))) {
        return;
    }
    auto document{ioj::editor::find_level_authoring_document(*actor->GetLevel())};
    if (document && *document) {
        if (property == TEXT("ActorLabel") &&
            (*document)->entities.ContainsByPredicate(
                [actor](auto const& binding) { return binding.actor == actor; })) {
            return;
        }
        (*document)->set_needs_export(true);
    }
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
void FSandboxEditorModule::on_pre_begin_pie(bool) {
    auto* world{GEditor->GetEditorWorldContext().World()};
    if (!world) {
        return;
    }
    for (auto* level : world->GetLevels()) {
        auto document{ioj::editor::find_level_authoring_document(*level)};
        if (document && *document) {
            auto stale{ioj::editor::s7_scene_needs_export(**document)};
            if (!stale) {
                UE_LOG(LogS7LevelAuthoring, Warning, TEXT("%s"), *stale.error());
            } else if (*stale) {
                UE_LOG(LogS7LevelAuthoring,
                       Warning,
                       TEXT("Scene edits need export. Playing the saved level script. Use Export "
                            "Scene."));
            }
        }
    }
}

// Menu Extensions
void FSandboxEditorModule::register_menu_extensions() {
    constexpr auto logger{NestedLogger<"register_menu_extensions">()};

    FToolMenuOwnerScoped owner{this};
    TRY_INIT_PTR(tool_menus, UToolMenus::Get());

    auto* const play_menu{tool_menus->ExtendMenu("LevelEditor.LevelEditorToolBar.PlayToolBar")};
    play_menu->FindOrAddSection("Play").AddEntry(FToolMenuEntry::InitToolBarButton(
        "SandboxPlayExternalMission",
        FUIAction{FExecuteAction::CreateStatic(&ioj::editor::play_external_mission),
                  FCanExecuteAction::CreateLambda(
                      [] { return ioj::editor::current_mission_document() != nullptr; })},
        LOCTEXT("PlayMission", "Play Mission"),
        LOCTEXT("PlayMissionTip", "Run the saved mission with the staged Sandbox host"),
        FSlateIcon(FAppStyle::GetAppStyleSetName(), "PlayWorld.PlayInViewport")));

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
    logger.log_log(TEXT("Registered menu extensions"));
}
#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FSandboxEditorModule, SandboxEditor)
