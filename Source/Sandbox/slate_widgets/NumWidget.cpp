#include "Sandbox/slate_widgets/NumWidget.h"

#include "SlateOptMacros.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SNumWidget::Construct(FArguments const& InArgs) {
    label = InArgs._label;

    ChildSlot[SNew(SHorizontalBox) +
              SHorizontalBox::Slot().AutoWidth()[SNew(STextBlock).Text(label)]];
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

TSharedRef<SWidget> UNumWidget::RebuildWidget() {
    slate_widget = SNew(SNumWidget).label(FText::FromString(TEXT("Default")));

    return slate_widget.ToSharedRef();
}
void UNumWidget::ReleaseSlateResources(bool bReleaseChildren) {
    Super::ReleaseSlateResources(bReleaseChildren);
    slate_widget.Reset();
}
