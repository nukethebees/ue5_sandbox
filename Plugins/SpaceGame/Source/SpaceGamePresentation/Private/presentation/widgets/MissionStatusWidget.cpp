#include "SpaceGamePresentation/presentation/widgets/MissionStatusWidget.h"
#include <SpaceGamePresentation/support/logging/PresentationLogCategories.h>

#include <SandboxCore/error_msg.h>
#include <SandboxCoreEngine/enums.h>
#include <SandboxCoreEngine/uobject_utils.h>
#include <SandboxGameShared/ui/widgets/ValueWidget.h>
#include <SpaceGamePresentation/presentation/HUDManager.h>
#include <SpaceGameSimulation/support/logging/SandboxLogCategories.h>

void UMissionStatusWidget::NativeConstruct() {
    Super::NativeConstruct();
    check(check_widget_bindings());
}

auto UMissionStatusWidget::RebuildWidget() -> TSharedRef<SWidget> {
    auto const content{Super::RebuildWidget()};
    return hud_style_ ? ml::ioj::make_hud_panel(hud_style_.GetValue(), content) : content;
}

auto UMissionStatusWidget::check_widget_bindings() const -> bool {
    ml::FErrorMsg error_msg;
    if (ml::report_invalid_uobject_ptrs(
            {
                SANDBOX_NAMED_UOBJECT_PTR(mission_mode_widget),
                SANDBOX_NAMED_UOBJECT_PTR(mission_time_widget),
                SANDBOX_NAMED_UOBJECT_PTR(enemies_remaining_widget),
                SANDBOX_NAMED_UOBJECT_PTR(time_remaining_widget),
            },
            error_msg)) {
        UE_LOG(LogSandboxUI,
               Error,
               TEXT("UMissionStatusWidget: Invalid widget bindings: %s"),
               *error_msg.message);
        return false;
    }

    return true;
}

void UMissionStatusWidget::NativePreConstruct() {
    Super::NativePreConstruct();
    if (!check_widget_bindings()) {
        return;
    }

    mission_mode_widget->set_format_spec(mission_mode_format);
    mission_time_widget->set_format_spec(mission_time_format);
    enemies_remaining_widget->set_format_spec(enemies_remaining_format);
    time_remaining_widget->set_format_spec(time_remaining_format);

    if (IsDesignTime()) {
        set_mission_values(
            ETestMissionMode::KillEnemiesWithinTime, ETestMissionState::Running, 37.5f, 82.5f, 12);
    }
}

void UMissionStatusWidget::set_mission_data(ml::hud_manager::FMissionDataCache const& data) {
    set_mission_values(data.static_data.mission_mode,
                       data.status_data.mission_state,
                       data.status_data.mission_stopwatch,
                       data.status_data.time_remaining,
                       data.status_data.enemies_remaining);
}

void UMissionStatusWidget::set_mission_mode(ETestMissionMode const new_mode,
                                            ETestMissionState const initial_state) {
    current_mission_mode = new_mode;
    auto const mode_name{ml::to_display_string_view(new_mode)};
    auto const state_name{ml::to_string_without_type_prefix(initial_state)};
    mission_mode_widget->update(mode_name, FStringView{state_name});
    apply_mission_state_style(initial_state);
}

void UMissionStatusWidget::set_mission_state(ETestMissionState const new_state) {
    auto const mode_name{ml::to_display_string_view(current_mission_mode)};
    auto const state_name{ml::to_string_without_type_prefix(new_state)};
    mission_mode_widget->update(mode_name, FStringView{state_name});
    apply_mission_state_style(new_state);
}

void UMissionStatusWidget::apply_hud_style(ml::ioj::FGameHudStyle const& style) {
    hud_style_ = style;
    mission_mode_widget->set_format_spec(TEXT("{0} // {1}"));
    mission_time_widget->set_format_spec(TEXT("MISSION TIME // {0}"));
    enemies_remaining_widget->set_format_spec(TEXT("ENEMIES REMAINING // {0}"));
    time_remaining_widget->set_format_spec(TEXT("TIME REMAINING // {0}"));
    mission_mode_widget->set_text_style(style.primary_text);
    mission_time_widget->set_text_style(style.secondary_text);
    enemies_remaining_widget->set_text_style(style.secondary_text);
    time_remaining_widget->set_text_style(style.warning_text);
}

void UMissionStatusWidget::apply_mission_state_style(ETestMissionState const state) {
    if (!hud_style_ || !mission_mode_widget) {
        return;
    }

    auto const& style{hud_style_.GetValue()};
    auto const* text_style{&style.primary_text};
    if (state == ETestMissionState::Succeeded) {
        text_style = &style.success_text;
    } else if (state == ETestMissionState::Failed) {
        text_style = &style.danger_text;
    } else if (state == ETestMissionState::Running) {
        text_style = &style.accent_text;
    }
    mission_mode_widget->set_text_style(*text_style);
}

void UMissionStatusWidget::set_mission_time(float const mission_time) {
    mission_time_widget->update(mission_time);
}

void UMissionStatusWidget::set_enemies_remaining(int32 const enemies_remaining) {
    auto const show_enemies{current_mission_mode == ETestMissionMode::KillEnemies ||
                            current_mission_mode == ETestMissionMode::KillEnemiesWithinTime};
    enemies_remaining_widget->SetVisibility(show_enemies ? ESlateVisibility::Visible
                                                         : ESlateVisibility::Collapsed);
    if (show_enemies) {
        enemies_remaining_widget->update(enemies_remaining);
    }
}

void UMissionStatusWidget::set_time_remaining(float const time_remaining) {
    auto const show_time{current_mission_mode == ETestMissionMode::SurviveTime ||
                         current_mission_mode == ETestMissionMode::KillEnemiesWithinTime};
    time_remaining_widget->SetVisibility(show_time ? ESlateVisibility::Visible
                                                   : ESlateVisibility::Collapsed);
    if (show_time) {
        time_remaining_widget->update(time_remaining);
    }
}

void UMissionStatusWidget::set_font_size(int32 const new_font_size) {
    font_size = new_font_size;
    mission_mode_widget->set_font_size(font_size);
    mission_time_widget->set_font_size(font_size);
    enemies_remaining_widget->set_font_size(font_size);
    time_remaining_widget->set_font_size(font_size);
}

void UMissionStatusWidget::set_mission_values(ETestMissionMode const mission_mode,
                                              ETestMissionState const mission_state,
                                              float const mission_time,
                                              float const time_remaining,
                                              int32 const enemies_remaining) {
    set_mission_mode(mission_mode, mission_state);
    set_mission_time(mission_time);
    set_enemies_remaining(enemies_remaining);
    set_time_remaining(time_remaining);
}
