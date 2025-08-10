// Fill out your copyright notice in the Description page of Project Settings.

#include "MyHUD.h"
#include "UObject/ConstructorHelpers.h"

template <typename T>
using CFinder = ConstructorHelpers::FClassFinder<T>;

AMyHUD::AMyHUD() {
    static constexpr auto fuel_widget_path{TEXT("/Sandbox/widgets/fuel_widget")};
    static CFinder<UFuelWidget> fuel_widget_object(fuel_widget_path);
    if (fuel_widget_object.Succeeded()) {
        fuel_widget_class = fuel_widget_object.Class;
    }

    static constexpr auto jump_widget_path{TEXT("/Sandbox/widgets/jump_widget")};
    static CFinder<UJumpWidget> jump_widget_obj(jump_widget_path);
    if (jump_widget_obj.Succeeded()) {
        jump_widget_class = jump_widget_obj.Class;
    }
}

void AMyHUD::BeginPlay() {
    Super::BeginPlay();

    if (fuel_widget_class) {
        fuel_widget = CreateWidget<UFuelWidget>(GetWorld(), fuel_widget_class);
        if (fuel_widget) {
            fuel_widget->AddToViewport();
        }
    }

    if (jump_widget_class) {
        jump_widget = CreateWidget<UJumpWidget>(GetWorld(), jump_widget_class);
        if (jump_widget) {
            jump_widget->AddToViewport();
        }
    }
}
