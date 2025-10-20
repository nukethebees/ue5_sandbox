#include "Sandbox/ui/options_menu/widgets/umg/VideoSettingRowWidget.h"

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
        if (setting_name_text) {
            setting_name_text->SetText(FText::FromString(data.get_config()->setting_name));
        }
    });

    setup_input_widgets_for_type();
    update_display_values();
    update_reset_button_state();
}

void UVideoSettingRowWidget::reset_to_original_value() {
    log_verbose(TEXT("reset_to_original_value"));

    visit_row_data([this](auto& data) { data.reset_pending_value(); });

    update_display_values();
    update_reset_button_state();
    notify_setting_changed(EVideoRowSettingChangeType::ValueReset);
}

FText const& UVideoSettingRowWidget::on_text() const {
    static auto const txt{FText::FromString(TEXT("On"))};
    return txt;
}
FText const& UVideoSettingRowWidget::off_text() const {
    static auto const txt{FText::FromString(TEXT("Off"))};
    return txt;
}

void UVideoSettingRowWidget::setup_input_widgets_for_type() {
    // Setup widgets based on data type using concepts
    visit_row_data([this](auto const& data) {
        using UnrealT = std::remove_cvref_t<decltype(data)>::UnrealT;

        if constexpr (ml::is_numeric<UnrealT>) {
            setup_numeric_widgets(data);
        } else {
            setup_boolean_widgets();
        }
    });

    // Setup pending value input widget (always present)
    if (pending_value_input) {
        pending_value_input->OnTextCommitted.AddDynamic(
            this, &UVideoSettingRowWidget::handle_text_committed);
        pending_value_input->SetJustification(ETextJustify::Center);
        log_verbose(TEXT("Pending value input widget bound"));
    } else {
        log_warning(TEXT("Pending value input widget not bound"));
    }
}

void UVideoSettingRowWidget::setup_boolean_widgets() {
    // Show boolean controls, hide numeric controls
    if (toggle_button) {
        toggle_button->SetVisibility(ESlateVisibility::Visible);
        toggle_button->set_label(FText::FromString(TEXT("Toggle")));
        toggle_button->on_clicked.AddDynamic(this, &UVideoSettingRowWidget::handle_button_clicked);
        log_verbose(TEXT("Toggle button configured and shown"));
    }

    if (numeric_slider) {
        numeric_slider->SetVisibility(ESlateVisibility::Hidden);
    }

    if (pending_value_input) {
        pending_value_input->SetIsReadOnly(true);
        log_verbose(TEXT("Pending input set to read-only for boolean type"));
    }
}

void UVideoSettingRowWidget::setup_reset_button() {
    if (reset_button) {
        reset_button->on_clicked.AddDynamic(this, &UVideoSettingRowWidget::reset_to_original_value);
        reset_button->set_label(FText::FromString(TEXT("Reset")));
        log_verbose(TEXT("Reset button bound and configured"));
    } else {
        log_verbose(TEXT("Reset button not bound - using global reset only"));
    }
}

void UVideoSettingRowWidget::update_display_values() {
    if (!current_value_text || !pending_value_input) {
        return;
    }

    visit_row_data([this](auto const& data) {
        using UnrealT = std::remove_cvref_t<decltype(data)>::UnrealT;

        current_value_text->SetText(data.get_current_display_text());
        pending_value_input->SetText(data.get_pending_display_text());

        if constexpr (ml::is_numeric<UnrealT>) {
            if (numeric_slider) {
                numeric_slider->SetValue(data.slider_pending());
            }
        }
    });
}

void UVideoSettingRowWidget::update_reset_button_state() {
    if (!reset_button) {
        return;
    }
    reset_button->SetIsEnabled(has_pending_changes());
}

void UVideoSettingRowWidget::handle_button_clicked() {
    visit_row_data([this](auto& data) {
        using ConfigType = std::decay_t<decltype(*data.get_config())>;
        if constexpr (std::is_same_v<ConfigType, BoolSettingConfig>) {
            auto const new_value{!data.get_display_value()};
            data.set_pending_value(new_value);

            update_display_values();
            update_reset_button_state();
            notify_setting_changed(EVideoRowSettingChangeType::ValueChanged);
        }
    });
}

void UVideoSettingRowWidget::handle_slider_changed(float value) {
    visit_row_data([this, value](auto& data) {
        using UnrealT = std::remove_cvref_t<decltype(data)>::UnrealT;

        if constexpr (ml::is_numeric<UnrealT>) {
            data.set_pending_value(value);

            if (pending_value_input) {
                pending_value_input->SetText(data.get_pending_display_text());
            }
        }
    });

    update_reset_button_state();
    notify_setting_changed(EVideoRowSettingChangeType::ValueChanged);
}

void UVideoSettingRowWidget::handle_text_committed(FText const& text,
                                                   ETextCommit::Type commit_type) {
    visit_row_data([this, &text](auto& data) {
        using ConfigType = std::remove_cvref_t<decltype(data)>::ConfigT;
        using SettingT = typename ConfigType::SettingT;
        using UnrealT = typename ConfigType::UnrealT;

        if constexpr (ml::is_numeric<UnrealT>) {
            SettingT value;
            if constexpr (std::is_same_v<SettingT, float>) {
                value = FCString::Atof(*text.ToString());
            } else {
                value = static_cast<SettingT>(FCString::Atoi(*text.ToString()));
            }

            data.set_pending_value(value);
            if (numeric_slider) {
                numeric_slider->SetValue(data.slider_pending());
            }
            if (pending_value_input) {
                pending_value_input->SetText(data.get_pending_display_text());
            }
        }
    });

    update_reset_button_state();
    notify_setting_changed(EVideoRowSettingChangeType::ValueChanged);
}

void UVideoSettingRowWidget::notify_setting_changed(EVideoRowSettingChangeType change_type) {
    on_setting_changed.Broadcast(change_type);
}

void UVideoSettingRowWidget::apply_pending_changes() {
    visit_row_data([this](auto& data) { data.write_pending_value_to_process(); });
    update_display_values();
    update_reset_button_state();
}

void UVideoSettingRowWidget::refresh_current_value() {
    visit_row_data([this](auto& data) { data.refresh_current_value(); });
    update_display_values();
    update_reset_button_state();
}

bool UVideoSettingRowWidget::has_pending_changes() const {
    return visit_row_data([](auto const& data) { return data.has_pending_change(); });
}
