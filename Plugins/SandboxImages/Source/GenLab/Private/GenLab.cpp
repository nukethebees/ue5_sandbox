#include "Generation/LabImageWriter.h"

#include "Framework/Notifications/NotificationManager.h"
#include "HAL/IConsoleManager.h"
#include "Modules/ModuleManager.h"
#include "Styling/AppStyle.h"
#include "ToolMenus.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "FGenLabModule"

class FGenLabModule final : public IModuleInterface {
  public:
    void StartupModule() override {
        generate_images_command_ = IConsoleManager::Get().RegisterConsoleCommand(
            TEXT("SandboxImages.GenerateLabImages"),
            TEXT("Regenerates the proof-of-concept PNGs in the SandboxImages plugin content."),
            FConsoleCommandDelegate::CreateStatic([]() {
                auto const success{SandboxImages::GenLab::regenerate_all()};
                static_cast<void>(success);
            }),
            ECVF_Default);

        UToolMenus::RegisterStartupCallback(
            FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FGenLabModule::register_menus));
    }

    void ShutdownModule() override {
        UToolMenus::UnRegisterStartupCallback(this);
        UToolMenus::UnregisterOwner(this);

        if (generate_images_command_ != nullptr) {
            IConsoleManager::Get().UnregisterConsoleObject(generate_images_command_);
            generate_images_command_ = nullptr;
        }
    }
  private:
    void register_menus() {
        FToolMenuOwnerScoped const owner_scope{this};
        auto const action{
            FUIAction{FExecuteAction::CreateRaw(this, &FGenLabModule::generate_images)}};
        auto const icon{FSlateIcon{FAppStyle::GetAppStyleSetName(), TEXT("Icons.Refresh")}};

        auto* const toolbar{
            UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.LevelEditorToolBar.User"))};
        auto& toolbar_section{
            toolbar->FindOrAddSection(TEXT("SandboxImages"), LOCTEXT("Section", "Sandbox Images"))};
        toolbar_section.AddEntry(
            FToolMenuEntry::InitToolBarButton(TEXT("GenerateSandboxImages"),
                                              action,
                                              LOCTEXT("GenerateButton", "Generate Lab Images"),
                                              LOCTEXT("GenerateButtonTooltip",
                                                      "Regenerate the procedural PNGs in "
                                                      "Plugins/SandboxImages/Content/Lab/Images."),
                                              icon));

        auto* const tools_menu{UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.MainMenu.Tools"))};
        auto& tools_section{tools_menu->FindOrAddSection(TEXT("SandboxImages"),
                                                         LOCTEXT("Section", "Sandbox Images"))};
        tools_section.AddMenuEntry(TEXT("GenerateSandboxImages"),
                                   LOCTEXT("GenerateMenuItem", "Generate Sandbox Lab Images"),
                                   LOCTEXT("GenerateMenuItemTooltip",
                                           "Regenerate the procedural PNGs in "
                                           "Plugins/SandboxImages/Content/Lab/Images."),
                                   icon,
                                   action);
    }

    void generate_images() {
        auto const success{SandboxImages::GenLab::regenerate_all()};
        FNotificationInfo notification{
            success ? LOCTEXT("GenerateSucceeded", "Sandbox Lab images regenerated.")
                    : LOCTEXT("GenerateFailed",
                              "Sandbox Lab image generation failed. See Output Log.")};
        notification.ExpireDuration = 4.0f;
        notification.Image = FAppStyle::GetBrush(success ? TEXT("Icons.SuccessWithColor")
                                                         : TEXT("Icons.ErrorWithColor"));
        FSlateNotificationManager::Get().AddNotification(notification);
    }

    IConsoleObject* generate_images_command_{};
};

IMPLEMENT_MODULE(FGenLabModule, GenLab)

#undef LOCTEXT_NAMESPACE
