#include "Sandbox/widgets/MainMenuFrontWidget.h"

#include "Kismet/KismetSystemLibrary.h"
#include "Sandbox/widgets/MainMenuWidget.h"

bool UMainMenuFrontWidget::Initialize() {
    if (!Super::Initialize()) {
        return false;
    }

    UE_LOG(LogTemp, Display, TEXT("Checking UMainMenuFrontWidget pointers"));
    check(play_button);
    check(quit_button);

    UE_LOG(LogTemp, Display, TEXT("Binding UMainMenuFrontWidget actions."));
    play_button->OnClicked.AddDynamic(this, &UMainMenuFrontWidget::on_play_button_clicked);
    quit_button->OnClicked.AddDynamic(this, &UMainMenuFrontWidget::on_quit_button_clicked);

    return true;
}
void UMainMenuFrontWidget::on_play_button_clicked() {
    if (parent_menu) {
        parent_menu->show_level_select_menu();
    }
}
void UMainMenuFrontWidget::on_quit_button_clicked() {
    UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
}
