#include "Editor.h"
#include "LevelEditorSubsystem.h"
#include "Modules/ModuleManager.h"
#include "ToolMenus.h"

DEFINE_LOG_CATEGORY_STATIC(LogSandboxShadersEditor, Log, All);

namespace {
void open_showcase() {
    if (GEditor == nullptr) {
        UE_LOG(LogSandboxShadersEditor, Error, TEXT("Cannot open the showcase without GEditor."));
        return;
    }

    auto* const level_editor{GEditor->GetEditorSubsystem<ULevelEditorSubsystem>()};
    if (level_editor == nullptr ||
        !level_editor->LoadLevel(TEXT("/SandboxShaders/Showcase/SandboxShaders_Showcase"))) {
        UE_LOG(LogSandboxShadersEditor,
               Error,
               TEXT("Could not open /SandboxShaders/Showcase/SandboxShaders_Showcase."));
    }
}
}

class FSandboxShadersEditorModule final : public IModuleInterface {
  public:
    void StartupModule() override {
        UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(
            this, &FSandboxShadersEditorModule::register_menus));
    }

    void ShutdownModule() override {
        UToolMenus::UnRegisterStartupCallback(this);
        UToolMenus::UnregisterOwner(this);
    }
  private:
    void register_menus() {
        FToolMenuOwnerScoped const owner{this};
        auto* const menu{UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.MainMenu.Window"))};
        auto& section{menu->FindOrAddSection(TEXT("SandboxShaders"))};
        section.Label = NSLOCTEXT("SandboxShaders", "MenuSection", "Sandbox Shaders");
        section.AddMenuEntry(TEXT("OpenSandboxShadersShowcase"),
                             NSLOCTEXT("SandboxShaders", "OpenShowcase", "Open Shader Showcase"),
                             NSLOCTEXT("SandboxShaders",
                                       "OpenShowcaseTooltip",
                                       "Open the SandboxShaders experiment showcase map."),
                             FSlateIcon(),
                             FUIAction{FExecuteAction::CreateStatic(&open_showcase)});
    }
};

IMPLEMENT_MODULE(FSandboxShadersEditorModule, SandboxShadersEditor)
