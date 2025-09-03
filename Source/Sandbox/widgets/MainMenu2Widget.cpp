#include "Sandbox/widgets/MainMenu2Widget.h"

#include "Kismet/KismetSystemLibrary.h"

void UMainMenu2Widget::NativeConstruct() {
    Super::NativeConstruct();

    if (quit_button) {
        quit_button->on_clicked.AddDynamic(this, &UMainMenu2Widget::handle_quit);
    }
}
void UMainMenu2Widget::handle_quit() {
    UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
}
