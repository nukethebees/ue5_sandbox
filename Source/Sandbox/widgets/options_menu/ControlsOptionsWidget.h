// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"

#include "ControlsOptionsWidget.generated.h"

UCLASS()
class SANDBOX_API UControlsOptionsWidget : public UUserWidget {
    GENERATED_BODY()
  protected:
    virtual void NativeConstruct() override;

    // Placeholder for future control settings
    // Examples: key bindings, mouse sensitivity, invert mouse, gamepad settings, etc.
};
