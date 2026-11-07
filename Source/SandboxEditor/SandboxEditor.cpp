#include "SandboxEditor/SandboxEditor.h"

#include "SandboxEditor/levels/S7LevelAuthoringDocument.h"
#include "SandboxEditor/levels/S7LevelAuthoringMode.h"
#include "SandboxEditor/levels/S7LevelAuthoringSession.h"
#include "SandboxEditor/levels/S7LevelPlayableSetup.h"
#include "SandboxEditor/levels/S7LevelScriptEditor.h"
#include "SandboxGameShared/utilities/macros/null_checks.hpp"
#include <SpaceGame/simulation/TestBatchOrchestrator.h>

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
#include <EngineUtils.h>
#include <Features/IModularFeatures.h>
#include <HAL/FileManager.h>

#define LOCTEXT_NAMESPACE "FSandboxEditorModule"

FName const FSandboxEditorModule::s7_level_script_editor_tab_id{
    TEXT("Sandbox.S7LevelScriptEditor")};

void FSandboxEditorModule::StartupModule() {
    IModularFeatures::Get().RegisterModularFeature(IPIEAuthorizer::GetModularFeatureName(), this);
    s7_pie_handle_ = FWorldDelegates::OnWorldInitializedActors.AddLambda(
        [this](FActorsInitializedParams const& params) {
            if (params.World->WorldType != EWorldType::PIE || !prepared_level_) {
                return;
            }
            auto prepared{
                ioj::editor::launch_s7_level_for_pie(*params.World,
                                                     std::move(prepared_level_->definition),
                                                     std::move(prepared_level_->simulation))};
            prepared_level_.reset();
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
    prepared_level_.reset();
    auto* const world{GEditor ? GEditor->GetEditorWorldContext().World() : nullptr};
    if (!IsValid(world)) {
        return MakeValue(true);
    }
    ATestBatchOrchestrator* orchestrator{};
    for (TActorIterator<ATestBatchOrchestrator> it{world}; it; ++it) {
        auto const mode{it->get_start_mode()};
        if (mode != EOrchestratorStartMode::AuthoredLevel &&
            mode != EOrchestratorStartMode::Automatic) {
            continue;
        }
        if (orchestrator) {
            return MakeError(
                FText::FromString(TEXT("Playable maps require one level orchestrator.")));
        }
        orchestrator = *it;
    }
    if (!orchestrator) {
        return MakeValue(true);
    }
    auto document{ioj::editor::find_level_authoring_document(*orchestrator->GetLevel())};
    if (!document) {
        return MakeError(FText::FromString(document.error()));
    }
    if (!*document || !IFileManager::Get().FileExists(*(*document)->source_path)) {
        return MakeError(FText::FromString(
            TEXT("Configure this playable map with an existing Scheme entry point before PIE.")));
    }
    auto exported{ioj::editor::export_s7_editor_config(*orchestrator->GetLevel(), **document)};
    if (!exported) {
        return MakeError(FText::FromString(exported.error()));
    }
    auto definition{ioj::editor::read_s7_authored_level(**document)};
    if (!definition) {
        return MakeError(FText::FromString(definition.error()));
    }
    auto compiled{
        ioj::editor::compile_playable_s7_definition(*definition, *(*document)->level_config)};
    if (!compiled) {
        return MakeError(FText::FromString(compiled.error()));
    }
    prepared_level_.emplace(std::move(*definition), std::move(*compiled));
    return MakeValue(true);
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
