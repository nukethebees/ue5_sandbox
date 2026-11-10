#include "SpaceGame/ui/LevelCompletionWidget.h"

#include "SLevelCompletionView.h"
#include "SpaceGame/system/GameSubsystem.h"
#include "SpaceGamePresentation/ui/style/SpaceGameUiTheme.h"
#include "SpaceGameSimulation/support/logging/SandboxLogCategories.h"
#include <SpaceGamePresentation/support/logging/PresentationLogCategories.h>

#include <Components/NativeWidgetHost.h>
#include <Engine/GameInstance.h>

namespace ioj {
ULevelCompletionWidget::ULevelCompletionWidget() {
    SetIsFocusable(true);
}

void ULevelCompletionWidget::prepare_for_open(FLevelCompletion completion) {
    completion_ = MoveTemp(completion);
    action_requested_ = false;
    publish_report();
}

void ULevelCompletionWidget::NativeOnInitialized() {
    Super::NativeOnInitialized();

    auto* const game_instance{GetGameInstance()};
    game_ = UGameSubsystem::get(game_instance);
    if (!IsValid(game_)) {
        UE_LOG(LogSandboxUI,
               Warning,
               TEXT("ULevelCompletionWidget: Game subsystem is unavailable; using the default "
                    "UI theme."));
        auto const* const default_theme{GetDefault<USpaceGameUiTheme>()};
        check(IsValid(default_theme));
        fallback_style_ = default_theme->compile();
    }

    if (!IsValid(view_host)) {
        UE_LOG(LogSandboxUI,
               Error,
               TEXT("ULevelCompletionWidget: The native view host is unavailable."));
        return;
    }

    auto const* const style{IsValid(game_) ? &game_->get_ui_style() : &fallback_style_};
    view_host->SetContent(SAssignNew(view_, SLevelCompletionView)
                              .Style(style)
                              .Audio(IsValid(game_) ? game_->get_audio() : FGameAudioFacade{})
                              .OnReturnToMissionControl(FSimpleDelegate::CreateUObject(
                                  this, &ThisClass::request_return_to_mission_control)));
    publish_report();
}

auto ULevelCompletionWidget::NativeGetDesiredFocusTarget() const -> UWidget* {
    return const_cast<ULevelCompletionWidget*>(this);
}

auto ULevelCompletionWidget::NativeOnHandleBackAction() -> bool {
    return true;
}

auto ULevelCompletionWidget::NativeOnFocusReceived(FGeometry const& geometry,
                                                   FFocusEvent const& focus_event) -> FReply {
    static_cast<void>(geometry);
    static_cast<void>(focus_event);
    if (view_.IsValid()) {
        view_->focus_primary_action();
    }
    return FReply::Handled();
}

void ULevelCompletionWidget::ReleaseSlateResources(bool const release_children) {
    Super::ReleaseSlateResources(release_children);
    view_.Reset();
}

void ULevelCompletionWidget::request_return_to_mission_control() {
    if (action_requested_) {
        return;
    }
    action_requested_ = true;
    return_to_level_select_requested.Broadcast();
}

void ULevelCompletionWidget::publish_report() {
    if (view_.IsValid()) {
        view_->update_report(completion_);
    }
}
}
