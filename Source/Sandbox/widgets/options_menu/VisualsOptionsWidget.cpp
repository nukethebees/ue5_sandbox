#include "Sandbox/widgets/options_menu/VisualsOptionsWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Sandbox/widgets/options_menu/VideoSettingRowWidget.h"

void UVisualsOptionsWidget::NativeConstruct() {
    Super::NativeConstruct();

    log_verbose(TEXT("NativeConstruct() begin"));

    if (apply_button) {
        apply_button->OnClicked.AddDynamic(this, &UVisualsOptionsWidget::handle_apply_clicked);
        // Will be enabled when changes are pending
        apply_button->SetIsEnabled(false);
    } else {
        log_warning(TEXT("ApplyButton is null."));
    }

    if (!validate_widget_class()) {
        log_error(TEXT("video_setting_row_widget_class not set in Blueprint"));
        return;
    }

    initialize_video_settings();
    populate_settings_ui();

    log_verbose(TEXT("settings_container has %d children."),
                settings_container->GetChildrenCount());
    log_verbose(TEXT("NativeConstruct() end"));
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
    // Note: Frame rate limit uses float methods, so we'll add it as a float setting instead
    auto& float_settings_for_framerate{
        VideoSettingsHelpers::get_settings_array<float>(video_settings)};
    float_settings_for_framerate.Add(FloatSettingConfig{TEXT("Frame Rate Limit"),
                                                        EVideoSettingType::TextBox,
                                                        SettingRange<float>{30.0f, 240.0f},
                                                        &UGameUserSettings::GetFrameRateLimit,
                                                        &UGameUserSettings::SetFrameRateLimit});
}

void UVisualsOptionsWidget::populate_settings_ui() {
    if (!settings_scroll_box) {
        log_warning(TEXT("VisualsOptionsWidget: settings_scroll_box not bound"));
        return;
    }

    if (!settings_container) {
        log_error(TEXT("settings_container not bound to Blueprint widget"));
        return;
    }

    // Clear existing children and reset widget array
    settings_container->ClearChildren();
    setting_row_widgets.Empty();

    create_setting_rows();
}

void UVisualsOptionsWidget::create_setting_rows() {
    log_verbose(TEXT("Creating setting rows"));

    log_verbose(TEXT("Creating setting row: bool"));
    create_rows_for_type<bool>();

    log_verbose(TEXT("Creating setting row: float"));
    create_rows_for_type<float>();

    log_verbose(TEXT("Creating setting row: int32"));
    create_rows_for_type<int32>();
}

template <typename T>
void UVisualsOptionsWidget::create_rows_for_type() {
    auto const& settings_array{VideoSettingsHelpers::get_settings_array<T>(video_settings)};

    log_verbose(TEXT("Settings array has %d elements."), settings_array.Num());
    for (auto const& setting : settings_array) {
        auto const row_name{FString::Printf(TEXT("Row_%d"), row_idx)};
        log_verbose(TEXT("Constructing UVideoSettingRowWidget::%s"), *row_name);

        auto* row_widget{
            CreateWidget<UVideoSettingRowWidget>(this, video_setting_row_widget_class, *row_name)};
        if (!row_widget) {
            log_warning(TEXT("Failed to construct UVideoSettingRowWidget::%s"), *row_name);
            continue;
        }

        setting_row_widgets.Add(row_widget);
        // Add immediately to vertical box to trigger NativeConstruct()
        auto* vertical_slot{settings_container->AddChildToVerticalBox(row_widget)};
        if (vertical_slot) {
            vertical_slot->SetPadding(FMargin{0.0f, 2.0f, 0.0f, 2.0f});
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

        row_idx++;
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

bool UVisualsOptionsWidget::validate_widget_class() const {
    if (!video_setting_row_widget_class) {
        log_error(
            TEXT("video_setting_row_widget_class is not set. Please set it in the Blueprint."));
        return false;
    }
    return true;
}
