#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

#include "Sandbox/mixins/log_msg_mixin.hpp"

class FSandboxEditorModule
    : public IModuleInterface
    , public ml::LogMsgMixin<"FSandboxEditorModule"> {
  public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
  private:
    void register_menu_extensions();
    void on_generate_data_asset_code();
    void on_generate_typedefs();
};
