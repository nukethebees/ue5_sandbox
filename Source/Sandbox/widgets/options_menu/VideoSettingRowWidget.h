#pragma once

#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "CoreMinimal.h"
#include "Sandbox/data/video_options/VideoSettingsData.h"
#include "Sandbox/mixins/log_msg_mixin.hpp"
#include "Sandbox/widgets/TextButtonWidget.h"

#include "VideoSettingRowWidget.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnVideoSettingChanged,
                                            EVideoRowSettingChangeType,
                                            change_type);

UCLASS()
class SANDBOX_API UVideoSettingRowWidget
    : public UUserWidget
    , public ml::LogMsgMixin<"UVideoSettingRowWidget"> {
    GENERATED_BODY()
  public:
    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnVideoSettingChanged on_setting_changed;

    void initialize_with_row_data(VideoRow const& row_data);
    void reset_to_original_value();

    void update_current_value_display();
    void apply_pending_changes();
    bool has_pending_changes() const;
  protected:
    virtual void NativeConstruct() override;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* setting_name_text{nullptr};

    UPROPERTY(meta = (BindWidget))
    UTextBlock* current_value_text{nullptr};

    UPROPERTY(meta = (BindWidget))
    UTextButtonWidget* button_widget{nullptr};

    UPROPERTY(meta = (BindWidget))
    USlider* slider_widget{nullptr};

    UPROPERTY(meta = (BindWidget))
    UEditableTextBox* text_widget{nullptr};

    UPROPERTY(meta = (BindWidget))
    UTextButtonWidget* reset_button{nullptr};
  private:
    void setup_input_widgets_for_type();
    void setup_reset_button();

    void update_boolean_display();
    void update_float_display();
    void update_int_display();

    UFUNCTION()
    void handle_button_clicked();

    UFUNCTION()
    void handle_slider_changed(float value);

    UFUNCTION()
    void handle_text_committed(FText const& text, ETextCommit::Type commit_type);

    UFUNCTION()
    void handle_reset_clicked();

    template <typename T>
    T get_current_value_from_settings() const;

    template <typename T>
    void set_value_to_settings(T value);

    void notify_setting_changed(EVideoRowSettingChangeType change_type);
    void update_display_values();
    void update_reset_button_state();

    VideoRow row_data{};
    EVideoSettingType setting_type{EVideoSettingType::TextBox};

    // Helper methods for variant operations
    template <typename Visitor>
    auto visit_row_data(Visitor&& visitor) {
        return std::visit(std::forward<Visitor>(visitor), row_data);
    }

    template <typename Visitor>
    auto visit_row_data(Visitor&& visitor) const {
        return std::visit(std::forward<Visitor>(visitor), row_data);
    }
};
