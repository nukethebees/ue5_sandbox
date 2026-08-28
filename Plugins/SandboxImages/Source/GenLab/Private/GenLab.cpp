#include "Editor/SGenLab.h"
#include "Generation/LabImageWriter.h"

#include "Framework/Docking/TabManager.h"
#include "HAL/IConsoleManager.h"
#include "Modules/ModuleManager.h"
#include "Styling/AppStyle.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"

#define LOCTEXT_NAMESPACE "FGenLabModule"

namespace {
FName const image_lab_tab_name{TEXT("SandboxImages.GenLab")};
}

class FGenLabModule final : public IModuleInterface {
  public:
    using ThisClass = FGenLabModule;

    void StartupModule() override {
        generate_images_command_ = IConsoleManager::Get().RegisterConsoleCommand(
            TEXT("SandboxImages.GenerateLabImages"),
            TEXT("Regenerates the proof-of-concept PNGs in the SandboxImages plugin content."),
            FConsoleCommandDelegate::CreateStatic([]() {
                auto const success{SandboxImages::GenLab::regenerate_all()};
                static_cast<void>(success);
            }),
            ECVF_Default);

        FGlobalTabmanager::Get()
            ->RegisterNomadTabSpawner(image_lab_tab_name,
                                      FOnSpawnTab::CreateRaw(this, &ThisClass::spawn_image_lab_tab))
            .SetDisplayName(LOCTEXT("ImageLabTab", "Sandbox Image Lab"))
            .SetTooltipText(LOCTEXT("ImageLabTabTooltip",
                                    "Experiment with and generate procedural technical textures."))
            .SetGroup(WorkspaceMenu::GetMenuStructure().GetDeveloperToolsMiscCategory());

        UToolMenus::RegisterStartupCallback(
            FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FGenLabModule::register_menus));
    }

    void ShutdownModule() override {
        UToolMenus::UnRegisterStartupCallback(this);
        UToolMenus::UnregisterOwner(this);
        FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(image_lab_tab_name);

        if (generate_images_command_ != nullptr) {
            IConsoleManager::Get().UnregisterConsoleObject(generate_images_command_);
            generate_images_command_ = nullptr;
        }
    }
  private:
    void register_menus() {
        FToolMenuOwnerScoped const owner_scope{this};
        auto const action{FUIAction{FExecuteAction::CreateRaw(this, &ThisClass::open_image_lab)}};
        auto const icon{FSlateIcon{FAppStyle::GetAppStyleSetName(), TEXT("Icons.Image")}};

        auto* const toolbar{
            UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.LevelEditorToolBar.User"))};
        auto& toolbar_section{
            toolbar->FindOrAddSection(TEXT("SandboxImages"), LOCTEXT("Section", "Sandbox Images"))};
        toolbar_section.AddEntry(FToolMenuEntry::InitToolBarButton(
            TEXT("GenerateSandboxImages"),
            action,
            LOCTEXT("GenerateButton", "Image Lab"),
            LOCTEXT("GenerateButtonTooltip", "Open the procedural image Lab."),
            icon));

        auto* const tools_menu{UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.MainMenu.Tools"))};
        auto& tools_section{tools_menu->FindOrAddSection(TEXT("SandboxImages"),
                                                         LOCTEXT("Section", "Sandbox Images"))};
        tools_section.AddMenuEntry(
            TEXT("GenerateSandboxImages"),
            LOCTEXT("GenerateMenuItem", "Sandbox Image Lab"),
            LOCTEXT("GenerateMenuItemTooltip", "Open the procedural image Lab."),
            icon,
            action);
    }

    void open_image_lab() { FGlobalTabmanager::Get()->TryInvokeTab(image_lab_tab_name); }

    auto spawn_image_lab_tab(FSpawnTabArgs const&) -> TSharedRef<SDockTab> {
        return SNew(SDockTab).TabRole(ETabRole::NomadTab)[SNew(SGenLab)];
    }

    IConsoleObject* generate_images_command_{};
};

IMPLEMENT_MODULE(FGenLabModule, GenLab)

#undef LOCTEXT_NAMESPACE
