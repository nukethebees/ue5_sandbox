#include "SandboxEditor/SandboxEditor.h"

#include "SandboxEditor/levels/S7LevelAuthoringMode.h"
#include "SandboxEditor/levels/S7LevelPlayableSetup.h"
#include "SandboxEditor/levels/S7LevelScriptEditor.h"
#include "SandboxGameShared/utilities/macros/null_checks.hpp"

#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "EditorModeManager.h"
#include "Framework/Docking/TabManager.h"
#include "Misc/CoreDelegates.h"
#include "Modules/ModuleManager.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"
#include <Engine/World.h>
#define LOCTEXT_NAMESPACE "FSandboxEditorModule"

FName const FSandboxEditorModule::s7_level_script_editor_tab_id{
    TEXT("Sandbox.S7LevelScriptEditor")};

void FSandboxEditorModule::StartupModule() {
    s7_pie_handle_ = FWorldDelegates::OnWorldInitializedActors.AddLambda(
        [](FActorsInitializedParams const& params) {
            if (params.World->WorldType == EWorldType::PIE) {
                ioj::editor::prepare_s7_scene_for_pie(*params.World);
            }
        });
    constexpr auto logger{NestedLogger<"StartupModule">()};
    logger.log_verbose(TEXT("Module starting up!"));
    // No factory registration needed - MaterialExpressions control their own UI

    FGlobalTabmanager::Get()
        ->RegisterNomadTabSpawner(
            s7_level_script_editor_tab_id,
            FOnSpawnTab::CreateRaw(this, &FSandboxEditorModule::spawn_s7_level_script_editor))
        .SetDisplayName(LOCTEXT("S7LevelScriptEditor", "S7 Script Editor"))
        .SetTooltipText(
            LOCTEXT("S7LevelScriptEditorTooltip", "Edit the active Space Game Level S7 source."))
        .SetGroup(WorkspaceMenu::GetMenuStructure().GetLevelEditorCategory());

    // Register menu extensions after ToolMenus module is loaded
    if (UToolMenus::IsToolMenuUIEnabled()) {
        register_menu_extensions();
    } else {
        // Delay registration until ToolMenus is ready
        FCoreDelegates::GetOnPostEngineInit().AddLambda([this]() {
            if (UToolMenus::IsToolMenuUIEnabled()) {
                register_menu_extensions();
            }
        });
    }
}
void FSandboxEditorModule::ShutdownModule() {
    FWorldDelegates::OnWorldInitializedActors.Remove(s7_pie_handle_);
    if (IsValid(GEditor) && GEditor->PlayWorld != nullptr) {
        GEditor->EndPlayMap();
    }

    if (auto script_editor{s7_level_script_editor_.Pin()}) {
        script_editor->release_mode();
    }
    s7_level_script_editor_.Reset();
    FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(s7_level_script_editor_tab_id);
}

auto FSandboxEditorModule::open_s7_level_script_editor() -> TSharedPtr<SDockTab> {
    return FGlobalTabmanager::Get()->TryInvokeTab(s7_level_script_editor_tab_id);
}

auto FSandboxEditorModule::spawn_s7_level_script_editor(FSpawnTabArgs const&)
    -> TSharedRef<SDockTab> {
    auto script_editor{SNew(SS7LevelScriptEditor)};
    s7_level_script_editor_ = script_editor;
    return SNew(SDockTab).TabRole(ETabRole::NomadTab)[script_editor];
}

// Menu Extensions
void FSandboxEditorModule::register_menu_extensions() {
    constexpr auto logger{NestedLogger<"register_menu_extensions">()};

    TRY_INIT_PTR(tool_menus, UToolMenus::Get());

    TRY_INIT_PTR(menu, tool_menus->ExtendMenu("LevelEditor.LevelEditorToolBar.User"));
    FToolMenuSection& section{menu->AddSection("SandboxTools", FText::FromString("Sandbox Tools"))};

    section.AddEntry(FToolMenuEntry::InitToolBarButton(
        "SpaceGameLevelAuthoring",
        FUIAction(FExecuteAction::CreateLambda([]() {
            GLevelEditorModeTools().ActivateMode(US7LevelAuthoringMode::mode_id);
            FSandboxEditorModule::open_s7_level_script_editor();
        })),
        FText::FromString("Space Game Level"),
        FText::FromString("Open the S7 level-authoring mode"),
        FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Edit")));

    logger.log_log(TEXT("Registered menu extensions"));
}
#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FSandboxEditorModule, SandboxEditor)
