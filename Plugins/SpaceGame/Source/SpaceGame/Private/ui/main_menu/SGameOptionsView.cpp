#include "SGameOptionsView.h"

#include "SandboxUI/slate/SlateSlots.h"
#include "SandboxUI/widgets/SettingsWidgets.h"
#include "SpaceGame/settings/FlightModelEditor.h"
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
        if (managed_binding_.IsSet() && managed_binding_->modified) {
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
                          return NSLOCTEXT("OptionsMenu", "ResetAllControls", "Reset All Controls");
                      })
                      .ToolTipText_Lambda([this] {
                          if (active_tab_ != EOptionsTab::Controls) {
                              return FText::GetEmpty();
                          }
                          return NSLOCTEXT(
                              "OptionsMenu",
                              "ResetAllControlsTip",
                              "Reset response settings and bindings for both devices.");
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
    using TranslationAxis = ::ioj::sim::player::TranslationAxisConfig;
    using TranslationAxes = ::ioj::sim::player::TranslationAxesConfig;
    using RotationAxis = ::ioj::sim::player::RotationAxisConfig;
    using RotationAxes = ::ioj::sim::player::RotationAxesConfig;
    using TranslationSemantic = ::ioj::sim::player::TranslationSemantic;
    using RotationSemantic = ::ioj::sim::player::RotationSemantic;
    using ResponseMode = ::ioj::sim::player::ResponseMode;
    using ReferenceFrame = ::ioj::sim::player::ReferenceFrame;
    using FacingCoupling = ::ioj::sim::player::FacingVelocityCoupling;

    auto flight_rows{SNew(SVerticalBox)};
    auto const weak_settings{settings_};
    auto const field_label = [](FText const& prefix, FText const& field) {
        return FText::Format(
            NSLOCTEXT("OptionsMenu", "FlightModelFieldLabel", "{0} {1}"), prefix, field);
    };
    auto const edit_flight_model = [weak_settings](auto setter) {
        if (auto* const current{weak_settings.Get()}) {
            auto profile{current->flight_model_profile()};
            setter(profile.config);
            return current->set_flight_model_profile(MoveTemp(profile));
        }
        return false;
    };
    auto const add_flight_slider = [this, &flight_rows, weak_settings, edit_flight_model](
                                       FText const& label,
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
            style_->settings().row_padding)[SNew(SSettingsSlider)
                                                .Style(&style_->settings())
                                                .Label(label)
                                                .ToolTipText(tooltip)
                                                .Value(value)
                                                .ValueText(value_text)
                                                .Minimum(minimum)
                                                .Maximum(maximum)
                                                .Step(step)
                                                .OnValueChanged_Lambda(
                                                    [edit_flight_model, setter](float const next) {
                                                        edit_flight_model(
                                                            [setter, next](FlightConfig& config) {
                                                                setter(config, next);
                                                            });
                                                    })];
    };
    auto const add_flight_choice = [this, &flight_rows, weak_settings, edit_flight_model](
                                       FText const& label,
                                       FText const& tooltip,
                                       TArray<FText> options,
                                       auto getter,
                                       auto setter) {
        auto selected{TAttribute<int32>::CreateLambda([weak_settings, getter] {
            auto const* const current{weak_settings.Get()};
            return current != nullptr ? getter(current->flight_model_profile().config) : INDEX_NONE;
        })};
        auto const option_count{options.Num()};
        flight_rows->AddSlot().AutoHeight().Padding(style_->settings().row_padding)
            [SNew(SSettingsChoice)
                 .Style(&style_->settings())
                 .Label(label)
                 .ToolTipText(tooltip)
                 .Options(MoveTemp(options))
                 .SelectedIndex(selected)
                 .OnSelectionChanged_Lambda(
                     [this, edit_flight_model, setter, option_count](int32 const index) {
                         if (index >= 0 && index < option_count &&
                             edit_flight_model([setter, index](FlightConfig& config) {
                                 setter(config, index);
                             })) {
                             request_controls_rebuild({}, false);
                         }
                     })];
    };
    auto const add_flight_toggle = [this, &flight_rows, weak_settings, edit_flight_model](
                                       FText const& label,
                                       FText const& tooltip,
                                       auto getter,
                                       auto setter) {
        auto checked{TAttribute<bool>::CreateLambda([weak_settings, getter] {
            auto const* const current{weak_settings.Get()};
            return current != nullptr && getter(current->flight_model_profile().config);
        })};
        flight_rows->AddSlot().AutoHeight().Padding(
            style_->settings()
                .row_padding)[SNew(SSettingsToggle)
                                  .Style(&style_->settings())
                                  .Label(label)
                                  .ToolTipText(tooltip)
                                  .Checked(checked)
                                  .OnCheckStateChanged_Lambda([this, edit_flight_model, setter](
                                                                  ECheckBoxState const state) {
                                      if (edit_flight_model([setter, state](FlightConfig& config) {
                                              setter(config, state == ECheckBoxState::Checked);
                                          })) {
                                          request_controls_rebuild({}, false);
                                      }
                                  })];
    };
    auto const add_speed_limit =
        [&](FText const& label, FText const& tooltip, auto getter, auto setter) {
            auto const unlimited{getter(settings->flight_model_profile().config) ==
                                 ::ioj::sim::player::effectively_unlimited_speed};
            add_flight_toggle(
                field_label(label, NSLOCTEXT("OptionsMenu", "UnlimitedSuffix", "Unlimited")),
                NSLOCTEXT("OptionsMenu",
                          "UnlimitedFlightSpeedTip",
                          "Use the largest finite float as an effective cap without changing the "
                          "requested speed."),
                [getter](FlightConfig const& config) {
                    return getter(config) == ::ioj::sim::player::effectively_unlimited_speed;
                },
                [getter, setter](FlightConfig& config, bool const enabled) {
                    auto const current{getter(config)};
                    setter(config,
                           enabled ? ::ioj::sim::player::effectively_unlimited_speed
                           : current == ::ioj::sim::player::effectively_unlimited_speed ? 100000.f
                                                                                        : current);
                });
            if (!unlimited) {
                add_flight_slider(label, tooltip, 0.f, 1000000.f, 100.f, getter, setter);
            }
        };
    auto const add_response = [&](FText const& prefix,
                                  auto get_response,
                                  float const rate_maximum) {
        add_flight_choice(
            field_label(prefix, NSLOCTEXT("OptionsMenu", "ResponseModeField", "Response")),
            NSLOCTEXT("OptionsMenu",
                      "FlightResponseModeTip",
                      "Choose immediate, rate-limited, or damped second-order response."),
            {NSLOCTEXT("OptionsMenu", "DirectResponse", "Direct"),
             NSLOCTEXT("OptionsMenu", "RateLimitedResponse", "Rate Limited"),
             NSLOCTEXT("OptionsMenu", "SecondOrderResponse", "Second Order")},
            [get_response](FlightConfig const& config) {
                return static_cast<int32>(get_response(config).mode);
            },
            [get_response](FlightConfig& config, int32 const index) {
                get_response(config).mode = static_cast<ResponseMode>(index);
            });

        auto const mode{get_response(settings->flight_model_profile().config).mode};
        if (mode == ResponseMode::RateLimited) {
            add_flight_slider(
                field_label(prefix,
                            NSLOCTEXT("OptionsMenu", "ResponseIncreaseField", "Increase Rate")),
                NSLOCTEXT("OptionsMenu",
                          "FlightResponseIncreaseTip",
                          "Maximum response increase per second."),
                0.f,
                rate_maximum,
                rate_maximum > 100.f ? 100.f : 0.05f,
                [get_response](FlightConfig const& config) {
                    return get_response(config).rate_limited.increasing_rate;
                },
                [get_response](FlightConfig& config, float const value) {
                    get_response(config).rate_limited.increasing_rate = value;
                });
            add_flight_slider(
                field_label(prefix,
                            NSLOCTEXT("OptionsMenu", "ResponseDecreaseField", "Decrease Rate")),
                NSLOCTEXT("OptionsMenu",
                          "FlightResponseDecreaseTip",
                          "Maximum response decrease per second."),
                0.f,
                rate_maximum,
                rate_maximum > 100.f ? 100.f : 0.05f,
                [get_response](FlightConfig const& config) {
                    return get_response(config).rate_limited.decreasing_rate;
                },
                [get_response](FlightConfig& config, float const value) {
                    get_response(config).rate_limited.decreasing_rate = value;
                });
        } else if (mode == ResponseMode::SecondOrder) {
            add_flight_slider(
                field_label(prefix, NSLOCTEXT("OptionsMenu", "SettlingTimeField", "Settling Time")),
                NSLOCTEXT("OptionsMenu",
                          "FlightSettlingTimeTip",
                          "Second-order settling time in seconds."),
                0.05f,
                10.f,
                0.05f,
                [get_response](FlightConfig const& config) {
                    return get_response(config).second_order.settling_time;
                },
                [get_response](FlightConfig& config, float const value) {
                    get_response(config).second_order.settling_time = value;
                });
            add_flight_slider(
                field_label(prefix, NSLOCTEXT("OptionsMenu", "DampingRatioField", "Damping Ratio")),
                NSLOCTEXT("OptionsMenu", "FlightDampingRatioTip", "Second-order damping ratio."),
                0.01f,
                0.99f,
                0.01f,
                [get_response](FlightConfig const& config) {
                    return get_response(config).second_order.damping_ratio;
                },
                [get_response](FlightConfig& config, float const value) {
                    get_response(config).second_order.damping_ratio = value;
                });
        }
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

    add_section(NSLOCTEXT("OptionsMenu", "FlightModelStatusSection", "Flight Model"), flight_rows);

    auto const add_translation_axis = [&](FText const& axis_name,
                                          TranslationAxis TranslationAxes::* member) {
        auto const axis = [member](auto& config) -> auto& { return config.translation.*member; };
        auto const baseline{::ioj::sim::player::make_flight_model_profile(
            settings->flight_model_profile().base_preset)};
        auto const& authored_axis{axis(baseline.config)};
        if (authored_axis.manual.semantic == TranslationSemantic::Disabled &&
            authored_axis.automatic.semantic == TranslationSemantic::Disabled) {
            return;
        }
        flight_rows = SNew(SVerticalBox);
        auto const& current_axis{axis(settings->flight_model_profile().config)};
        auto const manual_prefix{
            field_label(axis_name, NSLOCTEXT("OptionsMenu", "ManualChannel", "Manual"))};
        auto const automatic_prefix{
            field_label(axis_name, NSLOCTEXT("OptionsMenu", "AutomaticChannel", "Automatic"))};

        auto const semantic_options =
            TArray<FText>{NSLOCTEXT("OptionsMenu", "TranslationDisabled", "Disabled"),
                          NSLOCTEXT("OptionsMenu", "TranslationTargetSpeed", "Target Speed"),
                          NSLOCTEXT("OptionsMenu", "TranslationTargetVelocity", "Target Velocity"),
                          NSLOCTEXT("OptionsMenu", "TranslationAcceleration", "Acceleration")};
        if (authored_axis.manual.semantic != TranslationSemantic::Disabled) {
            add_flight_choice(
                field_label(manual_prefix, NSLOCTEXT("OptionsMenu", "SemanticField", "Semantic")),
                NSLOCTEXT("OptionsMenu",
                          "ManualTranslationSemanticTip",
                          "How manual intent controls this axis."),
                semantic_options,
                [axis](FlightConfig const& config) {
                    return static_cast<int32>(axis(config).manual.semantic);
                },
                [axis](FlightConfig& config, int32 const index) {
                    apply_flight_model_translation_semantic_edit(
                        axis(config),
                        EFlightModelTranslationChannel::Manual,
                        static_cast<TranslationSemantic>(index));
                });
        }
        if (authored_axis.manual.semantic != TranslationSemantic::Disabled &&
            current_axis.manual.semantic != TranslationSemantic::Disabled) {
            add_flight_choice(
                field_label(manual_prefix,
                            NSLOCTEXT("OptionsMenu", "ReferenceFrameField", "Reference Frame")),
                NSLOCTEXT("OptionsMenu",
                          "TranslationFrameTip",
                          "Interpret movement in ship or world space."),
                {NSLOCTEXT("OptionsMenu", "ShipFrame", "Ship"),
                 NSLOCTEXT("OptionsMenu", "WorldFrame", "World")},
                [axis](FlightConfig const& config) {
                    return static_cast<int32>(axis(config).manual.reference_frame);
                },
                [axis](FlightConfig& config, int32 const index) {
                    axis(config).manual.reference_frame = static_cast<ReferenceFrame>(index);
                });
            add_response(
                manual_prefix,
                [axis](auto& config) -> auto& { return axis(config).manual.response; },
                100000.f);
        }

        if (authored_axis.automatic.semantic != TranslationSemantic::Disabled) {
            add_flight_choice(
                field_label(automatic_prefix,
                            NSLOCTEXT("OptionsMenu", "SemanticFieldAutomatic", "Semantic")),
                NSLOCTEXT("OptionsMenu",
                          "AutomaticTranslationSemanticTip",
                          "How automatic intent controls this axis."),
                semantic_options,
                [axis](FlightConfig const& config) {
                    return static_cast<int32>(axis(config).automatic.semantic);
                },
                [axis](FlightConfig& config, int32 const index) {
                    apply_flight_model_translation_semantic_edit(
                        axis(config),
                        EFlightModelTranslationChannel::Automatic,
                        static_cast<TranslationSemantic>(index));
                });
        }
        if (authored_axis.automatic.semantic != TranslationSemantic::Disabled &&
            current_axis.automatic.semantic != TranslationSemantic::Disabled) {
            add_flight_choice(
                field_label(
                    automatic_prefix,
                    NSLOCTEXT("OptionsMenu", "ReferenceFrameFieldAutomatic", "Reference Frame")),
                NSLOCTEXT("OptionsMenu",
                          "AutomaticTranslationFrameTip",
                          "Interpret automatic movement in ship or world space."),
                {NSLOCTEXT("OptionsMenu", "ShipFrameAutomatic", "Ship"),
                 NSLOCTEXT("OptionsMenu", "WorldFrameAutomatic", "World")},
                [axis](FlightConfig const& config) {
                    return static_cast<int32>(axis(config).automatic.reference_frame);
                },
                [axis](FlightConfig& config, int32 const index) {
                    axis(config).automatic.reference_frame = static_cast<ReferenceFrame>(index);
                });
            add_flight_slider(
                field_label(automatic_prefix,
                            NSLOCTEXT("OptionsMenu", "AutomaticValueField", "Value")),
                NSLOCTEXT("OptionsMenu",
                          "AutomaticTranslationValueTip",
                          "Normalized automatic intent from -1 to 1."),
                -1.f,
                1.f,
                0.05f,
                [axis](FlightConfig const& config) {
                    return axis(config).automatic.automatic_value;
                },
                [axis](FlightConfig& config, float const value) {
                    axis(config).automatic.automatic_value = value;
                });
            add_response(
                automatic_prefix,
                [axis](auto& config) -> auto& { return axis(config).automatic.response; },
                100000.f);
        }

        auto const manual_targets{
            current_axis.manual.semantic == TranslationSemantic::TargetSpeed ||
            current_axis.manual.semantic == TranslationSemantic::TargetVelocity};
        auto const automatic_targets{
            current_axis.automatic.semantic == TranslationSemantic::TargetSpeed ||
            current_axis.automatic.semantic == TranslationSemantic::TargetVelocity};
        auto const uses_targets{manual_targets || automatic_targets};
        auto const uses_acceleration{
            current_axis.manual.semantic == TranslationSemantic::Acceleration ||
            current_axis.automatic.semantic == TranslationSemantic::Acceleration};
        auto const active{current_axis.manual.semantic != TranslationSemantic::Disabled ||
                          current_axis.automatic.semantic != TranslationSemantic::Disabled};

        for (auto const boosted : {false, true}) {
            if (boosted && !settings->flight_model_profile().config.boost.available) {
                continue;
            }
            auto const drive_prefix{
                field_label(axis_name,
                            boosted ? NSLOCTEXT("OptionsMenu", "BoostedDrive", "Boosted")
                                    : NSLOCTEXT("OptionsMenu", "NormalDrive", "Normal"))};
            auto const drive = [axis, boosted](auto& config) -> auto& {
                return boosted ? axis(config).boosted : axis(config).normal;
            };
            if (uses_targets) {
                add_flight_slider(
                    field_label(drive_prefix,
                                NSLOCTEXT("OptionsMenu", "PositiveTargetSpeed", "+ Target Speed")),
                    NSLOCTEXT("OptionsMenu",
                              "PositiveTargetSpeedTip",
                              "Requested positive speed at full input."),
                    0.f,
                    100000.f,
                    100.f,
                    [drive](FlightConfig const& config) {
                        return drive(config).positive_target_speed;
                    },
                    [drive](FlightConfig& config, float const value) {
                        drive(config).positive_target_speed = value;
                    });
                add_flight_slider(
                    field_label(drive_prefix,
                                NSLOCTEXT("OptionsMenu", "NegativeTargetSpeed", "- Target Speed")),
                    NSLOCTEXT("OptionsMenu",
                              "NegativeTargetSpeedTip",
                              "Requested negative speed magnitude at full input."),
                    0.f,
                    100000.f,
                    100.f,
                    [drive](FlightConfig const& config) {
                        return drive(config).negative_target_speed;
                    },
                    [drive](FlightConfig& config, float const value) {
                        drive(config).negative_target_speed = value;
                    });
            }
            if (active) {
                add_speed_limit(
                    field_label(drive_prefix,
                                NSLOCTEXT("OptionsMenu", "PositiveSpeedLimit", "+ Speed Limit")),
                    NSLOCTEXT("OptionsMenu",
                              "PositiveSpeedLimitTip",
                              "Maximum permitted positive component speed."),
                    [drive](FlightConfig const& config) {
                        return drive(config).positive_speed_limit;
                    },
                    [drive](FlightConfig& config, float const value) {
                        drive(config).positive_speed_limit = value;
                    });
                add_speed_limit(
                    field_label(drive_prefix,
                                NSLOCTEXT("OptionsMenu", "NegativeSpeedLimit", "- Speed Limit")),
                    NSLOCTEXT("OptionsMenu",
                              "NegativeSpeedLimitTip",
                              "Maximum permitted negative component speed magnitude."),
                    [drive](FlightConfig const& config) {
                        return drive(config).negative_speed_limit;
                    },
                    [drive](FlightConfig& config, float const value) {
                        drive(config).negative_speed_limit = value;
                    });
            }
            if (uses_acceleration) {
                add_flight_slider(
                    field_label(drive_prefix,
                                NSLOCTEXT("OptionsMenu", "PositiveAcceleration", "+ Acceleration")),
                    NSLOCTEXT("OptionsMenu",
                              "PositiveAccelerationTip",
                              "Positive acceleration at full input."),
                    0.f,
                    100000.f,
                    100.f,
                    [drive](FlightConfig const& config) {
                        return drive(config).positive_acceleration;
                    },
                    [drive](FlightConfig& config, float const value) {
                        drive(config).positive_acceleration = value;
                    });
                add_flight_slider(
                    field_label(drive_prefix,
                                NSLOCTEXT("OptionsMenu", "NegativeAcceleration", "- Acceleration")),
                    NSLOCTEXT("OptionsMenu",
                              "NegativeAccelerationTip",
                              "Negative acceleration magnitude at full input."),
                    0.f,
                    100000.f,
                    100.f,
                    [drive](FlightConfig const& config) {
                        return drive(config).negative_acceleration;
                    },
                    [drive](FlightConfig& config, float const value) {
                        drive(config).negative_acceleration = value;
                    });
            }
        }
        add_flight_slider(
            field_label(axis_name, NSLOCTEXT("OptionsMenu", "PassiveDrag", "Passive Drag")),
            NSLOCTEXT("OptionsMenu", "PassiveDragTip", "Continuous passive component-speed loss."),
            0.f,
            100000.f,
            100.f,
            [axis](FlightConfig const& config) { return axis(config).passive_drag; },
            [axis](FlightConfig& config, float const value) { axis(config).passive_drag = value; });
        add_flight_choice(
            field_label(
                axis_name,
                NSLOCTEXT("OptionsMenu", "PassiveDragReferenceFrame", "Passive Drag Frame")),
            NSLOCTEXT("OptionsMenu",
                      "PassiveDragReferenceFrameTip",
                      "Apply passive component drag in ship or world space."),
            {NSLOCTEXT("OptionsMenu", "PassiveDragShipFrame", "Ship"),
             NSLOCTEXT("OptionsMenu", "PassiveDragWorldFrame", "World")},
            [axis](FlightConfig const& config) {
                return static_cast<int32>(axis(config).passive_drag_reference_frame);
            },
            [axis](FlightConfig& config, int32 const index) {
                axis(config).passive_drag_reference_frame = static_cast<ReferenceFrame>(index);
            });
        add_flight_slider(
            field_label(axis_name,
                        NSLOCTEXT("OptionsMenu", "ActiveStabilization", "Active Stabilization")),
            NSLOCTEXT("OptionsMenu",
                      "ActiveStabilizationTip",
                      "Counter-thrust toward zero while neither channel commands movement."),
            0.f,
            100000.f,
            100.f,
            [axis](FlightConfig const& config) { return axis(config).active_stabilization_rate; },
            [axis](FlightConfig& config, float const value) {
                axis(config).active_stabilization_rate = value;
            });
        add_flight_choice(
            field_label(axis_name,
                        NSLOCTEXT("OptionsMenu",
                                  "ActiveStabilizationReferenceFrame",
                                  "Active Stabilization Frame")),
            NSLOCTEXT("OptionsMenu",
                      "ActiveStabilizationReferenceFrameTip",
                      "Apply neutral counter-thrust in ship or world space."),
            {NSLOCTEXT("OptionsMenu", "ActiveStabilizationShipFrame", "Ship"),
             NSLOCTEXT("OptionsMenu", "ActiveStabilizationWorldFrame", "World")},
            [axis](FlightConfig const& config) {
                return static_cast<int32>(axis(config).active_stabilization_reference_frame);
            },
            [axis](FlightConfig& config, int32 const index) {
                axis(config).active_stabilization_reference_frame =
                    static_cast<ReferenceFrame>(index);
            });
        add_section(
            field_label(axis_name,
                        NSLOCTEXT("OptionsMenu", "TranslationSectionSuffix", "Translation")),
            flight_rows);
    };

    add_translation_axis(NSLOCTEXT("OptionsMenu", "ForwardAxis", "Forward"),
                         &TranslationAxes::forward);
    add_translation_axis(NSLOCTEXT("OptionsMenu", "RightAxis", "Right"), &TranslationAxes::right);
    add_translation_axis(NSLOCTEXT("OptionsMenu", "UpAxis", "Up"), &TranslationAxes::up);

    auto const add_rotation_axis = [&](FText const& axis_name,
                                       RotationAxis RotationAxes::* member) {
        auto const axis = [member](auto& config) -> auto& { return config.rotation.*member; };
        auto const baseline{::ioj::sim::player::make_flight_model_profile(
            settings->flight_model_profile().base_preset)};
        if (axis(baseline.config).manual_semantic == RotationSemantic::Disabled) {
            return;
        }
        flight_rows = SNew(SVerticalBox);
        auto const& current_axis{axis(settings->flight_model_profile().config)};
        add_flight_choice(
            field_label(axis_name, NSLOCTEXT("OptionsMenu", "RotationSemantic", "Semantic")),
            NSLOCTEXT("OptionsMenu", "RotationSemanticTip", "How manual input rotates this axis."),
            {NSLOCTEXT("OptionsMenu", "RotationDisabled", "Disabled"),
             NSLOCTEXT("OptionsMenu", "TargetAngularVelocity", "Target Angular Velocity"),
             NSLOCTEXT("OptionsMenu", "AngularAcceleration", "Angular Acceleration")},
            [axis](FlightConfig const& config) {
                return static_cast<int32>(axis(config).manual_semantic);
            },
            [axis](FlightConfig& config, int32 const index) {
                axis(config).manual_semantic = static_cast<RotationSemantic>(index);
            });
        if (current_axis.manual_semantic != RotationSemantic::Disabled) {
            add_flight_slider(
                field_label(axis_name, NSLOCTEXT("OptionsMenu", "MaximumRate", "Maximum Rate")),
                NSLOCTEXT("OptionsMenu",
                          "MaximumRotationRateTip",
                          "Maximum angular rate in degrees per second."),
                0.f,
                720.f,
                1.f,
                [axis](FlightConfig const& config) { return axis(config).maximum_rate; },
                [axis](FlightConfig& config, float const value) {
                    axis(config).maximum_rate = value;
                });
            if (current_axis.manual_semantic == RotationSemantic::AngularAcceleration) {
                add_flight_slider(
                    field_label(
                        axis_name,
                        NSLOCTEXT("OptionsMenu", "AngularAccelerationField", "Acceleration")),
                    NSLOCTEXT("OptionsMenu",
                              "AngularAccelerationTip",
                              "Angular acceleration at full input."),
                    0.f,
                    1440.f,
                    1.f,
                    [axis](FlightConfig const& config) { return axis(config).acceleration; },
                    [axis](FlightConfig& config, float const value) {
                        axis(config).acceleration = value;
                    });
            }
            add_response(
                axis_name, [axis](auto& config) -> auto& { return axis(config).response; }, 1440.f);
        }
        add_flight_toggle(
            field_label(axis_name,
                        NSLOCTEXT("OptionsMenu", "StabilizationEnabled", "Stabilization")),
            NSLOCTEXT("OptionsMenu",
                      "RotationStabilizationTip",
                      "Return this physical rotation axis to a target after input stops."),
            [axis](FlightConfig const& config) { return axis(config).stabilization.enabled; },
            [axis](FlightConfig& config, bool const value) {
                axis(config).stabilization.enabled = value;
            });
        if (current_axis.stabilization.enabled) {
            add_flight_slider(
                field_label(
                    axis_name,
                    NSLOCTEXT("OptionsMenu", "StabilizationTarget", "Stabilization Target")),
                NSLOCTEXT(
                    "OptionsMenu", "StabilizationTargetTip", "Target physical angle in degrees."),
                -180.f,
                180.f,
                1.f,
                [axis](FlightConfig const& config) {
                    return axis(config).stabilization.target_angle;
                },
                [axis](FlightConfig& config, float const value) {
                    axis(config).stabilization.target_angle = value;
                });
            add_flight_slider(
                field_label(axis_name,
                            NSLOCTEXT("OptionsMenu", "StabilizationDelay", "Stabilization Delay")),
                NSLOCTEXT("OptionsMenu",
                          "StabilizationDelayTip",
                          "Delay after rotation input before stabilization begins."),
                0.f,
                10.f,
                0.05f,
                [axis](FlightConfig const& config) { return axis(config).stabilization.delay; },
                [axis](FlightConfig& config, float const value) {
                    axis(config).stabilization.delay = value;
                });
            auto const stabilization_prefix{field_label(
                axis_name, NSLOCTEXT("OptionsMenu", "StabilizationResponse", "Stabilization"))};
            add_response(
                stabilization_prefix,
                [axis](auto& config) -> auto& { return axis(config).stabilization.response; },
                1440.f);
        }
        add_section(
            field_label(axis_name, NSLOCTEXT("OptionsMenu", "RotationSectionSuffix", "Rotation")),
            flight_rows);
    };

    add_rotation_axis(NSLOCTEXT("OptionsMenu", "PitchAxis", "Pitch"), &RotationAxes::pitch);
    add_rotation_axis(NSLOCTEXT("OptionsMenu", "YawAxis", "Yaw"), &RotationAxes::yaw);
    add_rotation_axis(NSLOCTEXT("OptionsMenu", "RollAxis", "Roll"), &RotationAxes::roll);

    flight_rows = SNew(SVerticalBox);
    auto const& current_config{settings->flight_model_profile().config};
    add_flight_choice(
        NSLOCTEXT("OptionsMenu", "FacingVelocityCoupling", "Facing / Velocity Coupling"),
        NSLOCTEXT("OptionsMenu",
                  "FacingVelocityCouplingTip",
                  "Choose independent velocity, gradual alignment, or locked facing."),
        {NSLOCTEXT("OptionsMenu", "IndependentCoupling", "Independent"),
         NSLOCTEXT("OptionsMenu", "AlignToFacingCoupling", "Align To Facing"),
         NSLOCTEXT("OptionsMenu", "LockedToFacingCoupling", "Locked To Facing")},
        [](FlightConfig const& config) { return static_cast<int32>(config.facing_velocity.mode); },
        [](FlightConfig& config, int32 const index) {
            config.facing_velocity.mode = static_cast<FacingCoupling>(index);
        });
    if (current_config.facing_velocity.mode == FacingCoupling::AlignToFacing) {
        add_flight_slider(
            NSLOCTEXT("OptionsMenu", "FacingAlignmentRate", "Alignment Rate"),
            NSLOCTEXT("OptionsMenu",
                      "FacingAlignmentRateTip",
                      "Maximum world-velocity alignment change per second."),
            0.f,
            100000.f,
            100.f,
            [](FlightConfig const& config) { return config.facing_velocity.alignment_rate; },
            [](FlightConfig& config, float const value) {
                config.facing_velocity.alignment_rate = value;
            });
        add_response(
            NSLOCTEXT("OptionsMenu", "FacingAlignment", "Alignment"),
            [](auto& config) -> auto& { return config.facing_velocity.response; },
            100000.f);
    }
    add_section(NSLOCTEXT("OptionsMenu", "FacingVelocitySection", "Facing and Velocity"),
                flight_rows);

    flight_rows = SNew(SVerticalBox);
    add_flight_toggle(
        NSLOCTEXT("OptionsMenu", "BoostAvailable", "Boost Available"),
        NSLOCTEXT("OptionsMenu",
                  "BoostAvailableTip",
                  "Allow boost intent to select boosted drive settings."),
        [](FlightConfig const& config) { return config.boost.available; },
        [](FlightConfig& config, bool const value) { config.boost.available = value; });
    if (current_config.boost.available) {
        add_flight_toggle(
            NSLOCTEXT("OptionsMenu", "AcceleratorActivatesBoost", "Accelerator Activates Boost"),
            NSLOCTEXT("OptionsMenu",
                      "AcceleratorActivatesBoostTip",
                      "Treat non-zero accelerator input as held boost intent for this model."),
            [](FlightConfig const& config) { return config.boost.accelerator_activates_boost; },
            [](FlightConfig& config, bool const value) {
                config.boost.accelerator_activates_boost = value;
            });
        add_flight_slider(
            NSLOCTEXT("OptionsMenu", "BoostEnergyDrain", "Boost Energy Drain"),
            NSLOCTEXT("OptionsMenu",
                      "BoostEnergyDrainTip",
                      "Energy fraction drained per second while boost is effective."),
            0.f,
            10.f,
            0.01f,
            [](FlightConfig const& config) { return config.boost.energy_drain_per_second; },
            [](FlightConfig& config, float const value) {
                config.boost.energy_drain_per_second = value;
            });
        add_response(
            NSLOCTEXT("OptionsMenu", "BoostResponsePrefix", "Boost"),
            [](auto& config) -> auto& { return config.boost.response; },
            100000.f);
    }
    auto const add_brake = [&](FText const& prefix, auto get_brake) {
        add_flight_toggle(
            field_label(prefix, NSLOCTEXT("OptionsMenu", "BrakeAvailable", "Available")),
            NSLOCTEXT("OptionsMenu", "BrakeAvailableTip", "Allow this braking action."),
            [get_brake](FlightConfig const& config) { return get_brake(config).available; },
            [get_brake](FlightConfig& config, bool const value) {
                get_brake(config).available = value;
            });
        if (!get_brake(settings->flight_model_profile().config).available) {
            return;
        }
        add_flight_slider(
            field_label(prefix, NSLOCTEXT("OptionsMenu", "BrakeTargetSpeed", "Target Speed")),
            NSLOCTEXT("OptionsMenu",
                      "BrakeTargetSpeedTip",
                      "World-speed floor approached by this brake."),
            0.f,
            100000.f,
            100.f,
            [get_brake](FlightConfig const& config) { return get_brake(config).target_speed; },
            [get_brake](FlightConfig& config, float const value) {
                get_brake(config).target_speed = value;
            });
        add_flight_slider(
            field_label(prefix, NSLOCTEXT("OptionsMenu", "BrakeDeceleration", "Deceleration")),
            NSLOCTEXT("OptionsMenu",
                      "BrakeDecelerationTip",
                      "Maximum physical world-speed reduction per second at full engagement."),
            0.f,
            100000.f,
            100.f,
            [get_brake](FlightConfig const& config) { return get_brake(config).deceleration; },
            [get_brake](FlightConfig& config, float const value) {
                get_brake(config).deceleration = value;
            });
        add_flight_slider(
            field_label(prefix, NSLOCTEXT("OptionsMenu", "BrakeEnergyDrain", "Energy Drain")),
            NSLOCTEXT("OptionsMenu",
                      "BrakeEnergyDrainTip",
                      "Energy fraction drained per second while this brake is effective."),
            0.f,
            10.f,
            0.01f,
            [get_brake](FlightConfig const& config) {
                return get_brake(config).energy_drain_per_second;
            },
            [get_brake](FlightConfig& config, float const value) {
                get_brake(config).energy_drain_per_second = value;
            });
        add_response(
            prefix,
            [get_brake](auto& config) -> auto& { return get_brake(config).response; },
            20.f);
    };
    add_brake(NSLOCTEXT("OptionsMenu", "BrakePrefix", "Brake"),
              [](auto& config) -> auto& { return config.brake; });
    add_brake(NSLOCTEXT("OptionsMenu", "EmergencyBrakePrefix", "Emergency Brake"),
              [](auto& config) -> auto& { return config.emergency_brake; });
    add_flight_slider(
        NSLOCTEXT("OptionsMenu", "EnergyRecharge", "Energy Recharge"),
        NSLOCTEXT("OptionsMenu",
                  "EnergyRechargeTip",
                  "Energy fraction restored per second when no draining action is effective."),
        0.f,
        10.f,
        0.01f,
        [](FlightConfig const& config) { return config.energy_recharge_per_second; },
        [](FlightConfig& config, float const value) { config.energy_recharge_per_second = value; });
    add_speed_limit(
        NSLOCTEXT("OptionsMenu", "NormalResultantLimit", "Normal Resultant Speed Limit"),
        NSLOCTEXT("OptionsMenu",
                  "NormalResultantLimitTip",
                  "Maximum total world-space speed outside boost."),
        [](FlightConfig const& config) { return config.maximum_resultant_speed; },
        [](FlightConfig& config, float const value) { config.maximum_resultant_speed = value; });
    if (current_config.boost.available) {
        add_speed_limit(
            NSLOCTEXT("OptionsMenu", "BoostedResultantLimit", "Boosted Resultant Speed Limit"),
            NSLOCTEXT("OptionsMenu",
                      "BoostedResultantLimitTip",
                      "Maximum total world-space speed while boost is effective."),
            [](FlightConfig const& config) { return config.boosted_maximum_resultant_speed; },
            [](FlightConfig& config, float const value) {
                config.boosted_maximum_resultant_speed = value;
            });
    }
    add_section(NSLOCTEXT("OptionsMenu", "FlightActionsSection", "Flight Actions and Limits"),
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
                          .scope = controls_scope_,
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
                               .scope = controls_scope_,
                           });
                           return FReply::Handled();
                       })]];
    add_section(NSLOCTEXT("OptionsMenu", "InputDeviceSection", "Input Device"), device_rows);
    register_controls_focus(
        FControlsFocusIdentity{
            .kind = EControlsFocusKind::Device,
            .device = EGameSettingDevice::KeyboardMouse,
            .scope = controls_scope_,
        },
        keyboard_mouse_button.ToSharedRef());
    register_controls_focus(
        FControlsFocusIdentity{
            .kind = EControlsFocusKind::Device,
            .device = EGameSettingDevice::Controller,
            .scope = controls_scope_,
        },
        controller_button.ToSharedRef());
    page_focus_actions_[static_cast<int32>(EOptionsTab::Controls)] = [keyboard_mouse_button] {
        keyboard_mouse_button->focus();
    };

    auto scope_rows{SNew(SVerticalBox)};
    auto scope_buttons{SNew(SHorizontalBox)};
    auto const scopes{TArray<EShipControlScope>{EShipControlScope::General,
                                                EShipControlScope::Starfox,
                                                EShipControlScope::Fighter,
                                                EShipControlScope::Skater,
                                                EShipControlScope::Gunship}};
    for (auto const scope : scopes) {
        auto const label{[scope] {
            switch (scope) {
                case EShipControlScope::General:
                    return FText::FromString(TEXT("General"));
                case EShipControlScope::Starfox:
                    return FText::FromString(TEXT("Starfox"));
                case EShipControlScope::Fighter:
                    return FText::FromString(TEXT("Fighter"));
                case EShipControlScope::Skater:
                    return FText::FromString(TEXT("Skater"));
                case EShipControlScope::Gunship:
                    return FText::FromString(TEXT("Gunship"));
            }
            return FText::GetEmpty();
        }()};
        TSharedPtr<SGameButton> button;
        scope_buttons->AddSlot().FillWidth(1.0f).Padding(
            FMargin{0.0f,
                    0.0f,
                    style_->settings().button_spacing,
                    0.0f})[SAssignNew(button, SGameButton)
                               .Style(&style_->button(EGameButtonStyle::Secondary))
                               .Audio(audio_)
                               .Selected(controls_scope_ == scope)
                               .Text(label)
                               .OnClicked_Lambda([this, scope] {
                                   controls_scope_ = scope;
                                   request_controls_rebuild(FControlsFocusIdentity{
                                       .kind = EControlsFocusKind::Scope,
                                       .device = controls_device_,
                                       .scope = scope,
                                   });
                                   return FReply::Handled();
                               })];
        register_controls_focus(
            FControlsFocusIdentity{
                .kind = EControlsFocusKind::Scope,
                .device = controls_device_,
                .scope = scope,
            },
            button.ToSharedRef());
    }
    scope_rows->AddSlot().AutoHeight()[scope_buttons];
    add_section(NSLOCTEXT("OptionsMenu", "ControlScopeSection", "Flight Mode"), scope_rows);

    add_setting_sections(controls_device_);

    auto const bindings{settings->control_bindings(controls_device_type(), controls_scope_)};
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
        .scope = binding.scope,
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

    auto const selected_device{FControlsFocusIdentity{
        .kind = EControlsFocusKind::Device,
        .device = controls_device_,
        .scope = controls_scope_,
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
