#include "Sandbox/ui/widgets/umg/TextButtonWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"

void UTextButtonWidget::NativeConstruct() {
    Super::NativeConstruct();

    if (button_widget) {
        button_widget->OnClicked.AddDynamic(this, &UTextButtonWidget::handle_click);
    }

    if (text_block) {
        text_block->SetVisibility(ESlateVisibility::HitTestInvisible);
    }

    set_label();
}
void UTextButtonWidget::SynchronizeProperties() {
    Super::SynchronizeProperties();
    set_label();
}
void UTextButtonWidget::handle_click() {
    on_clicked.Broadcast();
}
void UTextButtonWidget::set_label(FText const& txt) {
    label = txt;
    set_label();
}
void UTextButtonWidget::set_label() {
    if (text_block) {
        text_block->SetText(label);
    }
}
