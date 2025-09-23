// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Blueprint/UserWidget.h"
#include "Components/WidgetSwitcher.h"
#include "CoreMinimal.h"
#include "Sandbox/mixins/log_msg_mixin.hpp"
#include "Sandbox/widgets/CommonMenuDelegates.h"
#include "Sandbox/widgets/options_menu/AudioOptionsWidget.h"
#include "Sandbox/widgets/options_menu/ControlsOptionsWidget.h"
#include "Sandbox/widgets/options_menu/GameplayOptionsWidget.h"
#include "Sandbox/widgets/options_menu/VisualsOptionsWidget.h"
#include "Sandbox/widgets/TextButtonWidget.h"

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

    // Blueprint classes for tab content widgets
    UPROPERTY(EditDefaultsOnly,
              BlueprintReadOnly,
              Category = "Widget Classes",
              meta = (AllowPrivateAccess = "true"))
    TSubclassOf<UGameplayOptionsWidget> gameplay_options_widget_class{nullptr};

    UPROPERTY(EditDefaultsOnly,
              BlueprintReadOnly,
              Category = "Widget Classes",
              meta = (AllowPrivateAccess = "true"))
    TSubclassOf<UVisualsOptionsWidget> visuals_options_widget_class{nullptr};

    UPROPERTY(EditDefaultsOnly,
              BlueprintReadOnly,
              Category = "Widget Classes",
              meta = (AllowPrivateAccess = "true"))
    TSubclassOf<UAudioOptionsWidget> audio_options_widget_class{nullptr};

    UPROPERTY(EditDefaultsOnly,
              BlueprintReadOnly,
              Category = "Widget Classes",
              meta = (AllowPrivateAccess = "true"))
    TSubclassOf<UControlsOptionsWidget> controls_options_widget_class{nullptr};
  public:
    // Back navigation event
    UPROPERTY(BlueprintAssignable)
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
    bool validate_widget_classes() const;

    EOptionsTab current_tab{EOptionsTab::Gameplay};
};
