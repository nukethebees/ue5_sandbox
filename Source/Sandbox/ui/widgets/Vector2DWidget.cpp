#include "Sandbox/ui/widgets/Vector2DWidget.h"

#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"

void UVector2DWidget::NativeConstruct() {
    Super::NativeConstruct();

    update(FVector2D::ZeroVector);
}

void UVector2DWidget::update(FVector2D const value) {
    if (!canvas_panel || !background_widget || !cursor_widget) { return; }

    auto* cursor_slot{Cast<UCanvasPanelSlot>(cursor_widget->Slot)};
    if (!cursor_slot) { return; }

    auto const cursor_position{FVector2D{
        FMath::Clamp(value.X * 0.5 + 0.5, 0.0, 1.0),
        FMath::Clamp(value.Y * 0.5 + 0.5, 0.0, 1.0),
    }};

    cursor_slot->SetAnchors(
        FAnchors{static_cast<float>(cursor_position.X), static_cast<float>(cursor_position.Y)});
    cursor_slot->SetAlignment(FVector2D{0.5, 0.5});
    cursor_slot->SetPosition(FVector2D::ZeroVector);
}
