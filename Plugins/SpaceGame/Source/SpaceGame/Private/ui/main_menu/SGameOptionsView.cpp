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

    auto panel{SNew(SBorder)
                   .BorderImage(&style_->chrome().body_background)
                   .Padding(style_->settings().body_padding)
                       [SNew(SVerticalBox) + SVerticalBox::Slot().AutoHeight()[header] +
                        SVerticalBox::Slot().FillHeight(1.0f).Padding(FMargin{0.0f, 18.0f})[body] +
                        SVerticalBox::Slot().AutoHeight()[footer]]};

    dirty_prompt_->SetVisibility(EVisibility::Collapsed);
    display_prompt_->SetVisibility(EVisibility::Collapsed);
    ChildSlot[SNew(SOverlay) + SOverlay::Slot()[panel] +
              SOverlay::Slot()[dirty_prompt_.ToSharedRef()] +
              SOverlay::Slot()[display_prompt_.ToSharedRef()]];
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
    auto buttons{TArray<TSharedPtr<SGameButton>>{}};
    if (dirty_prompt_visible_) {
        buttons = {dirty_apply_button_, dirty_discard_button_, dirty_stay_button_};
    } else if (display_prompt_visible_) {
        buttons = {confirm_display_button_, revert_display_button_};
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
        SWidgetSwitcher::Slot()[build_category_page(EGameSettingCategory::Controls)] +
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
