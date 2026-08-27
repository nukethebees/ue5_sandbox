#include "SpaceGame/ui/PauseMenuWidget.h"

#include "SpaceGame/support/logging/SandboxLogCategories.h"

#include <Components/Button.h>
#include <Components/TextBlock.h>

namespace ml::ioj {
namespace {
FLinearColor const selected_tab_colour{0.16f, 0.38f, 0.55f, 1.f};
FLinearColor const inactive_tab_colour{0.12f, 0.14f, 0.16f, 1.f};
}

void UPauseMenuWidget::NativeOnInitialized() {
    Super::NativeOnInitialized();

    if (!IsValid(resume_button) || !IsValid(overview_button) || !IsValid(stats_button) ||
        !IsValid(options_button)) {
        UE_LOG(LogSandboxUI,
               Error,
               TEXT("UPauseMenuWidget::NativeOnInitialized: One or more buttons are invalid."));
        return;
    }

    resume_button->OnClicked.AddDynamic(this, &ThisClass::handle_resume);
    overview_button->OnClicked.AddDynamic(this, &ThisClass::handle_overview);
    stats_button->OnClicked.AddDynamic(this, &ThisClass::handle_stats);
    options_button->OnClicked.AddDynamic(this, &ThisClass::handle_options);
}

void UPauseMenuWidget::NativeConstruct() {
    Super::NativeConstruct();
    set_active_tab(EPauseMenuTab::Overview);
}

void UPauseMenuWidget::focus_resume_button() {
    if (!IsValid(resume_button)) {
        UE_LOG(LogSandboxUI,
               Warning,
               TEXT("UPauseMenuWidget::focus_resume_button: Resume button is invalid."));
        return;
    }

    resume_button->SetKeyboardFocus();
}

void UPauseMenuWidget::handle_resume() {
    resume_requested.Broadcast();
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
    set_tab_button_state(*overview_button, tab == EPauseMenuTab::Overview);
    set_tab_button_state(*stats_button, tab == EPauseMenuTab::Stats);
    set_tab_button_state(*options_button, tab == EPauseMenuTab::Options);
}

void UPauseMenuWidget::set_tab_button_state(UButton& button, bool const selected) {
    button.SetBackgroundColor(selected ? selected_tab_colour : inactive_tab_colour);
}
}
