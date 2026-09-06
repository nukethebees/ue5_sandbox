#pragma once

#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/SCompoundWidget.h"

#include "SettingsWidgets.generated.h"

namespace SlateGenerated {
struct SSettingsRowBuilder;
struct SSettingsSectionBuilder;
struct SSettingsSliderBuilder;
}

USTRUCT(BlueprintType)
struct SANDBOXUI_API FSettingsStyle {
    GENERATED_BODY()

    FSettingsStyle();

    UPROPERTY(EditAnywhere, Category = "Text")
    FTextBlockStyle section_text{};

    UPROPERTY(EditAnywhere, Category = "Text")
    FTextBlockStyle label_text{};

    UPROPERTY(EditAnywhere, Category = "Text")
    FTextBlockStyle value_text{};

    UPROPERTY(EditAnywhere, Category = "Text")
    FTextBlockStyle disabled_text{};

    UPROPERTY(EditAnywhere, Category = "Text")
    FTextBlockStyle empty_text{};

    UPROPERTY(EditAnywhere, Category = "Controls")
    FSliderStyle slider{};

    UPROPERTY(EditAnywhere, Category = "Controls")
    FCheckBoxStyle toggle{};

    UPROPERTY(EditAnywhere, Category = "Controls")
    FComboBoxStyle combo_box{};

    UPROPERTY(EditAnywhere, Category = "Controls")
    FTableRowStyle combo_row{};

    UPROPERTY(EditAnywhere, Category = "Controls")
    FScrollBarStyle scroll_bar{};

    UPROPERTY(EditAnywhere, Category = "Panels")
    FSlateBrush page_background{};

    UPROPERTY(EditAnywhere, Category = "Panels")
    FSlateBrush section_background{};

    UPROPERTY(EditAnywhere, Category = "Panels")
    FSlateBrush section_border{};

    UPROPERTY(EditAnywhere, Category = "Panels")
    FSlateBrush value_background{};

    UPROPERTY(EditAnywhere, Category = "Layout")
    FMargin page_margin{48.0f};

    UPROPERTY(EditAnywhere, Category = "Layout")
    FMargin body_padding{24.0f};

    UPROPERTY(EditAnywhere, Category = "Layout")
    FMargin section_padding{20.0f};

    UPROPERTY(EditAnywhere, Category = "Layout")
    FMargin section_border_thickness{1.0f};

    UPROPERTY(EditAnywhere, Category = "Layout")
    FMargin section_title_padding{0.0f, 0.0f, 0.0f, 14.0f};

    UPROPERTY(EditAnywhere, Category = "Layout")
    FMargin row_padding{0.0f, 4.0f};

    UPROPERTY(EditAnywhere, Category = "Layout")
    float section_spacing{18.0f};

    UPROPERTY(EditAnywhere, Category = "Layout")
    float header_spacing{22.0f};

    UPROPERTY(EditAnywhere, Category = "Layout")
    float footer_spacing{20.0f};

    UPROPERTY(EditAnywhere, Category = "Layout")
    float tab_spacing{8.0f};

    UPROPERTY(EditAnywhere, Category = "Layout")
    float button_spacing{12.0f};

    UPROPERTY(EditAnywhere, Category = "Layout")
    FVector2f window_size{1240.0f, 760.0f};

    UPROPERTY(EditAnywhere, Category = "Layout")
    float label_width{300.0f};

    UPROPERTY(EditAnywhere, Category = "Layout")
    float control_width{420.0f};

    UPROPERTY(EditAnywhere, Category = "Layout")
    float row_minimum_height{36.0f};

    UPROPERTY(EditAnywhere, Category = "Layout")
    float label_control_spacing{32.0f};

    UPROPERTY(EditAnywhere, Category = "Layout")
    float value_text_width{64.0f};

    UPROPERTY(EditAnywhere, Category = "Layout")
    float value_text_spacing{12.0f};
};

SANDBOXUI_API auto default_settings_style() -> FSettingsStyle const&;

class SANDBOXUI_API SSettingsRow : public SCompoundWidget {
    friend struct SlateGenerated::SSettingsRowBuilder;
  public:
    SLATE_BEGIN_ARGS(SSettingsRow)
        : _Style(&default_settings_style())
        , _Label()
        , _ToolTipText()
        , _ControlEnabled(true) {}
    SLATE_ARGUMENT(FSettingsStyle const*, Style)
    SLATE_ATTRIBUTE(FText, Label)
    SLATE_ATTRIBUTE(FText, ToolTipText)
    SLATE_ATTRIBUTE(bool, ControlEnabled)
    SLATE_DEFAULT_SLOT(FArguments, Content)
    SLATE_END_ARGS()

    void Construct(FArguments const& args);
};

class SANDBOXUI_API SSettingsSection : public SCompoundWidget {
    friend struct SlateGenerated::SSettingsSectionBuilder;
  public:
    SLATE_BEGIN_ARGS(SSettingsSection)
        : _Style(&default_settings_style())
        , _Title() {}
    SLATE_ARGUMENT(FSettingsStyle const*, Style)
    SLATE_ATTRIBUTE(FText, Title)
    SLATE_NAMED_SLOT(FArguments, Header)
    SLATE_DEFAULT_SLOT(FArguments, Content)
    SLATE_END_ARGS()

    void Construct(FArguments const& args);
};

class SANDBOXUI_API SSettingsSlider : public SCompoundWidget {
    friend struct SlateGenerated::SSettingsSliderBuilder;
  public:
    SLATE_BEGIN_ARGS(SSettingsSlider)
        : _Style(&default_settings_style())
        , _Label()
        , _ToolTipText()
        , _Value()
        , _ValueText()
        , _Minimum(0.0f)
        , _Maximum(1.0f)
        , _Step(0.01f)
        , _ControlEnabled(true) {}
    SLATE_ARGUMENT(FSettingsStyle const*, Style)
    SLATE_ATTRIBUTE(FText, Label)
    SLATE_ATTRIBUTE(FText, ToolTipText)
    SLATE_ATTRIBUTE(float, Value)
    SLATE_ATTRIBUTE(FText, ValueText)
    SLATE_ARGUMENT(float, Minimum)
    SLATE_ARGUMENT(float, Maximum)
    SLATE_ARGUMENT(float, Step)
    SLATE_ATTRIBUTE(bool, ControlEnabled)
    SLATE_EVENT(FOnFloatValueChanged, OnValueChanged)
    SLATE_END_ARGS()

    void Construct(FArguments const& args);
    void focus();

    static auto normalize(float value, float minimum, float maximum) -> float;
    static auto denormalize(float value, float minimum, float maximum, float step) -> float;
  private:
    auto normalized_value() const -> float;
    void handle_value_changed(float value);

    TAttribute<float> value_{};
    FOnFloatValueChanged on_value_changed_{};
    TSharedPtr<SSlider> slider_{};
    float minimum_{};
    float maximum_{1.0f};
    float step_{0.01f};
};

DECLARE_DELEGATE_OneParam(FOnSettingsChoiceChanged, int32);

class SANDBOXUI_API SSettingsChoice : public SCompoundWidget {
  public:
    SLATE_BEGIN_ARGS(SSettingsChoice)
        : _Style(&default_settings_style())
        , _Label()
        , _ToolTipText()
        , _SelectedIndex(INDEX_NONE)
        , _ControlEnabled(true) {}
    SLATE_ARGUMENT(FSettingsStyle const*, Style)
    SLATE_ATTRIBUTE(FText, Label)
    SLATE_ATTRIBUTE(FText, ToolTipText)
    SLATE_ARGUMENT(TArray<FText>, Options)
    SLATE_ATTRIBUTE(int32, SelectedIndex)
    SLATE_ATTRIBUTE(bool, ControlEnabled)
    SLATE_EVENT(FOnSettingsChoiceChanged, OnSelectionChanged)
    SLATE_END_ARGS()

    void Construct(FArguments const& args);
    void focus();
  private:
    auto make_option_widget(TSharedPtr<FText> option) const -> TSharedRef<SWidget>;
    auto selected_text() const -> FText;
    void handle_opening();
    void handle_selection_changed(TSharedPtr<FText> option, ESelectInfo::Type selection_type);

    TArray<TSharedPtr<FText>> options_{};
    TAttribute<int32> selected_index_{};
    FOnSettingsChoiceChanged on_selection_changed_{};
    TSharedPtr<SComboBox<TSharedPtr<FText>>> combo_box_{};
    FSettingsStyle const* style_{};
    bool refreshing_{};
};

class SANDBOXUI_API SSettingsToggle : public SCompoundWidget {
  public:
    SLATE_BEGIN_ARGS(SSettingsToggle)
        : _Style(&default_settings_style())
        , _Label()
        , _ToolTipText()
        , _Checked(false)
        , _ControlEnabled(true) {}
    SLATE_ARGUMENT(FSettingsStyle const*, Style)
    SLATE_ATTRIBUTE(FText, Label)
    SLATE_ATTRIBUTE(FText, ToolTipText)
    SLATE_ATTRIBUTE(bool, Checked)
    SLATE_ATTRIBUTE(bool, ControlEnabled)
    SLATE_EVENT(FOnCheckStateChanged, OnCheckStateChanged)
    SLATE_END_ARGS()

    void Construct(FArguments const& args);
    void focus();
  private:
    TSharedPtr<SCheckBox> toggle_{};
};

class SANDBOXUI_API SSettingsReadOnlyRow : public SCompoundWidget {
  public:
    SLATE_BEGIN_ARGS(SSettingsReadOnlyRow)
        : _Style(&default_settings_style())
        , _Label()
        , _Value()
        , _ToolTipText() {}
    SLATE_ARGUMENT(FSettingsStyle const*, Style)
    SLATE_ATTRIBUTE(FText, Label)
    SLATE_ATTRIBUTE(FText, Value)
    SLATE_ATTRIBUTE(FText, ToolTipText)
    SLATE_END_ARGS()

    void Construct(FArguments const& args);
};
