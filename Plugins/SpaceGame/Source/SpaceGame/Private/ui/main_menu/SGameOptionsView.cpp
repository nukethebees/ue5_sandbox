#include "SGameOptionsView.h"

#include "SandboxUI/slate/SlateSlots.h"
#include "SandboxUI/widgets/SettingsWidgets.h"
#include "SpaceGame/settings/GameSettingsBackend.h"
#include "SpaceGame/settings/GameSettingsSubsystem.h"
#include "SpaceGame/system/GameSubsystem.h"
#include "SpaceGame/ui/common/HiveWidgets.h"
#include "SpaceGame/ui/common/SGameButton.h"

#include "Framework/Application/SlateApplication.h"
#include "InputCoreTypes.h"
#include "Misc/StringBuilder.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SEditableText.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace ml::ioj {
namespace {
struct FSimdFeatureDisplay {
    bool supported{};
    TCHAR const* name{};
};

auto category_label(EGameSettingCategory const category) -> FText {
    for (auto const& descriptor : game_setting_category_descriptors()) {
        if (descriptor.id == category) {
            return descriptor.label;
        }
    }
    return FText::GetEmpty();
}

auto section_label(FGameSettingDescriptor const& descriptor) -> FText {
    switch (descriptor.id) {
        case EGameSetting::Resolution:
        case EGameSetting::WindowMode:
        case EGameSetting::VSync:
        case EGameSetting::FrameRateLimit:
        case EGameSetting::ResolutionScale:
            return NSLOCTEXT("OptionsMenu", "DisplaySection", "Display");
        case EGameSetting::AAMethod:
        case EGameSetting::OverallQuality:
        case EGameSetting::ViewDistanceQuality:
        case EGameSetting::AAQuality:
        case EGameSetting::ShadowQuality:
        case EGameSetting::GlobalIlluminationQuality:
        case EGameSetting::ReflectionsQuality:
        case EGameSetting::PostProcessingQuality:
        case EGameSetting::TextureQuality:
        case EGameSetting::EffectsQuality:
        case EGameSetting::ShadingQuality:
            return NSLOCTEXT("OptionsMenu", "QualitySection", "Quality");
        case EGameSetting::Bloom:
        case EGameSetting::MotionBlur:
            return NSLOCTEXT(
                "OptionsMenu", "PostProcessingEffectsSection", "Post Processing Effects");
        case EGameSetting::MasterVolume:
        case EGameSetting::MusicVolume:
        case EGameSetting::SfxVolume:
        case EGameSetting::UIVolume:
            return NSLOCTEXT("OptionsMenu", "VolumeSection", "Volume");
        case EGameSetting::Bees:
            return NSLOCTEXT("OptionsMenu", "GameplaySection", "Gameplay");
        case EGameSetting::MouseTurnSensitivity:
        case EGameSetting::GamepadTurnSensitivity:
        case EGameSetting::InvertMousePitch:
        case EGameSetting::InvertGamepadPitch:
            return NSLOCTEXT("OptionsMenu", "ResponseSection", "Response");
        case EGameSetting::PlayerShipFlightControlPreset:
            return NSLOCTEXT("OptionsMenu", "FlightControlsSection", "Flight Controls");
        case EGameSetting::GamepadTurnDeadZone:
        case EGameSetting::GamepadMoveDeadZone:
            return NSLOCTEXT("OptionsMenu", "ResponseSection", "Response");
    }
    return category_label(descriptor.category);
}

auto friendly_platform_name(FString value) -> FText {
#if PLATFORM_WINDOWS
    static_cast<void>(value);
    return NSLOCTEXT("OptionsMenu", "WindowsPlatform", "Windows");
#else
    value.RemoveFromEnd(TEXT("Editor"));
    return FText::FromString(value.IsEmpty() ? TEXT("Unknown") : value);
#endif
}

auto capability_text(FString const& value) -> FText {
    return FText::FromString(value.IsEmpty() ? TEXT("Unknown") : value);
}

auto simd_feature_text(std::initializer_list<FSimdFeatureDisplay> const features) -> FText {
    TStringBuilder<128> supported_features;
    for (auto const& feature : features) {
        if (feature.supported) {
            if (supported_features.Len() > 0) {
                supported_features.Append(TEXT(", "));
            }
            supported_features.Append(feature.name);
        }
    }
    if (supported_features.Len() == 0) {
        return NSLOCTEXT("OptionsMenu", "SimdUnsupported", "Unsupported");
    }
    return FText::FromString(FString{supported_features});
}

#if PLATFORM_WINDOWS
auto large_page_access_text(ELargePageAccessStatus const status) -> FText {
    switch (status) {
        case ELargePageAccessStatus::Unsupported:
            return NSLOCTEXT("OptionsMenu", "LargePagesUnsupported", "Unsupported");
        case ELargePageAccessStatus::Enabled:
            return NSLOCTEXT("OptionsMenu", "LargePagesEnabled", "Enabled");
        case ELargePageAccessStatus::PrivilegeUnavailable:
            return NSLOCTEXT(
                "OptionsMenu", "LargePagesPrivilegeUnavailable", "Privilege unavailable");
        case ELargePageAccessStatus::QueryFailed:
            return NSLOCTEXT("OptionsMenu", "LargePagesQueryFailed", "Unknown (query failed)");
    }
    return NSLOCTEXT("OptionsMenu", "UnknownValue", "Unknown");
}
#endif
}

/* **************************************** */
// Lifecycle and state
/* **************************************** */
void SGameOptionsView::Construct(FArguments const& args) {
    settings_ = args._Settings;
    capabilities_ = args._Capabilities;
    style_ = args._Style;
    audio_ = args._Audio;
    active_tab_ = args._InitialTab;
    check(style_ != nullptr);
    on_tab_changed_ = args._OnTabChanged;
    on_apply_ = args._OnApply;
    on_reset_ = args._OnReset;
    on_dirty_apply_ = args._OnDirtyApply;
    on_dirty_discard_ = args._OnDirtyDiscard;
    on_dirty_stay_ = args._OnDirtyStay;
    on_confirm_display_ = args._OnConfirmDisplay;
    on_revert_display_ = args._OnRevertDisplay;
    on_interaction_modal_changed_ = args._OnInteractionModalChanged;
    page_focus_actions_.SetNum(static_cast<int32>(EOptionsTab::System) + 1);

    auto header{build_header()};
    auto body{build_body()};
    auto footer{build_footer()};
    dirty_prompt_ = build_dirty_prompt();
    display_prompt_ = build_display_prompt();
    binding_management_prompt_ = build_binding_management_prompt();
    capture_prompt_ = build_capture_prompt();
    conflict_prompt_ = build_conflict_prompt();

    auto panel{SNew(SBorder)
                   .BorderImage(&style_->chrome().body_background)
                   .Padding(style_->settings().body_padding)
                       [SNew(SVerticalBox) + SVerticalBox::Slot().AutoHeight()[header] +
                        SVerticalBox::Slot().FillHeight(1.0f).Padding(FMargin{0.0f, 18.0f})[body] +
                        SVerticalBox::Slot().AutoHeight()[footer]]};

    dirty_prompt_->SetVisibility(EVisibility::Collapsed);
    display_prompt_->SetVisibility(EVisibility::Collapsed);
    binding_management_prompt_->SetVisibility(EVisibility::Collapsed);
    capture_prompt_->SetVisibility(EVisibility::Collapsed);
    conflict_prompt_->SetVisibility(EVisibility::Collapsed);
    ChildSlot[SNew(SOverlay) + SOverlay::Slot()[panel] +
              SOverlay::Slot()[dirty_prompt_.ToSharedRef()] +
              SOverlay::Slot()[display_prompt_.ToSharedRef()] +
              SOverlay::Slot()[binding_management_prompt_.ToSharedRef()] +
              SOverlay::Slot()[capture_prompt_.ToSharedRef()] +
              SOverlay::Slot()[conflict_prompt_.ToSharedRef()]];
    refresh();
}

void SGameOptionsView::set_active_tab(EOptionsTab const tab) {
    active_tab_ = tab;
    if (page_switcher_.IsValid()) {
        page_switcher_->SetActiveWidgetIndex(static_cast<int32>(active_tab_));
    }
    refresh();
}

void SGameOptionsView::refresh() {
    auto* const settings{settings_.Get()};
    if (settings == nullptr || style_ == nullptr) {
        return;
    }

    auto const category{active_category()};
    if (reset_button_.IsValid()) {
        reset_button_->SetEnabled(category.IsSet() &&
                                  !settings->is_at_defaults(category.GetValue()) &&
                                  !settings->is_awaiting_display_confirmation());
    }
    if (apply_button_.IsValid()) {
        apply_button_->SetEnabled(settings->is_dirty() &&
                                  !settings->is_awaiting_display_confirmation());
    }

    auto const display_visible{settings->is_awaiting_display_confirmation()};
    if (display_visible != display_prompt_visible_) {
        if (display_visible) {
            remember_focus();
            if (confirm_display_button_.IsValid()) {
                confirm_display_button_->focus();
            }
        } else {
            restore_focus();
        }
        display_prompt_visible_ = display_visible;
    }
    if (display_prompt_.IsValid()) {
        display_prompt_->SetVisibility(display_visible ? EVisibility::Visible
                                                       : EVisibility::Collapsed);
    }
}

void SGameOptionsView::refresh_controls() {
    rebuild_controls_page();
    refresh();
}

void SGameOptionsView::focus_content() {
    auto const index{static_cast<int32>(active_tab_)};
    if (page_focus_actions_.IsValidIndex(index) && page_focus_actions_[index]) {
        page_focus_actions_[index]();
        return;
    }
    FSlateApplication::Get().SetKeyboardFocus(SharedThis(this), EFocusCause::SetDirectly);
}

void SGameOptionsView::show_dirty_prompt() {
    if (dirty_prompt_visible_) {
        return;
    }
    remember_focus();
    dirty_prompt_visible_ = true;
    dirty_prompt_->SetVisibility(EVisibility::Visible);
    if (dirty_apply_button_.IsValid()) {
        dirty_apply_button_->focus();
    }
}

void SGameOptionsView::hide_dirty_prompt() {
    if (!dirty_prompt_visible_) {
        return;
    }
    dirty_prompt_visible_ = false;
    dirty_prompt_->SetVisibility(EVisibility::Collapsed);
    restore_focus();
}

auto SGameOptionsView::is_dirty_prompt_visible() const -> bool {
    return dirty_prompt_visible_;
}

auto SGameOptionsView::dismiss_interaction_modal() -> bool {
    if (captured_binding_.IsSet()) {
        close_binding_prompt(true);
        return true;
    }
    if (binding_management_visible_) {
        close_binding_management(true);
        return true;
    }
    return false;
}

/* **************************************** */
// Input handling
/* **************************************** */
auto SGameOptionsView::OnFocusReceived(FGeometry const& geometry, FFocusEvent const& focus_event)
    -> FReply {
    static_cast<void>(geometry);
    static_cast<void>(focus_event);
    return FReply::Handled();
}

auto SGameOptionsView::OnKeyDown(FGeometry const& geometry, FKeyEvent const& key_event) -> FReply {
    auto const key{key_event.GetKey()};
    if (captured_binding_.IsSet() &&
        (key == EKeys::Escape || key == EKeys::Gamepad_FaceButton_Right)) {
        close_binding_prompt(true);
        return FReply::Handled();
    }
    if (binding_management_visible_ &&
        (key == EKeys::Escape || key == EKeys::Gamepad_FaceButton_Right)) {
        close_binding_management(true);
        return FReply::Handled();
    }
    if (captured_binding_.IsSet() && captured_chord_.IsSet() && !chord_capture_.is_complete()) {
        if (key_event.IsRepeat()) {
            return FReply::Handled();
        }
        return accept_chord_key(key, true);
    }
    if (captured_binding_.IsSet() && !captured_chord_.IsSet() && captured_key_ == EKeys::Invalid) {
        return accept_binding_key(key);
    }

    auto buttons{TArray<TSharedPtr<SGameButton>>{}};
    if (dirty_prompt_visible_) {
        buttons = {dirty_apply_button_, dirty_discard_button_, dirty_stay_button_};
    } else if (display_prompt_visible_) {
        buttons = {confirm_display_button_, revert_display_button_};
    } else if (captured_chord_.IsSet() && capture_prompt_->GetVisibility().IsVisible()) {
        buttons = {chord_confirm_button_, chord_clear_button_, chord_cancel_button_};
    } else if (captured_binding_.IsSet()) {
        buttons = {conflict_replace_button_, conflict_cancel_button_};
    } else if (binding_management_visible_) {
        buttons = {binding_change_button_};
        if (managed_binding_.IsSet() && managed_binding_->current_key.IsValid()) {
            buttons.Add(binding_clear_button_);
        }
        if (managed_binding_.IsSet() && managed_binding_->modified &&
            !managed_binding_->custom_profile) {
            buttons.Add(binding_reset_button_);
        }
        buttons.Add(binding_cancel_button_);
    } else {
        return SCompoundWidget::OnKeyDown(geometry, key_event);
    }

    auto direction{0};
    if (key == EKeys::Left || key == EKeys::Gamepad_DPad_Left || key == EKeys::Up ||
        key == EKeys::Gamepad_DPad_Up) {
        direction = -1;
    } else if (key == EKeys::Right || key == EKeys::Gamepad_DPad_Right || key == EKeys::Down ||
               key == EKeys::Gamepad_DPad_Down) {
        direction = 1;
    }
    if (direction == 0) {
        return SCompoundWidget::OnKeyDown(geometry, key_event);
    }

    auto current_index{0};
    auto const button_count{buttons.Num()};
    for (int32 index{}; index < button_count; ++index) {
        if (buttons[index].IsValid() && buttons[index]->has_focus()) {
            current_index = index;
            break;
        }
    }
    auto const next_index{(current_index + direction + button_count) % button_count};
    buttons[next_index]->focus();
    return FReply::Handled();
}

auto SGameOptionsView::OnKeyUp(FGeometry const& geometry, FKeyEvent const& key_event) -> FReply {
    if (captured_binding_.IsSet() && captured_chord_.IsSet()) {
        return release_chord_key(key_event.GetKey());
    }
    return SCompoundWidget::OnKeyUp(geometry, key_event);
}

auto SGameOptionsView::OnMouseButtonDown(FGeometry const& geometry,
                                         FPointerEvent const& mouse_event) -> FReply {
    if (captured_binding_.IsSet() && captured_chord_.IsSet() && !chord_capture_.is_complete()) {
        return accept_chord_key(mouse_event.GetEffectingButton(), true);
    }
    if (captured_binding_.IsSet() && !captured_chord_.IsSet() && captured_key_ == EKeys::Invalid) {
        return accept_binding_key(mouse_event.GetEffectingButton());
    }
    return SCompoundWidget::OnMouseButtonDown(geometry, mouse_event);
}

auto SGameOptionsView::OnMouseButtonUp(FGeometry const& geometry, FPointerEvent const& mouse_event)
    -> FReply {
    if (captured_binding_.IsSet() && captured_chord_.IsSet()) {
        return release_chord_key(mouse_event.GetEffectingButton());
    }
    return SCompoundWidget::OnMouseButtonUp(geometry, mouse_event);
}

auto SGameOptionsView::OnAnalogValueChanged(FGeometry const& geometry,
                                            FAnalogInputEvent const& analog_event) -> FReply {
    if (captured_binding_.IsSet() && captured_chord_.IsSet() && !chord_capture_.is_complete() &&
        FMath::Abs(analog_event.GetAnalogValue()) >= 0.5f) {
        return accept_chord_key(analog_event.GetKey(), false);
    }
    if (captured_binding_.IsSet() && !captured_chord_.IsSet() && captured_key_ == EKeys::Invalid &&
        FMath::Abs(analog_event.GetAnalogValue()) >= 0.5f) {
        return accept_binding_key(analog_event.GetKey());
    }
    return SCompoundWidget::OnAnalogValueChanged(geometry, analog_event);
}

auto SGameOptionsView::OnMouseWheel(FGeometry const& geometry, FPointerEvent const& mouse_event)
    -> FReply {
    if (captured_binding_.IsSet() && captured_chord_.IsSet() && !chord_capture_.is_complete() &&
        !FMath::IsNearlyZero(mouse_event.GetWheelDelta())) {
        return accept_chord_key(mouse_event.GetWheelDelta() > 0.0f ? EKeys::MouseScrollUp
                                                                   : EKeys::MouseScrollDown,
                                false);
    }
    if (captured_binding_.IsSet() && !captured_chord_.IsSet() && captured_key_ == EKeys::Invalid &&
        !FMath::IsNearlyZero(mouse_event.GetWheelDelta())) {
        return accept_binding_key(mouse_event.GetWheelDelta() > 0.0f ? EKeys::MouseScrollUp
                                                                     : EKeys::MouseScrollDown);
    }
    return SCompoundWidget::OnMouseWheel(geometry, mouse_event);
}

/* **************************************** */
// Layout construction
/* **************************************** */
auto SGameOptionsView::build_header() -> TSharedRef<SWidget> {
    return SNew(SVerticalBox) +
           SVerticalBox::Slot()
               .AutoHeight()[SNew(STextBlock)
                                 .Text(NSLOCTEXT("OptionsMenu", "Section", "SYSTEM CONFIGURATION"))
                                 .TextStyle(&style_->text(EGameTextStyle::Caption))] +
           SVerticalBox::Slot().AutoHeight().Padding(
               FMargin{0.0f, 4.0f})[SNew(STextBlock)
                                        .Text_Lambda([this] { return tab_text(active_tab_); })
                                        .TextStyle(&style_->text(EGameTextStyle::Heading1))];
}

auto SGameOptionsView::build_body() -> TSharedRef<SWidget> {
    SAssignNew(page_switcher_, SWidgetSwitcher).WidgetIndex(static_cast<int32>(active_tab_)) +
        SWidgetSwitcher::Slot()[build_category_page(EGameSettingCategory::Video)] +
        SWidgetSwitcher::Slot()[build_category_page(EGameSettingCategory::Gameplay)] +
        SWidgetSwitcher::Slot()[build_category_page(EGameSettingCategory::Audio)] +
        SWidgetSwitcher::Slot()[build_controls_page()] +
        SWidgetSwitcher::Slot()[build_category_page(EGameSettingCategory::Accessibility)] +
        SWidgetSwitcher::Slot()[build_system_page()];
    return page_switcher_.ToSharedRef();
}

auto SGameOptionsView::build_footer() -> TSharedRef<SWidget> {
    auto const spacing{style_->settings().button_spacing};
    return SNew(SBorder)
        .BorderImage(&style_->chrome().footer_background)
        .Padding(style_->chrome().footer_padding)
            [SNew(SHorizontalBox) + SHorizontalBox::Slot().FillWidth(1.0f) +
             SHorizontalBox::Slot().AutoWidth().Padding(FMargin{0.0f, 0.0f, spacing, 0.0f})
                 [SAssignNew(reset_button_, SGameButton)
                      .Style(&style_->button(EGameButtonStyle::Secondary))
                      .Audio(audio_)
                      .Text_Lambda([this] {
                          if (active_tab_ != EOptionsTab::Controls) {
                              return NSLOCTEXT("OptionsMenu", "ResetCategory", "Reset Category");
                          }
                          return control_reset_scope(active_control_profile_custom_) ==
                                         EControlResetScope::SettingsOnly
                                   ? NSLOCTEXT("OptionsMenu",
                                               "ResetControlSettings",
                                               "Reset Control Settings")
                                   : NSLOCTEXT(
                                         "OptionsMenu", "ResetAllControls", "Reset All Controls");
                      })
                      .ToolTipText_Lambda([this] {
                          if (active_tab_ != EOptionsTab::Controls) {
                              return FText::GetEmpty();
                          }
                          return control_reset_scope(active_control_profile_custom_) ==
                                         EControlResetScope::SettingsOnly
                                   ? NSLOCTEXT("OptionsMenu",
                                               "ResetCustomControlSettingsTip",
                                               "Reset all response settings. Custom bindings are "
                                               "preserved.")
                                   : NSLOCTEXT("OptionsMenu",
                                               "ResetAllControlsTip",
                                               "Reset response settings and bindings for both "
                                               "devices in this profile.");
                      })
                      .OnClicked_Lambda([delegate = on_reset_]() {
                          delegate.ExecuteIfBound();
                          return FReply::Handled();
                      })] +
             SHorizontalBox::Slot()
                 .AutoWidth()[SAssignNew(apply_button_, SGameButton)
                                  .Style(&style_->button(EGameButtonStyle::Primary))
                                  .Audio(audio_)
                                  .Text(NSLOCTEXT("OptionsMenu", "Apply", "Apply"))
                                  .OnClicked_Lambda([delegate = on_apply_]() {
                                      delegate.ExecuteIfBound();
                                      return FReply::Handled();
                                  })]];
}

auto SGameOptionsView::build_category_page(EGameSettingCategory const category)
    -> TSharedRef<SWidget> {
    auto content{SNew(SVerticalBox)};
    auto* const settings{settings_.Get()};
    auto const descriptors{settings != nullptr ? settings->descriptors(category)
                                               : TArray<FGameSettingDescriptor const*>{}};
    if (descriptors.IsEmpty()) {
        content->AddSlot().AutoHeight()[SNew(STextBlock)
                                            .Text(NSLOCTEXT("OptionsMenu",
                                                            "EmptyCategory",
                                                            "No settings in this category yet."))
                                            .TextStyle(&style_->settings().empty_text)];
    } else {
        FText current_section;
        TSharedPtr<SVerticalBox> rows;
        auto flush_section = [this, &content, &current_section, &rows] {
            if (!rows.IsValid()) {
                return;
            }
            content->AddSlot().AutoHeight().Padding(FMargin{
                0.0f,
                0.0f,
                0.0f,
                style_->settings()
                    .section_spacing})[SNew(SSettingsSection)
                                           .Style(&style_->settings())
                                           .Header()[SNew(SHiveSectionHeader)
                                                         .Style(style_)
                                                         .Icon(&style_->icon(EGameUiIcon::Hive))
                                                         .Text(current_section)]
                                           .Title(current_section)[rows.ToSharedRef()]];
        };

        for (auto const* const descriptor : descriptors) {
            auto const next_section{section_label(*descriptor)};
            if (!rows.IsValid() || !next_section.EqualTo(current_section)) {
                flush_section();
                current_section = next_section;
                rows = SNew(SVerticalBox);
            }
            TFunction<void()> focus_action;
            auto const row{build_setting_row(*descriptor, focus_action)};
            auto const page_index{static_cast<int32>(category)};
            if (!page_focus_actions_[page_index] && focus_action) {
                page_focus_actions_[page_index] = MoveTemp(focus_action);
            }
            rows->AddSlot().AutoHeight().Padding(style_->settings().row_padding)[row];
        }
        flush_section();
    }

    return SNew(SScrollBox)
               .ScrollBarStyle(&style_->settings().scroll_bar)
               .Orientation(Orient_Vertical)
               .ScrollBarAlwaysVisible(false)
               .AnimateWheelScrolling(true) +
           SScrollBox::Slot()[content];
}

auto SGameOptionsView::build_controls_page() -> TSharedRef<SWidget> {
    SAssignNew(controls_content_, SVerticalBox);
    auto result{SAssignNew(controls_scroll_box_, SScrollBox)
                    .ScrollBarStyle(&style_->settings().scroll_bar)
                    .Orientation(Orient_Vertical)
                    .ScrollBarAlwaysVisible(false)
                    .AnimateWheelScrolling(true) +
                SScrollBox::Slot()[controls_content_.ToSharedRef()]};
    rebuild_controls_page();
    return result;
}

/* **************************************** */
// Control bindings
/* **************************************** */
void SGameOptionsView::rebuild_controls_page(TOptional<FControlsFocusIdentity> requested_focus,
                                             bool const reset_scroll) {
    auto* const settings{settings_.Get()};
    if (!controls_content_.IsValid() || settings == nullptr) {
        return;
    }

    auto const focus_to_restore{requested_focus.IsSet() ? requested_focus
                                                        : controls_focus_identity()};
    auto const scroll_offset{controls_scroll_box_.IsValid() && !reset_scroll
                                 ? controls_scroll_box_->GetScrollOffset()
                                 : 0.0f};
    controls_content_->ClearChildren();
    controls_focus_targets_.Reset();

    auto const add_section = [this](FText const& title, TSharedRef<SVerticalBox> const& rows) {
        controls_content_->AddSlot().AutoHeight().Padding(
            FMargin{0.0f,
                    0.0f,
                    0.0f,
                    style_->settings()
                        .section_spacing})[SNew(SSettingsSection)
                                               .Style(&style_->settings())
                                               .Header()[SNew(SHiveSectionHeader)
                                                             .Style(style_)
                                                             .Icon(&style_->icon(EGameUiIcon::Hive))
                                                             .Text(title)]
                                               .Title(title)[rows]];
    };

    auto const profiles{settings->control_profiles()};
    TArray<FText> profile_labels;
    profile_labels.Reserve(profiles.Num());
    for (auto const& profile : profiles) {
        profile_labels.Add(
            profile.modified && !profile.custom
                ? FText::Format(
                      NSLOCTEXT("OptionsMenu", "ModifiedControlProfile", "{0} (Modified)"),
                      profile.display_name)
                : profile.display_name);
    }
    auto const* const active_profile{
        profiles.FindByPredicate([](auto const& profile) { return profile.active; })};
    active_control_profile_custom_ = active_profile != nullptr && active_profile->custom;
    TSharedPtr<SSettingsChoice> profile_choice;
    auto profile_rows{SNew(SVerticalBox)};
    profile_rows->AddSlot().AutoHeight().Padding(style_->settings().row_padding)
        [SAssignNew(profile_choice, SSettingsChoice)
             .Style(&style_->settings())
             .Label(NSLOCTEXT("OptionsMenu", "ControlProfile", "Control Profile"))
             .ToolTipText(NSLOCTEXT(
                 "OptionsMenu", "ControlProfileTip", "Choose a preset or custom control profile."))
             .Options(MoveTemp(profile_labels))
             .SelectedIndex_Lambda([weak_settings = settings_, profiles] {
                 auto const* const current{weak_settings.Get()};
                 if (current == nullptr) {
                     return int32{INDEX_NONE};
                 }
                 auto const current_profiles{current->control_profiles()};
                 return current_profiles.IndexOfByPredicate(
                     [](FControlProfileView const& profile) { return profile.active; });
             })
             .OnSelectionChanged_Lambda([this, profiles](int32 const index) {
                 if (auto* const current{settings_.Get()};
                     profiles.IsValidIndex(index) && current != nullptr &&
                     current->set_control_profile(profiles[index].id)) {
                     control_profile_error_ = FText::GetEmpty();
                     request_controls_rebuild(FControlsFocusIdentity{
                         .kind = EControlsFocusKind::Profile,
                     });
                 }
             })];
    auto profile_actions{SNew(SHorizontalBox)};
    profile_actions->AddSlot().AutoWidth().Padding(FMargin{
        0.0f,
        0.0f,
        style_->settings().button_spacing,
        0.0f})[SNew(SGameButton)
                   .Style(&style_->button(EGameButtonStyle::Secondary))
                   .Audio(audio_)
                   .Text(NSLOCTEXT("OptionsMenu", "CreateCustomProfile", "Copy to New Custom"))
                   .OnClicked_Lambda([this] {
                       if (auto* const current{settings_.Get()}) {
                           if (current->create_custom_control_profile()) {
                               control_profile_error_ = FText::GetEmpty();
                           } else {
                               control_profile_error_ =
                                   NSLOCTEXT("OptionsMenu",
                                             "CreateCustomProfileFailed",
                                             "Could not create the custom control profile.");
                           }
                           request_controls_rebuild(FControlsFocusIdentity{
                               .kind = EControlsFocusKind::Profile,
                           });
                       }
                       return FReply::Handled();
                   })];
    if (active_profile != nullptr && active_profile->custom) {
        auto const name_input{
            SNew(SBorder)
                .BorderImage(&style_->chrome().frame_border)
                .Padding(FMargin{1.0f})
                    [SNew(SBorder)
                         .BorderImage(&style_->chrome().body_background)
                         .Padding(FMargin{12.0f, 8.0f})
                             [SNew(SEditableText)
                                  .Text(active_profile->display_name)
                                  .HintText(NSLOCTEXT(
                                      "OptionsMenu", "CustomProfileNameHint", "Profile name"))
                                  .Font(style_->text(EGameTextStyle::Body).Font)
                                  .ColorAndOpacity(style_->palette().text_primary)
                                  .SelectAllTextWhenFocused(true)
                                  .OnTextCommitted_Lambda(
                                      [this](FText const& text, ETextCommit::Type const commit) {
                                          if (commit == ETextCommit::OnCleared) {
                                              return;
                                          }
                                          auto* const current{settings_.Get()};
                                          if (current == nullptr ||
                                              !current->rename_active_custom_control_profile(
                                                  text.ToString())) {
                                              control_profile_error_ = NSLOCTEXT(
                                                  "OptionsMenu",
                                                  "RenameCustomProfileFailed",
                                                  "Profile names must be unique and contain 1–48 "
                                                  "characters.");
                                          } else {
                                              control_profile_error_ = FText::GetEmpty();
                                          }
                                          request_controls_rebuild(FControlsFocusIdentity{
                                              .kind = EControlsFocusKind::Profile,
                                          });
                                      })]]};
        profile_rows->AddSlot().AutoHeight().Padding(style_->settings().row_padding)
            [SNew(SSettingsRow)
                 .Style(&style_->settings())
                 .Label(NSLOCTEXT("OptionsMenu", "CustomProfileName", "Profile Name"))
                 .ToolTipText(
                     NSLOCTEXT("OptionsMenu",
                               "CustomProfileNameTip",
                               "Rename this custom profile. Names must be unique."))[name_input]];
        profile_actions->AddSlot()
            .AutoWidth()[SNew(SGameButton)
                             .Style(&style_->button(EGameButtonStyle::Secondary))
                             .Audio(audio_)
                             .Text(NSLOCTEXT("OptionsMenu", "DeleteCustomProfile", "Delete Custom"))
                             .OnClicked_Lambda([this, profile_id = active_profile->id] {
                                 if (auto* const current{settings_.Get()}) {
                                     if (current->delete_custom_control_profile(profile_id)) {
                                         control_profile_error_ = FText::GetEmpty();
                                     } else {
                                         control_profile_error_ = NSLOCTEXT(
                                             "OptionsMenu",
                                             "DeleteCustomProfileFailed",
                                             "Could not delete the custom control profile.");
                                     }
                                     request_controls_rebuild(FControlsFocusIdentity{
                                         .kind = EControlsFocusKind::Profile,
                                     });
                                 }
                                 return FReply::Handled();
                             })];
    }
    profile_rows->AddSlot().AutoHeight().Padding(style_->settings().row_padding)[profile_actions];
    if (!control_profile_error_.IsEmpty()) {
        profile_rows->AddSlot().AutoHeight().Padding(
            style_->settings().row_padding)[SNew(STextBlock)
                                                .Text(control_profile_error_)
                                                .TextStyle(&style_->text(EGameTextStyle::Caption))
                                                .ColorAndOpacity(style_->palette().danger)];
    }
    page_focus_actions_[static_cast<int32>(EOptionsTab::Controls)] = [profile_choice] {
        profile_choice->focus();
    };
    add_section(NSLOCTEXT("OptionsMenu", "ProfilesSection", "Profiles"), profile_rows);

    auto const add_setting_sections = [this, settings, &add_section](
                                          EGameSettingDevice const device) {
        FText current_section;
        TSharedPtr<SVerticalBox> rows;
        auto const flush = [&] {
            if (rows.IsValid()) {
                add_section(current_section, rows.ToSharedRef());
            }
        };

        for (auto const* const descriptor : settings->descriptors(EGameSettingCategory::Controls)) {
            if (descriptor->device != device) {
                continue;
            }
            auto const next_section{section_label(*descriptor)};
            if (!rows.IsValid() || !next_section.EqualTo(current_section)) {
                flush();
                current_section = next_section;
                rows = SNew(SVerticalBox);
            }
            TFunction<void()> unused_focus;
            rows->AddSlot().AutoHeight().Padding(
                style_->settings().row_padding)[build_setting_row(*descriptor, unused_focus)];
        }
        flush();
    };

    add_setting_sections(EGameSettingDevice::Shared);

    using FlightConfig = ::ioj::sim::player::FlightModelConfig;
    auto flight_rows{SNew(SVerticalBox)};
    auto const weak_settings{settings_};
    auto const add_flight_slider = [this, &flight_rows, weak_settings](FText const& label,
                                                                       FText const& tooltip,
                                                                       float const minimum,
                                                                       float const maximum,
                                                                       float const step,
                                                                       auto getter,
                                                                       auto setter) {
        auto value{TAttribute<float>::CreateLambda([weak_settings, getter] {
            auto const* const current{weak_settings.Get()};
            return current != nullptr ? getter(current->flight_model_profile().config) : 0.f;
        })};
        auto value_text{TAttribute<FText>::CreateLambda([weak_settings, getter] {
            auto const* const current{weak_settings.Get()};
            return current != nullptr
                     ? FText::AsNumber(getter(current->flight_model_profile().config))
                     : FText::GetEmpty();
        })};
        flight_rows->AddSlot().AutoHeight().Padding(
            style_->settings()
                .row_padding)[SNew(SSettingsSlider)
                                  .Style(&style_->settings())
                                  .Label(label)
                                  .ToolTipText(tooltip)
                                  .Value(value)
                                  .ValueText(value_text)
                                  .Minimum(minimum)
                                  .Maximum(maximum)
                                  .Step(step)
                                  .OnValueChanged_Lambda([weak_settings, setter](float const next) {
                                      if (auto* const current{weak_settings.Get()}) {
                                          auto profile{current->flight_model_profile()};
                                          setter(profile.config, next);
                                          current->set_flight_model_profile(MoveTemp(profile));
                                      }
                                  })];
    };

    flight_rows->AddSlot().AutoHeight().Padding(style_->settings().row_padding)
        [SNew(SSettingsReadOnlyRow)
             .Style(&style_->settings())
             .Label(NSLOCTEXT("OptionsMenu", "FlightModelStatus", "Runtime Model"))
             .Value_Lambda([weak_settings] {
                 auto const* const current{weak_settings.Get()};
                 if (current == nullptr) {
                     return FText::GetEmpty();
                 }
                 auto const& profile{current->flight_model_profile()};
                 TCHAR const* name{TEXT("Starfox")};
                 switch (profile.base_preset) {
                     case ::ioj::sim::player::FlightModelPreset::Starfox:
                         break;
                     case ::ioj::sim::player::FlightModelPreset::Fighter:
                         name = TEXT("Fighter");
                         break;
                     case ::ioj::sim::player::FlightModelPreset::Skater:
                         name = TEXT("Skater");
                         break;
                     case ::ioj::sim::player::FlightModelPreset::Gunship:
                         name = TEXT("Gunship");
                         break;
                 }
                 return profile.customized ? FText::Format(NSLOCTEXT("OptionsMenu",
                                                                     "CustomFlightModel",
                                                                     "Custom (based on {0})"),
                                                           FText::FromString(name))
                                           : FText::FromString(name);
             })
             .ToolTipText(NSLOCTEXT("OptionsMenu",
                                    "FlightModelStatusTip",
                                    "Runtime edits apply immediately and last for this session."))];

    auto const& flight_config{settings->flight_model_profile().config};
    add_flight_slider(
        NSLOCTEXT("OptionsMenu", "ForwardSpeedLimit", "Forward Speed Limit"),
        NSLOCTEXT("OptionsMenu", "ForwardSpeedLimitTip", "Maximum normal forward speed."),
        0.f,
        50000.f,
        100.f,
        [](FlightConfig const& config) {
            return config.translation.forward.normal.positive_speed_limit;
        },
        [](FlightConfig& config, float const value) {
            config.translation.forward.normal.positive_speed_limit = value;
        });
    add_flight_slider(
        NSLOCTEXT("OptionsMenu", "ForwardAcceleration", "Forward Acceleration"),
        NSLOCTEXT("OptionsMenu", "ForwardAccelerationTip", "Normal forward thrust acceleration."),
        0.f,
        50000.f,
        100.f,
        [](FlightConfig const& config) {
            return config.translation.forward.normal.positive_acceleration;
        },
        [](FlightConfig& config, float const value) {
            config.translation.forward.normal.positive_acceleration = value;
        });
    add_flight_slider(
        NSLOCTEXT("OptionsMenu", "ForwardPassiveDrag", "Forward Passive Drag"),
        NSLOCTEXT(
            "OptionsMenu", "ForwardPassiveDragTip", "Passive speed loss applied continuously."),
        0.f,
        20000.f,
        100.f,
        [](FlightConfig const& config) { return config.translation.forward.passive_drag; },
        [](FlightConfig& config, float const value) {
            config.translation.forward.passive_drag = value;
        });
    add_flight_slider(
        NSLOCTEXT("OptionsMenu", "ForwardStabilization", "Forward Active Stabilization"),
        NSLOCTEXT("OptionsMenu",
                  "ForwardStabilizationTip",
                  "Counter-thrust applied toward zero speed while forward input is neutral."),
        0.f,
        30000.f,
        100.f,
        [](FlightConfig const& config) {
            return config.translation.forward.active_stabilization_rate;
        },
        [](FlightConfig& config, float const value) {
            config.translation.forward.active_stabilization_rate = value;
        });
    if (flight_config.translation.forward.automatic.semantic !=
        ::ioj::sim::player::TranslationSemantic::Disabled) {
        add_flight_slider(
            NSLOCTEXT("OptionsMenu", "ForwardAutomaticValue", "Automatic Forward Value"),
            NSLOCTEXT("OptionsMenu",
                      "ForwardAutomaticValueTip",
                      "Automatic forward speed or acceleration requested by this model."),
            -50000.f,
            50000.f,
            100.f,
            [](FlightConfig const& config) {
                return config.translation.forward.automatic.automatic_value;
            },
            [](FlightConfig& config, float const value) {
                config.translation.forward.automatic.automatic_value = value;
            });
    }
    if (flight_config.translation.forward.manual.semantic ==
        ::ioj::sim::player::TranslationSemantic::Acceleration) {
        add_flight_slider(
            NSLOCTEXT("OptionsMenu", "BoostForwardAcceleration", "Boost Forward Acceleration"),
            NSLOCTEXT("OptionsMenu",
                      "BoostForwardAccelerationTip",
                      "Forward thrust acceleration while boost is effective."),
            0.f,
            100000.f,
            100.f,
            [](FlightConfig const& config) {
                return config.translation.forward.boosted.positive_acceleration;
            },
            [](FlightConfig& config, float const value) {
                config.translation.forward.boosted.positive_acceleration = value;
            });
    }
    if (flight_config.translation.forward.manual.response.mode ==
        ::ioj::sim::player::ResponseMode::RateLimited) {
        add_flight_slider(
            NSLOCTEXT("OptionsMenu", "ForwardResponseIncrease", "Forward Response Acceleration"),
            NSLOCTEXT("OptionsMenu",
                      "ForwardResponseIncreaseTip",
                      "Rate at which forward target velocity increases."),
            0.f,
            60000.f,
            100.f,
            [](FlightConfig const& config) {
                return config.translation.forward.manual.response.rate_limited.increasing_rate;
            },
            [](FlightConfig& config, float const value) {
                config.translation.forward.manual.response.rate_limited.increasing_rate = value;
            });
        add_flight_slider(
            NSLOCTEXT("OptionsMenu", "ForwardResponseDecrease", "Forward Response Deceleration"),
            NSLOCTEXT("OptionsMenu",
                      "ForwardResponseDecreaseTip",
                      "Active counter-thrust rate toward the requested forward velocity."),
            0.f,
            60000.f,
            100.f,
            [](FlightConfig const& config) {
                return config.translation.forward.manual.response.rate_limited.decreasing_rate;
            },
            [](FlightConfig& config, float const value) {
                config.translation.forward.manual.response.rate_limited.decreasing_rate = value;
            });
    }
    if (flight_config.translation.right.manual.semantic !=
            ::ioj::sim::player::TranslationSemantic::Disabled ||
        flight_config.translation.right.automatic.semantic !=
            ::ioj::sim::player::TranslationSemantic::Disabled) {
        add_flight_slider(
            NSLOCTEXT("OptionsMenu", "RightSpeedLimit", "Lateral Speed Limit"),
            NSLOCTEXT("OptionsMenu", "RightSpeedLimitTip", "Maximum normal left/right speed."),
            0.f,
            30000.f,
            100.f,
            [](FlightConfig const& config) {
                return config.translation.right.normal.positive_speed_limit;
            },
            [](FlightConfig& config, float const value) {
                config.translation.right.normal.positive_speed_limit = value;
                config.translation.right.normal.negative_speed_limit = value;
            });
    }
    if (flight_config.translation.up.manual.semantic !=
            ::ioj::sim::player::TranslationSemantic::Disabled ||
        flight_config.translation.up.automatic.semantic !=
            ::ioj::sim::player::TranslationSemantic::Disabled) {
        add_flight_slider(
            NSLOCTEXT("OptionsMenu", "UpSpeedLimit", "Vertical Speed Limit"),
            NSLOCTEXT("OptionsMenu", "UpSpeedLimitTip", "Maximum normal up/down speed."),
            0.f,
            30000.f,
            100.f,
            [](FlightConfig const& config) {
                return config.translation.up.normal.positive_speed_limit;
            },
            [](FlightConfig& config, float const value) {
                config.translation.up.normal.positive_speed_limit = value;
                config.translation.up.normal.negative_speed_limit = value;
            });
    }
    add_flight_slider(
        NSLOCTEXT("OptionsMenu", "PitchRate", "Pitch Rate"),
        NSLOCTEXT(
            "OptionsMenu", "PitchRateTip", "Maximum physical pitch rate in degrees per second."),
        0.f,
        180.f,
        1.f,
        [](FlightConfig const& config) { return config.rotation.pitch.maximum_rate; },
        [](FlightConfig& config, float const value) {
            config.rotation.pitch.maximum_rate = value;
        });
    add_flight_slider(
        NSLOCTEXT("OptionsMenu", "YawRate", "Yaw Rate"),
        NSLOCTEXT("OptionsMenu", "YawRateTip", "Maximum physical yaw rate in degrees per second."),
        0.f,
        180.f,
        1.f,
        [](FlightConfig const& config) { return config.rotation.yaw.maximum_rate; },
        [](FlightConfig& config, float const value) { config.rotation.yaw.maximum_rate = value; });
    add_flight_slider(
        NSLOCTEXT("OptionsMenu", "BrakeDeceleration", "Brake Deceleration"),
        NSLOCTEXT("OptionsMenu",
                  "BrakeDecelerationTip",
                  "Rate at which braking reduces world-space speed."),
        0.f,
        60000.f,
        100.f,
        [](FlightConfig const& config) { return config.brake.deceleration; },
        [](FlightConfig& config, float const value) { config.brake.deceleration = value; });
    add_flight_slider(
        NSLOCTEXT("OptionsMenu", "EmergencyBrakeDeceleration", "Emergency Brake Deceleration"),
        NSLOCTEXT("OptionsMenu",
                  "EmergencyBrakeDecelerationTip",
                  "Heavy brake rate applied to the complete velocity vector."),
        0.f,
        100000.f,
        100.f,
        [](FlightConfig const& config) { return config.emergency_brake.deceleration; },
        [](FlightConfig& config, float const value) {
            config.emergency_brake.deceleration = value;
        });
    add_flight_slider(
        NSLOCTEXT("OptionsMenu", "BoostForwardSpeedLimit", "Boost Forward Speed Limit"),
        NSLOCTEXT("OptionsMenu",
                  "BoostForwardSpeedLimitTip",
                  "Maximum forward-axis speed while boost is effective."),
        0.f,
        100000.f,
        100.f,
        [](FlightConfig const& config) {
            return config.translation.forward.boosted.positive_speed_limit;
        },
        [](FlightConfig& config, float const value) {
            config.translation.forward.boosted.positive_speed_limit = value;
        });
    add_flight_slider(
        NSLOCTEXT("OptionsMenu", "BoostedSpeedLimit", "Boosted Resultant Speed Limit"),
        NSLOCTEXT(
            "OptionsMenu", "BoostedSpeedLimitTip", "Maximum total speed while boost is effective."),
        0.f,
        100000.f,
        100.f,
        [](FlightConfig const& config) { return config.boosted_maximum_resultant_speed; },
        [](FlightConfig& config, float const value) {
            config.boosted_maximum_resultant_speed = value;
        });

    auto const* second_order{flight_config.translation.forward.automatic.response.mode ==
                                     ::ioj::sim::player::ResponseMode::SecondOrder
                                 ? &flight_config.translation.forward.automatic.response
                             : flight_config.translation.forward.manual.response.mode ==
                                     ::ioj::sim::player::ResponseMode::SecondOrder
                                 ? &flight_config.translation.forward.manual.response
                                 : nullptr};
    if (second_order != nullptr) {
        auto const automatic{flight_config.translation.forward.automatic.response.mode ==
                             ::ioj::sim::player::ResponseMode::SecondOrder};
        add_flight_slider(
            NSLOCTEXT("OptionsMenu", "SecondOrderSettlingTime", "Second-Order Settling Time"),
            NSLOCTEXT("OptionsMenu",
                      "SecondOrderSettlingTimeTip",
                      "Settling time for the active forward response."),
            0.05f,
            10.f,
            0.05f,
            [automatic](FlightConfig const& config) {
                auto const& response{automatic ? config.translation.forward.automatic.response
                                               : config.translation.forward.manual.response};
                return response.second_order.settling_time;
            },
            [automatic](FlightConfig& config, float const value) {
                auto& response{automatic ? config.translation.forward.automatic.response
                                         : config.translation.forward.manual.response};
                response.second_order.settling_time = value;
            });
        add_flight_slider(
            NSLOCTEXT("OptionsMenu", "SecondOrderDamping", "Second-Order Damping Ratio"),
            NSLOCTEXT("OptionsMenu",
                      "SecondOrderDampingTip",
                      "Damping ratio for the active forward response."),
            0.01f,
            0.99f,
            0.01f,
            [automatic](FlightConfig const& config) {
                auto const& response{automatic ? config.translation.forward.automatic.response
                                               : config.translation.forward.manual.response};
                return response.second_order.damping_ratio;
            },
            [automatic](FlightConfig& config, float const value) {
                auto& response{automatic ? config.translation.forward.automatic.response
                                         : config.translation.forward.manual.response};
                response.second_order.damping_ratio = value;
            });
    }
    add_section(NSLOCTEXT("OptionsMenu", "FlightModelTuningSection", "Flight Model Tuning"),
                flight_rows);

    TSharedPtr<SGameButton> keyboard_mouse_button;
    TSharedPtr<SGameButton> controller_button;
    auto device_rows{SNew(SVerticalBox)};
    device_rows->AddSlot().AutoHeight().Padding(style_->settings().row_padding)
        [SNew(SHorizontalBox) +
         SHorizontalBox::Slot().FillWidth(1.0f).Padding(
             FMargin{0.0f, 0.0f, style_->settings().button_spacing, 0.0f})
             [SAssignNew(keyboard_mouse_button, SGameButton)
                  .Style(&style_->button(EGameButtonStyle::Secondary))
                  .Audio(audio_)
                  .Selected(controls_device_ == EGameSettingDevice::KeyboardMouse)
                  .Text(NSLOCTEXT("OptionsMenu", "KeyboardMouseDevice", "Keyboard & Mouse"))
                  .OnClicked_Lambda([this] {
                      controls_device_ = EGameSettingDevice::KeyboardMouse;
                      request_controls_rebuild(FControlsFocusIdentity{
                          .kind = EControlsFocusKind::Device,
                          .device = EGameSettingDevice::KeyboardMouse,
                      });
                      return FReply::Handled();
                  })] +
         SHorizontalBox::Slot().FillWidth(
             1.0f)[SAssignNew(controller_button, SGameButton)
                       .Style(&style_->button(EGameButtonStyle::Secondary))
                       .Audio(audio_)
                       .Selected(controls_device_ == EGameSettingDevice::Controller)
                       .Text(NSLOCTEXT("OptionsMenu", "ControllerDevice", "Controller"))
                       .OnClicked_Lambda([this] {
                           controls_device_ = EGameSettingDevice::Controller;
                           request_controls_rebuild(FControlsFocusIdentity{
                               .kind = EControlsFocusKind::Device,
                               .device = EGameSettingDevice::Controller,
                           });
                           return FReply::Handled();
                       })]];
    add_section(NSLOCTEXT("OptionsMenu", "InputDeviceSection", "Input Device"), device_rows);
    register_controls_focus(
        FControlsFocusIdentity{
            .kind = EControlsFocusKind::Device,
            .device = EGameSettingDevice::KeyboardMouse,
        },
        keyboard_mouse_button.ToSharedRef());
    register_controls_focus(
        FControlsFocusIdentity{
            .kind = EControlsFocusKind::Device,
            .device = EGameSettingDevice::Controller,
        },
        controller_button.ToSharedRef());

    add_setting_sections(controls_device_);

    auto const bindings{settings->control_bindings(controls_device_type())};
    FText binding_category;
    TSharedPtr<SVerticalBox> binding_rows;
    auto const flush_binding_category = [&] {
        if (binding_rows.IsValid()) {
            add_section(binding_category, binding_rows.ToSharedRef());
        }
    };
    for (auto const& binding : bindings) {
        if (!binding_rows.IsValid() || !binding.display_category.EqualTo(binding_category)) {
            flush_binding_category();
            binding_category = binding.display_category;
            binding_rows = SNew(SVerticalBox);
        }
        binding_rows->AddSlot().AutoHeight().Padding(style_->settings().row_padding)
            [SNew(SSettingsRow)
                 .Style(&style_->settings())
                 .Label(binding.display_name)
                 .ToolTipText(NSLOCTEXT("OptionsMenu",
                                        "ManageBindingTip",
                                        "Open this binding to change, clear, or reset it."))
                     [build_binding_button(binding)]];
    }
    flush_binding_category();
    if (bindings.IsEmpty()) {
        auto empty_rows{SNew(SVerticalBox)};
        empty_rows->AddSlot()
            .AutoHeight()[SNew(STextBlock)
                              .Text(NSLOCTEXT(
                                  "OptionsMenu", "NoControlBindings", "No bindings available."))
                              .TextStyle(&style_->settings().empty_text)];
        add_section(NSLOCTEXT("OptionsMenu", "BindingsSection", "Bindings"), empty_rows);
    }

    pending_controls_focus_ = focus_to_restore;
    pending_controls_scroll_offset_ = scroll_offset;
    RegisterActiveTimer(0.0f,
                        FWidgetActiveTimerDelegate::CreateSP(
                            this, &SGameOptionsView::handle_deferred_controls_restore));
}

void SGameOptionsView::request_controls_rebuild(TOptional<FControlsFocusIdentity> requested_focus,
                                                bool const reset_scroll) {
    pending_controls_rebuild_focus_ = requested_focus;
    pending_controls_reset_scroll_ = reset_scroll;
    if (controls_rebuild_pending_) {
        return;
    }
    controls_rebuild_pending_ = true;
    RegisterActiveTimer(0.0f,
                        FWidgetActiveTimerDelegate::CreateSP(
                            this, &SGameOptionsView::handle_deferred_controls_rebuild));
}

auto SGameOptionsView::handle_deferred_controls_rebuild(double const current_time,
                                                        float const delta_time)
    -> EActiveTimerReturnType {
    static_cast<void>(current_time);
    static_cast<void>(delta_time);
    controls_rebuild_pending_ = false;
    auto const requested_focus{pending_controls_rebuild_focus_};
    auto const reset_scroll{pending_controls_reset_scroll_};
    pending_controls_rebuild_focus_.Reset();
    pending_controls_reset_scroll_ = false;
    rebuild_controls_page(requested_focus, reset_scroll);
    refresh();
    return EActiveTimerReturnType::Stop;
}

auto SGameOptionsView::handle_deferred_controls_restore(double const current_time,
                                                        float const delta_time)
    -> EActiveTimerReturnType {
    static_cast<void>(current_time);
    static_cast<void>(delta_time);
    if (controls_scroll_box_.IsValid()) {
        controls_scroll_box_->SetScrollOffset(pending_controls_scroll_offset_);
    }
    if (pending_controls_focus_.IsSet()) {
        restore_controls_focus(pending_controls_focus_.GetValue());
    }
    pending_controls_focus_.Reset();
    return EActiveTimerReturnType::Stop;
}

auto SGameOptionsView::build_binding_button(FControlBindingView const& binding)
    -> TSharedRef<SWidget> {
    auto const component_key_text{binding.current_key.IsValid()
                                      ? binding.current_key.GetDisplayName()
                                      : NSLOCTEXT("OptionsMenu", "UnboundControl", "Unbound")};
    auto key_text{component_key_text};
    if (binding.chord.IsSet() && binding.current_key.IsValid()) {
        auto const chord_key_text{binding.chord->current_key.IsValid()
                                      ? binding.chord->current_key.GetDisplayName()
                                      : NSLOCTEXT("OptionsMenu", "UnboundChord", "Unbound")};
        key_text = FText::Format(NSLOCTEXT("OptionsMenu", "ChordBindingFormat", "{0} + {1}"),
                                 chord_key_text,
                                 component_key_text);
    }

    TSharedPtr<SGameButton> button;
    auto result{SAssignNew(button, SGameButton)
                    .Style(&style_->button(EGameButtonStyle::Secondary))
                    .Audio(audio_)
                    .Text(key_text)
                    .OnClicked_Lambda([this, binding] {
                        open_binding_management(binding);
                        return FReply::Handled();
                    })};
    register_controls_focus(binding_focus_identity(binding), button.ToSharedRef());
    return result;
}

void SGameOptionsView::open_binding_management(FControlBindingView const& binding) {
    managed_binding_ = binding;
    binding_management_visible_ = true;
    binding_management_prompt_->SetVisibility(EVisibility::Visible);
    binding_change_button_->focus();
    on_interaction_modal_changed_.ExecuteIfBound(true);
}

void SGameOptionsView::close_binding_management(bool const restore_focus) {
    if (!binding_management_visible_) {
        return;
    }
    auto const identity{
        managed_binding_.IsSet()
            ? TOptional<FControlsFocusIdentity>{binding_focus_identity(managed_binding_.GetValue())}
            : TOptional<FControlsFocusIdentity>{}};
    binding_management_visible_ = false;
    binding_management_prompt_->SetVisibility(EVisibility::Collapsed);
    managed_binding_.Reset();
    if (restore_focus && identity.IsSet()) {
        restore_controls_focus(identity.GetValue());
    }
    on_interaction_modal_changed_.ExecuteIfBound(false);
}

void SGameOptionsView::begin_binding_capture(FControlBindingView const& binding) {
    binding_management_visible_ = false;
    binding_management_prompt_->SetVisibility(EVisibility::Collapsed);
    captured_binding_ = binding.address;
    captured_chord_.Reset();
    chord_capture_.clear();
    captured_key_ = EKeys::Invalid;
    capture_error_ = FText::GetEmpty();
    capture_prompt_->SetVisibility(EVisibility::Visible);
    FSlateApplication::Get().SetKeyboardFocus(SharedThis(this), EFocusCause::SetDirectly);
}

void SGameOptionsView::begin_chord_capture(FControlBindingView const& binding) {
    if (!binding.chord.IsSet()) {
        begin_binding_capture(binding);
        return;
    }
    binding_management_visible_ = false;
    binding_management_prompt_->SetVisibility(EVisibility::Collapsed);
    captured_binding_ = binding.address;
    captured_chord_ = binding.chord;
    captured_chord_dependent_count_ = 0;
    if (auto const* const settings{settings_.Get()}) {
        for (auto const& candidate :
             settings->control_bindings(EHardwareDevicePrimaryType::Unspecified)) {
            if (candidate.chord.IsSet() && candidate.chord->address == binding.chord->address) {
                ++captured_chord_dependent_count_;
            }
        }
    }
    clear_chord_capture();
    capture_prompt_->SetVisibility(EVisibility::Visible);
    FSlateApplication::Get().SetKeyboardFocus(SharedThis(this), EFocusCause::SetDirectly);
}

auto SGameOptionsView::accept_binding_key(FKey const key) -> FReply {
    auto* const settings{settings_.Get()};
    if (settings == nullptr || !captured_binding_.IsSet() || !key.IsValid()) {
        return FReply::Handled();
    }
    auto const mappings{settings->control_bindings(EHardwareDevicePrimaryType::Unspecified)};
    auto const* const target{mappings.FindByPredicate([this](auto const& candidate) {
        return candidate.address == captured_binding_.GetValue();
    })};
    if (target == nullptr ||
        (target->device_type == EHardwareDevicePrimaryType::Gamepad) != key.IsGamepadKey()) {
        capture_error_ =
            target != nullptr && target->device_type == EHardwareDevicePrimaryType::Gamepad
                ? NSLOCTEXT("OptionsMenu",
                            "ControllerInputRequired",
                            "Use a controller input for this binding.")
                : NSLOCTEXT("OptionsMenu",
                            "KeyboardMouseInputRequired",
                            "Use a keyboard or mouse input for this binding.");
        return FReply::Handled();
    }

    capture_error_ = FText::GetEmpty();
    auto const conflicts{settings->binding_conflicts(captured_binding_.GetValue(), key)};
    capture_prompt_->SetVisibility(EVisibility::Collapsed);
    if (!conflicts.IsEmpty()) {
        captured_key_ = key;
        conflict_prompt_->SetVisibility(EVisibility::Visible);
        conflict_replace_button_->focus();
        return FReply::Handled();
    }
    if (!settings->set_control_binding(captured_binding_.GetValue(), key, false)) {
        capture_error_ = NSLOCTEXT(
            "OptionsMenu", "BindingApplyFailed", "Could not apply the binding. Please try again.");
        capture_prompt_->SetVisibility(EVisibility::Visible);
        return FReply::Handled();
    }
    complete_binding_change();
    return FReply::Handled();
}

auto SGameOptionsView::accept_chord_key(FKey const key, bool const can_be_held) -> FReply {
    auto* const settings{settings_.Get()};
    if (settings == nullptr || !captured_binding_.IsSet() || !captured_chord_.IsSet() ||
        !key.IsValid() || chord_capture_.is_complete()) {
        return FReply::Handled();
    }
    auto const mappings{settings->control_bindings(EHardwareDevicePrimaryType::Unspecified)};
    auto const* const target{mappings.FindByPredicate([this](auto const& candidate) {
        return candidate.address == captured_binding_.GetValue();
    })};
    if (target == nullptr ||
        (target->device_type == EHardwareDevicePrimaryType::Gamepad) != key.IsGamepadKey()) {
        capture_error_ =
            target != nullptr && target->device_type == EHardwareDevicePrimaryType::Gamepad
                ? NSLOCTEXT("OptionsMenu",
                            "ControllerInputRequired",
                            "Use a controller input for this binding.")
                : NSLOCTEXT("OptionsMenu",
                            "KeyboardMouseInputRequired",
                            "Use a keyboard or mouse input for this binding.");
        return FReply::Handled();
    }
    capture_error_ = FText::GetEmpty();
    if (chord_capture_.accept(key, can_be_held) && chord_confirm_button_.IsValid()) {
        chord_confirm_button_->focus();
    }
    return FReply::Handled();
}

auto SGameOptionsView::release_chord_key(FKey const key) -> FReply {
    chord_capture_.release(key);
    return FReply::Handled();
}

void SGameOptionsView::clear_chord_capture() {
    chord_capture_.clear();
    capture_error_ = FText::GetEmpty();
}

auto SGameOptionsView::confirm_chord_capture() -> FReply {
    auto* const settings{settings_.Get()};
    if (settings == nullptr || !captured_binding_.IsSet() || !captured_chord_.IsSet() ||
        !chord_capture_.is_complete()) {
        return FReply::Handled();
    }
    auto const conflicts{settings->chord_binding_conflicts(
        captured_binding_.GetValue(), chord_capture_.activator_key(), chord_capture_.action_key())};
    capture_prompt_->SetVisibility(EVisibility::Collapsed);
    if (!conflicts.IsEmpty()) {
        conflict_prompt_->SetVisibility(EVisibility::Visible);
        conflict_replace_button_->focus();
        return FReply::Handled();
    }
    if (!settings->set_control_chord(captured_binding_.GetValue(),
                                     chord_capture_.activator_key(),
                                     chord_capture_.action_key(),
                                     false)) {
        capture_error_ = NSLOCTEXT(
            "OptionsMenu", "BindingApplyFailed", "Could not apply the binding. Please try again.");
        capture_prompt_->SetVisibility(EVisibility::Visible);
        return FReply::Handled();
    }
    complete_binding_change();
    return FReply::Handled();
}

void SGameOptionsView::close_binding_prompt(bool const restore_focus) {
    auto const was_open{captured_binding_.IsSet()};
    auto const identity{
        managed_binding_.IsSet()
            ? TOptional<FControlsFocusIdentity>{binding_focus_identity(managed_binding_.GetValue())}
            : TOptional<FControlsFocusIdentity>{}};
    if (capture_prompt_.IsValid()) {
        capture_prompt_->SetVisibility(EVisibility::Collapsed);
    }
    if (conflict_prompt_.IsValid()) {
        conflict_prompt_->SetVisibility(EVisibility::Collapsed);
    }
    captured_binding_.Reset();
    captured_chord_.Reset();
    clear_chord_capture();
    captured_key_ = EKeys::Invalid;
    captured_chord_dependent_count_ = 0;
    managed_binding_.Reset();
    if (restore_focus && identity.IsSet()) {
        restore_controls_focus(identity.GetValue());
    }
    if (was_open) {
        on_interaction_modal_changed_.ExecuteIfBound(false);
    }
}

void SGameOptionsView::complete_binding_change() {
    auto const identity{
        managed_binding_.IsSet()
            ? TOptional<FControlsFocusIdentity>{binding_focus_identity(managed_binding_.GetValue())}
            : TOptional<FControlsFocusIdentity>{}};
    close_binding_prompt(false);
    request_controls_rebuild(identity);
}

/* **************************************** */
// Settings and prompts
/* **************************************** */
auto SGameOptionsView::build_setting_row(FGameSettingDescriptor const& descriptor,
                                         TFunction<void()>& focus_action) -> TSharedRef<SWidget> {
    auto const weak_settings{settings_};
    auto available{TAttribute<bool>::CreateLambda([weak_settings, setting = descriptor.id] {
        auto const* const settings{weak_settings.Get()};
        return settings != nullptr && settings->is_available(setting);
    })};

    switch (descriptor.control_kind) {
        case ESettingControlKind::Toggle: {
            TSharedPtr<SSettingsToggle> control;
            auto checked{TAttribute<bool>::CreateLambda([weak_settings, setting = descriptor.id] {
                auto const* const settings{weak_settings.Get()};
                if (settings == nullptr) {
                    return false;
                }
                auto const value{settings->value(setting)};
                auto const* const typed{std::get_if<bool>(&value)};
                return typed != nullptr && *typed;
            })};

            auto row{SAssignNew(control, SSettingsToggle)
                         .Style(&style_->settings())
                         .Label(descriptor.label)
                         .ToolTipText(descriptor.tooltip)
                         .Checked(checked)
                         .ControlEnabled(available)
                         .OnCheckStateChanged_Lambda([weak_settings, setting = descriptor.id](
                                                         ECheckBoxState const state) {
                             if (auto* const settings{weak_settings.Get()}) {
                                 settings->set_setting(
                                     setting, FGameSettingValue{state == ECheckBoxState::Checked});
                             }
                         })};

            focus_action = [control] { control->focus(); };
            return row;
        }
        case ESettingControlKind::Choice: {
            TSharedPtr<SSettingsChoice> control;
            auto const options{settings_.IsValid() ? settings_->options(descriptor.id)
                                                   : TArray<FGameSettingOption>{}};
            TArray<FText> labels;
            labels.Reserve(options.Num());
            for (auto const& option : options) {
                labels.Add(option.label);
            }

            auto selected{
                TAttribute<int32>::CreateLambda([weak_settings, setting = descriptor.id, options] {
                    auto const* const settings{weak_settings.Get()};
                    if (settings == nullptr) {
                        return int32{INDEX_NONE};
                    }
                    auto const value{settings->value(setting)};
                    return options.IndexOfByPredicate([&value](FGameSettingOption const& option) {
                        return option.value == value;
                    });
                })};

            auto row{
                SAssignNew(control, SSettingsChoice)
                    .Style(&style_->settings())
                    .Label(descriptor.label)
                    .ToolTipText(descriptor.tooltip)
                    .Options(MoveTemp(labels))
                    .SelectedIndex(selected)
                    .ControlEnabled(available)
                    .OnSelectionChanged_Lambda(
                        [this, weak_settings, setting = descriptor.id, options](int32 const index) {
                            if (auto* const settings{weak_settings.Get()};
                                options.IsValidIndex(index)) {
                                settings->set_setting(setting, options[index].value);
                                if (setting == EGameSetting::PlayerShipFlightControlPreset) {
                                    request_controls_rebuild({}, false);
                                }
                            }
                        })};

            focus_action = [control] { control->focus(); };
            return row;
        }
        case ESettingControlKind::FloatRange: {
            TSharedPtr<SSettingsSlider> control;
            auto value{TAttribute<float>::CreateLambda([weak_settings, setting = descriptor.id] {
                auto const* const settings{weak_settings.Get()};
                if (settings == nullptr) {
                    return 0.0f;
                }
                auto const current{settings->value(setting)};
                auto const* const typed{std::get_if<float>(&current)};
                return typed != nullptr ? *typed : 0.0f;
            })};

            auto value_text{TAttribute<FText>::CreateLambda(
                [this, descriptor] { return format_range_value(descriptor); })};

            auto row{SAssignNew(control, SSettingsSlider)
                         .Style(&style_->settings())
                         .Label(descriptor.label)
                         .ToolTipText(descriptor.tooltip)
                         .Value(value)
                         .ValueText(value_text)
                         .Minimum(static_cast<float>(descriptor.minimum))
                         .Maximum(static_cast<float>(descriptor.maximum))
                         .Step(static_cast<float>(descriptor.step))
                         .ControlEnabled(available)
                         .OnValueChanged_Lambda(
                             [weak_settings, setting = descriptor.id](float const next) {
                                 if (auto* const settings{weak_settings.Get()}) {
                                     settings->set_setting(setting, FGameSettingValue{next});
                                 }
                             })};

            focus_action = [control] { control->focus(); };
            return row;
        }
        case ESettingControlKind::IntegerRange: {
            TSharedPtr<SSettingsSlider> control;
            auto value{TAttribute<float>::CreateLambda([weak_settings, setting = descriptor.id] {
                auto const* const settings{weak_settings.Get()};
                if (settings == nullptr) {
                    return 0.0f;
                }
                auto const current{settings->value(setting)};
                auto const* const typed{std::get_if<int32>(&current)};
                return typed != nullptr ? static_cast<float>(*typed) : 0.0f;
            })};

            auto value_text{
                TAttribute<FText>::CreateLambda([weak_settings, setting = descriptor.id] {
                    auto const* const settings{weak_settings.Get()};
                    if (settings == nullptr) {
                        return FText::GetEmpty();
                    }
                    auto const current{settings->value(setting)};
                    auto const* const typed{std::get_if<int32>(&current)};
                    return typed != nullptr ? FText::AsNumber(*typed) : FText::GetEmpty();
                })};

            auto row{SAssignNew(control, SSettingsSlider)
                         .Style(&style_->settings())
                         .Label(descriptor.label)
                         .ToolTipText(descriptor.tooltip)
                         .Value(value)
                         .ValueText(value_text)
                         .Minimum(static_cast<float>(descriptor.minimum))
                         .Maximum(static_cast<float>(descriptor.maximum))
                         .Step(static_cast<float>(descriptor.step))
                         .ControlEnabled(available)
                         .OnValueChanged_Lambda(
                             [weak_settings, setting = descriptor.id](float const next) {
                                 if (auto* const settings{weak_settings.Get()}) {
                                     settings->set_setting(
                                         setting, FGameSettingValue{FMath::RoundToInt32(next)});
                                 }
                             })};

            focus_action = [control] { control->focus(); };
            return row;
        }
        case ESettingControlKind::Custom:
            return SNew(SSettingsReadOnlyRow)
                .Style(&style_->settings())
                .Label(descriptor.label)
                .Value(NSLOCTEXT("OptionsMenu", "UnsupportedCustomSetting", "Unavailable"))
                .ToolTipText(descriptor.tooltip);
    }

    return SNew(STextBlock).Text(NSLOCTEXT("OptionsMenu", "InvalidSetting", "Invalid setting"));
}

auto SGameOptionsView::build_system_page() -> TSharedRef<SWidget> {
    auto content{SNew(SVerticalBox)};
    if (capabilities_ == nullptr) {
        content->AddSlot().AutoHeight()[SNew(STextBlock)
                                            .Text(NSLOCTEXT("OptionsMenu",
                                                            "SystemUnavailable",
                                                            "System information is unavailable."))
                                            .TextStyle(&style_->settings().empty_text)];
        return SNew(SScrollBox).ScrollBarStyle(&style_->settings().scroll_bar) +
               SScrollBox::Slot()[content];
    }

    auto const add_section = [this, &content](FText title,
                                              TArray<TPair<FText, FText>> const& values) {
        auto rows{SNew(SVerticalBox)};
        for (auto const& value : values) {
            rows->AddSlot().AutoHeight().Padding(
                style_->settings().row_padding)[SNew(SSettingsReadOnlyRow)
                                                    .Style(&style_->settings())
                                                    .Label(value.Key)
                                                    .Value(value.Value)];
        }
        content->AddSlot().AutoHeight().Padding(
            FMargin{0.0f,
                    0.0f,
                    0.0f,
                    style_->settings()
                        .section_spacing})[SNew(SSettingsSection)
                                               .Style(&style_->settings())
                                               .Header()[SNew(SHiveSectionHeader)
                                                             .Style(style_)
                                                             .Icon(&style_->icon(EGameUiIcon::Hive))
                                                             .Text(title)]
                                               .Title(MoveTemp(title))[rows]];
    };

    auto operating_system{capabilities_->operating_system_version};
    if (!capabilities_->operating_system_subversion.IsEmpty()) {
        if (!operating_system.IsEmpty()) {
            operating_system += TEXT(" ");
        }
        operating_system += capabilities_->operating_system_subversion;
    }

    add_section(NSLOCTEXT("OptionsMenu", "PlatformSection", "Platform"),
                {{NSLOCTEXT("OptionsMenu", "PlatformLabel", "Platform"),
                  friendly_platform_name(capabilities_->platform_name)},
                 {NSLOCTEXT("OptionsMenu", "ArchitectureLabel", "Architecture"),
                  capability_text(capabilities_->host_architecture)},
                 {NSLOCTEXT("OptionsMenu", "OperatingSystemLabel", "Operating System"),
                  capability_text(operating_system)}});
    add_section(
        NSLOCTEXT("OptionsMenu", "ProcessorSection", "Processor"),
        {{NSLOCTEXT("OptionsMenu", "CpuVendorLabel", "CPU Vendor"),
          capability_text(capabilities_->cpu_vendor)},
         {NSLOCTEXT("OptionsMenu", "CpuLabel", "CPU"), capability_text(capabilities_->cpu_brand)},
         {NSLOCTEXT("OptionsMenu", "PhysicalCoresLabel", "Physical Cores"),
          FText::AsNumber(capabilities_->physical_core_count)},
         {NSLOCTEXT("OptionsMenu", "LogicalCoresLabel", "Logical Cores"),
          FText::AsNumber(capabilities_->logical_core_count)}});

    auto const& simd{capabilities_->cpu_simd};
    if (simd.available) {
        add_section(NSLOCTEXT("OptionsMenu", "SimdSection", "SIMD Features"),
                    {{NSLOCTEXT("OptionsMenu", "SseLabel", "SSE"),
                      simd_feature_text({{simd.sse, TEXT("SSE")},
                                         {simd.sse2, TEXT("SSE2")},
                                         {simd.sse3, TEXT("SSE3")},
                                         {simd.ssse3, TEXT("SSSE3")},
                                         {simd.sse4_1, TEXT("SSE4.1")},
                                         {simd.sse4_2, TEXT("SSE4.2")},
                                         {simd.sse4a, TEXT("SSE4a")}})},
                     {NSLOCTEXT("OptionsMenu", "AvxLabel", "AVX"),
                      simd_feature_text({{simd.avx, TEXT("AVX")},
                                         {simd.avx2, TEXT("AVX2")},
                                         {simd.avx_vnni, TEXT("AVX-VNNI")}})},
                     {NSLOCTEXT("OptionsMenu", "Avx512Label", "AVX-512"),
                      simd_feature_text({{simd.avx512_f, TEXT("AVX-512F")},
                                         {simd.avx512_cd, TEXT("AVX-512CD")},
                                         {simd.avx512_bw, TEXT("AVX-512BW")},
                                         {simd.avx512_dq, TEXT("AVX-512DQ")},
                                         {simd.avx512_vl, TEXT("AVX-512VL")}})},
                     {NSLOCTEXT("OptionsMenu", "AmxLabel", "AMX"),
                      simd_feature_text({{simd.amx_tile, TEXT("AMX-TILE")},
                                         {simd.amx_bf16, TEXT("AMX-BF16")},
                                         {simd.amx_int8, TEXT("AMX-INT8")},
                                         {simd.amx_fp16, TEXT("AMX-FP16")}})}});
    } else {
        add_section(NSLOCTEXT("OptionsMenu", "SimdSection", "SIMD Features"),
                    {{NSLOCTEXT("OptionsMenu", "SimdDetectionLabel", "Detection"),
                      NSLOCTEXT("OptionsMenu", "SimdDetectionUnavailable", "Unavailable")}});
    }

    add_section(NSLOCTEXT("OptionsMenu", "GraphicsSection", "Graphics"),
                {{NSLOCTEXT("OptionsMenu", "GpuLabel", "GPU"),
                  capability_text(capabilities_->primary_gpu_brand)}});
    add_section(NSLOCTEXT("OptionsMenu", "MemorySection", "Memory"),
                {{NSLOCTEXT("OptionsMenu", "PhysicalMemoryLabel", "Physical Memory"),
                  FText::AsMemory(capabilities_->total_physical_memory_bytes)}});

#if PLATFORM_WINDOWS
    auto const large_page_minimum{
        capabilities_->windows.large_page_minimum_bytes == 0
            ? NSLOCTEXT("OptionsMenu", "LargePageUnsupported", "Unsupported")
            : FText::AsMemory(capabilities_->windows.large_page_minimum_bytes)};
    add_section(NSLOCTEXT("OptionsMenu", "WindowsSection", "Windows"),
                {{NSLOCTEXT("OptionsMenu", "LargePageMinimumLabel", "Minimum Large Page"),
                  large_page_minimum},
                 {NSLOCTEXT("OptionsMenu", "LargePageAccessLabel", "Large Page Access"),
                  large_page_access_text(capabilities_->windows.large_page_access_status)}});
#endif

    return SNew(SScrollBox)
               .ScrollBarStyle(&style_->settings().scroll_bar)
               .AnimateWheelScrolling(true) +
           SScrollBox::Slot()[content];
}

auto SGameOptionsView::build_dirty_prompt() -> TSharedRef<SWidget> {
    auto const& primary{style_->button(EGameButtonStyle::Primary)};
    auto const& secondary{style_->button(EGameButtonStyle::Secondary)};
    auto dirty_apply{SAssignNew(dirty_apply_button_, SGameButton)
                         .Style(&primary)
                         .Audio(audio_)
                         .Text(NSLOCTEXT("OptionsMenu", "PromptApply", "Apply"))
                         .OnClicked_Lambda([delegate = on_dirty_apply_]() {
                             delegate.ExecuteIfBound();
                             return FReply::Handled();
                         })};
    auto dirty_discard{SAssignNew(dirty_discard_button_, SGameButton)
                           .Style(&secondary)
                           .Audio(audio_)
                           .Text(NSLOCTEXT("OptionsMenu", "PromptDiscard", "Discard"))
                           .OnClicked_Lambda([delegate = on_dirty_discard_]() {
                               delegate.ExecuteIfBound();
                               return FReply::Handled();
                           })};
    auto dirty_stay{SAssignNew(dirty_stay_button_, SGameButton)
                        .Style(&secondary)
                        .Audio(audio_)
                        .Text(NSLOCTEXT("OptionsMenu", "PromptStay", "Stay"))
                        .OnClicked_Lambda([delegate = on_dirty_stay_]() {
                            delegate.ExecuteIfBound();
                            return FReply::Handled();
                        })};
    return build_modal(
        NSLOCTEXT("OptionsMenu", "UnsavedPrompt", "Apply your changes before leaving?"),
        {dirty_apply, dirty_discard, dirty_stay});
}

auto SGameOptionsView::build_display_prompt() -> TSharedRef<SWidget> {
    auto const& primary{style_->button(EGameButtonStyle::Primary)};
    auto const& secondary{style_->button(EGameButtonStyle::Secondary)};
    auto confirm{SAssignNew(confirm_display_button_, SGameButton)
                     .Style(&primary)
                     .Audio(audio_)
                     .Text(NSLOCTEXT("OptionsMenu", "KeepChanges", "Keep Changes"))
                     .OnClicked_Lambda([delegate = on_confirm_display_]() {
                         delegate.ExecuteIfBound();
                         return FReply::Handled();
                     })};
    auto revert{SAssignNew(revert_display_button_, SGameButton)
                    .Style(&secondary)
                    .Audio(audio_)
                    .Text(NSLOCTEXT("OptionsMenu", "RevertChanges", "Revert"))
                    .OnClicked_Lambda([delegate = on_revert_display_]() {
                        delegate.ExecuteIfBound();
                        return FReply::Handled();
                    })};
    auto const weak_settings{settings_};
    auto title{TAttribute<FText>::CreateLambda([weak_settings] {
        auto const* const settings{weak_settings.Get()};
        auto const seconds{settings != nullptr ? settings->display_confirmation_seconds_remaining()
                                               : 0};
        return FText::Format(NSLOCTEXT("OptionsMenu",
                                       "DisplayConfirmation",
                                       "Keep these display settings? Reverting in {0} seconds."),
                             FText::AsNumber(seconds));
    })};
    return build_modal(MoveTemp(title), {confirm, revert});
}

auto SGameOptionsView::build_binding_management_prompt() -> TSharedRef<SWidget> {
    auto change{SAssignNew(binding_change_button_, SGameButton)
                    .Style(&style_->button(EGameButtonStyle::Primary))
                    .Audio(audio_)
                    .Text(NSLOCTEXT("OptionsMenu", "ChangeBinding", "Change"))
                    .OnClicked_Lambda([this] {
                        if (!managed_binding_.IsSet()) {
                            return FReply::Handled();
                        }
                        auto const binding{managed_binding_.GetValue()};
                        if (binding.chord.IsSet()) {
                            begin_chord_capture(binding);
                        } else {
                            begin_binding_capture(binding);
                        }
                        return FReply::Handled();
                    })};
    auto clear{SAssignNew(binding_clear_button_, SGameButton)
                   .Style(&style_->button(EGameButtonStyle::Secondary))
                   .Audio(audio_)
                   .Text(NSLOCTEXT("OptionsMenu", "ClearBinding", "Clear"))
                   .Enabled_Lambda([this] {
                       return managed_binding_.IsSet() && managed_binding_->current_key.IsValid();
                   })
                   .OnClicked_Lambda([this] {
                       if (!managed_binding_.IsSet()) {
                           return FReply::Handled();
                       }
                       auto const identity{binding_focus_identity(managed_binding_.GetValue())};
                       auto const address{managed_binding_->address};
                       if (auto* const settings{settings_.Get()}) {
                           settings->clear_control_binding(address);
                       }
                       close_binding_management(false);
                       request_controls_rebuild(identity);
                       return FReply::Handled();
                   })};
    auto reset{SAssignNew(binding_reset_button_, SGameButton)
                   .Style(&style_->button(EGameButtonStyle::Secondary))
                   .Audio(audio_)
                   .Text(NSLOCTEXT("OptionsMenu", "ResetBinding", "Reset"))
                   .Enabled_Lambda(
                       [this] { return managed_binding_.IsSet() && managed_binding_->modified; })
                   .Visibility_Lambda([this] {
                       return managed_binding_.IsSet() && managed_binding_->custom_profile
                                ? EVisibility::Collapsed
                                : EVisibility::Visible;
                   })
                   .OnClicked_Lambda([this] {
                       if (!managed_binding_.IsSet()) {
                           return FReply::Handled();
                       }
                       auto const identity{binding_focus_identity(managed_binding_.GetValue())};
                       auto const address{managed_binding_->address};
                       if (auto* const settings{settings_.Get()}) {
                           settings->reset_control_binding(address);
                       }
                       close_binding_management(false);
                       request_controls_rebuild(identity);
                       return FReply::Handled();
                   })};
    auto cancel{SAssignNew(binding_cancel_button_, SGameButton)
                    .Style(&style_->button(EGameButtonStyle::Secondary))
                    .Audio(audio_)
                    .Text(NSLOCTEXT("OptionsMenu", "CancelBindingManagement", "Cancel"))
                    .OnClicked_Lambda([this] {
                        close_binding_management(true);
                        return FReply::Handled();
                    })};
    auto title{TAttribute<FText>::CreateLambda([this] {
        if (!managed_binding_.IsSet()) {
            return FText::GetEmpty();
        }
        auto const& binding{managed_binding_.GetValue()};
        auto const component_key_text{binding.current_key.IsValid()
                                          ? binding.current_key.GetDisplayName()
                                          : NSLOCTEXT("OptionsMenu", "UnboundControl", "Unbound")};
        auto key_text{component_key_text};
        if (binding.chord.IsSet() && binding.current_key.IsValid()) {
            auto const chord_key_text{binding.chord->current_key.IsValid()
                                          ? binding.chord->current_key.GetDisplayName()
                                          : NSLOCTEXT("OptionsMenu", "UnboundChord", "Unbound")};
            key_text = FText::Format(NSLOCTEXT("OptionsMenu", "ChordBindingFormat", "{0} + {1}"),
                                     chord_key_text,
                                     component_key_text);
        }
        return FText::Format(
            NSLOCTEXT("OptionsMenu", "BindingManagementPrompt", "{0}\n\nCurrent binding: {1}"),
            binding.display_name,
            key_text);
    })};
    return build_modal(MoveTemp(title), {change, clear, reset, cancel});
}

auto SGameOptionsView::build_capture_prompt() -> TSharedRef<SWidget> {
    auto confirm{SAssignNew(chord_confirm_button_, SGameButton)
                     .Style(&style_->button(EGameButtonStyle::Primary))
                     .Audio(audio_)
                     .Text(NSLOCTEXT("OptionsMenu", "ConfirmChordCapture", "Confirm"))
                     .Enabled_Lambda(
                         [this] { return captured_chord_.IsSet() && chord_capture_.is_complete(); })
                     .Visibility_Lambda([this] {
                         return captured_chord_.IsSet() ? EVisibility::Visible
                                                        : EVisibility::Collapsed;
                     })
                     .OnClicked_Lambda([this] { return confirm_chord_capture(); })};
    auto clear{SAssignNew(chord_clear_button_, SGameButton)
                   .Style(&style_->button(EGameButtonStyle::Secondary))
                   .Audio(audio_)
                   .Text(NSLOCTEXT("OptionsMenu", "ClearChordCapture", "Clear Capture"))
                   .Visibility_Lambda([this] {
                       return captured_chord_.IsSet() ? EVisibility::Visible
                                                      : EVisibility::Collapsed;
                   })
                   .OnClicked_Lambda([this] {
                       clear_chord_capture();
                       FSlateApplication::Get().SetKeyboardFocus(SharedThis(this),
                                                                 EFocusCause::SetDirectly);
                       return FReply::Handled();
                   })};
    auto cancel{SAssignNew(chord_cancel_button_, SGameButton)
                    .Style(&style_->button(EGameButtonStyle::Secondary))
                    .Audio(audio_)
                    .Text(NSLOCTEXT("OptionsMenu", "CancelChordCapture", "Cancel"))
                    .Visibility_Lambda([this] {
                        return captured_chord_.IsSet() ? EVisibility::Visible
                                                       : EVisibility::Collapsed;
                    })
                    .OnClicked_Lambda([this] {
                        close_binding_prompt(true);
                        return FReply::Handled();
                    })};
    auto title{TAttribute<FText>::CreateLambda([this] {
        if (!capture_error_.IsEmpty()) {
            return capture_error_;
        }
        if (!captured_chord_.IsSet()) {
            return controls_device_type() == EHardwareDevicePrimaryType::Gamepad
                     ? NSLOCTEXT("OptionsMenu",
                                 "CaptureControllerBindingPrompt",
                                 "Press a controller input. Press Back to cancel.")
                     : NSLOCTEXT("OptionsMenu",
                                 "CaptureKeyboardMouseBindingPrompt",
                                 "Press a keyboard or mouse input. Press Back to cancel.");
        }

        FText candidate;
        if (chord_capture_.is_complete()) {
            candidate =
                FText::Format(NSLOCTEXT("OptionsMenu", "CapturedChord", "Captured: {0} + {1}"),
                              chord_capture_.activator_key().GetDisplayName(),
                              chord_capture_.action_key().GetDisplayName());
        } else if (chord_capture_.held_key().IsValid()) {
            candidate = FText::Format(NSLOCTEXT("OptionsMenu", "PartialChord", "Held: {0} + …"),
                                      chord_capture_.held_key().GetDisplayName());
        } else {
            candidate = NSLOCTEXT("OptionsMenu", "WaitingForChord", "Waiting for input…");
        }
        auto const shared_warning{
            captured_chord_dependent_count_ > 1
                ? FText::Format(NSLOCTEXT("OptionsMenu",
                                          "SharedChordActivator",
                                          "\nThe first input is shared by {0} chorded bindings."),
                                FText::AsNumber(captured_chord_dependent_count_))
                : FText::GetEmpty()};
        auto const instruction{
            controls_device_type() == EHardwareDevicePrimaryType::Gamepad
                ? NSLOCTEXT("OptionsMenu",
                            "CaptureControllerChordPrompt",
                            "Hold a controller activator, then press the controller action input.")
                : NSLOCTEXT("OptionsMenu",
                            "CaptureKeyboardMouseChordPrompt",
                            "Hold a keyboard or mouse activator, then press the action input.")};
        return FText::Format(NSLOCTEXT("OptionsMenu", "CaptureChordPrompt", "{0}\n\n{1}{2}"),
                             instruction,
                             candidate,
                             shared_warning);
    })};
    return build_modal(MoveTemp(title), {confirm, clear, cancel});
}

auto SGameOptionsView::build_conflict_prompt() -> TSharedRef<SWidget> {
    auto replace{SAssignNew(conflict_replace_button_, SGameButton)
                     .Style(&style_->button(EGameButtonStyle::Primary))
                     .Audio(audio_)
                     .Text(NSLOCTEXT("OptionsMenu", "ReplaceBinding", "Replace"))
                     .OnClicked_Lambda([this] {
                         auto applied{false};
                         if (auto* const settings{settings_.Get()};
                             captured_binding_.IsSet() && settings != nullptr) {
                             if (captured_chord_.IsSet()) {
                                 applied =
                                     settings->set_control_chord(captured_binding_.GetValue(),
                                                                 chord_capture_.activator_key(),
                                                                 chord_capture_.action_key(),
                                                                 true);
                             } else {
                                 applied = settings->set_control_binding(
                                     captured_binding_.GetValue(), captured_key_, true);
                             }
                         }
                         if (!applied) {
                             capture_error_ =
                                 NSLOCTEXT("OptionsMenu",
                                           "BindingApplyFailed",
                                           "Could not apply the binding. Please try again.");
                             return FReply::Handled();
                         }
                         complete_binding_change();
                         return FReply::Handled();
                     })};
    auto cancel{SAssignNew(conflict_cancel_button_, SGameButton)
                    .Style(&style_->button(EGameButtonStyle::Secondary))
                    .Audio(audio_)
                    .Text(NSLOCTEXT("OptionsMenu", "CancelBinding", "Cancel"))
                    .OnClicked_Lambda([this] {
                        close_binding_prompt(true);
                        return FReply::Handled();
                    })};
    auto title{TAttribute<FText>::CreateLambda([this] {
        if (!capture_error_.IsEmpty()) {
            return capture_error_;
        }
        auto const captured_input{
            captured_chord_.IsSet() && chord_capture_.is_complete()
                ? FText::Format(NSLOCTEXT("OptionsMenu", "ChordConflictValue", "{0} + {1}"),
                                chord_capture_.activator_key().GetDisplayName(),
                                chord_capture_.action_key().GetDisplayName())
                : captured_key_.GetDisplayName()};

        TArray<FText> conflicting_controls;
        if (auto* const settings{settings_.Get()};
            captured_binding_.IsSet() && settings != nullptr) {
            auto const conflicts{
                captured_chord_.IsSet() && chord_capture_.is_complete()
                    ? settings->chord_binding_conflicts(captured_binding_.GetValue(),
                                                        chord_capture_.activator_key(),
                                                        chord_capture_.action_key())
                    : settings->binding_conflicts(captured_binding_.GetValue(), captured_key_)};
            conflicting_controls.Reserve(conflicts.Num());
            for (auto const& conflict : conflicts) {
                conflicting_controls.Add(conflict.display_name);
            }
        }

        auto const conflicting_control_text{
            conflicting_controls.IsEmpty()
                ? NSLOCTEXT("OptionsMenu", "UnknownBindingConflict", "another control")
                : FText::Join(NSLOCTEXT("OptionsMenu", "BindingConflictListSeparator", ", "),
                              conflicting_controls)};
        auto const replace_text{
            conflicting_controls.Num() == 1
                ? NSLOCTEXT("OptionsMenu", "BindingConflictReplaceSingle", "Replace that binding?")
                : NSLOCTEXT(
                      "OptionsMenu", "BindingConflictReplaceMultiple", "Replace those bindings?")};
        return FText::Format(NSLOCTEXT("OptionsMenu",
                                       "BindingConflictPrompt",
                                       "{0} is already assigned to:\n{1}\n\n{2}"),
                             captured_input,
                             conflicting_control_text,
                             replace_text);
    })};
    return build_modal(MoveTemp(title), {replace, cancel});
}

auto SGameOptionsView::build_modal(TAttribute<FText> title,
                                   TArray<TSharedRef<SGameButton>> const& buttons)
    -> TSharedRef<SWidget> {
    auto actions{SNew(SHorizontalBox)};
    auto const button_count{buttons.Num()};
    for (int32 index{}; index < button_count; ++index) {
        actions->AddSlot().AutoWidth().Padding(
            FMargin{index == 0 ? 0.0f : style_->settings().button_spacing, 0.0f})[buttons[index]];
    }

    return SNew(SBorder)
        .BorderImage(&style_->chrome().modal_overlay)
        .Padding(FMargin{48.0f})
        .HAlign(HAlign_Center)
        .VAlign(VAlign_Center)[SNew(SBox).WidthOverride(
            560.0f)[SNew(SHiveFrame)
                        .Style(&style_->chrome())
                            [SNew(SBorder)
                                 .BorderImage(&style_->chrome().body_background)
                                 .Padding(FMargin{28.0f})
                                     [SNew(SVerticalBox) +
                                      SVerticalBox::Slot().AutoHeight()
                                          [SNew(STextBlock)
                                               .Text(MoveTemp(title))
                                               .TextStyle(&style_->text(EGameTextStyle::Heading3))
                                               .AutoWrapText(true)] +
                                      SVerticalBox::Slot().AutoHeight().Padding(
                                          FMargin{0.0f, 24.0f, 0.0f, 0.0f})[actions]]]]];
}

/* **************************************** */
// Formatting and focus helpers
/* **************************************** */
auto SGameOptionsView::tab_text(EOptionsTab const tab) const -> FText {
    switch (tab) {
        case EOptionsTab::Video:
            return NSLOCTEXT("OptionsMenu", "VideoTab", "Video");
        case EOptionsTab::Gameplay:
            return NSLOCTEXT("OptionsMenu", "GameplayTab", "Gameplay");
        case EOptionsTab::Audio:
            return NSLOCTEXT("OptionsMenu", "AudioTab", "Audio");
        case EOptionsTab::Controls:
            return NSLOCTEXT("OptionsMenu", "ControlsTab", "Controls");
        case EOptionsTab::Accessibility:
            return NSLOCTEXT("OptionsMenu", "AccessibilityTab", "Accessibility");
        case EOptionsTab::System:
            return NSLOCTEXT("OptionsMenu", "SystemTab", "System");
    }
    return FText::GetEmpty();
}

auto SGameOptionsView::setting_float(EGameSetting const setting) const -> float {
    auto const* const settings{settings_.Get()};
    if (settings == nullptr) {
        return 0.0f;
    }
    auto const value{settings->value(setting)};
    auto const* const typed{std::get_if<float>(&value)};
    return typed != nullptr ? *typed : 0.0f;
}

auto SGameOptionsView::format_range_value(FGameSettingDescriptor const& descriptor) const -> FText {
    auto const value{setting_float(descriptor.id)};
    switch (descriptor.id) {
        case EGameSetting::MasterVolume:
        case EGameSetting::MusicVolume:
        case EGameSetting::SfxVolume:
        case EGameSetting::UIVolume:
            return FText::Format(NSLOCTEXT("OptionsMenu", "NormalizedPercent", "{0}%"),
                                 FText::AsNumber(FMath::RoundToInt32(value * 100.0f)));
        case EGameSetting::ResolutionScale:
            return FText::Format(NSLOCTEXT("OptionsMenu", "Percent", "{0}%"),
                                 FText::AsNumber(FMath::RoundToInt32(value)));
        default:
            return FText::AsNumber(value);
    }
}

auto SGameOptionsView::active_category() const -> TOptional<EGameSettingCategory> {
    switch (active_tab_) {
        case EOptionsTab::Video:
            return EGameSettingCategory::Video;
        case EOptionsTab::Gameplay:
            return EGameSettingCategory::Gameplay;
        case EOptionsTab::Audio:
            return EGameSettingCategory::Audio;
        case EOptionsTab::Controls:
            return EGameSettingCategory::Controls;
        case EOptionsTab::Accessibility:
            return EGameSettingCategory::Accessibility;
        case EOptionsTab::System:
            return {};
    }
    return {};
}

auto SGameOptionsView::controls_device_type() const -> EHardwareDevicePrimaryType {
    return controls_device_ == EGameSettingDevice::Controller
             ? EHardwareDevicePrimaryType::Gamepad
             : EHardwareDevicePrimaryType::KeyboardAndMouse;
}

auto SGameOptionsView::controls_focus_identity() const -> TOptional<FControlsFocusIdentity> {
    for (auto const& target : controls_focus_targets_) {
        if (target.has_focus && target.has_focus()) {
            return target.identity;
        }
    }
    return {};
}

auto SGameOptionsView::binding_focus_identity(FControlBindingView const& binding) const
    -> FControlsFocusIdentity {
    return {
        .kind = EControlsFocusKind::Binding,
        .device = binding.device_type == EHardwareDevicePrimaryType::Gamepad
                    ? EGameSettingDevice::Controller
                    : EGameSettingDevice::KeyboardMouse,
        .binding = control_binding_identity(binding.address, binding.device_type),
    };
}

void SGameOptionsView::register_controls_focus(FControlsFocusIdentity const identity,
                                               TSharedRef<SGameButton> const& button) {
    controls_focus_targets_.Add({
        .identity = identity,
        .focus = [button] { button->focus(); },
        .has_focus = [button] { return button->has_focus(); },
    });
}

void SGameOptionsView::restore_controls_focus(FControlsFocusIdentity const& identity) {
    auto* target{
        controls_focus_targets_.FindByPredicate([&identity](FControlsFocusTarget const& candidate) {
            return candidate.identity == identity;
        })};
    if (target != nullptr && target->focus) {
        target->focus();
        return;
    }

    if (identity.kind == EControlsFocusKind::Profile) {
        auto const index{static_cast<int32>(EOptionsTab::Controls)};
        if (page_focus_actions_.IsValidIndex(index) && page_focus_actions_[index]) {
            page_focus_actions_[index]();
            return;
        }
    }

    auto const selected_device{FControlsFocusIdentity{
        .kind = EControlsFocusKind::Device,
        .device = controls_device_,
    }};
    target = controls_focus_targets_.FindByPredicate(
        [&selected_device](FControlsFocusTarget const& candidate) {
            return candidate.identity == selected_device;
        });
    if (target != nullptr && target->focus) {
        target->focus();
        return;
    }
    focus_content();
}

void SGameOptionsView::remember_focus() {
    previous_focus_ = FSlateApplication::Get().GetKeyboardFocusedWidget();
}

void SGameOptionsView::restore_focus() {
    if (auto const previous{previous_focus_.Pin()}; previous.IsValid()) {
        FSlateApplication::Get().SetKeyboardFocus(previous, EFocusCause::SetDirectly);
    } else {
        focus_content();
    }
    previous_focus_.Reset();
}

} // namespace ml::ioj
