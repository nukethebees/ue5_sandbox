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
    UFUNCTION()
    void reset_to_original_value();

    UFUNCTION()
    void update_display_values();
    UFUNCTION()
    void apply_pending_changes();
    UFUNCTION()
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
      UFUNCTION()
      FText const & on_text() const;
      UFUNCTION()
      FText const & off_text() const;
      UFUNCTION()
      FText const & bool_text(bool state) const {
          return state ? on_text() : off_text();
      }

    void setup_input_widgets_for_type();
    void setup_reset_button();

    UFUNCTION()
    void handle_button_clicked();

    UFUNCTION()
    void handle_slider_changed(float value);

    UFUNCTION()
    void handle_text_committed(FText const& text, ETextCommit::Type commit_type);

    template <typename T>
    T get_current_value_from_settings() const;

    template <typename T>
    void set_value_to_settings(T value);

    void notify_setting_changed(EVideoRowSettingChangeType change_type);
    void update_reset_button_state();

    VideoRow row_data{};
    EVideoSettingType setting_type{EVideoSettingType::TextBox};

    template <typename Self, typename Visitor>
    decltype(auto) visit_row_data(this Self&& self, Visitor&& visitor) {
        return std::visit(std::forward<Visitor>(visitor), std::forward_like<Self>(self.row_data));
    }
};
