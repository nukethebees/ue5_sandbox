// Copyright Epic Games, Inc. All Rights Reserved.

#include "SandboxEditorTools.h"

#include "SSandboxEditorToolsMainPanel.h"

#include "Framework/Docking/TabManager.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"

#define LOCTEXT_NAMESPACE "FSandboxEditorToolsModule"

void FSandboxEditorToolsModule::StartupModule() {
    auto& menu_entry{
        FGlobalTabmanager::Get()
            ->RegisterNomadTabSpawner(
                tab_name,
                FOnSpawnTab::CreateRaw(this, &FSandboxEditorToolsModule::SpawnMainPanelTab))
            .SetDisplayName(LOCTEXT("SandboxEditorToolsTab", "Sandbox Tools"))
            .SetMenuType(ETabSpawnerMenuType::Enabled)};
    menu_entry.SetGroup(WorkspaceMenu::GetMenuStructure().GetLevelEditorCategory());
}

void FSandboxEditorToolsModule::ShutdownModule() {
    FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(tab_name);
}

auto FSandboxEditorToolsModule::SpawnMainPanelTab(FSpawnTabArgs const&) -> TSharedRef<SDockTab> {
    if (!main_panel.IsValid()) {
        SAssignNew(main_panel, SSandboxEditorToolsMainPanel);
    }

    auto const tab{SNew(SDockTab).TabRole(ETabRole::NomadTab)};
    tab->SetContent(main_panel.ToSharedRef());

    return tab;
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FSandboxEditorToolsModule, SandboxEditorTools)
