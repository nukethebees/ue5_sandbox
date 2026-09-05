#include "SpaceGame/ui/main_menu/OptionsWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Engine/GameInstance.h"
#include "SpaceGame/settings/GameSettingsSubsystem.h"
#include "SpaceGame/support/logging/SandboxLogCategories.h"
#include "SpaceGame/ui/main_menu/SettingsRowWidget.h"

#include <Components/Button.h>
#include <Components/HorizontalBox.h>
#include <Components/Overlay.h>
#include <Components/PanelWidget.h>
#include <Components/ScrollBox.h>
#include <Components/TextBlock.h>
#include <Components/VerticalBox.h>
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

    auto* game_instance{GetGameInstance()};
    if (!IsValid(game_instance)) {
        UE_LOG(LogSandboxUI,
               Warning,
               TEXT("UOptionsWidget: Game instance is invalid; settings UI is unavailable."));
        return;
    }
    settings_ = game_instance->GetSubsystem<UGameSettingsSubsystem>();
    if (!IsValid(settings_)) {
        UE_LOG(LogSandboxUI, Error, TEXT("UOptionsWidget: Settings subsystem is invalid."));
        return;
    }
    settings_->settings_changed.AddUObject(this, &ThisClass::refresh_settings_ui);
    settings_->display_confirmation_changed.AddUObject(
        this, &ThisClass::handle_display_confirmation_changed);
}

void UOptionsWidget::NativeConstruct() {
    Super::NativeConstruct();
    if (IsValid(settings_)) {
        settings_->begin_edit();
        build_settings_pages();
        refresh_settings_ui();
    }
    set_active_tab(EOptionsTab::Video);
}

void UOptionsWidget::focus_active_tab() {
    auto* const focus_target{get_focus_target()};
    if (!IsValid(focus_target)) {
        UE_LOG(LogSandboxUI,
               Warning,
               TEXT("UOptionsWidget::focus_active_tab: Active tab button is invalid."));
        return;
    }

    focus_target->SetKeyboardFocus();
}

auto UOptionsWidget::get_focus_target() const -> UWidget* {
    switch (active_tab_) {
        case EOptionsTab::Video: {
            return video_button;
        }
        case EOptionsTab::Gameplay: {
            return gameplay_button;
        }
        case EOptionsTab::Audio: {
            return audio_button;
        }
        case EOptionsTab::Controls: {
            return controls_button;
        }
        case EOptionsTab::Accessibility: {
            return accessibility_button;
        }
    }
    return nullptr;
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
    if (IsValid(settings_) && settings_->is_dirty()) {
        if (IsValid(dirty_modal_)) {
            dirty_modal_->SetVisibility(ESlateVisibility::Visible);
        }
        return;
    }
    if (IsValid(settings_)) {
        settings_->cancel();
    }
    back_requested.Broadcast();
}

void UOptionsWidget::handle_apply() {
    if (IsValid(settings_)) {
        settings_->apply();
    }
}

void UOptionsWidget::handle_reset() {
    if (IsValid(settings_)) {
        settings_->reset_category(active_category());
    }
}

void UOptionsWidget::handle_dirty_apply() {
    if (!IsValid(settings_)) {
        return;
    }
    settings_->apply();
    if (IsValid(dirty_modal_)) {
        dirty_modal_->SetVisibility(ESlateVisibility::Collapsed);
    }
    if (!settings_->is_awaiting_display_confirmation()) {
        back_requested.Broadcast();
    }
}

void UOptionsWidget::handle_dirty_discard() {
    if (IsValid(settings_)) {
        settings_->cancel();
    }
    if (IsValid(dirty_modal_)) {
        dirty_modal_->SetVisibility(ESlateVisibility::Collapsed);
    }
    back_requested.Broadcast();
}

void UOptionsWidget::handle_dirty_stay() {
    if (IsValid(dirty_modal_)) {
        dirty_modal_->SetVisibility(ESlateVisibility::Collapsed);
    }
}

void UOptionsWidget::handle_confirm_display() {
    if (IsValid(settings_)) {
        settings_->confirm_display_changes();
    }
}

void UOptionsWidget::handle_revert_display() {
    if (IsValid(settings_)) {
        settings_->revert_display_changes();
    }
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
    refresh_settings_ui();
}

void UOptionsWidget::set_tab_button_state(UButton& button, bool const selected) {
    button.SetBackgroundColor(selected ? options_selected_tab_colour : options_inactive_tab_colour);
}

void UOptionsWidget::build_settings_pages() {
    if (settings_pages_built_ || !IsValid(settings_)) {
        return;
    }
    settings_pages_built_ = true;
    build_category_page(*video_page, EGameSettingCategory::Video);
    build_category_page(*gameplay_page, EGameSettingCategory::Gameplay);
    build_category_page(*audio_page, EGameSettingCategory::Audio);
    build_category_page(*controls_page, EGameSettingCategory::Controls);
    build_category_page(*accessibility_page, EGameSettingCategory::Accessibility);
    build_modals();
}

void UOptionsWidget::build_category_page(UWidget& page, EGameSettingCategory const category) {
    auto* panel{Cast<UPanelWidget>(&page)};
    if (!IsValid(panel)) {
        UE_LOG(
            LogSandboxUI, Error, TEXT("Options page %s is not a panel widget."), *page.GetName());
        return;
    }
    panel->ClearChildren();

    auto* scroll{WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass())};
    auto* content{WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass())};
    scroll->AddChild(content);
    panel->AddChild(scroll);

    auto const descriptors{settings_->descriptors(category)};
    for (auto const* descriptor : descriptors) {
        auto* row{WidgetTree->ConstructWidget<USettingsRowWidget>(
            USettingsRowWidget::StaticClass(),
            *FString::Printf(TEXT("setting_%s"), descriptor->name))};
        row->configure(*settings_, *descriptor);
        content->AddChildToVerticalBox(row);
        settings_rows_.Add(row);
    }

    if (descriptors.IsEmpty()) {
        auto* empty_text{WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass())};
        empty_text->SetText(FText::FromString(TEXT("No settings in this category yet.")));
        content->AddChildToVerticalBox(empty_text);
    }

    auto* actions{WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass())};
    auto* apply{WidgetTree->ConstructWidget<UButton>(UButton::StaticClass())};
    auto* apply_text{WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass())};
    apply_text->SetText(FText::FromString(TEXT("Apply")));
    apply->AddChild(apply_text);
    apply->OnClicked.AddDynamic(this, &ThisClass::handle_apply);
    actions->AddChildToHorizontalBox(apply);
    apply_buttons_.Add(apply);

    auto* reset{WidgetTree->ConstructWidget<UButton>(UButton::StaticClass())};
    auto* reset_text{WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass())};
    reset_text->SetText(FText::FromString(TEXT("Reset Category")));
    reset->AddChild(reset_text);
    reset->OnClicked.AddDynamic(this, &ThisClass::handle_reset);
    actions->AddChildToHorizontalBox(reset);
    content->AddChildToVerticalBox(actions);
}

void UOptionsWidget::build_modals() {
    auto make_button = [this](UVerticalBox& content, TCHAR const* label) {
        auto* button{WidgetTree->ConstructWidget<UButton>(UButton::StaticClass())};
        auto* text{WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass())};
        text->SetText(FText::FromString(label));
        button->AddChild(text);
        content.AddChildToVerticalBox(button);
        return button;
    };

    dirty_modal_ =
        WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("dirty_modal"));
    auto* dirty_content{WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass())};
    auto* dirty_text{WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass())};
    dirty_text->SetText(FText::FromString(TEXT("Apply unsaved settings?")));
    dirty_content->AddChildToVerticalBox(dirty_text);
    make_button(*dirty_content, TEXT("Apply"))
        ->OnClicked.AddDynamic(this, &ThisClass::handle_dirty_apply);
    make_button(*dirty_content, TEXT("Discard"))
        ->OnClicked.AddDynamic(this, &ThisClass::handle_dirty_discard);
    make_button(*dirty_content, TEXT("Stay"))
        ->OnClicked.AddDynamic(this, &ThisClass::handle_dirty_stay);
    dirty_modal_->AddChild(dirty_content);
    dirty_modal_->SetVisibility(ESlateVisibility::Collapsed);
    root_widget->AddChildToOverlay(dirty_modal_);

    display_modal_ =
        WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("display_modal"));
    auto* display_content{WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass())};
    display_countdown_ = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    display_content->AddChildToVerticalBox(display_countdown_);
    make_button(*display_content, TEXT("Keep Changes"))
        ->OnClicked.AddDynamic(this, &ThisClass::handle_confirm_display);
    make_button(*display_content, TEXT("Revert"))
        ->OnClicked.AddDynamic(this, &ThisClass::handle_revert_display);
    display_modal_->AddChild(display_content);
    display_modal_->SetVisibility(ESlateVisibility::Collapsed);
    root_widget->AddChildToOverlay(display_modal_);
}

void UOptionsWidget::refresh_settings_ui() {
    if (!IsValid(settings_)) {
        return;
    }
    for (auto const& row_ptr : settings_rows_) {
        auto* row{row_ptr.Get()};
        if (IsValid(row)) {
            row->refresh();
        }
    }
    for (auto const& button_ptr : apply_buttons_) {
        auto* button{button_ptr.Get()};
        if (IsValid(button)) {
            button->SetIsEnabled(settings_->is_dirty() &&
                                 !settings_->is_awaiting_display_confirmation());
        }
    }
    if (IsValid(display_countdown_) && settings_->is_awaiting_display_confirmation()) {
        display_countdown_->SetText(FText::FromString(
            FString::Printf(TEXT("Keep these display settings? Reverting in %d seconds."),
                            settings_->display_confirmation_seconds_remaining())));
    }
}

void UOptionsWidget::handle_display_confirmation_changed(bool const visible) {
    if (IsValid(display_modal_)) {
        display_modal_->SetVisibility(visible ? ESlateVisibility::Visible
                                              : ESlateVisibility::Collapsed);
    }
    refresh_settings_ui();
}

auto UOptionsWidget::active_category() const -> EGameSettingCategory {
    switch (active_tab_) {
        case EOptionsTab::Gameplay: {
            return EGameSettingCategory::Gameplay;
        }
        case EOptionsTab::Audio: {
            return EGameSettingCategory::Audio;
        }
        case EOptionsTab::Controls: {
            return EGameSettingCategory::Controls;
        }
        case EOptionsTab::Accessibility: {
            return EGameSettingCategory::Accessibility;
        }
        default: {
            return EGameSettingCategory::Video;
        }
    }
}
}
