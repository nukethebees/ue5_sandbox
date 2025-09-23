#include "Sandbox/widgets/options_menu/OptionsWidget.h"

void UOptionsWidget::NativeConstruct() {
    Super::NativeConstruct();

    // Validate widget classes are set in Blueprint
    if (!validate_widget_classes()) {
        log_error(TEXT("Widget classes not properly set in Blueprint"));
        return;
    }

    // Bind tab button clicks
    if (gameplay_tab_button) {
        gameplay_tab_button->on_clicked.AddDynamic(this, &UOptionsWidget::handle_gameplay_tab);
    }
    if (visuals_tab_button) {
        visuals_tab_button->on_clicked.AddDynamic(this, &UOptionsWidget::handle_visuals_tab);
    }
    if (audio_tab_button) {
        audio_tab_button->on_clicked.AddDynamic(this, &UOptionsWidget::handle_audio_tab);
    }
    if (controls_tab_button) {
        controls_tab_button->on_clicked.AddDynamic(this, &UOptionsWidget::handle_controls_tab);
    }

    // Bind back button click
    if (back_button) {
        back_button->on_clicked.AddDynamic(this, &UOptionsWidget::handle_back);
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

bool UOptionsWidget::validate_widget_classes() const {
    bool all_valid{true};

    if (!gameplay_options_widget_class) {
        log_warning(TEXT("gameplay_options_widget_class not set in Blueprint"));
        all_valid = false;
    }
    if (!visuals_options_widget_class) {
        log_warning(TEXT("visuals_options_widget_class not set in Blueprint"));
        all_valid = false;
    }
    if (!audio_options_widget_class) {
        log_warning(TEXT("audio_options_widget_class not set in Blueprint"));
        all_valid = false;
    }
    if (!controls_options_widget_class) {
        log_warning(TEXT("controls_options_widget_class not set in Blueprint"));
        all_valid = false;
    }

    return all_valid;
}
