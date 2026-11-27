#include "SGameOptionsView.h"

#include "SpaceGame/settings/GameVideoSettings.h"
#include "SpaceGame/settings/OptionsDraft.h"
#include "SpaceGame/ui/common/HiveWidgets.h"
#include "SpaceGame/ui/common/SGameButton.h"
#include "SpaceGame/UnrealMessages.h"
#include <ioj/input/logical/flight_controls.h>
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

#include <algorithm>

namespace ioj {
namespace {
auto binding_label(PhysicalKey key) -> FText {
    return FText::FromString(to_fstring(player_menu().keyboard.label(key)));
}

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
    setting_focus_.Reset();
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

    dirty_prompt_->SetVisibility(player_menu().prompt == OptionsPrompt::Dirty
                                     ? EVisibility::Visible
                                     : EVisibility::Collapsed);
    auto const category{settings_category(page_.Get())};
    if (reset_button_.IsValid()) {
        reset_button_->SetEnabled(category.has_value() && !settings->is_at_defaults(*category) &&
                                  !settings->is_awaiting_display_confirmation());
    }
    if (apply_button_.IsValid()) {
        apply_button_->SetEnabled(settings->is_dirty() &&
                                  !settings->is_awaiting_display_confirmation());
    }

    auto const display_visible{settings->is_awaiting_display_confirmation()};
    if (display_visible != display_prompt_visible_) {
        display_prompt_visible_ = display_visible;
        if (display_visible) {
            if (confirm_display_button_.IsValid()) {
                confirm_display_button_->focus();
            }
        } else {
            restore_focus();
        }
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
    present({player_menu().focus, MenuOperation::Focus});
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

void SGameOptionsView::present(MenuUpdate const& update) {
    auto const activate{update.operation == MenuOperation::Activate};
    if (update.operation == MenuOperation::BindingPrompt) {
        present_binding_prompt();
        return;
    }
    refresh();
    if (update.operation == MenuOperation::RefreshSettings) {
        return;
    }
    if (update.target.kind == MenuTarget::Kind::Setting) {
        auto const setting{update.target.setting()};
        if (auto* focus{setting_focus_.Find(setting)}) {
            (*focus)();
        }
    } else if (update.target.kind == MenuTarget::Kind::Device) {
        auto const device{update.target.device()};
        if (activate) {
            request_controls_rebuild(true);
        } else {
            restore_controls_focus(FControlsFocusIdentity{device});
        }
    } else if (update.target.kind == MenuTarget::Kind::Mapping) {
        auto const mapping{update.target.mapping()};
        if (activate) {
            auto const rows{player_menu().binding_rows()};
            auto const row{std::ranges::find_if(
                rows, [&](auto const& item) { return item.control.mapping == mapping; })};
            check(row != rows.end());
            open_binding_management(*row);
        } else {
            restore_controls_focus(FControlsFocusIdentity{mapping});
        }
    } else if (update.target.kind == MenuTarget::Kind::Control) {
        auto const control{update.target.control()};
        auto apply = [activate](auto const& button, auto const& action) {
            if (activate) {
                action.ExecuteIfBound();
            } else {
                button->focus();
            }
        };
        switch (control) {
            case MenuControl::Apply:
                apply(apply_button_, on_apply_);
                break;
            case MenuControl::ResetCategory:
                apply(reset_button_, on_reset_);
                break;
            case MenuControl::Save:
                apply(dirty_apply_button_, on_apply_);
                break;
            case MenuControl::Discard:
                dirty_discard_button_->focus();
                break;
            case MenuControl::Stay:
                dirty_stay_button_->focus();
                break;
            case MenuControl::KeepDisplay:
                apply(confirm_display_button_, on_confirm_display_);
                break;
            case MenuControl::RevertDisplay:
                apply(revert_display_button_, on_revert_display_);
                break;
            default:
                break;
        }
    }
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
    if (page_.Get() == MainMenuItem::Controls) {
        return build_controls_page();
    }
    if (auto category{settings_category(page_.Get())}) {
        return build_category_page(*category);
    }
    return page_.Get() == MainMenuItem::System ? build_system_page() : SNullWidget::NullWidget;
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
                          if (page_.Get() != MainMenuItem::Controls) {
                              return NSLOCTEXT("OptionsMenu", "ResetCategory", "Reset Category");
                          }
                          return NSLOCTEXT("OptionsMenu", "ResetAllControls", "Reset All Controls");
                      })
                      .ToolTipText_Lambda([this] {
                          if (page_.Get() != MainMenuItem::Controls) {
                              return FText::GetEmpty();
                          }
                          return NSLOCTEXT(
                              "OptionsMenu",
                              "ResetAllControlsTip",
                              "Reset response settings and bindings for both devices.");
                      })
                      .OnClicked_Lambda([]() {
                          hit_menu_control(MenuControl::ResetCategory);
                          return FReply::Handled();
                      })] +
             SHorizontalBox::Slot()
                 .AutoWidth()[SAssignNew(apply_button_, SGameButton)
                                  .Style(&style_->button(EGameButtonStyle::Primary))
                                  .Audio(audio_)
                                  .Text(NSLOCTEXT("OptionsMenu", "Save", "Save"))
                                  .OnClicked_Lambda([]() {
                                      hit_menu_control(MenuControl::Apply);
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
            auto const row{build_setting_row(*descriptor)};
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
void SGameOptionsView::rebuild_controls_page(bool const reset_scroll) {
    auto* const settings{settings_.Get()};
    if (!controls_content_.IsValid() || settings == nullptr) {
        return;
    }

    auto const scroll_offset{controls_scroll_box_.IsValid() && !reset_scroll
                                 ? controls_scroll_box_->GetScrollOffset()
                                 : 0.0f};
    setting_focus_.Reset();
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
            rows->AddSlot().AutoHeight().Padding(
                style_->settings().row_padding)[build_setting_row(*descriptor)];
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
                  .Selected(player_menu().device == GameSettingDevice::KeyboardMouse)
                  .Text(NSLOCTEXT("OptionsMenu", "KeyboardMouseDevice", "Keyboard & Mouse"))
                  .OnClicked_Lambda([this] {
                      hit_menu_control(GameSettingDevice::KeyboardMouse);
                      return FReply::Handled();
                  })] +
         SHorizontalBox::Slot().FillWidth(
             1.0f)[SAssignNew(controller_button, SGameButton)
                       .Style(&style_->button(EGameButtonStyle::Secondary))
                       .Audio(audio_)
                       .Selected(player_menu().device == GameSettingDevice::Controller)
                       .Text(NSLOCTEXT("OptionsMenu", "ControllerDevice", "Controller"))
                       .OnClicked_Lambda([this] {
                           hit_menu_control(GameSettingDevice::Controller);
                           return FReply::Handled();
                       })]];
    add_section(NSLOCTEXT("OptionsMenu", "InputDeviceSection", "Input Device"), device_rows);
    register_controls_focus(FControlsFocusIdentity{GameSettingDevice::KeyboardMouse},
                            keyboard_mouse_button.ToSharedRef());
    register_controls_focus(FControlsFocusIdentity{GameSettingDevice::Controller},
                            controller_button.ToSharedRef());

    add_setting_sections(GameSettingDevice::Shared);
    add_setting_sections(player_menu().device);

    auto const bindings{player_menu().binding_rows()};
    std::optional<BindingGroup> binding_category;
    TSharedPtr<SVerticalBox> binding_rows;
    auto const flush_binding_category = [&] {
        if (binding_rows.IsValid()) {
            add_section(FText::FromString(to_fstring(to_string(*binding_category))),
                        binding_rows.ToSharedRef());
        }
    };
    for (auto const& binding : bindings) {
        if (binding_category != binding.control.group) {
            flush_binding_category();
            binding_category = binding.control.group;
            binding_rows = SNew(SVerticalBox);
        }
        binding_rows->AddSlot().AutoHeight().Padding(style_->settings().row_padding)
            [SNew(SSettingsRow)
                 .Style(&style_->settings())
                 .Label(FText::FromString(to_fstring(binding.control.label)))
                 .ToolTipText(NSLOCTEXT("OptionsMenu",
                                        "ManageBindingTip",
                                        "Open this binding to change, clear, or reset it."))
                     [build_binding_button(binding)]];
    }
    flush_binding_category();
    if (bindings.empty()) {
        auto empty_rows{SNew(SVerticalBox)};
        empty_rows->AddSlot()
            .AutoHeight()[SNew(STextBlock)
                              .Text(NSLOCTEXT(
                                  "OptionsMenu", "NoControlBindings", "No bindings available."))
                              .TextStyle(&style_->settings().empty_text)];
        add_section(NSLOCTEXT("OptionsMenu", "BindingsSection", "Bindings"), empty_rows);
    }

    pending_controls_scroll_offset_ = scroll_offset;
    RegisterActiveTimer(0.0f,
                        FWidgetActiveTimerDelegate::CreateSP(
                            this, &SGameOptionsView::handle_deferred_controls_restore));
}

void SGameOptionsView::request_controls_rebuild(bool const reset_scroll) {
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
    auto const reset_scroll{pending_controls_reset_scroll_};
    pending_controls_reset_scroll_ = false;
    rebuild_controls_page(reset_scroll);
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
    auto const& menu{player_menu()};
    if (menu.page == MainMenuItem::Controls && menu.prompt == OptionsPrompt::None &&
        menu.draft.binding_edit().prompt == BindingPrompt::Closed && controls_content_) {
        present({menu.focus, MenuOperation::Focus});
    }
    return EActiveTimerReturnType::Stop;
}

auto SGameOptionsView::build_binding_button(ControlBindingRow const& binding)
    -> TSharedRef<SWidget> {
    TSharedPtr<SGameButton> button;
    auto result{SAssignNew(button, SGameButton)
                    .Style(&style_->button(EGameButtonStyle::Secondary))
                    .Audio(audio_)
                    .Text_Lambda([key = binding.current_key] { return binding_label(key); })
                    .OnClicked_Lambda([mapping = binding.control.mapping] {
                        hit_menu_control(mapping);
                        return FReply::Handled();
                    })};
    register_controls_focus(binding.control.mapping, button.ToSharedRef());
    return result;
}

void SGameOptionsView::open_binding_management(ControlBindingRow const& binding) {
    managed_binding_ = &binding.control;
    on_interaction_modal_changed_.ExecuteIfBound(true);
    present_binding_prompt();
}

void SGameOptionsView::present_binding_prompt() {
    auto const& edit{player_menu().draft.binding_edit()};
    auto const managing{edit.prompt == BindingPrompt::Manage};
    auto const capturing{edit.prompt == BindingPrompt::Capture ||
                         edit.prompt == BindingPrompt::Unsupported};
    auto const conflicts{edit.prompt == BindingPrompt::Conflict};
    binding_management_prompt_->SetVisibility(managing ? EVisibility::Visible
                                                       : EVisibility::Collapsed);
    capture_prompt_->SetVisibility(capturing ? EVisibility::Visible : EVisibility::Collapsed);
    conflict_prompt_->SetVisibility(conflicts ? EVisibility::Visible : EVisibility::Collapsed);
    if (edit.prompt == BindingPrompt::Closed) {
        managed_binding_ = nullptr;
        on_interaction_modal_changed_.ExecuteIfBound(false);
        request_controls_rebuild();
        refresh();
    } else if (capturing) {
        FSlateApplication::Get().SetKeyboardFocus(SharedThis(this), EFocusCause::SetDirectly);
    } else {
        switch (edit.selected) {
            case BindingChoice::Change:
                binding_change_button_->focus();
                break;
            case BindingChoice::Clear:
                binding_clear_button_->focus();
                break;
            case BindingChoice::Reset:
                binding_reset_button_->focus();
                break;
            case BindingChoice::Replace:
                conflict_replace_button_->focus();
                break;
            case BindingChoice::Cancel:
                (conflicts ? conflict_cancel_button_ : binding_cancel_button_)->focus();
                break;
        }
    }
}

/* **************************************** */
// Settings and prompts
/* **************************************** */
auto SGameOptionsView::build_setting_row(GameSettingDescriptor const& descriptor)
    -> TSharedRef<SWidget> {
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

            setting_focus_.Add(descriptor.id, [control] { control->focus(); });
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

            setting_focus_.Add(descriptor.id, [control] { control->focus(); });
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

            setting_focus_.Add(descriptor.id, [control] { control->focus(); });
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
                         .OnClicked_Lambda([]() {
                             hit_menu_control(MenuControl::Save);
                             return FReply::Handled();
                         })};
    auto dirty_discard{SAssignNew(dirty_discard_button_, SGameButton)
                           .Style(&secondary)
                           .Audio(audio_)
                           .Text(NSLOCTEXT("OptionsMenu", "PromptDiscard", "Discard"))
                           .OnClicked_Lambda([]() {
                               hit_menu_control(MenuControl::Discard);
                               return FReply::Handled();
                           })};
    auto dirty_stay{SAssignNew(dirty_stay_button_, SGameButton)
                        .Style(&secondary)
                        .Audio(audio_)
                        .Text(NSLOCTEXT("OptionsMenu", "PromptStay", "Stay"))
                        .OnClicked_Lambda([]() {
                            hit_menu_control(MenuControl::Stay);
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
                     .OnClicked_Lambda([]() {
                         hit_menu_control(MenuControl::KeepDisplay);
                         return FReply::Handled();
                     })};
    auto revert{SAssignNew(revert_display_button_, SGameButton)
                    .Style(&secondary)
                    .Audio(audio_)
                    .Text(NSLOCTEXT("OptionsMenu", "RevertChanges", "Revert"))
                    .OnClicked_Lambda([]() {
                        hit_menu_control(MenuControl::RevertDisplay);
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
                        hit_menu_control(BindingChoice::Change);
                        return FReply::Handled();
                    })};
    auto clear{SAssignNew(binding_clear_button_, SGameButton)
                   .Style(&style_->button(EGameButtonStyle::Secondary))
                   .Audio(audio_)
                   .Text(NSLOCTEXT("OptionsMenu", "ClearBinding", "Clear"))
                   .Enabled_Lambda([this] {
                       return player_menu().draft.binding_action_available(BindingChoice::Clear);
                   })
                   .OnClicked_Lambda([this] {
                       hit_menu_control(BindingChoice::Clear);
                       return FReply::Handled();
                   })};
    auto reset{SAssignNew(binding_reset_button_, SGameButton)
                   .Style(&style_->button(EGameButtonStyle::Secondary))
                   .Audio(audio_)
                   .Text(NSLOCTEXT("OptionsMenu", "ResetBinding", "Reset"))
                   .Enabled_Lambda([this] {
                       return player_menu().draft.binding_action_available(BindingChoice::Reset);
                   })
                   .OnClicked_Lambda([this] {
                       hit_menu_control(BindingChoice::Reset);
                       return FReply::Handled();
                   })};
    auto cancel{SAssignNew(binding_cancel_button_, SGameButton)
                    .Style(&style_->button(EGameButtonStyle::Secondary))
                    .Audio(audio_)
                    .Text(NSLOCTEXT("OptionsMenu", "CancelBindingManagement", "Cancel"))
                    .OnClicked_Lambda([this] {
                        hit_menu_control(BindingChoice::Cancel);
                        return FReply::Handled();
                    })};
    auto title{TAttribute<FText>::CreateLambda([this] {
        if (!managed_binding_) {
            return FText::GetEmpty();
        }
        auto const& binding{*managed_binding_};
        auto const key_text{
            binding_label(player_menu().draft.pending().controls.bindings[binding.mapping])};
        return FText::Format(
            NSLOCTEXT("OptionsMenu", "BindingManagementPrompt", "{0}\n\nCurrent binding: {1}"),
            FText::FromString(to_fstring(binding.label)),
            key_text);
    })};
    return build_modal(MoveTemp(title), {change, clear, reset, cancel});
}

auto SGameOptionsView::build_capture_prompt() -> TSharedRef<SWidget> {
    auto title{TAttribute<FText>::CreateLambda([this] {
        if (player_menu().draft.binding_edit().prompt == BindingPrompt::Unsupported) {
            return NSLOCTEXT(
                "OptionsMenu", "UnsupportedBinding", "Choose a supported input for this control.");
        }
        return player_menu().device == GameSettingDevice::Controller
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
                         hit_menu_control(BindingChoice::Replace);
                         return FReply::Handled();
                     })};
    auto cancel{SAssignNew(conflict_cancel_button_, SGameButton)
                    .Style(&style_->button(EGameButtonStyle::Secondary))
                    .Audio(audio_)
                    .Text(NSLOCTEXT("OptionsMenu", "CancelBinding", "Cancel"))
                    .OnClicked_Lambda([this] {
                        hit_menu_control(BindingChoice::Cancel);
                        return FReply::Handled();
                    })};
    auto title{TAttribute<FText>::CreateLambda([this] {
        auto const& edit{player_menu().draft.binding_edit()};
        auto const captured_input{binding_label(edit.candidate)};
        TArray<FText> conflicting_controls;
        for (auto const& control : flight_controls()) {
            if (std::ranges::contains(edit.conflicts, control.mapping)) {
                conflicting_controls.Add(FText::FromString(to_fstring(control.label)));
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

void SGameOptionsView::register_controls_focus(FControlsFocusIdentity const identity,
                                               TSharedRef<SGameButton> const& button) {
    controls_focus_targets_.Add({
        .identity = identity,
        .focus = [button] { button->focus(); },
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
}

void SGameOptionsView::restore_focus() {
    present({player_menu().focus, MenuOperation::Focus});
}

} // namespace ioj
