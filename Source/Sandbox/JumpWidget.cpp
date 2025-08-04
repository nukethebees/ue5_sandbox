#include "JumpWidget.h"

void UJumpWidget::NativeConstruct() {
    Super::NativeConstruct();
}

void UJumpWidget::update_jump(int32 new_jump) {
    static auto const fmt{FText::FromStringView(TEXT("Jumps: {0}"))};

    if (jump_text) {
        if (previous_jump != new_jump) {
            previous_jump = new_jump;
            auto const display{FText::Format(fmt, FText::AsNumber(new_jump))};
            jump_text->SetText(display);
        }
    }
}
