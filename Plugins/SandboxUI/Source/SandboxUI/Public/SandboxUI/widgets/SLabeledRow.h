#pragma once

#include "Layout/Margin.h"
#include "Widgets/SCompoundWidget.h"

class SANDBOXUI_API SLabeledRow : public SCompoundWidget {
  public:
    SLATE_BEGIN_ARGS(SLabeledRow)
        : _Label()
        , _LabelMinWidth(0.0f)
        , _Spacing(8.0f) {}
    SLATE_ATTRIBUTE(FText, Label)
    SLATE_ARGUMENT(float, LabelMinWidth)
    SLATE_ARGUMENT(float, Spacing)
    SLATE_DEFAULT_SLOT(FArguments, Content)
    SLATE_END_ARGS()

    void Construct(FArguments const& args);
};
