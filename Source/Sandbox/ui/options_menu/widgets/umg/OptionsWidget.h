// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/WidgetSwitcher.h"
#include "Sandbox/logging/LogMsgMixin.hpp"
#include "Sandbox/ui/delegates/CommonMenuDelegates.h"
#include "Sandbox/ui/options_menu/widgets/umg/AudioOptionsWidget.h"
#include "Sandbox/ui/options_menu/widgets/umg/ControlsOptionsWidget.h"
#include "Sandbox/ui/options_menu/widgets/umg/GameplayOptionsWidget.h"
#include "Sandbox/ui/options_menu/widgets/umg/VisualsOptionsWidget.h"
#include "Sandbox/ui/widgets/umg/TextButtonWidget.h"

#include "OptionsWidget.generated.h"

UENUM(BlueprintType)
enum class EOptionsTab : uint8 { Gameplay = 0, Visuals = 1, Audio = 2, Controls = 3 };

UCLASS()
class SANDBOX_API UOptionsWidget
    : public UUserWidget
    , public ml::LogMsgMixin<"UVideoSettingRowWidget"> {
    GENERATED_BODY()
  protected:
    virtual void NativeConstruct() override;

    UPROPERTY(meta = (BindWidget))
    UWidgetSwitcher* tab_switcher{nullptr};

    // Tab buttons
    UPROPERTY(meta = (BindWidget))
    UTextButtonWidget* gameplay_tab_button{nullptr};
    UPROPERTY(meta = (BindWidget))
    UTextButtonWidget* visuals_tab_button{nullptr};
    UPROPERTY(meta = (BindWidget))
    UTextButtonWidget* audio_tab_button{nullptr};
    UPROPERTY(meta = (BindWidget))
    UTextButtonWidget* controls_tab_button{nullptr};

    // Back button
    UPROPERTY(meta = (BindWidget))
    UTextButtonWidget* back_button{nullptr};

    // Tab content widgets (bound from Blueprint)
    UPROPERTY(meta = (BindWidget))
    UGameplayOptionsWidget* gameplay_tab{nullptr};
    UPROPERTY(meta = (BindWidget))
    UVisualsOptionsWidget* visuals_tab{nullptr};
    UPROPERTY(meta = (BindWidget))
    UAudioOptionsWidget* audio_tab{nullptr};
    UPROPERTY(meta = (BindWidget))
    UControlsOptionsWidget* controls_tab{nullptr};
  public:
    // Back navigation event
    FBackRequested back_requested;
  private:
    UFUNCTION()
    void handle_gameplay_tab();
    UFUNCTION()
    void handle_visuals_tab();
    UFUNCTION()
    void handle_audio_tab();
    UFUNCTION()
    void handle_controls_tab();
    UFUNCTION()
    void handle_back();

    void set_active_tab(EOptionsTab tab);
    void update_tab_button_states(EOptionsTab active_tab);

    EOptionsTab current_tab{EOptionsTab::Gameplay};
};
