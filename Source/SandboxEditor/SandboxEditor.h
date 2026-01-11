#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

#include "Sandbox/logging/mixins/LogMsgMixin.hpp"

class FMenuBarBuilder;
class FMenuBuilder;

class FSandboxEditorModule
    : public IModuleInterface
    , public ml::LogMsgMixin<"FSandboxEditorModule"> {
  public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
  private:
    // Editor toolbar menu
    void create_sandbox_editor_toolbar_menu();
    void create_sandbox_editor_toolbar_menu_pulldown(FMenuBarBuilder& menu_bar_builder);
    void create_sandbox_editor_toolbar_menu_items(FMenuBuilder& menu_builder);
    // Menu Extensions
    void register_menu_extensions();
    void on_generate_typedefs();
    // Custom properties
    void register_custom_properties();
    void unregister_custom_properties();
};

