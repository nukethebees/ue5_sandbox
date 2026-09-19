#pragma once

#include "CoreMinimal.h"
#include "Delegates/IDelegateInstance.h"
#include "Modules/ModuleManager.h"

#include "SandboxGameShared/logging/LogMsgMixin.hpp"

class FMenuBarBuilder;
class FMenuBuilder;
class FExtender;
class FUICommandList;
class AActor;
class FPropertyEditorModule;
class FSpawnTabArgs;
class SDockTab;
class SS7LevelScriptEditor;

class SANDBOXEDITOR_API FSandboxEditorModule
    : public IModuleInterface
    , public ml::LogMsgMixin<"FSandboxEditorModule"> {
  public:
    static FName const s7_level_script_editor_tab_id;

    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
    static auto open_s7_level_script_editor() -> TSharedPtr<SDockTab>;
  private:
    auto spawn_s7_level_script_editor(FSpawnTabArgs const& arguments) -> TSharedRef<SDockTab>;
    void create_sandbox_editor_menus();

    // Editor toolbar menu
    void create_sandbox_editor_toolbar_menu_pulldown(FMenuBarBuilder& menu_bar_builder);
    void create_sandbox_editor_toolbar_menu_items(FMenuBuilder& menu_builder);

    // Editor context menu
    static auto on_extend_level_editor_menu(TSharedRef<FUICommandList> const command_list,
                                            TArray<AActor*> selected_actors)
        -> TSharedRef<FExtender>;
    // Menu Extensions
    void register_menu_extensions();
    void on_generate_typedefs();

    // Custom properties
    void register_custom_properties();
    void unregister_custom_properties();

    FDelegateHandle context_menu_delegate;
    TArray<FName> registered_properties;
    TArray<FName> registered_class_layouts;
    TWeakPtr<SS7LevelScriptEditor> s7_level_script_editor_{};
};
