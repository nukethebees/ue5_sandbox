#include "Sandbox/slate_widgets/NumWidget.h"

TSharedRef<SWidget> UNumWidget::RebuildWidget() {
    slate_widget = SNew(SNumWidget<float>).label(FText::FromString(TEXT("Default"))).value(0);

    return slate_widget.ToSharedRef();
}
void UNumWidget::ReleaseSlateResources(bool bReleaseChildren) {
    Super::ReleaseSlateResources(bReleaseChildren);
    slate_widget.Reset();
}
