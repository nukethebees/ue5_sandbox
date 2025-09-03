#include "Sandbox/widgets/TextButtonWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"

void UTextButtonWidget::NativeConstruct() {
    Super::NativeConstruct();

    if (button) {
        button->OnClicked.AddDynamic(this, &UTextButtonWidget::handle_click);
    }

    if (text_block) {
        text_block->SetText(label);
    }
}

void UTextButtonWidget::handle_click() {
    on_clicked.Broadcast();
}
