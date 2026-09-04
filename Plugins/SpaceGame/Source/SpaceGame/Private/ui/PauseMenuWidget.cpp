#include "SpaceGame/ui/PauseMenuWidget.h"

#include "SpaceGame/support/logging/SandboxLogCategories.h"
#include "SpaceGame/ui/common/MenuButtonWidget.h"

#include <Components/TextBlock.h>
#include <Input/CommonUIInputTypes.h>
#include <Input/UIActionBinding.h>
#include <InputAction.h>

namespace ml::ioj {
void UPauseMenuWidget::NativeOnInitialized() {
    Super::NativeOnInitialized();

    if (!IsValid(resume_button) || !IsValid(overview_button) || !IsValid(stats_button) ||
        !IsValid(options_button) || !IsValid(return_to_level_select_button) ||
        !IsValid(quit_button)) {
        UE_LOG(LogSandboxUI,
               Error,
               TEXT("UPauseMenuWidget::NativeOnInitialized: One or more buttons are invalid."));
        return;
    }

    resume_button->OnClicked().AddUObject(this, &ThisClass::handle_resume);
    overview_button->OnClicked().AddUObject(this, &ThisClass::handle_overview);
    stats_button->OnClicked().AddUObject(this, &ThisClass::handle_stats);
    options_button->OnClicked().AddUObject(this, &ThisClass::handle_options);
    return_to_level_select_button->OnClicked().AddUObject(
        this, &ThisClass::handle_return_to_level_select);
    quit_button->OnClicked().AddUObject(this, &ThisClass::handle_quit);
    overview_button->SetIsSelectable(true);
    overview_button->SetIsToggleable(true);
    stats_button->SetIsSelectable(true);
    stats_button->SetIsToggleable(true);
    options_button->SetIsSelectable(true);
    options_button->SetIsToggleable(true);
    set_active_tab(EPauseMenuTab::Overview);
}

void UPauseMenuWidget::prepare_for_open(UInputAction& toggle_action) {
    toggle_action_ = &toggle_action;
    terminal_action_requested_ = false;
    set_active_tab(EPauseMenuTab::Overview);
}

void UPauseMenuWidget::NativeOnActivated() {
    set_active_tab(EPauseMenuTab::Overview);
    if (auto* const toggle_action{toggle_action_.Get()}; IsValid(toggle_action)) {
        FBindUIActionArgs const args{
            toggle_action,
            false,
            FSimpleDelegate::CreateUObject(this, &ThisClass::handle_toggle_action)};
        toggle_action_binding_ = RegisterUIActionBinding(args);
    }
    Super::NativeOnActivated();
}

void UPauseMenuWidget::NativeOnDeactivated() {
    if (toggle_action_binding_.IsValid()) {
        toggle_action_binding_.Unregister();
        toggle_action_binding_ = {};
    }
    Super::NativeOnDeactivated();
}

auto UPauseMenuWidget::NativeGetDesiredFocusTarget() const -> UWidget* {
    return resume_button;
}

void UPauseMenuWidget::handle_resume() {
    if (terminal_action_requested_) {
        return;
    }
    DeactivateWidget();
}

void UPauseMenuWidget::handle_overview() {
    set_active_tab(EPauseMenuTab::Overview);
}

void UPauseMenuWidget::handle_stats() {
    set_active_tab(EPauseMenuTab::Stats);
}

void UPauseMenuWidget::handle_options() {
    set_active_tab(EPauseMenuTab::Options);
}

void UPauseMenuWidget::handle_return_to_level_select() {
    if (terminal_action_requested_) {
        return;
    }
    terminal_action_requested_ = true;
    return_to_level_select_requested.Broadcast();
}

void UPauseMenuWidget::handle_quit() {
    if (terminal_action_requested_) {
        return;
    }
    terminal_action_requested_ = true;
    quit_requested.Broadcast();
}

void UPauseMenuWidget::handle_toggle_action() {
    if (terminal_action_requested_) {
        return;
    }
    DeactivateWidget();
}

void UPauseMenuWidget::set_active_tab(EPauseMenuTab const tab) {
    if (!IsValid(page_heading) || !IsValid(page_placeholder) || !IsValid(overview_button) ||
        !IsValid(stats_button) || !IsValid(options_button)) {
        UE_LOG(LogSandboxUI,
               Error,
               TEXT("UPauseMenuWidget::set_active_tab: One or more bound widgets are invalid."));
        return;
    }

    FText heading;
    FText placeholder;
    switch (tab) {
        case EPauseMenuTab::Overview: {
            heading = NSLOCTEXT("PauseMenu", "OverviewHeading", "Overview");
            placeholder =
                NSLOCTEXT("PauseMenu", "OverviewPlaceholder", "Overview placeholder content");
            break;
        }
        case EPauseMenuTab::Stats: {
            heading = NSLOCTEXT("PauseMenu", "StatsHeading", "Stats");
            placeholder = NSLOCTEXT("PauseMenu", "StatsPlaceholder", "Stats placeholder content");
            break;
        }
        case EPauseMenuTab::Options: {
            heading = NSLOCTEXT("PauseMenu", "OptionsHeading", "Options");
            placeholder =
                NSLOCTEXT("PauseMenu", "OptionsPlaceholder", "Options placeholder content");
            break;
        }
        default: {
            UE_LOG(LogSandboxUI,
                   Error,
                   TEXT("UPauseMenuWidget::set_active_tab: Unhandled tab value %d."),
                   static_cast<int32>(tab));
            return;
        }
    }

    active_tab = tab;
    page_heading->SetText(heading);
    page_placeholder->SetText(placeholder);
    overview_button->SetIsSelected(tab == EPauseMenuTab::Overview);
    stats_button->SetIsSelected(tab == EPauseMenuTab::Stats);
    options_button->SetIsSelected(tab == EPauseMenuTab::Options);
}
}
