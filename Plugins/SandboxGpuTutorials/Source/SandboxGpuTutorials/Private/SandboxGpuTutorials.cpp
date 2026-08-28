#include "Showcase/SSandboxGpuTutorialsShowcase.h"

#include "Framework/Commands/UIAction.h"
#include "Framework/Docking/TabManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "ShaderCore.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"

#define LOCTEXT_NAMESPACE "FSandboxGpuTutorialsModule"

namespace {
FName const showcase_tab_name{TEXT("SandboxGpuTutorials.Showcase")};
}

class FSandboxGpuTutorialsModule final : public IModuleInterface {
  public:
    using ThisClass = FSandboxGpuTutorialsModule;

    void StartupModule() override {
        auto const plugin{IPluginManager::Get().FindPlugin(TEXT("SandboxGpuTutorials"))};
        check(plugin.IsValid());

        auto const shader_directory{FPaths::Combine(plugin->GetBaseDir(), TEXT("Shaders"))};
        FString const virtual_shader_directory{TEXT("/Plugin/SandboxGpuTutorials")};
        auto const& shader_mappings{AllShaderSourceDirectoryMappings()};
        if (auto const* existing_directory{shader_mappings.Find(virtual_shader_directory)}) {
            ensureMsgf(FPaths::IsSamePath(*existing_directory, shader_directory),
                       TEXT("%s is already mapped to '%s', expected '%s'."),
                       *virtual_shader_directory,
                       **existing_directory,
                       *shader_directory);
        } else {
            AddShaderSourceDirectoryMapping(virtual_shader_directory, shader_directory);
        }

        FGlobalTabmanager::Get()
            ->RegisterNomadTabSpawner(showcase_tab_name,
                                      FOnSpawnTab::CreateRaw(this, &ThisClass::spawn_showcase_tab))
            .SetDisplayName(LOCTEXT("ShowcaseTab", "Sandbox GPU Tutorials"))
            .SetTooltipText(LOCTEXT("ShowcaseTabTooltip", "Open the GPU UI tutorial showcase."))
            .SetGroup(WorkspaceMenu::GetMenuStructure().GetDeveloperToolsMiscCategory());

        UToolMenus::RegisterStartupCallback(
            FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &ThisClass::register_menus));
    }

    void ShutdownModule() override {
        UToolMenus::UnRegisterStartupCallback(this);
        UToolMenus::UnregisterOwner(this);
        FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(showcase_tab_name);
    }
  private:
    void register_menus() {
        FToolMenuOwnerScoped const owner_scope{this};
        auto* const menu{UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.MainMenu.Tools"))};
        auto& section{menu->FindOrAddSection(TEXT("Programming"))};
        section.AddMenuEntry(TEXT("SandboxGpuTutorialsShowcase"),
                             LOCTEXT("OpenShowcase", "Sandbox GPU Tutorials"),
                             LOCTEXT("OpenShowcaseTooltip", "Open the GPU UI tutorial showcase."),
                             FSlateIcon{},
                             FUIAction{FExecuteAction::CreateLambda([]() {
                                 FGlobalTabmanager::Get()->TryInvokeTab(showcase_tab_name);
                             })});
    }

    auto spawn_showcase_tab(FSpawnTabArgs const&) -> TSharedRef<SDockTab> {
        return SNew(SDockTab).TabRole(ETabRole::NomadTab)[SNew(SSandboxGpuTutorialsShowcase)];
    }
};

IMPLEMENT_MODULE(FSandboxGpuTutorialsModule, SandboxGpuTutorials)

#undef LOCTEXT_NAMESPACE
