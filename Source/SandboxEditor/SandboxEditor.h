#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

#include "Sandbox/logging/mixins/LogMsgMixin.hpp"

class FSandboxEditorModule
    : public IModuleInterface
    , public ml::LogMsgMixin<"FSandboxEditorModule"> {
  public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
  private:
    void register_menu_extensions();
    void register_custom_properties();
    void unregister_custom_properties();
    void on_generate_typedefs();
};
