// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"

#include "VisualsOptionsWidget.generated.h"

UCLASS()
class SANDBOX_API UVisualsOptionsWidget : public UUserWidget {
    GENERATED_BODY()
  protected:
    virtual void NativeConstruct() override;

    // Placeholder for future visual settings
    // Examples: resolution, fullscreen, graphics quality, FOV, etc.
};