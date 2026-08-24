#include "SpaceGame/presentation/widgets/Vector2DWidget.h"

#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"

void UVector2DWidget::NativeConstruct() {
    Super::NativeConstruct();

    if (name_text) {
        name_text->SetText(name);
    }
    if (value_text) {
        value_text->SetVisibility(show_value ? ESlateVisibility::Visible
                                             : ESlateVisibility::Collapsed);
    }

    update(FVector2D::ZeroVector);
}

void UVector2DWidget::update(FVector2D const value) {
    if (value_text) {
        auto number_format{FNumberFormattingOptions{}};
        number_format.SetMinimumFractionalDigits(1);
        number_format.SetMaximumFractionalDigits(1);

        auto const display_value{FText::Format(INVTEXT("{0}, {1} ({2})"),
                                               FText::AsNumber(value.X, &number_format),
                                               FText::AsNumber(value.Y, &number_format),
                                               FText::AsNumber(value.Size(), &number_format))};
        value_text->SetText(display_value);
    }

    if (!canvas_panel || !background_widget || !cursor_widget) {
        return;
    }

    auto* const cursor_slot{Cast<UCanvasPanelSlot>(cursor_widget->Slot)};
    if (!cursor_slot) {
        return;
    }

    auto const cursor_position{FVector2D{
        FMath::GetMappedRangeValueClamped(FVector2D{-1.f, 1.f}, FVector2D{0.f, 1.f}, value.X),
        FMath::GetMappedRangeValueClamped(FVector2D{-1.f, 1.f}, FVector2D{1.f, 0.f}, value.Y),
    }};

    cursor_slot->SetAnchors(FAnchors{
        static_cast<float>(cursor_position.X),
        static_cast<float>(cursor_position.Y),
    });
    cursor_slot->SetAlignment(FVector2D{0.5, 0.5});
    cursor_slot->SetPosition(FVector2D::ZeroVector);
}

void UVector2DWidget::set_font_size(int32 const new_font_size) {
    font_size = new_font_size;

    if (name_text) {
        auto font{name_text->GetFont()};
        font.Size = font_size;
        name_text->SetFont(font);
    }

    if (value_text) {
        auto font{value_text->GetFont()};
        font.Size = font_size;
        value_text->SetFont(font);
    }
}
