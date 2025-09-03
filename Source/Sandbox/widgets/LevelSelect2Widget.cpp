#include "Sandbox/widgets/LevelSelect2Widget.h"

void ULevelSelect2Widget::NativeConstruct() {
    Super::NativeConstruct();

    if (back_button) {
        back_button->on_clicked.AddDynamic(this, &ULevelSelect2Widget::handle_back);
    }
}
void ULevelSelect2Widget::handle_back() {
    back_requested.Broadcast();
}
