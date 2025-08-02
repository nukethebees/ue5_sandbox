// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "CoreMinimal.h"

#include "FuelWidget.generated.h"

UCLASS()
class SANDBOX_API UFuelWidget : public UUserWidget {
    GENERATED_BODY()
  public:
    UFUNCTION(BlueprintCallable)
    void update_fuel(float new_fuel);
  protected:
    virtual void NativeConstruct() override;

    UPROPERTY(meta = (BindWidget))
    class UTextBlock* fuel_text;
};
