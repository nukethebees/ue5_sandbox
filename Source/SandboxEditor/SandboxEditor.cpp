#include "SandboxEditor/SandboxEditor.h"

#include "SandboxEditor/levels/S7LevelAuthoringDocument.h"
#include "SandboxEditor/levels/S7LevelAuthoringMode.h"
#include "SandboxEditor/levels/S7LevelAuthoringSession.h"
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
#include <Features/IModularFeatures.h>

#define LOCTEXT_NAMESPACE "FSandboxEditorModule"

FName const FSandboxEditorModule::s7_level_script_editor_tab_id{
    TEXT("Sandbox.S7LevelScriptEditor")};

void FSandboxEditorModule::StartupModule() {
    IModularFeatures::Get().RegisterModularFeature(IPIEAuthorizer::GetModularFeatureName(), this);
    s7_pie_handle_ = FWorldDelegates::OnWorldInitializedActors.AddLambda(
        [](FActorsInitializedParams const& params) {
            auto const prepared{ioj::editor::prepare_s7_level_for_pie(*params.World)};
            if (!prepared) {
                UE_LOG(LogTemp, Error, TEXT("S7 PIE preparation failed: %s"), *prepared.error());
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
    IModularFeatures::Get().UnregisterModularFeature(IPIEAuthorizer::GetModularFeatureName(), this);
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

auto FSandboxEditorModule::IsPIEAuthorizedInternal(bool) const -> TValueOrError<bool, FText> {
    return MakeValue(true);
}

auto FSandboxEditorModule::RequestPIEPermissionInternal(bool) const -> TValueOrError<bool, FText> {
    auto* mode{Cast<US7LevelAuthoringMode>(
        GLevelEditorModeTools().GetActiveScriptableMode(US7LevelAuthoringMode::mode_id))};
    if (!mode) {
        auto* const world{GEditor ? GEditor->GetEditorWorldContext().World() : nullptr};
        auto* const level{IsValid(world) ? world->GetCurrentLevel() : nullptr};
        if (!IsValid(level)) {
            return MakeValue(true);
        }
        auto const document{ioj::editor::find_level_authoring_document(*level)};
        if (!document) {
            return MakeError(FText::FromString(document.error()));
        }
        if (!*document || (*document)->source_path.IsEmpty()) {
            return MakeValue(true);
        }
        GLevelEditorModeTools().ActivateMode(US7LevelAuthoringMode::mode_id);
        mode = Cast<US7LevelAuthoringMode>(
            GLevelEditorModeTools().GetActiveScriptableMode(US7LevelAuthoringMode::mode_id));
        if (!mode) {
            return MakeError(LOCTEXT("S7ReplayUnavailable",
                                     "Could not open the S7 authoring document before play."));
        }
    }
    return MakeValue(mode->prepare_for_play());
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
        FText::FromString("Open the bidirectional S7 level-authoring mode"),
        FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Edit")));

    logger.log_log(TEXT("Registered menu extensions"));
}
#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FSandboxEditorModule, SandboxEditor)
