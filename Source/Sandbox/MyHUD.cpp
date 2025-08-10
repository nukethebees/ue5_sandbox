// Fill out your copyright notice in the Description page of Project Settings.

#include "MyHUD.h"
#include "UObject/ConstructorHelpers.h"

template <typename T>
using CFinder = ConstructorHelpers::FClassFinder<T>;

void AMyHUD::BeginPlay() {
    Super::BeginPlay();

    if (fuel_widget_class) {
        fuel_widget = CreateWidget<UFuelWidget>(GetWorld(), fuel_widget_class);
        if (fuel_widget) {
            fuel_widget->AddToViewport();
        }
    } else {
        UE_LOG(LogTemp, Error, TEXT("MyHUD: No fuel widget class."));
    }

    if (jump_widget_class) {
        jump_widget = CreateWidget<UJumpWidget>(GetWorld(), jump_widget_class);
        if (jump_widget) {
            jump_widget->AddToViewport();
        }
    } else {
        UE_LOG(LogTemp, Error, TEXT("MyHUD: No jump widget class."));
    }
}

void AMyHUD::update_fuel(float new_fuel) {
    if (fuel_widget) {
        fuel_widget->update_fuel(new_fuel);
    }
}

void AMyHUD::update_jump(int32 new_jump) {
    if (jump_widget) {
        jump_widget->update_jump(new_jump);
    }
}
