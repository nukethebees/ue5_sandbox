// Fill out your copyright notice in the Description page of Project Settings.

#include "Sandbox/widgets/ValueWidget.h"

void UValueWidget::NativeConstruct() {
    format_spec_text = FText::FromName(format_spec);
}
