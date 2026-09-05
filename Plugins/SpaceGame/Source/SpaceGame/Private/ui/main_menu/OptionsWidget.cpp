#include "SpaceGame/ui/main_menu/OptionsWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Engine/GameInstance.h"
#include "SpaceGame/settings/GameSettingsSubsystem.h"
#include "SpaceGame/support/logging/SandboxLogCategories.h"
#include "SpaceGame/system/GameSubsystem.h"
#include "SpaceGame/ui/main_menu/SettingsRowWidget.h"

#include <Components/Button.h>
#include <Components/HorizontalBox.h>
#include <Components/HorizontalBoxSlot.h>
#include <Components/Overlay.h>
#include <Components/PanelWidget.h>
#include <Components/ScrollBox.h>
#include <Components/TextBlock.h>
#include <Components/VerticalBox.h>
#include <Components/VerticalBoxSlot.h>
#include <Components/WidgetSwitcher.h>

namespace ml::ioj {
namespace {
FLinearColor const options_selected_tab_colour{0.16f, 0.38f, 0.55f, 1.f};
FLinearColor const options_inactive_tab_colour{0.12f, 0.14f, 0.16f, 1.f};

auto make_capability_text(FString const& value) -> FText {
    return FText::FromString(value.IsEmpty() ? TEXT("Unknown") : value);
}

void add_capability_heading(UWidgetTree& tree, UVerticalBox& content, TCHAR const* const label) {
    auto* const heading{tree.ConstructWidget<UTextBlock>(UTextBlock::StaticClass())};
    heading->SetText(FText::FromString(label));
    auto font{heading->GetFont()};
    font.Size = 24;
    heading->SetFont(font);
    content.AddChildToVerticalBox(heading)->SetPadding(FMargin{0.f, 12.f, 0.f, 8.f});
}

void add_capability_row(UWidgetTree& tree,
                        UVerticalBox& content,
                        TCHAR const* const label,
                        FText value) {
    auto* const row{tree.ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass())};
    auto* const label_text{tree.ConstructWidget<UTextBlock>(UTextBlock::StaticClass())};
    auto* const value_text{tree.ConstructWidget<UTextBlock>(UTextBlock::StaticClass())};
    label_text->SetText(FText::FromString(label));
    value_text->SetText(MoveTemp(value));
    value_text->SetAutoWrapText(true);

    auto* const label_slot{row->AddChildToHorizontalBox(label_text)};
    label_slot->SetSize(FSlateChildSize{ESlateSizeRule::Fill});
    label_slot->SetPadding(FMargin{0.f, 2.f, 20.f, 2.f});
    auto* const value_slot{row->AddChildToHorizontalBox(value_text)};
    value_slot->SetSize(FSlateChildSize{ESlateSizeRule::Fill});
    value_slot->SetPadding(FMargin{0.f, 2.f});
    content.AddChildToVerticalBox(row);
}

void copy_box_slot_layout(UWidget const& source, UWidget& target) {
    if (auto const* const source_slot{Cast<UHorizontalBoxSlot>(source.Slot)}) {
        auto* const target_slot{Cast<UHorizontalBoxSlot>(target.Slot)};
        if (IsValid(target_slot)) {
            target_slot->SetPadding(source_slot->GetPadding());
            target_slot->SetSize(source_slot->GetSize());
            target_slot->SetHorizontalAlignment(source_slot->GetHorizontalAlignment());
            target_slot->SetVerticalAlignment(source_slot->GetVerticalAlignment());
        }
        return;
    }

    if (auto const* const source_slot{Cast<UVerticalBoxSlot>(source.Slot)}) {
        auto* const target_slot{Cast<UVerticalBoxSlot>(target.Slot)};
        if (IsValid(target_slot)) {
            target_slot->SetPadding(source_slot->GetPadding());
            target_slot->SetSize(source_slot->GetSize());
            target_slot->SetHorizontalAlignment(source_slot->GetHorizontalAlignment());
            target_slot->SetVerticalAlignment(source_slot->GetVerticalAlignment());
        }
    }
}

#if PLATFORM_WINDOWS
auto format_large_page_access(ELargePageAccessStatus const status) -> FText {
    switch (status) {
        case ELargePageAccessStatus::Unsupported: {
            return FText::FromString(TEXT("Unsupported"));
        }
        case ELargePageAccessStatus::Enabled: {
            return FText::FromString(TEXT("Enabled"));
        }
        case ELargePageAccessStatus::PrivilegeUnavailable: {
            return FText::FromString(TEXT("Privilege unavailable"));
        }
        case ELargePageAccessStatus::QueryFailed: {
            return FText::FromString(TEXT("Unknown (query failed)"));
        }
    }
    return FText::FromString(TEXT("Unknown"));
}
#endif
}

void UOptionsWidget::NativeOnInitialized() {
    Super::NativeOnInitialized();

    build_system_tab();
    if (!IsValid(video_button) || !IsValid(gameplay_button) || !IsValid(audio_button) ||
        !IsValid(controls_button) || !IsValid(accessibility_button) || !IsValid(system_button_) ||
        !IsValid(back_button)) {
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
    system_button_->OnClicked.AddDynamic(this, &ThisClass::handle_system);
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
    build_system_page();
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
        case EOptionsTab::System: {
            return system_button_;
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

void UOptionsWidget::handle_system() {
    set_active_tab(EOptionsTab::System);
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
        !IsValid(system_page_) || !IsValid(video_button) || !IsValid(gameplay_button) ||
        !IsValid(audio_button) || !IsValid(controls_button) || !IsValid(accessibility_button) ||
        !IsValid(system_button_)) {
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
        case EOptionsTab::System: {
            page = system_page_;
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
    set_tab_button_state(*system_button_, tab == EOptionsTab::System);
    refresh_settings_ui();
}

void UOptionsWidget::set_tab_button_state(UButton& button, bool const selected) {
    button.SetBackgroundColor(selected ? options_selected_tab_colour : options_inactive_tab_colour);
}

void UOptionsWidget::build_system_tab() {
    if (IsValid(system_button_) && IsValid(system_page_)) {
        return;
    }
    if (!IsValid(WidgetTree) || !IsValid(accessibility_button) || !IsValid(tab_switcher)) {
        UE_LOG(LogSandboxUI,
               Error,
               TEXT("UOptionsWidget::build_system_tab: Required tab widgets are invalid."));
        return;
    }

    auto* const tab_buttons{Cast<UPanelWidget>(accessibility_button->GetParent())};
    if (!IsValid(tab_buttons)) {
        UE_LOG(LogSandboxUI,
               Error,
               TEXT("UOptionsWidget::build_system_tab: Tab button container is invalid."));
        return;
    }

    system_button_ =
        WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("system_button"));
    system_button_->SetStyle(accessibility_button->GetStyle());
    auto* const label{WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass())};
    label->SetText(FText::FromString(TEXT("System")));
    if (accessibility_button->GetChildrenCount() > 0) {
        auto const* const source_label{Cast<UTextBlock>(accessibility_button->GetChildAt(0))};
        if (IsValid(source_label)) {
            label->SetFont(source_label->GetFont());
            label->SetColorAndOpacity(source_label->GetColorAndOpacity());
        }
    }
    system_button_->AddChild(label);
    tab_buttons->AddChild(system_button_);
    copy_box_slot_layout(*accessibility_button, *system_button_);

    system_page_ =
        WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("system_page"));
    tab_switcher->AddChild(system_page_);
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

void UOptionsWidget::build_system_page() {
    if (system_page_built_) {
        return;
    }

    auto* const panel{Cast<UPanelWidget>(system_page_)};
    if (!IsValid(panel)) {
        UE_LOG(LogSandboxUI, Error, TEXT("System options page is not a panel widget."));
        return;
    }

    auto* const game_instance{GetGameInstance()};
    if (!IsValid(game_instance)) {
        UE_LOG(LogSandboxUI,
               Warning,
               TEXT("UOptionsWidget: Game instance is invalid; system information is "
                    "unavailable."));
        return;
    }
    auto const* const game_subsystem{game_instance->GetSubsystem<UGameSubsystem>()};
    if (!IsValid(game_subsystem)) {
        UE_LOG(LogSandboxUI,
               Error,
               TEXT("UOptionsWidget: Game subsystem is invalid; system information is "
                    "unavailable."));
        return;
    }

    system_page_built_ = true;
    panel->ClearChildren();

    auto* const scroll{WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass())};
    auto* const content{WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass())};
    scroll->AddChild(content);
    panel->AddChild(scroll);

    auto const& capabilities{game_subsystem->get_platform_capabilities()};
    auto operating_system{capabilities.operating_system_version};
    if (!capabilities.operating_system_subversion.IsEmpty()) {
        if (!operating_system.IsEmpty()) {
            operating_system += TEXT(" ");
        }
        operating_system += capabilities.operating_system_subversion;
    }

    add_capability_heading(*WidgetTree, *content, TEXT("System"));
    add_capability_row(
        *WidgetTree, *content, TEXT("Platform"), make_capability_text(capabilities.platform_name));
    add_capability_row(*WidgetTree,
                       *content,
                       TEXT("Architecture"),
                       make_capability_text(capabilities.host_architecture));
    add_capability_row(
        *WidgetTree, *content, TEXT("Operating system"), make_capability_text(operating_system));
    add_capability_row(
        *WidgetTree, *content, TEXT("CPU vendor"), make_capability_text(capabilities.cpu_vendor));
    add_capability_row(
        *WidgetTree, *content, TEXT("CPU"), make_capability_text(capabilities.cpu_brand));
    add_capability_row(
        *WidgetTree, *content, TEXT("GPU"), make_capability_text(capabilities.primary_gpu_brand));
    add_capability_row(*WidgetTree,
                       *content,
                       TEXT("Physical cores"),
                       FText::AsNumber(capabilities.physical_core_count));
    add_capability_row(*WidgetTree,
                       *content,
                       TEXT("Logical cores"),
                       FText::AsNumber(capabilities.logical_core_count));
    add_capability_row(*WidgetTree,
                       *content,
                       TEXT("Physical memory"),
                       FText::AsMemory(capabilities.total_physical_memory_bytes));

#if PLATFORM_WINDOWS
    add_capability_heading(*WidgetTree, *content, TEXT("Windows"));
    auto const large_page_minimum{
        capabilities.windows.large_page_minimum_bytes == 0
            ? FText::FromString(TEXT("Unsupported"))
            : FText::AsMemory(capabilities.windows.large_page_minimum_bytes)};
    add_capability_row(*WidgetTree, *content, TEXT("Minimum large-page size"), large_page_minimum);
    add_capability_row(*WidgetTree,
                       *content,
                       TEXT("Large-page access"),
                       format_large_page_access(capabilities.windows.large_page_access_status));
#endif
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
