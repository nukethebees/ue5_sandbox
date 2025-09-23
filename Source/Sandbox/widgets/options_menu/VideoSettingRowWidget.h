#pragma once

#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "CoreMinimal.h"
#include "Sandbox/mixins/log_msg_mixin.hpp"
#include "Sandbox/widgets/options_menu/VideoSettingsData.h"

#include "VideoSettingRowWidget.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnVideoSettingChanged);

UCLASS()
class SANDBOX_API UVideoSettingRowWidget
    : public UUserWidget
    , public ml::LogMsgMixin<"UVideoSettingRowWidget"> {
    GENERATED_BODY()
  public:
    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnVideoSettingChanged on_setting_changed;

    void initialize_for_boolean_setting(BoolSettingConfig const& config);
    void initialize_for_float_setting(FloatSettingConfig const& config);
    void initialize_for_int_setting(IntSettingConfig const& config);

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
    UButton* button_widget{nullptr};

    UPROPERTY(meta = (BindWidget))
    USlider* slider_widget{nullptr};

    UPROPERTY(meta = (BindWidget))
    UEditableTextBox* text_widget{nullptr};

    UPROPERTY(meta = (BindWidget))
    UTextBlock* button_text{nullptr};
  private:
    void setup_input_widgets_for_type();

    void update_boolean_display();
    void update_float_display();
    void update_int_display();

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

    EVideoSettingType setting_type{EVideoSettingType::TextBox};

    // Store config data for different types
    BoolSettingConfig const* bool_config{nullptr};
    FloatSettingConfig const* float_config{nullptr};
    IntSettingConfig const* int_config{nullptr};

    // Pending values (before apply is clicked)
    bool pending_bool_value{false};
    float pending_float_value{0.0f};
    int32 pending_int_value{0};

    bool has_pending_bool_change{false};
    bool has_pending_float_change{false};
    bool has_pending_int_change{false};
};
