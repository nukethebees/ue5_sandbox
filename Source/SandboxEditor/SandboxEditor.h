#pragma once

#include "SandboxGameShared/logging/LogMsgMixin.hpp"

#include "CoreMinimal.h"
#include "Delegates/IDelegateInstance.h"
#include "Modules/ModuleManager.h"
#include <IPIEAuthorizer.h>

class FSpawnTabArgs;
class SDockTab;
class SS7LevelScriptEditor;

class SANDBOXEDITOR_API FSandboxEditorModule
    : public IModuleInterface
    , public IPIEAuthorizer
    , public ioj::LogMsgMixin<"FSandboxEditorModule"> {
  public:
    static FName const s7_level_script_editor_tab_id;

    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
    static auto open_s7_level_script_editor() -> TSharedPtr<SDockTab>;
  private:
    auto IsPIEAuthorizedInternal(bool simulate) const -> TValueOrError<bool, FText> override;
    auto RequestPIEPermissionInternal(bool simulate) const -> TValueOrError<bool, FText> override;

    auto spawn_s7_level_script_editor(FSpawnTabArgs const& arguments) -> TSharedRef<SDockTab>;
    void register_menu_extensions();

    FDelegateHandle s7_pie_handle_{};
    TWeakPtr<SS7LevelScriptEditor> s7_level_script_editor_{};
};
