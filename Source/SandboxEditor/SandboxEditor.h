#pragma once

#include "SandboxGameShared/logging/LogMsgMixin.hpp"

#include "CoreMinimal.h"
#include "Delegates/IDelegateInstance.h"
#include "Modules/ModuleManager.h"

class SANDBOXEDITOR_API FSandboxEditorModule
    : public IModuleInterface
    , public ioj::LogMsgMixin<"FSandboxEditorModule"> {
  public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
  private:
    void register_menu_extensions();

    FDelegateHandle s7_pie_handle_{};
};
