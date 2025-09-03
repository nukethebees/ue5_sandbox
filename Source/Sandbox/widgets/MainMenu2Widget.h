// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "Sandbox/widgets/TextButtonWidget.h"

#include "MainMenu2Widget.generated.h"

UCLASS()
class SANDBOX_API UMainMenu2Widget : public UUserWidget {
    GENERATED_BODY()
  protected:
    virtual void NativeConstruct() override;

    UPROPERTY(meta = (BindWidget))
    UTextButtonWidget* play_button;
    UPROPERTY(meta = (BindWidget))
    UTextButtonWidget* quit_button;
private:
    UFUNCTION()
    void handle_quit();
};
