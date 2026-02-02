#pragma once

#include "CoreMinimal.h"
#include "Delegates/IDelegateInstance.h"
#include "Modules/ModuleManager.h"

#include "Sandbox/logging/LogMsgMixin.hpp"

class FMenuBarBuilder;
class FMenuBuilder;
class FExtender;
class FUICommandList;
class AActor;

class FSandboxEditorModule
    : public IModuleInterface
    , public ml::LogMsgMixin<"FSandboxEditorModule"> {
  public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
  private:
    void create_sandbox_editor_menus();
    // Editor toolbar menu
    void create_sandbox_editor_toolbar_menu_pulldown(FMenuBarBuilder& menu_bar_builder);
    void create_sandbox_editor_toolbar_menu_items(FMenuBuilder& menu_builder);
    // Editor context menu
    static auto on_extend_level_editor_menu(TSharedRef<FUICommandList> const command_list,
                                            TArray<AActor*> selected_actors)
        -> TSharedRef<FExtender>;
    static void create_sandbox_editor_context_menu_items(FMenuBuilder& menu_builder);
    // Menu Extensions
    void register_menu_extensions();
    void on_generate_typedefs();
    // Custom properties
    void register_custom_properties();
    void unregister_custom_properties();

    FDelegateHandle context_menu_delegate;
};
