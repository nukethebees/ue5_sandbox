#include "SGameOptionsView.h"

#include "input/BindingCapture.h"
#include "input/BindingKey.h"
#include "SpaceGame/settings/GameVideoSettings.h"
#include "SpaceGame/settings/OptionsDraft.h"
#include "SpaceGame/ui/common/HiveWidgets.h"
#include "SpaceGame/ui/common/SGameButton.h"
#include <ioj/input/flight_controls.h>
#include <ioj/system_capabilities.h>

#include "SandboxUI/slate/SlateSlots.h"
#include "SandboxUI/widgets/SettingsWidgets.h"
#include <SandboxCoreEngine/strings.h>

#include "Framework/Application/SlateApplication.h"
#include "InputCoreTypes.h"
#include "Misc/StringBuilder.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SEditableText.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#include <type_traits>

namespace ioj {
namespace {
struct FSimdFeatureDisplay {
    bool supported{};
    TCHAR const* name{};
};

auto category_label(GameSettingCategory const category) -> FText {
    for (auto const& descriptor : game_setting_category_descriptors()) {
        if (descriptor.id == category) {
            return FText::FromString(UTF8_TO_TCHAR(descriptor.label.data()));
        }
    }
    return FText::GetEmpty();
}

auto section_label(GameSettingDescriptor const& descriptor) -> FText {
    switch (descriptor.id) {
        case GameSetting::Resolution:
        case GameSetting::WindowMode:
        case GameSetting::VSync:
        case GameSetting::FrameRateLimit:
        case GameSetting::ResolutionScale:
            return NSLOCTEXT("OptionsMenu", "DisplaySection", "Display");
        case GameSetting::AAMethod:
        case GameSetting::OverallQuality:
        case GameSetting::ViewDistanceQuality:
        case GameSetting::AAQuality:
        case GameSetting::ShadowQuality:
        case GameSetting::GlobalIlluminationQuality:
        case GameSetting::ReflectionsQuality:
        case GameSetting::PostProcessingQuality:
        case GameSetting::TextureQuality:
        case GameSetting::EffectsQuality:
        case GameSetting::ShadingQuality:
            return NSLOCTEXT("OptionsMenu", "QualitySection", "Quality");
        case GameSetting::Bloom:
        case GameSetting::MotionBlur:
            return NSLOCTEXT(
                "OptionsMenu", "PostProcessingEffectsSection", "Post Processing Effects");
        case GameSetting::MasterVolume:
        case GameSetting::MusicVolume:
        case GameSetting::SfxVolume:
        case GameSetting::UIVolume:
            return NSLOCTEXT("OptionsMenu", "VolumeSection", "Volume");
        case GameSetting::MouseTurnSensitivity:
        case GameSetting::GamepadTurnSensitivity:
        case GameSetting::InvertMousePitch:
        case GameSetting::InvertMouseYaw:
        case GameSetting::InvertGamepadPitch:
        case GameSetting::InvertGamepadYaw:
        case GameSetting::GamepadTurnDeadZone:
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
auto large_page_access_text(LargePageAccess const status) -> FText {
    switch (status) {
        case LargePageAccess::Unsupported:
            return NSLOCTEXT("OptionsMenu", "LargePagesUnsupported", "Unsupported");
        case LargePageAccess::Enabled:
            return NSLOCTEXT("OptionsMenu", "LargePagesEnabled", "Enabled");
        case LargePageAccess::PrivilegeUnavailable:
            return NSLOCTEXT(
                "OptionsMenu", "LargePagesPrivilegeUnavailable", "Privilege unavailable");
        case LargePageAccess::QueryFailed:
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
    page_ = args._Page;
    check(style_ != nullptr);
    on_apply_ = args._OnApply;
    on_reset_ = args._OnReset;
    on_dirty_apply_ = args._OnDirtyApply;
    on_dirty_discard_ = args._OnDirtyDiscard;
    on_dirty_stay_ = args._OnDirtyStay;
    on_confirm_display_ = args._OnConfirmDisplay;
    on_revert_display_ = args._OnRevertDisplay;
    on_interaction_modal_changed_ = args._OnInteractionModalChanged;

    auto header{build_header()};
    auto body{SAssignNew(page_content_, SBox)[build_body()]};
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

void SGameOptionsView::show_page() {
    focus_page_ = {};
    controls_content_.Reset();
    controls_focus_targets_.Reset();
    page_content_->SetContent(build_body());
    refresh();
}

void SGameOptionsView::refresh() {
    auto* const settings{settings_.Get()};
    if (settings == nullptr || style_ == nullptr) {
        return;
    }

    auto const category{settings_category(page_.Get())};
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
    if (focus_page_) {
        focus_page_();
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
    auto buttons{TArray<TSharedPtr<SGameButton>>{}};
    if (dirty_prompt_visible_) {
        buttons = {dirty_apply_button_, dirty_discard_button_, dirty_stay_button_};
    } else if (display_prompt_visible_) {
        buttons = {confirm_display_button_, revert_display_button_};
    } else if (captured_binding_.IsSet()) {
        buttons = {conflict_replace_button_, conflict_cancel_button_};
    } else if (binding_management_visible_) {
        buttons = {binding_change_button_};
        if (managed_binding_.IsSet() && managed_binding_->current_key != PhysicalKey::Unbound) {
            buttons.Add(binding_clear_button_);
        }
        if (managed_binding_.IsSet() && managed_binding_->is_modified()) {
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

/* **************************************** */
// Layout construction
/* **************************************** */
auto SGameOptionsView::build_header() -> TSharedRef<SWidget> {
    return SNew(SVerticalBox) +
           SVerticalBox::Slot()
               .AutoHeight()[SNew(STextBlock)
                                 .Text(NSLOCTEXT("OptionsMenu", "Section", "SYSTEM CONFIGURATION"))
                                 .TextStyle(&style_->text(EGameTextStyle::Caption))] +
           SVerticalBox::Slot().AutoHeight().Padding(FMargin{
               0.0f, 4.0f})[SNew(STextBlock)
                                .Text_Lambda([this] {
                                    auto category{settings_category(page_.Get())};
                                    return category
                                             ? category_label(*category)
                                             : NSLOCTEXT("OptionsMenu", "SystemTab", "System");
                                })
                                .TextStyle(&style_->text(EGameTextStyle::Heading1))];
}

auto SGameOptionsView::build_body() -> TSharedRef<SWidget> {
    if (page_.Get() == EMenuPage::Controls) {
        return build_controls_page();
    }
    if (auto category{settings_category(page_.Get())}) {
        return build_category_page(*category);
    }
    return page_.Get() == EMenuPage::System ? build_system_page() : SNullWidget::NullWidget;
}

auto SGameOptionsView::build_footer() -> TSharedRef<SWidget> {
    auto const spacing{style_->settings().button_spacing};
    return SNew(SBorder)
        .Visibility_Lambda([this] {
            return settings_category(page_.Get()) ? EVisibility::Visible : EVisibility::Collapsed;
        })
        .BorderImage(&style_->chrome().footer_background)
        .Padding(style_->chrome().footer_padding)
            [SNew(SHorizontalBox) + SHorizontalBox::Slot().FillWidth(1.0f) +
             SHorizontalBox::Slot().AutoWidth().Padding(FMargin{0.0f, 0.0f, spacing, 0.0f})
                 [SAssignNew(reset_button_, SGameButton)
                      .Style(&style_->button(EGameButtonStyle::Secondary))
                      .Audio(audio_)
                      .Text_Lambda([this] {
                          if (page_.Get() != EMenuPage::Controls) {
                              return NSLOCTEXT("OptionsMenu", "ResetCategory", "Reset Category");
                          }
                          return NSLOCTEXT("OptionsMenu", "ResetAllControls", "Reset All Controls");
                      })
                      .ToolTipText_Lambda([this] {
                          if (page_.Get() != EMenuPage::Controls) {
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
                                  .Text(NSLOCTEXT("OptionsMenu", "Save", "Save"))
                                  .OnClicked_Lambda([delegate = on_apply_]() {
                                      delegate.ExecuteIfBound();
                                      return FReply::Handled();
                                  })]];
}

auto SGameOptionsView::build_category_page(GameSettingCategory const category)
    -> TSharedRef<SWidget> {
    auto content{SNew(SVerticalBox)};
    auto* const settings{settings_.Get()};
    auto const descriptors{settings != nullptr ? settings->descriptors(category)
                                               : TArray<GameSettingDescriptor const*>{}};
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
            if (!focus_page_ && focus_action) {
                focus_page_ = MoveTemp(focus_action);
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
                                          GameSettingDevice const device) {
        FText current_section;
        TSharedPtr<SVerticalBox> rows;
        auto const flush = [&] {
            if (rows.IsValid()) {
                add_section(current_section, rows.ToSharedRef());
            }
        };

        for (auto const* const descriptor : settings->descriptors(GameSettingCategory::Controls)) {
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
                  .Selected(controls_device_ == GameSettingDevice::KeyboardMouse)
                  .Text(NSLOCTEXT("OptionsMenu", "KeyboardMouseDevice", "Keyboard & Mouse"))
                  .OnClicked_Lambda([this] {
                      controls_device_ = GameSettingDevice::KeyboardMouse;
                      request_controls_rebuild(
                          FControlsFocusIdentity{GameSettingDevice::KeyboardMouse});
                      return FReply::Handled();
                  })] +
         SHorizontalBox::Slot().FillWidth(
             1.0f)[SAssignNew(controller_button, SGameButton)
                       .Style(&style_->button(EGameButtonStyle::Secondary))
                       .Audio(audio_)
                       .Selected(controls_device_ == GameSettingDevice::Controller)
                       .Text(NSLOCTEXT("OptionsMenu", "ControllerDevice", "Controller"))
                       .OnClicked_Lambda([this] {
                           controls_device_ = GameSettingDevice::Controller;
                           request_controls_rebuild(
                               FControlsFocusIdentity{GameSettingDevice::Controller});
                           return FReply::Handled();
                       })]];
    add_section(NSLOCTEXT("OptionsMenu", "InputDeviceSection", "Input Device"), device_rows);
    register_controls_focus(FControlsFocusIdentity{GameSettingDevice::KeyboardMouse},
                            keyboard_mouse_button.ToSharedRef());
    register_controls_focus(FControlsFocusIdentity{GameSettingDevice::Controller},
                            controller_button.ToSharedRef());
    focus_page_ = [keyboard_mouse_button] { keyboard_mouse_button->focus(); };

    add_setting_sections(GameSettingDevice::Shared);
    add_setting_sections(controls_device_);

    auto const bindings{settings->control_bindings(controls_device_)};
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
    TSharedPtr<SGameButton> button;
    auto result{SAssignNew(button, SGameButton)
                    .Style(&style_->button(EGameButtonStyle::Secondary))
                    .Audio(audio_)
                    .Text_Lambda([key = binding.current_key] { return binding_label(key); })
                    .OnClicked_Lambda([this, binding] {
                        open_binding_management(binding);
                        return FReply::Handled();
                    })};
    register_controls_focus(binding.mapping, button.ToSharedRef());
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
            ? TOptional<FControlsFocusIdentity>{FControlsFocusIdentity{managed_binding_->mapping}}
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
    captured_binding_ = binding.mapping;
    auto capture{MakeShared<FBindingCapture>()};
    capture->on_key.BindSP(this, &SGameOptionsView::handle_capture_key);
    AddMetadata(capture);
    captured_key_ = PhysicalKey::Unbound;
    capture_error_ = FText::GetEmpty();
    capture_prompt_->SetVisibility(EVisibility::Visible);
    FSlateApplication::Get().SetKeyboardFocus(SharedThis(this), EFocusCause::SetDirectly);
}

void SGameOptionsView::handle_capture_key(PhysicalKey key) {
    if (key == PhysicalKey::Escape || key == PhysicalKey::GamepadFaceButtonRight) {
        close_binding_prompt(true);
        return;
    }

    accept_binding_key(key);
}

void SGameOptionsView::accept_binding_key(PhysicalKey key) {
    auto* const settings{settings_.Get()};
    if (!settings || !captured_binding_) {
        return;
    }

    if (key != PhysicalKey::Unbound && !accepts_binding(*captured_binding_, key)) {
        capture_error_ = NSLOCTEXT(
            "OptionsMenu", "UnsupportedBinding", "Choose a supported input for this control.");
        return;
    }
    capture_error_ = FText::GetEmpty();
    auto const conflicts{settings->binding_conflicts(captured_binding_.GetValue(), key)};
    capture_prompt_->SetVisibility(EVisibility::Collapsed);
    if (!conflicts.IsEmpty()) {
        captured_key_ = key;
        conflict_prompt_->SetVisibility(EVisibility::Visible);
        conflict_replace_button_->focus();
        return;
    }
    if (!settings->set_control_binding(captured_binding_.GetValue(), key, false)) {
        capture_error_ = NSLOCTEXT(
            "OptionsMenu", "BindingApplyFailed", "Could not apply the binding. Please try again.");
        capture_prompt_->SetVisibility(EVisibility::Visible);
        return;
    }
    complete_binding_change();
}

void SGameOptionsView::close_binding_prompt(bool const restore_focus) {
    auto const was_open{captured_binding_.IsSet()};
    auto const identity{
        managed_binding_.IsSet()
            ? TOptional<FControlsFocusIdentity>{FControlsFocusIdentity{managed_binding_->mapping}}
            : TOptional<FControlsFocusIdentity>{}};
    if (capture_prompt_.IsValid()) {
        capture_prompt_->SetVisibility(EVisibility::Collapsed);
    }
    if (conflict_prompt_.IsValid()) {
        conflict_prompt_->SetVisibility(EVisibility::Collapsed);
    }
    if (auto capture{GetMetaData<FBindingCapture>()}) {
        RemoveMetaData(capture.ToSharedRef());
    }
    captured_binding_.Reset();
    captured_key_ = PhysicalKey::Unbound;
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
            ? TOptional<FControlsFocusIdentity>{FControlsFocusIdentity{managed_binding_->mapping}}
            : TOptional<FControlsFocusIdentity>{}};
    close_binding_prompt(false);
    request_controls_rebuild(identity);
}

/* **************************************** */
// Settings and prompts
/* **************************************** */
auto SGameOptionsView::build_setting_row(GameSettingDescriptor const& descriptor,
                                         TFunction<void()>& focus_action) -> TSharedRef<SWidget> {
    auto const weak_settings{settings_};
    auto available{TAttribute<bool>::CreateLambda([weak_settings, setting = descriptor.id] {
        auto const* const settings{weak_settings.Get()};
        return settings != nullptr && settings->is_available(setting);
    })};

    switch (descriptor.control_kind) {
        case SettingControlKind::Toggle: {
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
                         .Label(FText::FromString(UTF8_TO_TCHAR(descriptor.label.data())))
                         .ToolTipText(FText::FromString(UTF8_TO_TCHAR(descriptor.tooltip.data())))
                         .Checked(checked)
                         .ControlEnabled(available)
                         .OnCheckStateChanged_Lambda([weak_settings, setting = descriptor.id](
                                                         ECheckBoxState const state) {
                             if (auto* const settings{weak_settings.Get()}) {
                                 settings->set_setting(
                                     setting, GameSettingValue{state == ECheckBoxState::Checked});
                             }
                         })};

            focus_action = [control] { control->focus(); };
            return row;
        }
        case SettingControlKind::Choice: {
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
                    .Label(FText::FromString(UTF8_TO_TCHAR(descriptor.label.data())))
                    .ToolTipText(FText::FromString(UTF8_TO_TCHAR(descriptor.tooltip.data())))
                    .Options(MoveTemp(labels))
                    .SelectedIndex(selected)
                    .ControlEnabled(available)
                    .OnSelectionChanged_Lambda(
                        [this, weak_settings, setting = descriptor.id, options](int32 const index) {
                            if (auto* const settings{weak_settings.Get()};
                                options.IsValidIndex(index)) {
                                settings->set_setting(setting, options[index].value);
                            }
                        })};

            focus_action = [control] { control->focus(); };
            return row;
        }
        case SettingControlKind::FloatRange: {
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
                         .Label(FText::FromString(UTF8_TO_TCHAR(descriptor.label.data())))
                         .ToolTipText(FText::FromString(UTF8_TO_TCHAR(descriptor.tooltip.data())))
                         .Value(value)
                         .ValueText(value_text)
                         .Minimum(static_cast<float>(descriptor.minimum))
                         .Maximum(static_cast<float>(descriptor.maximum))
                         .Step(static_cast<float>(descriptor.step))
                         .ControlEnabled(available)
                         .OnValueChanged_Lambda(
                             [weak_settings, setting = descriptor.id](float const next) {
                                 if (auto* const settings{weak_settings.Get()}) {
                                     settings->set_setting(setting, GameSettingValue{next});
                                 }
                             })};

            focus_action = [control] { control->focus(); };
            return row;
        }
        case SettingControlKind::IntegerRange:
            break;
        case SettingControlKind::Custom:
            return SNew(SSettingsReadOnlyRow)
                .Style(&style_->settings())
                .Label(FText::FromString(UTF8_TO_TCHAR(descriptor.label.data())))
                .Value(NSLOCTEXT("OptionsMenu", "UnsupportedCustomSetting", "Unavailable"))
                .ToolTipText(FText::FromString(UTF8_TO_TCHAR(descriptor.tooltip.data())));
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

    auto operating_system{to_fstring(capabilities_->operating_system_version)};
    if (!capabilities_->operating_system_subversion.empty()) {
        if (!operating_system.IsEmpty()) {
            operating_system += TEXT(" ");
        }
        operating_system += to_fstring(capabilities_->operating_system_subversion);
    }

    add_section(NSLOCTEXT("OptionsMenu", "PlatformSection", "Platform"),
                {{NSLOCTEXT("OptionsMenu", "PlatformLabel", "Platform"),
                  friendly_platform_name(to_fstring(capabilities_->platform_name))},
                 {NSLOCTEXT("OptionsMenu", "ArchitectureLabel", "Architecture"),
                  capability_text(to_fstring(capabilities_->host_architecture))},
                 {NSLOCTEXT("OptionsMenu", "OperatingSystemLabel", "Operating System"),
                  capability_text(operating_system)}});
    add_section(NSLOCTEXT("OptionsMenu", "ProcessorSection", "Processor"),
                {{NSLOCTEXT("OptionsMenu", "CpuVendorLabel", "CPU Vendor"),
                  capability_text(to_fstring(capabilities_->cpu_vendor))},
                 {NSLOCTEXT("OptionsMenu", "CpuLabel", "CPU"),
                  capability_text(to_fstring(capabilities_->cpu_brand))},
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
                  capability_text(to_fstring(capabilities_->primary_gpu_brand))}});
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
                         .Text(NSLOCTEXT("OptionsMenu", "PromptSave", "Save"))
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
        NSLOCTEXT("OptionsMenu", "UnsavedPrompt", "Save your changes before leaving?"),
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
                        begin_binding_capture(binding);
                        return FReply::Handled();
                    })};
    auto clear{SAssignNew(binding_clear_button_, SGameButton)
                   .Style(&style_->button(EGameButtonStyle::Secondary))
                   .Audio(audio_)
                   .Text(NSLOCTEXT("OptionsMenu", "ClearBinding", "Clear"))
                   .Enabled_Lambda([this] {
                       return managed_binding_.IsSet() &&
                              managed_binding_->current_key != PhysicalKey::Unbound;
                   })
                   .OnClicked_Lambda([this] {
                       if (!managed_binding_.IsSet()) {
                           return FReply::Handled();
                       }
                       auto const identity{FControlsFocusIdentity{managed_binding_->mapping}};
                       if (auto* const settings{settings_.Get()}) {
                           settings->set_control_binding(
                               managed_binding_->mapping, PhysicalKey::Unbound, false);
                       }
                       close_binding_management(false);
                       request_controls_rebuild(identity);
                       return FReply::Handled();
                   })};
    auto reset{SAssignNew(binding_reset_button_, SGameButton)
                   .Style(&style_->button(EGameButtonStyle::Secondary))
                   .Audio(audio_)
                   .Text(NSLOCTEXT("OptionsMenu", "ResetBinding", "Reset"))
                   .Enabled_Lambda([this] {
                       return managed_binding_.IsSet() && managed_binding_->is_modified();
                   })
                   .OnClicked_Lambda([this] {
                       if (!managed_binding_.IsSet()) {
                           return FReply::Handled();
                       }
                       auto const binding{managed_binding_.GetValue()};
                       begin_binding_capture(binding);
                       accept_binding_key(binding.default_key);
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
        auto const key_text{binding_label(binding.current_key)};
        return FText::Format(
            NSLOCTEXT("OptionsMenu", "BindingManagementPrompt", "{0}\n\nCurrent binding: {1}"),
            binding.display_name,
            key_text);
    })};
    return build_modal(MoveTemp(title), {change, clear, reset, cancel});
}

auto SGameOptionsView::build_capture_prompt() -> TSharedRef<SWidget> {
    auto title{TAttribute<FText>::CreateLambda([this] {
        if (!capture_error_.IsEmpty()) {
            return capture_error_;
        }
        return controls_device_ == GameSettingDevice::Controller
                 ? NSLOCTEXT("OptionsMenu",
                             "CaptureControllerBindingPrompt",
                             "Press a controller input. Press Back to cancel.")
                 : NSLOCTEXT("OptionsMenu",
                             "CaptureKeyboardMouseBindingPrompt",
                             "Press a keyboard or mouse input. Press Escape to cancel.");
    })};
    return build_modal(MoveTemp(title), {});
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
                             applied = settings->set_control_binding(
                                 *captured_binding_, captured_key_, true);
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
        auto const captured_input{binding_label(captured_key_)};
        TArray<FText> conflicting_controls;
        if (auto* settings{settings_.Get()}; captured_binding_ && settings) {
            conflicting_controls = settings->binding_conflicts(*captured_binding_, captured_key_);
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
auto SGameOptionsView::setting_float(GameSetting const setting) const -> float {
    auto const* const settings{settings_.Get()};
    if (settings == nullptr) {
        return 0.0f;
    }
    auto const value{settings->value(setting)};
    auto const* const typed{std::get_if<float>(&value)};
    return typed != nullptr ? *typed : 0.0f;
}

auto SGameOptionsView::format_range_value(GameSettingDescriptor const& descriptor) const -> FText {
    auto const value{setting_float(descriptor.id)};
    switch (descriptor.id) {
        case GameSetting::MasterVolume:
        case GameSetting::MusicVolume:
        case GameSetting::SfxVolume:
        case GameSetting::UIVolume:
            return FText::Format(NSLOCTEXT("OptionsMenu", "NormalizedPercent", "{0}%"),
                                 FText::AsNumber(FMath::RoundToInt32(value * 100.0f)));
        case GameSetting::ResolutionScale:
            return FText::Format(NSLOCTEXT("OptionsMenu", "Percent", "{0}%"),
                                 FText::AsNumber(FMath::RoundToInt32(value)));
        default:
            return FText::AsNumber(value);
    }
}

auto SGameOptionsView::controls_focus_identity() const -> TOptional<FControlsFocusIdentity> {
    for (auto const& target : controls_focus_targets_) {
        if (target.has_focus && target.has_focus()) {
            return target.identity;
        }
    }
    return {};
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

    auto const selected_device{FControlsFocusIdentity{controls_device_}};
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

} // namespace ioj
