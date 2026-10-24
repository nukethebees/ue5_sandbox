#include "SpaceGamePresentation/presentation/widgets/MissionStatusWidget.h"

#include <SandboxGameShared/ui/widgets/ValueWidget.h>
#include <SpaceGamePresentation/presentation/HUDManager.h>
#include <SpaceGamePresentation/support/logging/PresentationLogCategories.h>
#include <SpaceGameSimulation/support/logging/SandboxLogCategories.h>

#include <SandboxCore/error_msg.h>
#include <SandboxCoreEngine/enums.h>
#include <SandboxCoreEngine/strings.h>
#include <SandboxCoreEngine/uobject_utils.h>

void UMissionStatusWidget::NativeConstruct() {
    Super::NativeConstruct();
    check(check_widget_bindings());
}

auto UMissionStatusWidget::RebuildWidget() -> TSharedRef<SWidget> {
    auto const content{Super::RebuildWidget()};
    return hud_style_ ? ioj::make_hud_panel(hud_style_.GetValue(), content) : content;
}

auto UMissionStatusWidget::check_widget_bindings() const -> bool {
    ioj::FErrorMsg error_msg;
    if (ioj::report_invalid_uobject_ptrs(
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
    // Preserve widget binding names used by existing assets, with objective-specific labels.
    mission_mode_widget->set_format_spec(TEXT("OBJECTIVES // {0}"));
    mission_time_widget->set_format_spec(TEXT("ACTIVE // {0}"));
    enemies_remaining_widget->set_format_spec(TEXT("COMPLETED // {0}"));
    time_remaining_widget->set_format_spec(TEXT("WAITING // {0}"));
    if (IsDesignTime()) {
        set_mission_data({.active_titles = TEXT("Protect the convoy"),
                          .active = 1,
                          .completed = 2,
                          .inactive = 1});
    }
}

void UMissionStatusWidget::set_mission_data(ioj::hud_manager::FMissionDataCache const& data) {
    mission_mode_widget->update(data.active_titles);
    mission_time_widget->update(data.active);
    enemies_remaining_widget->update(data.completed);
    time_remaining_widget->update(data.inactive);
}

void UMissionStatusWidget::apply_hud_style(ioj::FGameHudStyle const& style) {
    hud_style_ = style;
    mission_mode_widget->set_format_spec(TEXT("OBJECTIVES // {0}"));
    mission_time_widget->set_format_spec(TEXT("ACTIVE // {0}"));
    enemies_remaining_widget->set_format_spec(TEXT("COMPLETED // {0}"));
    time_remaining_widget->set_format_spec(TEXT("WAITING // {0}"));
    mission_mode_widget->set_text_style(style.primary_text);
    mission_time_widget->set_text_style(style.secondary_text);
    enemies_remaining_widget->set_text_style(style.secondary_text);
    time_remaining_widget->set_text_style(style.warning_text);
}

void UMissionStatusWidget::set_font_size(int32 const new_font_size) {
    font_size = new_font_size;
    mission_mode_widget->set_font_size(font_size);
    mission_time_widget->set_font_size(font_size);
    enemies_remaining_widget->set_font_size(font_size);
    time_remaining_widget->set_font_size(font_size);
}
