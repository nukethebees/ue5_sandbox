#include "Sandbox/ui/options_menu/widgets/umg/OptionsWidget.h"

#include "Logging/StructuredLog.h"


void UOptionsWidget::NativeConstruct() {
    Super::NativeConstruct();

    // Bind tab button clicks
    if (gameplay_tab_button) {
        gameplay_tab_button->on_clicked.AddUObject(this, &UOptionsWidget::handle_gameplay_tab);
    }
    if (visuals_tab_button) {
        visuals_tab_button->on_clicked.AddUObject(this, &UOptionsWidget::handle_visuals_tab);
    }
    if (audio_tab_button) {
        audio_tab_button->on_clicked.AddUObject(this, &UOptionsWidget::handle_audio_tab);
    }
    if (controls_tab_button) {
        controls_tab_button->on_clicked.AddUObject(this, &UOptionsWidget::handle_controls_tab);
    }

    // Bind back button click
    if (back_button) {
        back_button->on_clicked.AddUObject(this, &UOptionsWidget::handle_back);
    }

    // Set initial tab
    set_active_tab(EOptionsTab::Gameplay);
}

void UOptionsWidget::handle_gameplay_tab() {
    set_active_tab(EOptionsTab::Gameplay);
}

void UOptionsWidget::handle_visuals_tab() {
    set_active_tab(EOptionsTab::Visuals);
}

void UOptionsWidget::handle_audio_tab() {
    set_active_tab(EOptionsTab::Audio);
}

void UOptionsWidget::handle_controls_tab() {
    set_active_tab(EOptionsTab::Controls);
}

void UOptionsWidget::handle_back() {
    back_requested.Broadcast();
}

void UOptionsWidget::set_active_tab(EOptionsTab tab) {
    current_tab = tab;

    if (tab_switcher) {
        log_verbose(TEXT("Setting active widget to: %s"), *UEnum::GetValueAsString(tab));
        tab_switcher->SetActiveWidgetIndex(static_cast<int32>(tab));
    } else {
        UE_LOGFMT(LogTemp, Warning, "tab_switcher is nullptr.");
    }

    update_tab_button_states(tab);
}

void UOptionsWidget::update_tab_button_states(EOptionsTab active_tab) {
    // This would typically update button appearance to show which tab is active
    // Implementation depends on how TextButtonWidget handles selected states
    // For now, just ensuring the logic structure is in place
}
