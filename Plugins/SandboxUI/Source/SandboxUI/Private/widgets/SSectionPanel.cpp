#include "SandboxUI/widgets/SSectionPanel.h"

#include "SandboxUI/slate/SlateSlots.h"

#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace {
auto text_visibility(TAttribute<FText> const& text) -> EVisibility {
    return text.Get().IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible;
}
}

void SSectionPanel::Construct(FArguments const& args) {
    auto title{SNew(STextBlock)
                   .Text(args._Title)
                   .Justification(args._TitleJustification)
                   .Visibility_Lambda(
                       [title_text = args._Title]() { return text_visibility(title_text); })};
    if (args._TitleFont.IsSet()) {
        title->SetFont(args._TitleFont);
    }

    auto border{
        SNew(SBorder)
            .Padding(args._Padding)
            .BorderBackgroundColor(args._BorderBackgroundColor)
                [SNew(SVerticalBox) + SandboxUI::Slate::vbox_auto_slot(args._TitlePadding)[title] +
                 SandboxUI::Slate::vbox_auto_slot(args._DescriptionPadding)
                     [SNew(STextBlock)
                          .Text(args._Description)
                          .Visibility_Lambda([description = args._Description]() {
                              return text_visibility(description);
                          })] +
                 SandboxUI::Slate::vbox_fill_slot()[args._Content.Widget]]};
    if (args._BorderImage.IsSet()) {
        border->SetBorderImage(args._BorderImage);
    }

    ChildSlot[border];
}
