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
        case EGameSetting::AAQuality:
        case EGameSetting::ShadowQuality:
        case EGameSetting::TextureQuality:
        case EGameSetting::EffectsQuality:
        case EGameSetting::ReflectionsQuality:
        case EGameSetting::ShadingQuality:
            return NSLOCTEXT("OptionsMenu", "QualitySection", "Quality");
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
        case EGameSetting::GamepadTurnDeadZone:
        case EGameSetting::GamepadMoveDeadZone:
            return NSLOCTEXT("OptionsMenu", "DeadZoneSection", "Controller Dead Zones");
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

void SGameOptionsView::Construct(FArguments const& args) {
    settings_ = args._Settings;
    capabilities_ = args._Capabilities;
    style_ = args._Style;
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
    page_focus_actions_.SetNum(static_cast<int32>(EOptionsTab::System) + 1);

    auto header{build_header()};
    auto body{build_body()};
    auto footer{build_footer()};
    dirty_prompt_ = build_dirty_prompt();
    display_prompt_ = build_display_prompt();
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
    capture_prompt_->SetVisibility(EVisibility::Collapsed);
    conflict_prompt_->SetVisibility(EVisibility::Collapsed);
    ChildSlot[SNew(SOverlay) + SOverlay::Slot()[panel] +
              SOverlay::Slot()[dirty_prompt_.ToSharedRef()] +
              SOverlay::Slot()[display_prompt_.ToSharedRef()] +
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
        close_binding_prompt();
        return FReply::Handled();
    }
    if (captured_binding_.IsSet() && captured_chord_.IsSet() && captured_key_ == EKeys::Invalid) {
        return accept_chord_key(key, true);
    }
    if (captured_binding_.IsSet() && captured_key_ == EKeys::Invalid) {
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
    if (captured_binding_.IsSet() && captured_chord_.IsSet() && captured_key_ == EKeys::Invalid) {
        return accept_chord_key(mouse_event.GetEffectingButton(), true);
    }
    if (captured_binding_.IsSet() && captured_key_ == EKeys::Invalid) {
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
    if (captured_binding_.IsSet() && captured_chord_.IsSet() && captured_key_ == EKeys::Invalid &&
        FMath::Abs(analog_event.GetAnalogValue()) >= 0.5f) {
        return accept_chord_key(analog_event.GetKey(), false);
    }
    if (captured_binding_.IsSet() && captured_key_ == EKeys::Invalid &&
        FMath::Abs(analog_event.GetAnalogValue()) >= 0.5f) {
        return accept_binding_key(analog_event.GetKey());
    }
    return SCompoundWidget::OnAnalogValueChanged(geometry, analog_event);
}

auto SGameOptionsView::OnMouseWheel(FGeometry const& geometry, FPointerEvent const& mouse_event)
    -> FReply {
    if (captured_binding_.IsSet() && captured_chord_.IsSet() && captured_key_ == EKeys::Invalid &&
        !FMath::IsNearlyZero(mouse_event.GetWheelDelta())) {
        return accept_chord_key(mouse_event.GetWheelDelta() > 0.0f ? EKeys::MouseScrollUp
                                                                   : EKeys::MouseScrollDown,
                                false);
    }
    if (captured_binding_.IsSet() && captured_key_ == EKeys::Invalid &&
        !FMath::IsNearlyZero(mouse_event.GetWheelDelta())) {
        return accept_binding_key(mouse_event.GetWheelDelta() > 0.0f ? EKeys::MouseScrollUp
                                                                     : EKeys::MouseScrollDown);
    }
    return SCompoundWidget::OnMouseWheel(geometry, mouse_event);
}

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
                      .Text(NSLOCTEXT("OptionsMenu", "ResetCategory", "Reset Category"))
                      .OnClicked_Lambda([delegate = on_reset_]() {
                          delegate.ExecuteIfBound();
                          return FReply::Handled();
                      })] +
             SHorizontalBox::Slot()
                 .AutoWidth()[SAssignNew(apply_button_, SGameButton)
                                  .Style(&style_->button(EGameButtonStyle::Primary))
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
    rebuild_controls_page();
    return SNew(SScrollBox)
               .ScrollBarStyle(&style_->settings().scroll_bar)
               .Orientation(Orient_Vertical)
               .ScrollBarAlwaysVisible(false)
               .AnimateWheelScrolling(true) +
           SScrollBox::Slot()[controls_content_.ToSharedRef()];
}

void SGameOptionsView::rebuild_controls_page() {
    auto* const settings{settings_.Get()};
    if (!controls_content_.IsValid() || settings == nullptr) {
        return;
    }
    controls_content_->ClearChildren();

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
                     rebuild_controls_page();
                     refresh();
                 }
             })];
    auto profile_actions{SNew(SHorizontalBox)};
    profile_actions->AddSlot().AutoWidth().Padding(FMargin{
        0.0f,
        0.0f,
        style_->settings().button_spacing,
        0.0f})[SNew(SGameButton)
                   .Style(&style_->button(EGameButtonStyle::Secondary))
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
                           rebuild_controls_page();
                           refresh();
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
                                          rebuild_controls_page();
                                          refresh();
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
                                     RegisterActiveTimer(
                                         0.0f,
                                         FWidgetActiveTimerDelegate::CreateSP(
                                             this,
                                             &SGameOptionsView::handle_deferred_controls_rebuild));
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

    struct FBindingTableRow {
        FName mapping_name;
        FText display_name;
        FText display_category;
        TArray<FControlBindingView> keyboard_mouse;
        TArray<FControlBindingView> controller;
    };
    TArray<FBindingTableRow> binding_rows;
    for (auto const& binding :
         settings->control_bindings(EHardwareDevicePrimaryType::Unspecified)) {
        auto* row{binding_rows.FindByPredicate([&binding](auto const& candidate) {
            return candidate.mapping_name == binding.address.mapping_name;
        })};
        if (row == nullptr) {
            row = &binding_rows.Add_GetRef(FBindingTableRow{
                .mapping_name = binding.address.mapping_name,
                .display_name = binding.display_name,
                .display_category = binding.display_category,
            });
        }
        auto& device_bindings{binding.device_type == EHardwareDevicePrimaryType::Gamepad
                                  ? row->controller
                                  : row->keyboard_mouse};
        device_bindings.Add(binding);
    }

    FText binding_category;
    TSharedPtr<SVerticalBox> rows;
    auto flush_binding_category = [&] {
        if (rows.IsValid()) {
            add_section(binding_category, rows.ToSharedRef());
        }
    };
    for (auto const& row : binding_rows) {
        if (!rows.IsValid() || !row.display_category.EqualTo(binding_category)) {
            flush_binding_category();
            binding_category = row.display_category;
            rows = SNew(SVerticalBox);
            rows->AddSlot().AutoHeight().Padding(style_->settings().row_padding)
                [SNew(SHorizontalBox) +
                 SHorizontalBox::Slot().FillWidth(
                     0.4f)[SNew(STextBlock)
                               .Text(NSLOCTEXT("OptionsMenu", "ControlActionColumn", "Action"))
                               .TextStyle(&style_->text(EGameTextStyle::Caption))] +
                 SHorizontalBox::Slot().FillWidth(0.3f).Padding(
                     FMargin{style_->settings().button_spacing, 0.0f})
                     [SNew(STextBlock)
                          .Text(NSLOCTEXT("OptionsMenu", "KeyboardMouseColumn", "Keyboard & Mouse"))
                          .TextStyle(&style_->text(EGameTextStyle::Caption))] +
                 SHorizontalBox::Slot().FillWidth(0.3f).Padding(FMargin{
                     style_->settings().button_spacing,
                     0.0f})[SNew(STextBlock)
                                .Text(NSLOCTEXT("OptionsMenu", "ControllerColumn", "Controller"))
                                .TextStyle(&style_->text(EGameTextStyle::Caption))]];
        }
        rows->AddSlot().AutoHeight().Padding(style_->settings().row_padding)
            [SNew(SHorizontalBox) +
             SHorizontalBox::Slot().FillWidth(0.4f).VAlign(
                 VAlign_Center)[SNew(STextBlock)
                                    .Text(row.display_name)
                                    .TextStyle(&style_->text(EGameTextStyle::Body))] +
             SHorizontalBox::Slot().FillWidth(0.3f).Padding(FMargin{
                 style_->settings().button_spacing, 0.0f})[build_binding_cell(row.keyboard_mouse)] +
             SHorizontalBox::Slot().FillWidth(0.3f).Padding(FMargin{
                 style_->settings().button_spacing, 0.0f})[build_binding_cell(row.controller)]];
    }
    flush_binding_category();
    if (binding_rows.IsEmpty()) {
        auto empty_rows{SNew(SVerticalBox)};
        empty_rows->AddSlot()
            .AutoHeight()[SNew(STextBlock)
                              .Text(NSLOCTEXT(
                                  "OptionsMenu", "NoControlBindings", "No bindings available."))
                              .TextStyle(&style_->settings().empty_text)];
        add_section(NSLOCTEXT("OptionsMenu", "BindingsSection", "Bindings"), empty_rows);
    }

    FText response_section;
    TSharedPtr<SVerticalBox> response_rows;
    auto flush_response = [&] {
        if (response_rows.IsValid()) {
            add_section(response_section, response_rows.ToSharedRef());
        }
    };
    for (auto const* const descriptor : settings->descriptors(EGameSettingCategory::Controls)) {
        auto const next_section{section_label(*descriptor)};
        if (!response_rows.IsValid() || !next_section.EqualTo(response_section)) {
            flush_response();
            response_section = next_section;
            response_rows = SNew(SVerticalBox);
        }
        TFunction<void()> unused_focus;
        response_rows->AddSlot().AutoHeight().Padding(
            style_->settings().row_padding)[build_setting_row(*descriptor, unused_focus)];
    }
    flush_response();
}

auto SGameOptionsView::handle_deferred_controls_rebuild(double const current_time,
                                                        float const delta_time)
    -> EActiveTimerReturnType {
    static_cast<void>(current_time);
    static_cast<void>(delta_time);
    rebuild_controls_page();
    refresh();
    return EActiveTimerReturnType::Stop;
}

auto SGameOptionsView::build_binding_cell(TConstArrayView<FControlBindingView> const bindings)
    -> TSharedRef<SWidget> {
    auto result{SNew(SVerticalBox)};
    if (bindings.IsEmpty()) {
        result->AddSlot().AutoHeight().VAlign(VAlign_Center)
            [SNew(STextBlock).Text(INVTEXT("—")).TextStyle(&style_->settings().empty_text)];
        return result;
    }
    for (auto const& binding : bindings) {
        auto const component_key_text{binding.current_key.IsValid()
                                          ? binding.current_key.GetDisplayName()
                                          : NSLOCTEXT("OptionsMenu", "UnboundControl", "Unbound")};
        auto key_text{component_key_text};
        if (binding.chord.IsSet()) {
            auto const chord_key_text{binding.chord->current_key.IsValid()
                                          ? binding.chord->current_key.GetDisplayName()
                                          : NSLOCTEXT("OptionsMenu", "UnboundChord", "Unbound")};
            key_text = FText::Format(NSLOCTEXT("OptionsMenu", "ChordBindingFormat", "{0} + {1}"),
                                     chord_key_text,
                                     component_key_text);
        }
        result->AddSlot().AutoHeight().Padding(FMargin{0.0f, 2.0f})
            [SNew(SHorizontalBox) +
             SHorizontalBox::Slot().FillWidth(1.0f).Padding(
                 FMargin{0.0f, 0.0f, style_->settings().button_spacing, 0.0f})
                 [SNew(SGameButton)
                      .Style(&style_->button(EGameButtonStyle::Secondary))
                      .Text(key_text)
                      .ToolTipText(binding.chord.IsSet()
                                       ? NSLOCTEXT("OptionsMenu",
                                                   "ChordBindingTip",
                                                   "The chord activator is shown first. Click to "
                                                   "capture both inputs.")
                                       : FText::GetEmpty())
                      .OnClicked_Lambda([this, binding] {
                          if (binding.chord.IsSet()) {
                              begin_chord_capture(binding);
                          } else {
                              begin_binding_capture(binding.address);
                          }
                          return FReply::Handled();
                      })] +
             SHorizontalBox::Slot().AutoWidth().Padding(
                 FMargin{0.0f, 0.0f, style_->settings().button_spacing, 0.0f})
                 [SNew(SGameButton)
                      .Style(&style_->button(EGameButtonStyle::Secondary))
                      .Text(NSLOCTEXT("OptionsMenu", "ClearBinding", "Clear"))
                      .Enabled(binding.current_key.IsValid())
                      .OnClicked_Lambda([this, address = binding.address] {
                          if (auto* const current{settings_.Get()}) {
                              current->clear_control_binding(address);
                              rebuild_controls_page();
                              refresh();
                          }
                          return FReply::Handled();
                      })] +
             SHorizontalBox::Slot()
                 .AutoWidth()[SNew(SGameButton)
                                  .Style(&style_->button(EGameButtonStyle::Secondary))
                                  .Text(NSLOCTEXT("OptionsMenu", "ResetBinding", "Reset"))
                                  .Enabled(binding.modified)
                                  .Visibility(binding.custom_profile ? EVisibility::Collapsed
                                                                     : EVisibility::Visible)
                                  .OnClicked_Lambda([this, address = binding.address] {
                                      if (auto* const current{settings_.Get()}) {
                                          current->reset_control_binding(address);
                                          rebuild_controls_page();
                                          refresh();
                                      }
                                      return FReply::Handled();
                                  })]];
    }
    return result;
}

void SGameOptionsView::begin_binding_capture(FControlBindingAddress const& address) {
    remember_focus();
    captured_binding_ = address;
    captured_chord_.Reset();
    captured_key_ = EKeys::Invalid;
    capture_prompt_->SetVisibility(EVisibility::Visible);
    FSlateApplication::Get().SetKeyboardFocus(SharedThis(this), EFocusCause::SetDirectly);
}

void SGameOptionsView::begin_chord_capture(FControlBindingView const& binding) {
    if (!binding.chord.IsSet()) {
        begin_binding_capture(binding.address);
        return;
    }
    remember_focus();
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
        return FReply::Handled();
    }

    auto const conflicts{settings->binding_conflicts(captured_binding_.GetValue(), key)};
    capture_prompt_->SetVisibility(EVisibility::Collapsed);
    if (!conflicts.IsEmpty()) {
        captured_key_ = key;
        conflict_prompt_->SetVisibility(EVisibility::Visible);
        conflict_replace_button_->focus();
        return FReply::Handled();
    }
    settings->set_control_binding(captured_binding_.GetValue(), key, false);
    close_binding_prompt();
    rebuild_controls_page();
    refresh();
    return FReply::Handled();
}

auto SGameOptionsView::accept_chord_key(FKey const key, bool const can_be_held) -> FReply {
    auto* const settings{settings_.Get()};
    if (settings == nullptr || !captured_binding_.IsSet() || !captured_chord_.IsSet() ||
        !key.IsValid() || captured_key_.IsValid()) {
        return FReply::Handled();
    }
    auto const mappings{settings->control_bindings(EHardwareDevicePrimaryType::Unspecified)};
    auto const* const target{mappings.FindByPredicate([this](auto const& candidate) {
        return candidate.address == captured_binding_.GetValue();
    })};
    if (target == nullptr ||
        (target->device_type == EHardwareDevicePrimaryType::Gamepad) != key.IsGamepadKey()) {
        return FReply::Handled();
    }
    if (held_chord_keys_.Contains(key)) {
        return FReply::Handled();
    }
    if (held_chord_keys_.IsEmpty()) {
        if (can_be_held) {
            held_chord_keys_.Add(key);
        }
        return FReply::Handled();
    }

    captured_chord_activator_ = held_chord_keys_[0];
    captured_key_ = key;
    if (chord_confirm_button_.IsValid()) {
        chord_confirm_button_->focus();
    }
    return FReply::Handled();
}

auto SGameOptionsView::release_chord_key(FKey const key) -> FReply {
    if (captured_key_ == EKeys::Invalid) {
        held_chord_keys_.Remove(key);
    }
    return FReply::Handled();
}

void SGameOptionsView::clear_chord_capture() {
    held_chord_keys_.Reset();
    captured_chord_activator_ = EKeys::Invalid;
    captured_key_ = EKeys::Invalid;
}

auto SGameOptionsView::confirm_chord_capture() -> FReply {
    auto* const settings{settings_.Get()};
    if (settings == nullptr || !captured_binding_.IsSet() || !captured_chord_.IsSet() ||
        !captured_chord_activator_.IsValid() || !captured_key_.IsValid()) {
        return FReply::Handled();
    }
    auto const conflicts{settings->chord_binding_conflicts(
        captured_binding_.GetValue(), captured_chord_activator_, captured_key_)};
    capture_prompt_->SetVisibility(EVisibility::Collapsed);
    if (!conflicts.IsEmpty()) {
        conflict_prompt_->SetVisibility(EVisibility::Visible);
        conflict_replace_button_->focus();
        return FReply::Handled();
    }
    settings->set_control_chord(
        captured_binding_.GetValue(), captured_chord_activator_, captured_key_, false);
    close_binding_prompt();
    rebuild_controls_page();
    refresh();
    return FReply::Handled();
}

void SGameOptionsView::close_binding_prompt() {
    if (capture_prompt_.IsValid()) {
        capture_prompt_->SetVisibility(EVisibility::Collapsed);
    }
    if (conflict_prompt_.IsValid()) {
        conflict_prompt_->SetVisibility(EVisibility::Collapsed);
    }
    captured_binding_.Reset();
    captured_chord_.Reset();
    clear_chord_capture();
    captured_chord_dependent_count_ = 0;
    restore_focus();
}

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
            auto row{SAssignNew(control, SSettingsChoice)
                         .Style(&style_->settings())
                         .Label(descriptor.label)
                         .ToolTipText(descriptor.tooltip)
                         .Options(MoveTemp(labels))
                         .SelectedIndex(selected)
                         .ControlEnabled(available)
                         .OnSelectionChanged_Lambda(
                             [weak_settings, setting = descriptor.id, options](int32 const index) {
                                 if (auto* const settings{weak_settings.Get()};
                                     options.IsValidIndex(index)) {
                                     settings->set_setting(setting, options[index].value);
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
                         .Text(NSLOCTEXT("OptionsMenu", "PromptApply", "Apply"))
                         .OnClicked_Lambda([delegate = on_dirty_apply_]() {
                             delegate.ExecuteIfBound();
                             return FReply::Handled();
                         })};
    auto dirty_discard{SAssignNew(dirty_discard_button_, SGameButton)
                           .Style(&secondary)
                           .Text(NSLOCTEXT("OptionsMenu", "PromptDiscard", "Discard"))
                           .OnClicked_Lambda([delegate = on_dirty_discard_]() {
                               delegate.ExecuteIfBound();
                               return FReply::Handled();
                           })};
    auto dirty_stay{SAssignNew(dirty_stay_button_, SGameButton)
                        .Style(&secondary)
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
                     .Text(NSLOCTEXT("OptionsMenu", "KeepChanges", "Keep Changes"))
                     .OnClicked_Lambda([delegate = on_confirm_display_]() {
                         delegate.ExecuteIfBound();
                         return FReply::Handled();
                     })};
    auto revert{SAssignNew(revert_display_button_, SGameButton)
                    .Style(&secondary)
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

auto SGameOptionsView::build_capture_prompt() -> TSharedRef<SWidget> {
    auto confirm{SAssignNew(chord_confirm_button_, SGameButton)
                     .Style(&style_->button(EGameButtonStyle::Primary))
                     .Text(NSLOCTEXT("OptionsMenu", "ConfirmChordCapture", "Confirm"))
                     .Enabled_Lambda([this] {
                         return captured_chord_.IsSet() && captured_chord_activator_.IsValid() &&
                                captured_key_.IsValid();
                     })
                     .Visibility_Lambda([this] {
                         return captured_chord_.IsSet() ? EVisibility::Visible
                                                        : EVisibility::Collapsed;
                     })
                     .OnClicked_Lambda([this] { return confirm_chord_capture(); })};
    auto clear{SAssignNew(chord_clear_button_, SGameButton)
                   .Style(&style_->button(EGameButtonStyle::Secondary))
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
                    .Text(NSLOCTEXT("OptionsMenu", "CancelChordCapture", "Cancel"))
                    .Visibility_Lambda([this] {
                        return captured_chord_.IsSet() ? EVisibility::Visible
                                                       : EVisibility::Collapsed;
                    })
                    .OnClicked_Lambda([this] {
                        close_binding_prompt();
                        return FReply::Handled();
                    })};
    auto title{TAttribute<FText>::CreateLambda([this] {
        if (!captured_chord_.IsSet()) {
            return NSLOCTEXT("OptionsMenu",
                             "CaptureBindingPrompt",
                             "Press a keyboard, mouse, or controller button. Press Back to "
                             "cancel.");
        }

        FText candidate;
        if (captured_chord_activator_.IsValid() && captured_key_.IsValid()) {
            candidate =
                FText::Format(NSLOCTEXT("OptionsMenu", "CapturedChord", "Captured: {0} + {1}"),
                              captured_chord_activator_.GetDisplayName(),
                              captured_key_.GetDisplayName());
        } else if (!held_chord_keys_.IsEmpty()) {
            candidate = FText::Format(NSLOCTEXT("OptionsMenu", "PartialChord", "Held: {0} + …"),
                                      held_chord_keys_[0].GetDisplayName());
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
        return FText::Format(
            NSLOCTEXT("OptionsMenu",
                      "CaptureChordPrompt",
                      "Hold the activator, then press the action input.\n\n{0}{1}"),
            candidate,
            shared_warning);
    })};
    return build_modal(MoveTemp(title), {confirm, clear, cancel});
}

auto SGameOptionsView::build_conflict_prompt() -> TSharedRef<SWidget> {
    auto replace{SAssignNew(conflict_replace_button_, SGameButton)
                     .Style(&style_->button(EGameButtonStyle::Primary))
                     .Text(NSLOCTEXT("OptionsMenu", "ReplaceBinding", "Replace"))
                     .OnClicked_Lambda([this] {
                         if (auto* const settings{settings_.Get()};
                             captured_binding_.IsSet() && settings != nullptr) {
                             if (captured_chord_.IsSet()) {
                                 settings->set_control_chord(captured_binding_.GetValue(),
                                                             captured_chord_activator_,
                                                             captured_key_,
                                                             true);
                             } else {
                                 settings->set_control_binding(
                                     captured_binding_.GetValue(), captured_key_, true);
                             }
                         }
                         close_binding_prompt();
                         rebuild_controls_page();
                         refresh();
                         return FReply::Handled();
                     })};
    auto cancel{SAssignNew(conflict_cancel_button_, SGameButton)
                    .Style(&style_->button(EGameButtonStyle::Secondary))
                    .Text(NSLOCTEXT("OptionsMenu", "CancelBinding", "Cancel"))
                    .OnClicked_Lambda([this] {
                        close_binding_prompt();
                        return FReply::Handled();
                    })};
    auto title{TAttribute<FText>::CreateLambda([this] {
        auto const captured_input{
            captured_chord_.IsSet() && captured_chord_activator_.IsValid()
                ? FText::Format(NSLOCTEXT("OptionsMenu", "ChordConflictValue", "{0} + {1}"),
                                captured_chord_activator_.GetDisplayName(),
                                captured_key_.GetDisplayName())
                : captured_key_.GetDisplayName()};
        return FText::Format(NSLOCTEXT("OptionsMenu",
                                       "BindingConflictPrompt",
                                       "{0} is already assigned. Replace that binding?"),
                             captured_input);
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
