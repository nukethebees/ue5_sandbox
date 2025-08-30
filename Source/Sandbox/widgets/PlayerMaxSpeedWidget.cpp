// Fill out your copyright notice in the Description page of Project Settings.

#include "Sandbox/widgets/PlayerMaxSpeedWidget.h"

#include "Sandbox/widgets/JumpWidget.h"

void UPlayerMaxSpeedWidget::update(float speed) {
    static auto const fmt{FText::FromStringView(TEXT("Max Speed: {0}"))};

    if (text) {
        auto const display{FText::Format(fmt, FText::AsNumber(speed))};
        text->SetText(display);
    }
}
