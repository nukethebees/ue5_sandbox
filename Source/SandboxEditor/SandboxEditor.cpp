#include "SandboxEditor/SandboxEditor.h"

#include "SandboxEditor/levels/S7LevelAuthoringMode.h"
#include "SandboxEditor/levels/S7LevelPlayableSetup.h"
#include "SandboxGameShared/utilities/macros/null_checks.hpp"

#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "EditorModeManager.h"
#include "Misc/CoreDelegates.h"
#include "Modules/ModuleManager.h"
#include "ToolMenus.h"
#include <Engine/World.h>
#define LOCTEXT_NAMESPACE "FSandboxEditorModule"

void FSandboxEditorModule::StartupModule() {
    s7_pie_handle_ = FWorldDelegates::OnWorldInitializedActors.AddLambda(
        [](FActorsInitializedParams const& params) {
            if (params.World->WorldType == EWorldType::PIE) {
                ioj::editor::prepare_s7_scene_for_pie(*params.World);
            }
        });
    constexpr auto logger{NestedLogger<"StartupModule">()};
    logger.log_verbose(TEXT("Module starting up!"));
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
}

// Menu Extensions
void FSandboxEditorModule::register_menu_extensions() {
    constexpr auto logger{NestedLogger<"register_menu_extensions">()};

    TRY_INIT_PTR(tool_menus, UToolMenus::Get());

    TRY_INIT_PTR(menu, tool_menus->ExtendMenu("LevelEditor.LevelEditorToolBar.User"));
    FToolMenuSection& section{menu->AddSection("SandboxTools", FText::FromString("Sandbox Tools"))};

    section.AddEntry(FToolMenuEntry::InitToolBarButton(
        "SpaceGameLevelAuthoring",
        FUIAction(FExecuteAction::CreateLambda(
            []() { GLevelEditorModeTools().ActivateMode(US7LevelAuthoringMode::mode_id); })),
        FText::FromString("Space Game Level"),
        FText::FromString("Open the S7 level-authoring mode"),
        FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Edit")));

    logger.log_log(TEXT("Registered menu extensions"));
}
#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FSandboxEditorModule, SandboxEditor)
