#include "SpaceGame/ui/main_menu/MainMenuLandingWidget.h"

#include "SMainMenuView.h"
#include "SpaceGame/support/logging/SandboxLogCategories.h"
#include "SpaceGame/system/GameSubsystem.h"
#include "SpaceGame/ui/style/SpaceGameUiTheme.h"

#include <Engine/GameInstance.h>

namespace ml::ioj {
UMainMenuLandingWidget::UMainMenuLandingWidget(FObjectInitializer const& object_initializer)
    : Super(object_initializer) {
    SetIsFocusable(true);
}

void UMainMenuLandingWidget::NativeOnInitialized() {
    Super::NativeOnInitialized();

    auto* const game_instance{GetGameInstance()};
    game_ = IsValid(game_instance) ? game_instance->GetSubsystem<UGameSubsystem>() : nullptr;
    if (!IsValid(game_)) {
        UE_LOG(LogSandboxUI,
               Warning,
               TEXT("UMainMenuLandingWidget: Game subsystem is unavailable; using the default "
                    "UI theme."));
        auto const* const default_theme{GetDefault<USpaceGameUiTheme>()};
        check(IsValid(default_theme));
        fallback_style_ = default_theme->compile();
    }
}

auto UMainMenuLandingWidget::RebuildWidget() -> TSharedRef<SWidget> {
    auto const* const style{IsValid(game_) ? &game_->get_ui_style() : &fallback_style_};
    return SAssignNew(view_, SMainMenuView)
        .Style(style)
        .InitialAction(preferred_action_)
        .OnSelectMission(FSimpleDelegate::CreateUObject(this, &ThisClass::handle_select_mission))
        .OnSaveData(FSimpleDelegate::CreateUObject(this, &ThisClass::handle_save_data))
        .OnOptions(FSimpleDelegate::CreateUObject(this, &ThisClass::handle_options))
        .OnQuitGame(FSimpleDelegate::CreateUObject(this, &ThisClass::handle_quit_game));
}

void UMainMenuLandingWidget::ReleaseSlateResources(bool const release_children) {
    Super::ReleaseSlateResources(release_children);
    view_.Reset();
}

auto UMainMenuLandingWidget::NativeOnFocusReceived(FGeometry const& geometry,
                                                   FFocusEvent const& focus_event) -> FReply {
    static_cast<void>(geometry);
    static_cast<void>(focus_event);
    focus_preferred_action();
    return FReply::Handled();
}

void UMainMenuLandingWidget::set_preferred_action(EMainMenuAction const action) {
    preferred_action_ = action;
}

void UMainMenuLandingWidget::focus_preferred_action() {
    if (view_.IsValid()) {
        view_->focus_action(preferred_action_);
    }
}

void UMainMenuLandingWidget::handle_select_mission() {
    preferred_action_ = EMainMenuAction::SelectMission;
    select_mission_requested.Broadcast();
}

void UMainMenuLandingWidget::handle_save_data() {
    preferred_action_ = EMainMenuAction::SaveData;
    save_data_requested.Broadcast();
}

void UMainMenuLandingWidget::handle_options() {
    preferred_action_ = EMainMenuAction::Options;
    options_requested.Broadcast();
}

void UMainMenuLandingWidget::handle_quit_game() {
    preferred_action_ = EMainMenuAction::QuitGame;
    quit_game_requested.Broadcast();
}
} // namespace ml::ioj
