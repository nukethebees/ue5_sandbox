#include "SpaceGame/ui/main_menu/OptionsWidget.h"

#include "SpaceGame/support/logging/SandboxLogCategories.h"

#include <Components/Button.h>
#include <Components/WidgetSwitcher.h>

namespace ml::ioj {
namespace {
FLinearColor const options_selected_tab_colour{0.16f, 0.38f, 0.55f, 1.f};
FLinearColor const options_inactive_tab_colour{0.12f, 0.14f, 0.16f, 1.f};
}

void UOptionsWidget::NativeOnInitialized() {
    Super::NativeOnInitialized();

    if (!IsValid(video_button) || !IsValid(gameplay_button) || !IsValid(audio_button) ||
        !IsValid(controls_button) || !IsValid(accessibility_button) || !IsValid(back_button)) {
        UE_LOG(LogSandboxUI,
               Error,
               TEXT("UOptionsWidget::NativeOnInitialized: One or more buttons are invalid."));
        return;
    }

    video_button->OnClicked.AddDynamic(this, &ThisClass::handle_video);
    gameplay_button->OnClicked.AddDynamic(this, &ThisClass::handle_gameplay);
    audio_button->OnClicked.AddDynamic(this, &ThisClass::handle_audio);
    controls_button->OnClicked.AddDynamic(this, &ThisClass::handle_controls);
    accessibility_button->OnClicked.AddDynamic(this, &ThisClass::handle_accessibility);
    back_button->OnClicked.AddDynamic(this, &ThisClass::handle_back);
}

void UOptionsWidget::NativeConstruct() {
    Super::NativeConstruct();
    set_active_tab(EOptionsTab::Video);
}

void UOptionsWidget::focus_active_tab() {
    if (!IsValid(video_button)) {
        UE_LOG(LogSandboxUI,
               Warning,
               TEXT("UOptionsWidget::focus_active_tab: Video button is invalid."));
        return;
    }

    video_button->SetKeyboardFocus();
}

void UOptionsWidget::handle_video() {
    set_active_tab(EOptionsTab::Video);
}

void UOptionsWidget::handle_gameplay() {
    set_active_tab(EOptionsTab::Gameplay);
}

void UOptionsWidget::handle_audio() {
    set_active_tab(EOptionsTab::Audio);
}

void UOptionsWidget::handle_controls() {
    set_active_tab(EOptionsTab::Controls);
}

void UOptionsWidget::handle_accessibility() {
    set_active_tab(EOptionsTab::Accessibility);
}

void UOptionsWidget::handle_back() {
    back_requested.Broadcast();
}

void UOptionsWidget::set_active_tab(EOptionsTab const tab) {
    if (!IsValid(tab_switcher) || !IsValid(video_page) || !IsValid(gameplay_page) ||
        !IsValid(audio_page) || !IsValid(controls_page) || !IsValid(accessibility_page) ||
        !IsValid(video_button) || !IsValid(gameplay_button) || !IsValid(audio_button) ||
        !IsValid(controls_button) || !IsValid(accessibility_button)) {
        UE_LOG(LogSandboxUI,
               Error,
               TEXT("UOptionsWidget::set_active_tab: One or more bound widgets are invalid."));
        return;
    }

    UWidget* page{nullptr};
    switch (tab) {
        case EOptionsTab::Video: {
            page = video_page;
            break;
        }
        case EOptionsTab::Gameplay: {
            page = gameplay_page;
            break;
        }
        case EOptionsTab::Audio: {
            page = audio_page;
            break;
        }
        case EOptionsTab::Controls: {
            page = controls_page;
            break;
        }
        case EOptionsTab::Accessibility: {
            page = accessibility_page;
            break;
        }
        default: {
            UE_LOG(LogSandboxUI,
                   Error,
                   TEXT("UOptionsWidget::set_active_tab: Unhandled tab value %d."),
                   static_cast<int32>(tab));
            return;
        }
    }

    active_tab_ = tab;
    tab_switcher->SetActiveWidget(page);
    set_tab_button_state(*video_button, tab == EOptionsTab::Video);
    set_tab_button_state(*gameplay_button, tab == EOptionsTab::Gameplay);
    set_tab_button_state(*audio_button, tab == EOptionsTab::Audio);
    set_tab_button_state(*controls_button, tab == EOptionsTab::Controls);
    set_tab_button_state(*accessibility_button, tab == EOptionsTab::Accessibility);
}

void UOptionsWidget::set_tab_button_state(UButton& button, bool const selected) {
    button.SetBackgroundColor(selected ? options_selected_tab_colour : options_inactive_tab_colour);
}
}
