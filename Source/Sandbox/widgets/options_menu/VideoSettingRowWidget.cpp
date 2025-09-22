#include "Sandbox/widgets/options_menu/VideoSettingRowWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/HorizontalBoxSlot.h"
#include "GameFramework/GameUserSettings.h"

void UVideoSettingRowWidget::NativeConstruct() {
    Super::NativeConstruct();

    log_verbose(TEXT("NativeConstruct()"));

    create_widget_hierarchy();
}

void UVideoSettingRowWidget::create_widget_hierarchy() {
    log_verbose(TEXT("create_widget_hierarchy()"));

    // Create root horizontal container
    root_container = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(),
                                                                 TEXT("RootContainer"));
    if (!root_container) {
        log_error(TEXT("Failed to create root container"));
        return;
    }

    // Set as root widget
    WidgetTree->RootWidget = root_container;

    // Create setting name label
    setting_name_text =
        WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SettingNameText"));
    if (setting_name_text) {
        auto* name_slot{root_container->AddChildToHorizontalBox(setting_name_text)};
        if (name_slot) {
            name_slot->SetSize(FSlateChildSize{ESlateSizeRule::Fill});
            name_slot->SetPadding(FMargin{5.0f, 0.0f, 10.0f, 0.0f});
        }
    }

    // Create current value display
    current_value_text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),
                                                                 TEXT("CurrentValueText"));
    if (current_value_text) {
        auto* value_slot{root_container->AddChildToHorizontalBox(current_value_text)};
        if (value_slot) {
            value_slot->SetSize(FSlateChildSize{ESlateSizeRule::Automatic});
            value_slot->SetPadding(FMargin{5.0f, 0.0f, 10.0f, 0.0f});
        }
        current_value_text->SetText(FText::FromString(TEXT("--")));
    }

    // Create input container
    input_container = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(),
                                                                  TEXT("InputContainer"));
    if (input_container) {
        auto* input_slot{root_container->AddChildToHorizontalBox(input_container)};
        if (input_slot) {
            input_slot->SetSize(FSlateChildSize{ESlateSizeRule::Automatic});
            input_slot->SetPadding(FMargin{5.0f, 0.0f, 5.0f, 0.0f});
        }
    }

    log_verbose(TEXT("Widget hierarchy created successfully"));
}

void UVideoSettingRowWidget::initialize_for_boolean_setting(BoolSettingConfig const& config) {
    log_verbose(TEXT("initialize_for_boolean_setting"));

    bool_config = &config;
    setting_type = config.type;

    if (setting_name_text) {
        setting_name_text->SetText(FText::FromString(config.setting_name));
    }

    create_button_input();
    update_boolean_display();
}

void UVideoSettingRowWidget::initialize_for_float_setting(FloatSettingConfig const& config) {
    log_verbose(TEXT("initialize_for_float_setting"));

    float_config = &config;
    setting_type = config.type;

    if (setting_name_text) {
        setting_name_text->SetText(FText::FromString(config.setting_name));
    }

    if (config.type == EVideoSettingType::SliderWithText) {
        create_slider_with_text_input();
    } else {
        create_text_input();
    }
    update_float_display();
}

void UVideoSettingRowWidget::initialize_for_int_setting(IntSettingConfig const& config) {
    log_verbose(TEXT("initialize_for_int_setting"));

    int_config = &config;
    setting_type = config.type;

    if (setting_name_text) {
        setting_name_text->SetText(FText::FromString(config.setting_name));
    }

    if (config.type == EVideoSettingType::SliderWithText) {
        create_slider_with_text_input();
    } else {
        create_text_input();
    }
    update_int_display();
}

void UVideoSettingRowWidget::create_button_input() {
    if (!input_container) {
        return;
    }

    button_widget = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("Button"));
    if (button_widget) {
        button_widget->OnClicked.AddDynamic(this, &UVideoSettingRowWidget::handle_button_clicked);
        input_container->AddChildToHorizontalBox(button_widget);

        // Create text block for button label
        button_text =
            WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ButtonText"));
        if (button_text) {
            button_widget->AddChild(button_text);
        }
    }
}

void UVideoSettingRowWidget::create_slider_with_text_input() {
    if (!input_container) {
        return;
    }

    // Create slider
    slider_widget = WidgetTree->ConstructWidget<USlider>(USlider::StaticClass(), TEXT("Slider"));
    if (slider_widget) {
        slider_widget->OnValueChanged.AddDynamic(this,
                                                 &UVideoSettingRowWidget::handle_slider_changed);

        if (float_config) {
            slider_widget->SetMinValue(float_config->range.min);
            slider_widget->SetMaxValue(float_config->range.max);
        } else if (int_config) {
            slider_widget->SetMinValue(static_cast<float>(int_config->range.min));
            slider_widget->SetMaxValue(static_cast<float>(int_config->range.max));
        }

        input_container->AddChildToHorizontalBox(slider_widget);
    }

    // Create text box
    text_widget = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(),
                                                                TEXT("EditableText"));
    if (text_widget) {
        text_widget->OnTextCommitted.AddDynamic(this,
                                                &UVideoSettingRowWidget::handle_text_committed);
        text_widget->SetJustification(ETextJustify::Center);

        auto* text_slot{input_container->AddChildToHorizontalBox(text_widget)};
        if (text_slot) {
            text_slot->SetSize(FSlateChildSize{ESlateSizeRule::Fill});
            text_slot->SetHorizontalAlignment(HAlign_Fill);
        }
    }
}

void UVideoSettingRowWidget::create_text_input() {
    if (!input_container) {
        return;
    }

    text_widget = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(),
                                                                TEXT("EditableText2"));
    if (text_widget) {
        text_widget->OnTextCommitted.AddDynamic(this,
                                                &UVideoSettingRowWidget::handle_text_committed);
        text_widget->SetJustification(ETextJustify::Center);
        input_container->AddChildToHorizontalBox(text_widget);
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
    current_value_text->SetText(FText::FromString(current_value ? TEXT("On") : TEXT("Off")));

    if (button_text) {
        button_text->SetText(FText::FromString(current_value ? TEXT("On") : TEXT("Off")));
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
    if (button_text) {
        button_text->SetText(FText::FromString(pending_bool_value ? TEXT("On") : TEXT("Off")));
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
