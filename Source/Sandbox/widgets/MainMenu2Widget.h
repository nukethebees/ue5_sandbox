// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Blueprint/UserWidget.h"
#include "Components/WidgetSwitcher.h"
#include "CoreMinimal.h"
#include "Sandbox/widgets/LevelSelect2Widget.h"
#include "Sandbox/widgets/options_menu/OptionsWidget.h"
#include "Sandbox/widgets/TextButtonWidget.h"

#include "MainMenu2Widget.generated.h"

UENUM(BlueprintType)
enum class EMainMenu2MenuPage : uint8 { Main = 0, LevelSelect = 1, Options = 2 };

UCLASS()
class SANDBOX_API UMainMenu2Widget : public UUserWidget {
    GENERATED_BODY()
  protected:
    virtual void NativeConstruct() override;

    UPROPERTY(meta = (BindWidget))
    UWidgetSwitcher* widget_switcher{nullptr};
    UPROPERTY(meta = (BindWidget))
    UTextButtonWidget* play_button;
    UPROPERTY(meta = (BindWidget))
    UTextButtonWidget* options_button;
    UPROPERTY(meta = (BindWidget))
    UTextButtonWidget* quit_button;

    UPROPERTY(meta = (BindWidget))
    ULevelSelect2Widget* level_select_menu;
    UPROPERTY(meta = (BindWidget))
    UOptionsWidget* options_menu;
  private:
    UFUNCTION()
    void handle_play();
    UFUNCTION()
    void handle_options();
    UFUNCTION()
    void handle_quit();
    UFUNCTION()
    void return_to_main_page();
    void set_active_page(EMainMenu2MenuPage page);
};
