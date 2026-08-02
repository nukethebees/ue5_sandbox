#include "Sandbox/ui/widgets/Vector2DWidget.h"

#include "Sandbox/ui/widgets/ValueWidget.h"

#include <CoreMinimal.h>

void UVector2DWidget::NativeConstruct() {
    Super::NativeConstruct();

    if (!name.IsNone()) {
        FName const spec{FString::Printf(TEXT("%s: {0}"), *name.ToString())};
        value_widget->set_format_spec(spec);
    }
}

void UVector2DWidget::update(FVector2D const value) {
    if (!value_widget) { return; }

    value_widget->update(value.ToString());
}
