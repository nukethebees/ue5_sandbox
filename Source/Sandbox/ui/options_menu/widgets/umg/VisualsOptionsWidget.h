// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Components/ScrollBox.h"
#include "Components/VerticalBox.h"
#include "GameFramework/GameUserSettings.h"

#include "Sandbox/core/video_settings/VideoSettingsData.h"
#include "Sandbox/logging/mixins/LogMsgMixin.hpp"
#include "Sandbox/ui/widgets/umg/TextButtonWidget.h"

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
    UTextButtonWidget* reset_all_button{nullptr};

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
    void handle_reset_all_clicked();

    UFUNCTION()
    void handle_setting_changed(EVideoRowSettingChangeType change_type);

    bool has_pending_changes() const;
    void update_button_states();
    void reset_all_settings_to_original();
    void refresh_all_rows_from_settings();

    UGameUserSettings* get_game_user_settings() const;

    VideoSettingsTuple video_settings{};
    TArray<UVideoSettingRowWidget*> setting_row_widgets{};
    int32 pending_changes_count{0};
    int32 row_idx{0};

    bool validate_widget_class() const;
};
