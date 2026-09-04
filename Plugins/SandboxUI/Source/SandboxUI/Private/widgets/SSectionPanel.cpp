#include "SandboxUI/widgets/SSectionPanel.h"

#include "SandboxUI/slate/SlateSlots.h"

#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#include "generated/SSectionPanel.slate.generated.h"

namespace {
auto text_visibility(TAttribute<FText> const& text) -> EVisibility {
    return text.Get().IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible;
}
}

void SSectionPanel::Construct(FArguments const& args) {
    auto builder{SlateGenerated::SSectionPanelBuilder{*this}};
    auto title_visibility{[title_text = args._Title]() { return text_visibility(title_text); }};
    auto title{
        builder.BuildTitle(args._Title, args._TitleJustification, MoveTemp(title_visibility))};
    if (args._TitleFont.IsSet()) {
        title->SetFont(args._TitleFont);
    }

    auto description_visibility{
        [description = args._Description]() { return text_visibility(description); }};
    auto description{builder.BuildDescription(args._Description, MoveTemp(description_visibility))};

    auto border{builder.BuildPanel(args._Padding,
                                   args._BorderBackgroundColor,
                                   args._TitlePadding,
                                   args._DescriptionPadding,
                                   title,
                                   description,
                                   args._Content.Widget)};
    if (args._BorderImage.IsSet()) {
        border->SetBorderImage(args._BorderImage);
    }

    ChildSlot[border];
}
