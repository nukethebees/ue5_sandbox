// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "GameplayOptionsWidget.generated.h"

UCLASS()
class SANDBOX_API UGameplayOptionsWidget : public UUserWidget {
    GENERATED_BODY()
  protected:
    virtual void NativeConstruct() override;

    // Placeholder for future gameplay settings
    // Examples: difficulty, auto-save frequency, subtitles, etc.
};
