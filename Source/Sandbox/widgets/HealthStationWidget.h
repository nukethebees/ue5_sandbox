// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HealthStationWidget.generated.h"

class UTextBlock;
class UProgressBar;
struct FStationStateData;

UCLASS()
class SANDBOX_API UHealthStationWidget : public UUserWidget {
    GENERATED_BODY()
  public:
    UFUNCTION(BlueprintCallable, Category = "Health Station")
    void update(FStationStateData const & state_data);
  protected:
    UPROPERTY(meta = (BindWidget))
    UTextBlock* capacity_text;

    UPROPERTY(meta = (BindWidget))
    UProgressBar* cooldown_bar;
};
