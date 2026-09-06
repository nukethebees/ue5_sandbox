#include "SandboxUI/widgets/SettingsWidgets.h"

#include "SandboxUI/slate/SlateSlots.h"

#include "Framework/Application/SlateApplication.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#include "generated/SSettingsRow.slate.generated.h"
#include "generated/SSettingsSection.slate.generated.h"
#include "generated/SSettingsSlider.slate.generated.h"

namespace {
auto valid_style(FSettingsStyle const* const style) -> FSettingsStyle const& {
    return style != nullptr ? *style : default_settings_style();
}
}

FSettingsStyle::FSettingsStyle() {
    auto const& core_style{FCoreStyle::Get()};
    section_text = core_style.GetWidgetStyle<FTextBlockStyle>("NormalText");
    label_text = section_text;
    value_text = section_text;
    disabled_text = section_text;
    empty_text = section_text;
    slider = core_style.GetWidgetStyle<FSliderStyle>("Slider");
    toggle = core_style.GetWidgetStyle<FCheckBoxStyle>("Checkbox");
    combo_box = core_style.GetWidgetStyle<FComboBoxStyle>("ComboBox");
    combo_row = core_style.GetWidgetStyle<FTableRowStyle>("TableView.Row");
    scroll_bar = core_style.GetWidgetStyle<FScrollBarStyle>("ScrollBar");
    page_background.DrawAs = ESlateBrushDrawType::Box;
    page_background.TintColor = FSlateColor{FLinearColor{0.018f, 0.028f, 0.045f, 0.99f}};
    section_background.DrawAs = ESlateBrushDrawType::Box;
    section_background.TintColor = FSlateColor{FLinearColor{0.04f, 0.05f, 0.065f, 0.96f}};
    section_border = section_background;
    value_background = section_background;
}

auto default_settings_style() -> FSettingsStyle const& {
    static FSettingsStyle const style{};
    return style;
}

void SSettingsRow::Construct(FArguments const& args) {
    auto const& style{valid_style(args._Style)};
    auto const enabled{args._ControlEnabled};
    auto label_colour{[enabled, &style]() {
        return enabled.Get() ? style.label_text.ColorAndOpacity
                             : style.disabled_text.ColorAndOpacity;
    }};
    auto label{SNew(STextBlock)
                   .Text(args._Label)
                   .TextStyle(&style.label_text)
                   .ColorAndOpacity_Lambda(MoveTemp(label_colour))
                   .ToolTipText(args._ToolTipText)};
    auto control{SNew(SBox)
                     .IsEnabled(args._ControlEnabled)
                     .ToolTipText(args._ToolTipText)[args._Content.Widget]};

    auto const control_padding{FMargin{style.label_control_spacing, 0.0f}};
    ChildSlot[SlateGenerated::SSettingsRowBuilder{*this}.Build(style.row_minimum_height,
                                                               style.label_width,
                                                               control_padding,
                                                               style.control_width,
                                                               label,
                                                               control)];
}

void SSettingsSection::Construct(FArguments const& args) {
    auto const& style{valid_style(args._Style)};
    TSharedRef<SWidget> title{args._Header.Widget};
    if (title == SNullWidget::NullWidget) {
        title = SNew(STextBlock).Text(args._Title).TextStyle(&style.section_text);
    }
    ChildSlot[SlateGenerated::SSettingsSectionBuilder{*this}.Build(style.section_padding,
                                                                   &style.section_background,
                                                                   &style.section_border,
                                                                   style.section_border_thickness,
                                                                   style.section_title_padding,
                                                                   title,
                                                                   args._Content.Widget)];
}

void SSettingsSlider::Construct(FArguments const& args) {
    auto const& style{valid_style(args._Style)};
    minimum_ = args._Minimum;
    maximum_ = FMath::Max(args._Maximum, minimum_);
    step_ = FMath::Max(args._Step, UE_SMALL_NUMBER);
    value_ = args._Value;
    on_value_changed_ = args._OnValueChanged;

    auto const range{maximum_ - minimum_};
    auto const normalized_step{range > UE_SMALL_NUMBER ? FMath::Clamp(step_ / range, 0.0f, 1.0f)
                                                       : 1.0f};
    SAssignNew(slider_, SSlider)
        .Style(&style.slider)
        .Value(this, &SSettingsSlider::normalized_value)
        .StepSize(normalized_step)
        .MouseUsesStep(true)
        .RequiresControllerLock(false)
        .OnValueChanged(this, &SSettingsSlider::handle_value_changed);
    auto value_text{SNew(SBorder)
                        .BorderImage(&style.value_background)
                        .Padding(FMargin{6.0f, 2.0f})[SNew(STextBlock)
                                                          .Text(args._ValueText)
                                                          .TextStyle(&style.value_text)
                                                          .Justification(ETextJustify::Center)]};

    auto const value_padding{FMargin{style.value_text_spacing, 0.0f}};
    ChildSlot[SlateGenerated::SSettingsSliderBuilder{*this}.Build(args._Style,
                                                                  args._Label,
                                                                  args._ToolTipText,
                                                                  args._ControlEnabled,
                                                                  value_padding,
                                                                  style.value_text_width,
                                                                  slider_.ToSharedRef(),
                                                                  value_text)];
}

void SSettingsSlider::focus() {
    if (slider_.IsValid()) {
        FSlateApplication::Get().SetKeyboardFocus(slider_, EFocusCause::SetDirectly);
    }
}

auto SSettingsSlider::normalize(float const value, float const minimum, float const maximum)
    -> float {
    auto const range{maximum - minimum};
    return range > UE_SMALL_NUMBER ? FMath::Clamp((value - minimum) / range, 0.0f, 1.0f) : 0.0f;
}

auto SSettingsSlider::denormalize(float const value,
                                  float const minimum,
                                  float const maximum,
                                  float const step) -> float {
    if (maximum <= minimum) {
        return minimum;
    }
    auto const unclamped{minimum + FMath::Clamp(value, 0.0f, 1.0f) * (maximum - minimum)};
    auto const valid_step{FMath::Max(step, UE_SMALL_NUMBER)};
    auto const stepped{minimum +
                       FMath::RoundToFloat((unclamped - minimum) / valid_step) * valid_step};
    return FMath::Clamp(stepped, minimum, maximum);
}

auto SSettingsSlider::normalized_value() const -> float {
    return normalize(value_.Get(), minimum_, maximum_);
}

void SSettingsSlider::handle_value_changed(float const value) {
    on_value_changed_.ExecuteIfBound(denormalize(value, minimum_, maximum_, step_));
}

void SSettingsChoice::Construct(FArguments const& args) {
    style_ = &valid_style(args._Style);
    selected_index_ = args._SelectedIndex;
    on_selection_changed_ = args._OnSelectionChanged;
    options_.Reserve(args._Options.Num());
    for (auto const& option : args._Options) {
        options_.Add(MakeShared<FText>(option));
    }

    auto selected_item{options_.IsValidIndex(selected_index_.Get())
                           ? options_[selected_index_.Get()]
                           : TSharedPtr<FText>{}};
    SAssignNew(combo_box_, SComboBox<TSharedPtr<FText>>)
        .ComboBoxStyle(&style_->combo_box)
        .ItemStyle(&style_->combo_row)
        .OptionsSource(&options_)
        .InitiallySelectedItem(selected_item)
        .OnGenerateWidget(this, &SSettingsChoice::make_option_widget)
        .OnComboBoxOpening(this, &SSettingsChoice::handle_opening)
        .OnSelectionChanged(
            this,
            &SSettingsChoice::handle_selection_changed)[SNew(STextBlock)
                                                            .Text(this,
                                                                  &SSettingsChoice::selected_text)
                                                            .TextStyle(&style_->value_text)];

    ChildSlot[SNew(SSettingsRow)
                  .Style(args._Style)
                  .Label(args._Label)
                  .ToolTipText(args._ToolTipText)
                  .ControlEnabled(args._ControlEnabled)[combo_box_.ToSharedRef()]];
}

void SSettingsChoice::focus() {
    if (combo_box_.IsValid()) {
        FSlateApplication::Get().SetKeyboardFocus(combo_box_, EFocusCause::SetDirectly);
    }
}

auto SSettingsChoice::make_option_widget(TSharedPtr<FText> option) const -> TSharedRef<SWidget> {
    return SNew(STextBlock)
        .Text(option.IsValid() ? *option : FText::GetEmpty())
        .TextStyle(&style_->value_text);
}

auto SSettingsChoice::selected_text() const -> FText {
    auto const index{selected_index_.Get()};
    return options_.IsValidIndex(index) ? *options_[index] : FText::GetEmpty();
}

void SSettingsChoice::handle_opening() {
    auto const index{selected_index_.Get()};
    if (!options_.IsValidIndex(index) || !combo_box_.IsValid()) {
        return;
    }
    refreshing_ = true;
    combo_box_->SetSelectedItem(options_[index]);
    refreshing_ = false;
}

void SSettingsChoice::handle_selection_changed(TSharedPtr<FText> const option,
                                               ESelectInfo::Type const selection_type) {
    static_cast<void>(selection_type);
    if (refreshing_) {
        return;
    }
    auto const index{options_.IndexOfByKey(option)};
    if (index != INDEX_NONE) {
        on_selection_changed_.ExecuteIfBound(index);
    }
}

void SSettingsToggle::Construct(FArguments const& args) {
    auto const& style{valid_style(args._Style)};
    auto checked{args._Checked};
    SAssignNew(toggle_, SCheckBox)
        .Style(&style.toggle)
        .IsChecked_Lambda([checked]() {
            return checked.Get() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
        })
        .OnCheckStateChanged(args._OnCheckStateChanged);
    ChildSlot[SNew(SSettingsRow)
                  .Style(args._Style)
                  .Label(args._Label)
                  .ToolTipText(args._ToolTipText)
                  .ControlEnabled(args._ControlEnabled)[toggle_.ToSharedRef()]];
}

void SSettingsToggle::focus() {
    if (toggle_.IsValid()) {
        FSlateApplication::Get().SetKeyboardFocus(toggle_, EFocusCause::SetDirectly);
    }
}

void SSettingsReadOnlyRow::Construct(FArguments const& args) {
    auto const& style{valid_style(args._Style)};
    auto value{SNew(STextBlock)
                   .Text(args._Value)
                   .TextStyle(&style.value_text)
                   .AutoWrapText(true)
                   .ToolTipText(args._ToolTipText)};
    ChildSlot[SNew(SSettingsRow)
                  .Style(args._Style)
                  .Label(args._Label)
                  .ToolTipText(args._ToolTipText)[value]];
}
