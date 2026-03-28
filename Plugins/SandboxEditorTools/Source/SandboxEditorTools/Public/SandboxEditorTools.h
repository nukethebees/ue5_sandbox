// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

class SSandboxEditorToolsMainPanel;
class UEdToolsData;

class FSandboxEditorToolsModule : public IModuleInterface {
  public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
  protected:
    auto SpawnMainPanelTab(FSpawnTabArgs const& args) -> TSharedRef<SDockTab>;

    inline static FName tab_name{TEXT("SandboxEditorTabPanel")};

    TSharedPtr<SSandboxEditorToolsMainPanel> main_panel{nullptr};
    TSharedPtr<UEdToolsData> tool_data{nullptr};
};
