#pragma once

#include "Layout/Margin.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateColor.h"
#include "Styling/SlateTypes.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Text/STextBlock.h"

class SANDBOXUI_API SSectionPanel : public SCompoundWidget {
  public:
    SLATE_BEGIN_ARGS(SSectionPanel)
        : _Title()
        , _Description()
        , _BorderImage()
        , _BorderBackgroundColor(FLinearColor::White)
        , _TitleFont()
        , _TitleJustification(ETextJustify::Left)
        , _Padding(12.0f)
        , _TitlePadding(0.0f, 0.0f, 0.0f, 4.0f)
        , _DescriptionPadding(0.0f, 0.0f, 0.0f, 10.0f) {}
    SLATE_ATTRIBUTE(FText, Title)
    SLATE_ATTRIBUTE(FText, Description)
    SLATE_ATTRIBUTE(FSlateBrush const*, BorderImage)
    SLATE_ATTRIBUTE(FSlateColor, BorderBackgroundColor)
    SLATE_ATTRIBUTE(FSlateFontInfo, TitleFont)
    SLATE_ARGUMENT(ETextJustify::Type, TitleJustification)
    SLATE_ARGUMENT(FMargin, Padding)
    SLATE_ARGUMENT(FMargin, TitlePadding)
    SLATE_ARGUMENT(FMargin, DescriptionPadding)
    SLATE_DEFAULT_SLOT(FArguments, Content)
    SLATE_END_ARGS()

    void Construct(FArguments const& args);
};
