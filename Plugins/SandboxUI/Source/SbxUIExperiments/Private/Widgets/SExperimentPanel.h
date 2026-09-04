#pragma once

#include "Widgets/Input/SButton.h"
#include "Widgets/SCompoundWidget.h"

class SExperimentPanel final : public SCompoundWidget {
  public:
    SLATE_BEGIN_ARGS(SExperimentPanel)
        : _Title()
        , _Description() {}
    SLATE_ATTRIBUTE(FText, Title)
    SLATE_ATTRIBUTE(FText, Description)
    SLATE_NAMED_SLOT(FArguments, Controls)
    SLATE_NAMED_SLOT(FArguments, Preview)
    SLATE_END_ARGS()

    void Construct(FArguments const& args);
};

class SExperimentBenchmark final : public SCompoundWidget {
  public:
    SLATE_BEGIN_ARGS(SExperimentBenchmark)
        : _ButtonText()
        , _ToolTipText()
        , _OutputHeight(150.0f) {}
    SLATE_ATTRIBUTE(FText, ButtonText)
    SLATE_ATTRIBUTE(FText, ToolTipText)
    SLATE_EVENT(FOnClicked, OnClicked)
    SLATE_ARGUMENT(float, OutputHeight)
    SLATE_NAMED_SLOT(FArguments, Output)
    SLATE_END_ARGS()

    void Construct(FArguments const& args);
};
