// Fill out your copyright notice in the Description page of Project Settings.

#include "Sandbox/widgets/MainHUDWidget.h"

void UMainHUDWidget::update_fuel(float new_fuel) {
    if (fuel_widget) {
        fuel_widget->update_fuel(new_fuel);
    } else {
        UE_LOG(LogTemp, Error, TEXT("UMainHUDWidget: update_fuel no fuel_widget."));
    }
}

void UMainHUDWidget::update_jump(int32 new_jump) {
    if (jump_widget) {
        jump_widget->update_jump(new_jump);
    } else {
        UE_LOG(LogTemp, Error, TEXT("UMainHUDWidget: update_jump no jump_widget."));
    }
}
