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

    setup_reset_button();
}

void UVideoSettingRowWidget::initialize_with_row_data(VideoRow const& new_row_data) {
    log_verbose(TEXT("initialize_with_row_data"));

    row_data = new_row_data;

    // Extract setting type and name from variant
    visit_row_data([this](auto const& data) {
        setting_type = data.config->type;
        if (setting_name_text) {
            setting_name_text->SetText(FText::FromString(data.config->setting_name));
        }
    });

    setup_input_widgets_for_type();
    update_display_values();
    update_reset_button_state();
}

void UVideoSettingRowWidget::reset_to_original_value() {
    log_verbose(TEXT("reset_to_original_value"));

    visit_row_data([this](auto& data) { data.reset_pending(); });

    update_display_values();
    update_reset_button_state();
    notify_setting_changed(EVideoRowSettingChangeType::ValueReset);
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

                // Set slider range based on config from variant
                visit_row_data([this](auto const& data) {
                    using ConfigType = std::decay_t<decltype(*data.config)>;
                    if constexpr (std::is_same_v<ConfigType, FloatSettingConfig>) {
                        slider_widget->SetMinValue(data.config->range.min);
                        slider_widget->SetMaxValue(data.config->range.max);
                    } else if constexpr (std::is_same_v<ConfigType, IntSettingConfig>) {
                        slider_widget->SetMinValue(static_cast<float>(data.config->range.min));
                        slider_widget->SetMaxValue(static_cast<float>(data.config->range.max));
                    }
                });
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

void UVideoSettingRowWidget::setup_reset_button() {
    if (reset_button) {
        reset_button->on_clicked.AddDynamic(this, &UVideoSettingRowWidget::handle_reset_clicked);
        reset_button->set_label(FText::FromString(TEXT("Reset")));
        log_verbose(TEXT("Reset button bound and configured"));
    } else {
        log_verbose(TEXT("Reset button not bound - using global reset only"));
    }
}

void UVideoSettingRowWidget::update_current_value_display() {
    update_display_values();
}

void UVideoSettingRowWidget::update_display_values() {
    if (!current_value_text) {
        return;
    }

    visit_row_data([this](auto const& data) {
        using ConfigType = std::decay_t<decltype(*data.config)>;
        auto const display_value{data.get_display_value()};

        if constexpr (std::is_same_v<ConfigType, BoolSettingConfig>) {
            auto const text{display_value ? FText::FromString(TEXT("On"))
                                          : FText::FromString(TEXT("Off"))};
            current_value_text->SetText(text);
            if (button_widget) {
                button_widget->set_label(text);
            }
        } else if constexpr (std::is_same_v<ConfigType, FloatSettingConfig>) {
            current_value_text->SetText(FText::AsNumber(display_value));
            if (slider_widget) {
                slider_widget->SetValue(display_value);
            }
            if (text_widget) {
                text_widget->SetText(FText::AsNumber(display_value));
            }
        } else if constexpr (std::is_same_v<ConfigType, IntSettingConfig>) {
            current_value_text->SetText(FText::AsNumber(display_value));
            if (slider_widget) {
                slider_widget->SetValue(static_cast<float>(display_value));
            }
            if (text_widget) {
                text_widget->SetText(FText::AsNumber(display_value));
            }
        }
    });
}

void UVideoSettingRowWidget::update_reset_button_state() {
    if (!reset_button) {
        return;
    }

    bool const has_changes{
        visit_row_data([](auto const& data) { return data.has_pending_change(); })};

    reset_button->SetIsEnabled(has_changes);
}

void UVideoSettingRowWidget::handle_button_clicked() {
    visit_row_data([this](auto& data) {
        using ConfigType = std::decay_t<decltype(*data.config)>;
        if constexpr (std::is_same_v<ConfigType, BoolSettingConfig>) {
            // Toggle the current value
            auto const new_value{!data.get_display_value()};
            data.set_pending_value(new_value);

            // Update display immediately
            update_display_values();
            update_reset_button_state();
            notify_setting_changed(EVideoRowSettingChangeType::ValueChanged);
        }
    });
}

void UVideoSettingRowWidget::handle_slider_changed(float value) {
    visit_row_data([this, value](auto& data) {
        using ConfigType = std::decay_t<decltype(*data.config)>;
        if constexpr (std::is_same_v<ConfigType, FloatSettingConfig>) {
            data.set_pending_value(value);
            if (text_widget) {
                text_widget->SetText(FText::AsNumber(value));
            }
        } else if constexpr (std::is_same_v<ConfigType, IntSettingConfig>) {
            auto const int_value{static_cast<int32>(FMath::RoundToInt(value))};
            data.set_pending_value(int_value);
            if (text_widget) {
                text_widget->SetText(FText::AsNumber(int_value));
            }
        }
    });

    update_reset_button_state();
    notify_setting_changed(EVideoRowSettingChangeType::ValueChanged);
}

void UVideoSettingRowWidget::handle_text_committed(FText const& text,
                                                   ETextCommit::Type commit_type) {
    visit_row_data([this, &text](auto& data) {
        using ConfigType = std::decay_t<decltype(*data.config)>;
        if constexpr (std::is_same_v<ConfigType, FloatSettingConfig>) {
            auto const value{FCString::Atof(*text.ToString())};
            auto const clamped_value{
                FMath::Clamp(value, data.config->range.min, data.config->range.max)};

            data.set_pending_value(clamped_value);

            if (slider_widget) {
                slider_widget->SetValue(clamped_value);
            }
            text_widget->SetText(FText::AsNumber(clamped_value));
        } else if constexpr (std::is_same_v<ConfigType, IntSettingConfig>) {
            auto const value{FCString::Atoi(*text.ToString())};
            auto const clamped_value{
                FMath::Clamp(value, data.config->range.min, data.config->range.max)};

            data.set_pending_value(clamped_value);

            if (slider_widget) {
                slider_widget->SetValue(static_cast<float>(clamped_value));
            }
            text_widget->SetText(FText::AsNumber(clamped_value));
        }
    });

    update_reset_button_state();
    notify_setting_changed(EVideoRowSettingChangeType::ValueChanged);
}

void UVideoSettingRowWidget::handle_reset_clicked() {
    reset_to_original_value();
}

void UVideoSettingRowWidget::notify_setting_changed(EVideoRowSettingChangeType change_type) {
    on_setting_changed.Broadcast(change_type);
}

template <typename T>
T UVideoSettingRowWidget::get_current_value_from_settings() const {
    auto* settings{UGameUserSettings::GetGameUserSettings()};
    if (!settings) {
        return T{};
    }

    return visit_row_data([settings](auto const& data) -> T {
        using ConfigType = std::decay_t<decltype(*data.config)>;
        if constexpr (std::is_same_v<typename ConfigType::SettingT, T>) {
            if (data.config && data.config->getter) {
                return (settings->*(data.config->getter))();
            }
        }
        return T{};
    });
}

template <typename T>
void UVideoSettingRowWidget::set_value_to_settings(T value) {
    auto* settings{UGameUserSettings::GetGameUserSettings()};
    if (!settings) {
        return;
    }

    visit_row_data([settings, value](auto const& data) {
        using ConfigType = std::decay_t<decltype(*data.config)>;
        if constexpr (std::is_same_v<typename ConfigType::SettingT, T>) {
            if (data.config && data.config->setter) {
                (settings->*(data.config->setter))(value);
            }
        }
    });
}

void UVideoSettingRowWidget::apply_pending_changes() {
    visit_row_data([this](auto& data) {
        if (data.has_pending_change()) {
            // Apply pending value to settings
            auto const pending_value{*data.pending_value};
            set_value_to_settings(pending_value);

            // Update current value and clear pending
            data.current_value = pending_value;
            data.reset_pending();
        }
    });

    update_display_values();
    update_reset_button_state();
}

bool UVideoSettingRowWidget::has_pending_changes() const {
    return visit_row_data([](auto const& data) { return data.has_pending_change(); });
}
