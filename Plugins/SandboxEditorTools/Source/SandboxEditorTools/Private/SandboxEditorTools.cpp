// Copyright Epic Games, Inc. All Rights Reserved.

#include "SandboxEditorTools.h"

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
            .SetDisplayName(LOCTEXT("Sandbox Tools", "Sandbox editor tools"))
            .SetMenuType(ETabSpawnerMenuType::Enabled)};
    menu_entry.SetGroup(WorkspaceMenu::GetMenuStructure().GetLevelEditorCategory());
}

void FSandboxEditorToolsModule::ShutdownModule() {
    FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(tab_name);
}

auto FSandboxEditorToolsModule::SpawnMainPanelTab(FSpawnTabArgs const&) -> TSharedRef<SDockTab> {
    // clang-format off
    main_panel = 
        SNew(SDockTab)
        .TabRole(ETabRole::NomadTab)
        [
            SNew(STextBlock).Text(FText::FromString("Hello Editor"))
        ];
    // clang-format on

    return main_panel.ToSharedRef();
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FSandboxEditorToolsModule, SandboxEditorTools)
