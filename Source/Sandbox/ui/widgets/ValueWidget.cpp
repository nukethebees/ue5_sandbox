// Fill out your copyright notice in the Description page of Project Settings.

#include "Sandbox/ui/widgets/ValueWidget.h"

bool UValueWidget::Initialize() {
    if (!Super::Initialize()) {
        return false;
    }

    update_format_spec_text();
    return true;
}
void UValueWidget::NativePreConstruct() {
    Super::NativePreConstruct();

    update_format_spec_text();
}
void UValueWidget::NativeConstruct() {
    Super::NativeConstruct();

    update_format_spec_text();
}
void UValueWidget::update_format_spec_text() {
    format_spec_text = FText::FromName(format_spec);
    if (value_text) {
        auto font{value_text->GetFont()};
        font.Size = font_size;
        value_text->SetFont(font);
    }
    if (value_text->GetText().IsEmpty()) {
        value_text->SetText(format_spec_text);
    }
}

void UValueWidget::set_format_spec(FName const new_format_spec) {
    format_spec = new_format_spec;
    update_format_spec_text();
}
