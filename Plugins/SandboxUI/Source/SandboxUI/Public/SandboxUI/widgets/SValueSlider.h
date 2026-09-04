#pragma once

#include "Widgets/Input/SSlider.h"
#include "Widgets/SCompoundWidget.h"

namespace SlateGenerated {
struct SValueSliderBuilder;
}

class SANDBOXUI_API SValueSlider : public SCompoundWidget {
    friend struct SlateGenerated::SValueSliderBuilder;
  public:
    SLATE_BEGIN_ARGS(SValueSlider)
        : _Label()
        , _Value(0.0f)
        , _ValueText()
        , _LabelFillWidth(0.35f)
        , _ControlFillWidth(0.65f)
        , _LabelSpacing(8.0f)
        , _ValueTextMinWidth(48.0f)
        , _ValueTextSpacing(8.0f) {}
    SLATE_ATTRIBUTE(FText, Label)
    SLATE_ATTRIBUTE(float, Value)
    SLATE_ATTRIBUTE(FText, ValueText)
    SLATE_EVENT(FOnFloatValueChanged, OnValueChanged)
    SLATE_ARGUMENT(float, LabelFillWidth)
    SLATE_ARGUMENT(float, ControlFillWidth)
    SLATE_ARGUMENT(float, LabelSpacing)
    SLATE_ARGUMENT(float, ValueTextMinWidth)
    SLATE_ARGUMENT(float, ValueTextSpacing)
    SLATE_END_ARGS()

    void Construct(FArguments const& args);
};
