#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Sandbox/concepts/concepts.h"
#include "Sandbox/data/video_options/VideoSettingsData.h"
#include "Sandbox/mixins/LogMsgMixin.hpp"
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
    UFUNCTION()
    void reset_to_original_value();

    UFUNCTION()
    void update_display_values();
    UFUNCTION()
    void apply_pending_changes();
    UFUNCTION()
    bool has_pending_changes() const;
    UFUNCTION()
    void refresh_current_value();
  protected:
    virtual void NativeConstruct() override;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* setting_name_text{nullptr};

    UPROPERTY(meta = (BindWidget))
    UTextBlock* current_value_text{nullptr};

    UPROPERTY(meta = (BindWidget))
    UEditableTextBox* pending_value_input{nullptr};

    UPROPERTY(meta = (BindWidget))
    UTextButtonWidget* toggle_button{nullptr};

    UPROPERTY(meta = (BindWidget))
    USlider* numeric_slider{nullptr};

    UPROPERTY(meta = (BindWidget))
    UTextButtonWidget* reset_button{nullptr};
  private:
    FText const& on_text() const;
    FText const& off_text() const;
    FText const& bool_text(bool state) const { return state ? on_text() : off_text(); }

    void setup_input_widgets_for_type();
    void setup_reset_button();

    template <typename DataType>
    void setup_numeric_widgets(DataType const& data);
    void setup_boolean_widgets();

    UFUNCTION()
    void handle_button_clicked();

    UFUNCTION()
    void handle_slider_changed(float value);

    UFUNCTION()
    void handle_text_committed(FText const& text, ETextCommit::Type commit_type);

    void notify_setting_changed(EVideoRowSettingChangeType change_type);
    void update_reset_button_state();

    VideoRow row_data{};

    template <typename Self, typename Visitor>
    decltype(auto) visit_row_data(this Self&& self, Visitor&& visitor) {
        return std::visit(std::forward<Visitor>(visitor), std::forward_like<Self>(self.row_data));
    }
};

template <typename DataType>
void UVideoSettingRowWidget::setup_numeric_widgets(DataType const& data) {
    // Show numeric controls, hide boolean controls
    if (numeric_slider) {
        numeric_slider->SetVisibility(ESlateVisibility::Visible);
        numeric_slider->OnValueChanged.AddDynamic(this,
                                                  &UVideoSettingRowWidget::handle_slider_changed);

        // Set slider range
        numeric_slider->SetMinValue(data.slider_min());
        numeric_slider->SetMaxValue(data.slider_max());
        log_verbose(TEXT("Numeric slider configured and shown"));
    }

    if (toggle_button) {
        toggle_button->SetVisibility(ESlateVisibility::Hidden);
    }

    if (pending_value_input) {
        pending_value_input->SetIsReadOnly(false);
        log_verbose(TEXT("Pending input set to editable for numeric type"));
    }
}
