#include "JumpWidget.h"

void UJumpWidget::NativeConstruct() {
    Super::NativeConstruct();
}

void UJumpWidget::update_jump(int new_jump) {
    static auto const fmt{FText::FromStringView(TEXT("Jumps: {0}"))};

    if (jump_text) {
        auto const display{FText::Format(fmt, FText::AsNumber(new_jump))};
        jump_text->SetText(display);
    }
}
