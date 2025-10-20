#include "Sandbox/widgets/options_menu/VisualsOptionsWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Sandbox/widgets/options_menu/VideoSettingRowWidget.h"

void UVisualsOptionsWidget::NativeConstruct() {
    Super::NativeConstruct();

    log_verbose(TEXT("NativeConstruct() begin"));

    if (apply_button) {
        apply_button->on_clicked.AddDynamic(this, &UVisualsOptionsWidget::handle_apply_clicked);
        apply_button->set_label(FText::FromString(TEXT("Apply")));
        apply_button->SetIsEnabled(false);
    } else {
        log_warning(TEXT("ApplyButton is null."));
    }

    if (reset_all_button) {
        reset_all_button->on_clicked.AddDynamic(this,
                                                &UVisualsOptionsWidget::handle_reset_all_clicked);
        reset_all_button->set_label(FText::FromString(TEXT("Reset All")));
        reset_all_button->SetIsEnabled(false);
    } else {
        log_warning(TEXT("ResetAllButton is null."));
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

                                        {},
                                        &UGameUserSettings::IsVSyncEnabled,
                                        &UGameUserSettings::SetVSyncEnabled});

    bool_settings.Add(BoolSettingConfig{TEXT("Dynamic Resolution"),

                                        {},
                                        &UGameUserSettings::IsDynamicResolutionEnabled,
                                        &UGameUserSettings::SetDynamicResolutionEnabled});

    // Initialize float settings
    auto& float_settings{VideoSettingsHelpers::get_settings_array<float>(video_settings)};
    float_settings.Add(FloatSettingConfig{TEXT("Resolution Scale"),

                                          SettingRange<float>{0.5f, 2.0f},
                                          &UGameUserSettings::GetResolutionScaleNormalized,
                                          &UGameUserSettings::SetResolutionScaleNormalized});

    // Initialize quality settings
    constexpr auto quality_range{
        SettingRange<EVisualQualityLevel>{EVisualQualityLevel::Low, EVisualQualityLevel::Epic}};
    auto& quality_settings{
        VideoSettingsHelpers::get_config_settings_array<QualitySettingConfig>(video_settings)};

    // Quality settings (Low, Medium, High, Epic, Cinematic)
    quality_settings.Add(QualitySettingConfig{TEXT("View Distance Quality"),

                                              quality_range,
                                              &UGameUserSettings::GetViewDistanceQuality,
                                              &UGameUserSettings::SetViewDistanceQuality});

    quality_settings.Add(QualitySettingConfig{TEXT("Shadow Quality"),

                                              quality_range,
                                              &UGameUserSettings::GetShadowQuality,
                                              &UGameUserSettings::SetShadowQuality});

    quality_settings.Add(QualitySettingConfig{TEXT("Global Illumination Quality"),

                                              quality_range,
                                              &UGameUserSettings::GetGlobalIlluminationQuality,
                                              &UGameUserSettings::SetGlobalIlluminationQuality});

    quality_settings.Add(QualitySettingConfig{TEXT("Reflection Quality"),

                                              quality_range,
                                              &UGameUserSettings::GetReflectionQuality,
                                              &UGameUserSettings::SetReflectionQuality});

    quality_settings.Add(QualitySettingConfig{TEXT("Anti-Aliasing Quality"),

                                              quality_range,
                                              &UGameUserSettings::GetAntiAliasingQuality,
                                              &UGameUserSettings::SetAntiAliasingQuality});

    quality_settings.Add(QualitySettingConfig{TEXT("Texture Quality"),

                                              quality_range,
                                              &UGameUserSettings::GetTextureQuality,
                                              &UGameUserSettings::SetTextureQuality});

    quality_settings.Add(QualitySettingConfig{TEXT("Visual Effect Quality"),

                                              quality_range,
                                              &UGameUserSettings::GetVisualEffectQuality,
                                              &UGameUserSettings::SetVisualEffectQuality});

    quality_settings.Add(QualitySettingConfig{TEXT("Post Processing Quality"),

                                              quality_range,
                                              &UGameUserSettings::GetPostProcessingQuality,
                                              &UGameUserSettings::SetPostProcessingQuality});

    quality_settings.Add(QualitySettingConfig{TEXT("Foliage Quality"),

                                              quality_range,
                                              &UGameUserSettings::GetFoliageQuality,
                                              &UGameUserSettings::SetFoliageQuality});

    quality_settings.Add(QualitySettingConfig{TEXT("Shading Quality"),

                                              quality_range,
                                              &UGameUserSettings::GetShadingQuality,
                                              &UGameUserSettings::SetShadingQuality});

    quality_settings.Add(QualitySettingConfig{TEXT("Audio Quality Level"),

                                              quality_range,
                                              &UGameUserSettings::GetAudioQualityLevel,
                                              &UGameUserSettings::SetAudioQualityLevel});

    // Overall Scalability Level uses Auto range to support -1 (Auto/Custom)
    constexpr auto scalability_range{
        SettingRange<EVisualQualityLevel>{EVisualQualityLevel::Auto, EVisualQualityLevel::Epic}};
    quality_settings.Add(QualitySettingConfig{TEXT("Overall Scalability Level"),

                                              scalability_range,
                                              &UGameUserSettings::GetOverallScalabilityLevel,
                                              &UGameUserSettings::SetOverallScalabilityLevel});

    // Initialize int settings
    auto& int_settings{VideoSettingsHelpers::get_settings_array<int32>(video_settings)};

    // Note: Frame rate limit uses float methods, so we'll add it as a float setting instead
    auto& float_settings_for_framerate{
        VideoSettingsHelpers::get_settings_array<float>(video_settings)};
    float_settings_for_framerate.Add(FloatSettingConfig{TEXT("Frame Rate Limit"),

                                                        SettingRange<float>{0.0f, 240.0f},
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

    create_setting_rows();
}

void UVisualsOptionsWidget::create_setting_rows() {
    log_verbose(TEXT("Creating setting rows"));

    log_verbose(TEXT("Creating setting row: bool"));
    create_rows_for_type<BoolSettingConfig>();

    log_verbose(TEXT("Creating setting row: float"));
    create_rows_for_type<FloatSettingConfig>();

    log_verbose(TEXT("Creating setting row: int32"));
    create_rows_for_type<IntSettingConfig>();

    log_verbose(TEXT("Creating setting row: EVisualQualityLevel"));
    create_rows_for_type<QualitySettingConfig>();
}

template <typename T>
void UVisualsOptionsWidget::create_rows_for_type() {
    auto const& settings_array{VideoSettingsHelpers::get_config_settings_array<T>(video_settings)};

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

        // Get current value from settings and create row data
        auto* game_settings{get_game_user_settings()};
        if (!game_settings || !setting.getter) {
            log_warning(TEXT("Failed to get game settings or getter is null for setting: %s"),
                        *setting.setting_name);
            continue;
        }

        auto const current_value{setting.get_value(*game_settings)};
        auto const row_data{RowData<T>(&setting, current_value)};
        row_widget->initialize_with_row_data(row_data);
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

    game_settings->ApplySettings(false);
    game_settings->SaveSettings();
    refresh_all_rows_from_settings();

    pending_changes_count = 0;
    update_button_states();

    log_log(TEXT("Applied video settings changes"));
}

void UVisualsOptionsWidget::handle_setting_changed(EVideoRowSettingChangeType change_type) {
    // Update counter based on change type
    switch (change_type) {
        case EVideoRowSettingChangeType::ValueChanged: {
            pending_changes_count++;
            break;
        }
        case EVideoRowSettingChangeType::ValueReset: {
            pending_changes_count = FMath::Max(0, pending_changes_count - 1);
            break;
        }
    }

    update_button_states();
}

void UVisualsOptionsWidget::handle_reset_all_clicked() {
    log_verbose(TEXT("handle_reset_all_clicked"));

    reset_all_settings_to_original();
    pending_changes_count = 0;
    update_button_states();
}

void UVisualsOptionsWidget::reset_all_settings_to_original() {
    for (auto* row_widget : setting_row_widgets) {
        if (row_widget) {
            row_widget->reset_to_original_value();
        }
    }
}

void UVisualsOptionsWidget::refresh_all_rows_from_settings() {
    for (auto* row_widget : setting_row_widgets) {
        if (row_widget) {
            row_widget->refresh_current_value();
        }
    }
}

bool UVisualsOptionsWidget::has_pending_changes() const {
    for (auto const* row_widget : setting_row_widgets) {
        if (row_widget && row_widget->has_pending_changes()) {
            return true;
        }
    }
    return false;
}

void UVisualsOptionsWidget::update_button_states() {
    bool const has_changes{pending_changes_count > 0};

    if (apply_button) {
        apply_button->SetIsEnabled(has_changes);
    }

    if (reset_all_button) {
        reset_all_button->SetIsEnabled(has_changes);
    }

    log_verbose(TEXT("Button states updated: %d pending changes"), pending_changes_count);
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
