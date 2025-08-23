// Fill out your copyright notice in the Description page of Project Settings.

#include "MyHUD.h"
#include "UObject/ConstructorHelpers.h"

template <typename T>
using CFinder = ConstructorHelpers::FClassFinder<T>;

void AMyHUD::BeginPlay() {
    Super::BeginPlay();

    if (main_widget_class) {
        main_widget = CreateWidget<UMainHUDWidget>(GetWorld(), main_widget_class);
        if (main_widget) {
            main_widget->AddToViewport();
        }
    } else {
        UE_LOG(LogTemp, Error, TEXT("MyHUD: No main_widget_class."));
    }
}

void AMyHUD::update_fuel(float new_fuel) {
    if (main_widget) {
        main_widget->update_fuel(new_fuel);
    } else {
        UE_LOG(LogTemp, Error, TEXT("MyHUD: update_fuel no main_widget_class."));
    }
}

void AMyHUD::update_jump(int32 new_jump) {
    if (main_widget) {
        main_widget->update_jump(new_jump);
    } else {
        UE_LOG(LogTemp, Error, TEXT("MyHUD: update_jump no main_widget_class."));
    }
}

void AMyHUD::update_coin(int32 new_coin_count) {
    if (main_widget) {
        main_widget->update_coin(new_coin_count);
    } else {
        UE_LOG(LogTemp, Error, TEXT("MyHUD: update_coin no main_widget_class."));
    }
}
