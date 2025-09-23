// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Components/ScrollBox.h"
#include "Components/VerticalBox.h"
#include "CoreMinimal.h"
#include "GameFramework/GameUserSettings.h"
#include "Sandbox/mixins/log_msg_mixin.hpp"
#include "Sandbox/widgets/options_menu/VideoSettingsData.h"
#include "Sandbox/widgets/TextButtonWidget.h"

#include "VisualsOptionsWidget.generated.h"

class UVideoSettingRowWidget;

UCLASS()
class SANDBOX_API UVisualsOptionsWidget
    : public UUserWidget
    , public ml::LogMsgMixin<"UVisualsOptionsWidget"> {
    GENERATED_BODY()
  protected:
    virtual void NativeConstruct() override;

    UPROPERTY(meta = (BindWidget))
    UScrollBox* settings_scroll_box{nullptr};

    UPROPERTY(meta = (BindWidget))
    UTextButtonWidget* apply_button{nullptr};

    UPROPERTY(meta = (BindWidget))
    UVerticalBox* settings_container{nullptr};

    // Blueprint classes for different setting row types
    UPROPERTY(EditDefaultsOnly,
              BlueprintReadOnly,
              Category = "Widget Classes",
              meta = (AllowPrivateAccess = "true"))
    TSubclassOf<UVideoSettingRowWidget> video_setting_row_widget_class{nullptr};

    void initialize_video_settings();
    void populate_settings_ui();
    void create_setting_rows();

    template <typename T>
    void create_rows_for_type();

    UFUNCTION()
    void handle_apply_clicked();

    UFUNCTION()
    void handle_setting_changed();

    bool has_pending_changes() const;
    void update_apply_button_state();

    UGameUserSettings* get_game_user_settings() const;

    VideoSettingsTuple video_settings{};
    TArray<UVideoSettingRowWidget*> setting_row_widgets{};
    bool pending_changes{false};
    int32 row_idx{0};

    bool validate_widget_class() const;
};
