#include "Sandbox/widgets/MainMenuWidget.h"

bool UMainMenuWidget::Initialize() {
    if (!Super::Initialize()) {
        return false;
    }

    if (IsDesignTime()) {
        return true;
    }

    check(front_menu_widget);
    check(level_select_widget);

    front_menu_widget->parent_menu = this;
    level_select_widget->parent_menu = this;

    show_front_menu();

    return true;
}
void UMainMenuWidget::hide_all_widgets() {
    front_menu_widget->SetVisibility(ESlateVisibility::Collapsed);
    level_select_widget->SetVisibility(ESlateVisibility::Collapsed);
}
void UMainMenuWidget::show_front_menu() {
    show_menu(front_menu_widget);
}
void UMainMenuWidget::show_level_select_menu() {
    show_menu(level_select_widget);
}
