#include "Sandbox/slate_widgets/NumWidget.h"

TSharedRef<SWidget> UFloatNumWidget::RebuildWidget() {
    slate_widget = SNew(SNumWidget<float>).label(FText::FromString(TEXT("Default"))).value(0);

    return slate_widget.ToSharedRef();
}
void UFloatNumWidget::ReleaseSlateResources(bool bReleaseChildren) {
    Super::ReleaseSlateResources(bReleaseChildren);
    slate_widget.Reset();
}

TSharedRef<SWidget> UIntNumWidget::RebuildWidget() {
    slate_widget = SNew(SNumWidget<int32>).label(FText::FromString(TEXT("Default"))).value(0);

    return slate_widget.ToSharedRef();
}
void UIntNumWidget::ReleaseSlateResources(bool bReleaseChildren) {
    Super::ReleaseSlateResources(bReleaseChildren);
    slate_widget.Reset();
}
