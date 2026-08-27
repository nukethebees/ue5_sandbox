#include "Experiments/Radar3D/Radar3DShowcase.h"

#include "SRadar3DWidget.h"

#include "Styling/AppStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

URadar3DShowcase::URadar3DShowcase() {
    TabDisplayName = NSLOCTEXT("Radar3D", "ShowcaseTabName", "RDG 3D Radar Experiment");
    bAlwaysReregisterWithWindowsMenu = true;
}

TSharedRef<SWidget> URadar3DShowcase::RebuildWidget() {
    return SNew(SBorder)
        .BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
        .Padding(
            12.0f)[SNew(SVerticalBox) +
                   SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
                       [SNew(STextBlock)
                            .Text(NSLOCTEXT("Radar3D", "Title", "RDG 3D Radar Experiment"))
                            .Font(FAppStyle::Get().GetFontStyle("HeadingExtraSmall"))] +
                   SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 10.0f)
                       [SNew(STextBlock)
                            .Text(NSLOCTEXT("Radar3D",
                                            "Description",
                                            "Synthetic CPU contacts are rendered by RDG into one "
                                            "offscreen texture and displayed by Slate."))] +
                   SVerticalBox::Slot()
                       .FillHeight(1.0f)
                       .HAlign(HAlign_Center)
                       .VAlign(VAlign_Center)[SNew(SBox).WidthOverride(512.0f).HeightOverride(
                           512.0f)[SNew(SRadar3DWidget)]]];
}
