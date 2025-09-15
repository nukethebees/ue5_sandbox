// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"

#include "AudioOptionsWidget.generated.h"

UCLASS()
class SANDBOX_API UAudioOptionsWidget : public UUserWidget {
    GENERATED_BODY()
  protected:
    virtual void NativeConstruct() override;

    // Placeholder for future audio settings
    // Examples: master volume, music volume, SFX volume, voice volume, etc.
};
