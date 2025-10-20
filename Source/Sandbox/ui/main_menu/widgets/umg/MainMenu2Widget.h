// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/WidgetSwitcher.h"

#include "Sandbox/logging/mixins/LogMsgMixin.hpp"
#include "Sandbox/ui/main_menu/widgets/umg/LevelSelect2Widget.h"
#include "Sandbox/ui/options_menu/widgets/umg/OptionsWidget.h"
#include "Sandbox/ui/widgets/umg/TextButtonWidget.h"

#include "MainMenu2Widget.generated.h"

UENUM(BlueprintType)
enum class EMainMenu2MenuPage : uint8 { Main = 0, LevelSelect = 1, Options = 2 };

UCLASS()
class SANDBOX_API UMainMenu2Widget
    : public UUserWidget
    , public ml::LogMsgMixin<"UMainMenu2Widget"> {
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
