#include "Sandbox/widgets/options_menu/VisualsOptionsWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Sandbox/widgets/options_menu/VideoSettingRowWidget.h"

void UVisualsOptionsWidget::NativeConstruct() {
    Super::NativeConstruct();

    if (apply_button) {
        apply_button->OnClicked.AddDynamic(this, &UVisualsOptionsWidget::handle_apply_clicked);
        apply_button->SetIsEnabled(false);
    }

    initialize_video_settings();
    populate_settings_ui();
}

void UVisualsOptionsWidget::initialize_video_settings() {
    // Initialize boolean settings
    auto& bool_settings{VideoSettingsHelpers::get_settings_array<bool>(video_settings)};
    bool_settings.Add(BoolSettingConfig{TEXT("VSync"),
                                        EVideoSettingType::Checkbox,
                                        {},
                                        &UGameUserSettings::IsVSyncEnabled,
                                        &UGameUserSettings::SetVSyncEnabled});

    // Initialize float settings
    auto& float_settings{VideoSettingsHelpers::get_settings_array<float>(video_settings)};
    float_settings.Add(FloatSettingConfig{TEXT("Resolution Scale"),
                                          EVideoSettingType::SliderWithText,
                                          SettingRange<float>{0.5f, 2.0f},
                                          &UGameUserSettings::GetResolutionScaleNormalized,
                                          &UGameUserSettings::SetResolutionScaleNormalized});

    // Initialize int settings
    auto& int_settings{VideoSettingsHelpers::get_settings_array<int32>(video_settings)};
    int_settings.Add(IntSettingConfig{
        TEXT("Frame Rate Limit"),
        EVideoSettingType::TextBox,
        SettingRange<int32>{30, 240},
        // Note: These getters/setters may need to be verified for UE 5.6
        nullptr, // &UGameUserSettings::GetFrameRateLimit,
        nullptr  // &UGameUserSettings::SetFrameRateLimit
    });
}

void UVisualsOptionsWidget::populate_settings_ui() {
    if (!settings_scroll_box) {
        UE_LOG(LogTemp, Warning, TEXT("VisualsOptionsWidget: settings_scroll_box not bound"));
        return;
    }

    // Create vertical box container for settings
    // settings_container = CreateWidget<UVerticalBox>(this);
    settings_container =
        WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("VerticalBox"));
    if (!settings_container) {
        UE_LOG(LogTemp, Error, TEXT("Failed to create settings container"));
        return;
    }

    settings_scroll_box->ClearChildren();
    settings_scroll_box->AddChild(settings_container);
    setting_row_widgets.Empty();

    create_setting_rows();
}

void UVisualsOptionsWidget::create_setting_rows() {
    create_rows_for_type<bool>();
    create_rows_for_type<float>();
    create_rows_for_type<int32>();
}

template <typename T>
void UVisualsOptionsWidget::create_rows_for_type() {
    auto const& settings_array{VideoSettingsHelpers::get_settings_array<T>(video_settings)};

    for (auto const& setting : settings_array) {
        auto* row_widget{CreateWidget<UVideoSettingRowWidget>(this)};
        if (!row_widget) {
            continue;
        }

        // Initialize the row widget for the specific type
        if constexpr (std::is_same_v<T, bool>) {
            row_widget->initialize_for_boolean_setting(setting);
        } else if constexpr (std::is_same_v<T, float>) {
            row_widget->initialize_for_float_setting(setting);
        } else if constexpr (std::is_same_v<T, int32>) {
            row_widget->initialize_for_int_setting(setting);
        }

        // Bind to setting change events
        row_widget->on_setting_changed.AddDynamic(this,
                                                  &UVisualsOptionsWidget::handle_setting_changed);

        // Add to vertical box container
        auto* vertical_slot{settings_container->AddChildToVerticalBox(row_widget)};
        if (vertical_slot) {
            vertical_slot->SetPadding(FMargin{0.0f, 2.0f, 0.0f, 2.0f});
        }

        setting_row_widgets.Add(row_widget);
    }
}

void UVisualsOptionsWidget::handle_apply_clicked() {
    auto* game_settings{get_game_user_settings()};
    if (!game_settings) {
        return;
    }

    // Apply all pending changes
    for (auto* row_widget : setting_row_widgets) {
        if (row_widget) {
            row_widget->apply_pending_changes();
        }
    }

    // Apply and save settings
    game_settings->ApplySettings(false);
    game_settings->SaveSettings();

    pending_changes = false;
    update_apply_button_state();

    UE_LOG(LogTemp, Log, TEXT("Applied video settings changes"));
}

void UVisualsOptionsWidget::handle_setting_changed() {
    pending_changes = has_pending_changes();
    update_apply_button_state();
}

bool UVisualsOptionsWidget::has_pending_changes() const {
    for (auto const* row_widget : setting_row_widgets) {
        if (row_widget && row_widget->has_pending_changes()) {
            return true;
        }
    }
    return false;
}

void UVisualsOptionsWidget::update_apply_button_state() {
    if (apply_button) {
        apply_button->SetIsEnabled(pending_changes);
    }
}

UGameUserSettings* UVisualsOptionsWidget::get_game_user_settings() const {
    return UGameUserSettings::GetGameUserSettings();
}
