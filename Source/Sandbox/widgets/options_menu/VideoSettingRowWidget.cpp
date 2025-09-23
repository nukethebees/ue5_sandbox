#include "Sandbox/widgets/options_menu/VideoSettingRowWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/HorizontalBoxSlot.h"
#include "GameFramework/GameUserSettings.h"

void UVideoSettingRowWidget::NativeConstruct() {
    Super::NativeConstruct();

    log_verbose(TEXT("NativeConstruct()"));

    // Widget hierarchy is now created in Blueprint
    // Just verify that required widgets are bound
    if (!setting_name_text) {
        log_error(TEXT("setting_name_text not bound to Blueprint widget"));
    }
    if (!current_value_text) {
        log_error(TEXT("current_value_text not bound to Blueprint widget"));
    }
}

void UVideoSettingRowWidget::initialize_for_boolean_setting(BoolSettingConfig const& config) {
    log_verbose(TEXT("initialize_for_boolean_setting"));

    bool_config = &config;
    setting_type = config.type;

    if (setting_name_text) {
        setting_name_text->SetText(FText::FromString(config.setting_name));
    }

    setup_input_widgets_for_type();
    update_boolean_display();
}

void UVideoSettingRowWidget::initialize_for_float_setting(FloatSettingConfig const& config) {
    log_verbose(TEXT("initialize_for_float_setting"));

    float_config = &config;
    setting_type = config.type;

    if (setting_name_text) {
        setting_name_text->SetText(FText::FromString(config.setting_name));
    }

    setup_input_widgets_for_type();
    update_float_display();
}

void UVideoSettingRowWidget::initialize_for_int_setting(IntSettingConfig const& config) {
    log_verbose(TEXT("initialize_for_int_setting"));

    int_config = &config;
    setting_type = config.type;

    if (setting_name_text) {
        setting_name_text->SetText(FText::FromString(config.setting_name));
    }

    setup_input_widgets_for_type();
    update_int_display();
}

void UVideoSettingRowWidget::setup_input_widgets_for_type() {
    // Setup event bindings for widgets that are bound from Blueprint
    switch (setting_type) {
        case EVideoSettingType::Checkbox: {
            if (button_widget) {
                button_widget->on_clicked.AddDynamic(
                    this, &UVideoSettingRowWidget::handle_button_clicked);
                log_verbose(TEXT("Button widget bound and event connected"));
            } else {
                log_warning(TEXT("Button widget not bound for checkbox setting"));
            }
            break;
        }
        case EVideoSettingType::SliderWithText: {
            if (slider_widget) {
                slider_widget->OnValueChanged.AddDynamic(
                    this, &UVideoSettingRowWidget::handle_slider_changed);

                // Set slider range based on config
                if (float_config) {
                    slider_widget->SetMinValue(float_config->range.min);
                    slider_widget->SetMaxValue(float_config->range.max);
                } else if (int_config) {
                    slider_widget->SetMinValue(static_cast<float>(int_config->range.min));
                    slider_widget->SetMaxValue(static_cast<float>(int_config->range.max));
                }
                log_verbose(TEXT("Slider widget bound and configured"));
            } else {
                log_warning(TEXT("Slider widget not bound for slider setting"));
            }

            if (text_widget) {
                text_widget->OnTextCommitted.AddDynamic(
                    this, &UVideoSettingRowWidget::handle_text_committed);
                text_widget->SetJustification(ETextJustify::Center);
                log_verbose(TEXT("Text widget bound for slider setting"));
            } else {
                log_warning(TEXT("Text widget not bound for slider setting"));
            }
            break;
        }
        case EVideoSettingType::TextBox: {
            if (text_widget) {
                text_widget->OnTextCommitted.AddDynamic(
                    this, &UVideoSettingRowWidget::handle_text_committed);
                text_widget->SetJustification(ETextJustify::Center);
                log_verbose(TEXT("Text widget bound for text box setting"));
            } else {
                log_warning(TEXT("Text widget not bound for text box setting"));
            }
            break;
        }
    }
}

void UVideoSettingRowWidget::update_current_value_display() {
    switch (setting_type) {
        case EVideoSettingType::Checkbox: {
            update_boolean_display();
            break;
        }
        case EVideoSettingType::SliderWithText:
        case EVideoSettingType::TextBox: {
            if (float_config) {
                update_float_display();
            } else if (int_config) {
                update_int_display();
            }
            break;
        }
    }
}

void UVideoSettingRowWidget::update_boolean_display() {
    if (!bool_config || !current_value_text) {
        return;
    }

    auto const current_value{get_current_value_from_settings<bool>()};
    auto const& text{bool_text(current_value)};
    current_value_text->SetText(text);

    if (button_widget) {
        button_widget->set_label(text);
    }
}

void UVideoSettingRowWidget::update_float_display() {
    if (!float_config || !current_value_text) {
        return;
    }

    auto const current_value{get_current_value_from_settings<float>()};
    current_value_text->SetText(FText::AsNumber(current_value));

    if (slider_widget) {
        slider_widget->SetValue(current_value);
    }
    if (text_widget) {
        text_widget->SetText(FText::AsNumber(current_value));
    }
}

void UVideoSettingRowWidget::update_int_display() {
    if (!int_config || !current_value_text) {
        return;
    }

    auto const current_value{get_current_value_from_settings<int32>()};
    current_value_text->SetText(FText::AsNumber(current_value));

    if (slider_widget) {
        slider_widget->SetValue(static_cast<float>(current_value));
    }
    if (text_widget) {
        text_widget->SetText(FText::AsNumber(current_value));
    }
}

void UVideoSettingRowWidget::handle_button_clicked() {
    if (!bool_config) {
        return;
    }

    // Get current value and toggle it
    auto const current_value{get_current_value_from_settings<bool>()};
    pending_bool_value = !current_value;
    has_pending_bool_change = true;

    // Update button text immediately
    if (button_widget) {
        button_widget->set_label(bool_text(pending_bool_value));
    }

    on_setting_changed.Broadcast();
}

void UVideoSettingRowWidget::handle_slider_changed(float value) {
    if (float_config) {
        pending_float_value = value;
        has_pending_float_change = true;

        if (text_widget) {
            text_widget->SetText(FText::AsNumber(value));
        }
    } else if (int_config) {
        pending_int_value = static_cast<int32>(FMath::RoundToInt(value));
        has_pending_int_change = true;

        if (text_widget) {
            text_widget->SetText(FText::AsNumber(pending_int_value));
        }
    }

    on_setting_changed.Broadcast();
}

void UVideoSettingRowWidget::handle_text_committed(FText const& text,
                                                   ETextCommit::Type commit_type) {
    if (float_config) {
        auto const value{FCString::Atof(*text.ToString())};
        auto const clamped_value{
            FMath::Clamp(value, float_config->range.min, float_config->range.max)};

        pending_float_value = clamped_value;
        has_pending_float_change = true;

        if (slider_widget) {
            slider_widget->SetValue(clamped_value);
        }
        text_widget->SetText(FText::AsNumber(clamped_value));
    } else if (int_config) {
        auto const value{FCString::Atoi(*text.ToString())};
        auto const clamped_value{FMath::Clamp(value, int_config->range.min, int_config->range.max)};

        pending_int_value = clamped_value;
        has_pending_int_change = true;

        if (slider_widget) {
            slider_widget->SetValue(static_cast<float>(clamped_value));
        }
        text_widget->SetText(FText::AsNumber(clamped_value));
    }

    on_setting_changed.Broadcast();
}

FText const& UVideoSettingRowWidget::on_text() const {
    static auto const txt{FText::FromString(TEXT("On"))};
    return txt;
}
FText const& UVideoSettingRowWidget::off_text() const {
    static auto const txt{FText::FromString(TEXT("Off"))};
    return txt;
}

template <typename T>
T UVideoSettingRowWidget::get_current_value_from_settings() const {
    auto* settings{UGameUserSettings::GetGameUserSettings()};
    if (!settings) {
        return T{};
    }

    if constexpr (std::is_same_v<T, bool>) {
        if (bool_config && bool_config->getter) {
            return (settings->*(bool_config->getter))();
        }
    } else if constexpr (std::is_same_v<T, float>) {
        if (float_config && float_config->getter) {
            return (settings->*(float_config->getter))();
        }
    } else if constexpr (std::is_same_v<T, int32>) {
        if (int_config && int_config->getter) {
            return (settings->*(int_config->getter))();
        }
    }

    return T{};
}

template <typename T>
void UVideoSettingRowWidget::set_value_to_settings(T value) {
    auto* settings{UGameUserSettings::GetGameUserSettings()};
    if (!settings) {
        return;
    }

    if constexpr (std::is_same_v<T, bool>) {
        if (bool_config && bool_config->setter) {
            (settings->*(bool_config->setter))(value);
        }
    } else if constexpr (std::is_same_v<T, float>) {
        if (float_config && float_config->setter) {
            (settings->*(float_config->setter))(value);
        }
    } else if constexpr (std::is_same_v<T, int32>) {
        if (int_config && int_config->setter) {
            (settings->*(int_config->setter))(value);
        }
    }
}

void UVideoSettingRowWidget::apply_pending_changes() {
    if (has_pending_bool_change) {
        set_value_to_settings<bool>(pending_bool_value);
        has_pending_bool_change = false;
    }

    if (has_pending_float_change) {
        set_value_to_settings<float>(pending_float_value);
        has_pending_float_change = false;
    }

    if (has_pending_int_change) {
        set_value_to_settings<int32>(pending_int_value);
        has_pending_int_change = false;
    }

    update_current_value_display();
}

bool UVideoSettingRowWidget::has_pending_changes() const {
    return has_pending_bool_change || has_pending_float_change || has_pending_int_change;
}
