// Fill out your copyright notice in the Description page of Project Settings.

#include "Sandbox/ui/widgets/ValueWidget.h"

void UValueWidget::NativeConstruct() {
    format_spec_text = FText::FromName(format_spec);
    if (value_text) {
        auto font{value_text->GetFont()};
        font.Size = font_size;
        value_text->SetFont(font);
    }
}
