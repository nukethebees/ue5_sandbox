#include "Sandbox/widgets/MainMenu2Widget.h"

#include "Kismet/KismetSystemLibrary.h"
#include "Sandbox/utilities/levels.h"

void UMainMenu2Widget::NativeConstruct() {
    Super::NativeConstruct();

    if (play_button) {
        play_button->on_clicked.AddDynamic(this, &UMainMenu2Widget::handle_play);
    }

    if (quit_button) {
        quit_button->on_clicked.AddDynamic(this, &UMainMenu2Widget::handle_quit);
    }

    if (level_select_menu) {
        level_select_menu->back_requested.AddDynamic(this, &UMainMenu2Widget::return_to_main_page);
        static auto const level_directory{FName("/Game/Levels")};
        auto level_names{ml::get_all_level_names(level_directory)};
        level_names.Sort([](auto A, auto B) { return A.LexicalLess(B); });
        level_select_menu->populate_level_buttons(level_names);
    }
}
void UMainMenu2Widget::handle_quit() {
    UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
}
void UMainMenu2Widget::handle_play() {
    set_active_page(EMainMenu2MenuPage::LevelSelect);
}
void UMainMenu2Widget::return_to_main_page() {
    set_active_page(EMainMenu2MenuPage::Main);
}
void UMainMenu2Widget::set_active_page(EMainMenu2MenuPage page) {
    if (widget_switcher) {
        widget_switcher->SetActiveWidgetIndex(static_cast<int32>(page));
    } else {
        UE_LOGFMT(LogTemp, Warning, "widget_switcher is nullptr.");
    }
}
