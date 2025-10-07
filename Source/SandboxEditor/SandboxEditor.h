#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FSandboxEditorModule : public IModuleInterface {
  public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
  private:
    void register_menu_extensions();
    void on_generate_data_asset_code();
};
