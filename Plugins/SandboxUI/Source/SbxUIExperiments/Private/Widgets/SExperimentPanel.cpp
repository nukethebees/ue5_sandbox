#include "Widgets/SExperimentPanel.h"

#include "SandboxUI/slate/SlateSlots.h"
#include "SandboxUI/widgets/SSectionPanel.h"

#include "Styling/AppStyle.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"

#include "generated/SExperimentBenchmark.slate.generated.h"
#include "generated/SExperimentPanel.slate.generated.h"

void SExperimentPanel::Construct(FArguments const& args) {
    ChildSlot[SlateGenerated::SExperimentPanelBuilder{*this}.Build(
        args._Title,
        args._Description,
        FAppStyle::GetBrush("ToolPanel.GroupBorder"),
        FAppStyle::Get().GetFontStyle("HeadingExtraSmall"),
        args._Controls.Widget,
        args._Preview.Widget)];
}

void SExperimentBenchmark::Construct(FArguments const& args) {
    ChildSlot[SlateGenerated::SExperimentBenchmarkBuilder{*this}.Build(args._ButtonText,
                                                                       args._ToolTipText,
                                                                       args._OnClicked,
                                                                       args._OutputHeight,
                                                                       args._Output.Widget)];
}
