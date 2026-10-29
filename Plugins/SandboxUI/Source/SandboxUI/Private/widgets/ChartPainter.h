#pragma once

#include "Layout/Geometry.h"
#include "Rendering/DrawElements.h"
#include "Rendering/DrawElementTypes.h"
#include "Styling/CoreStyle.h"

namespace SandboxUI::Widgets {
class FChartPainter {
  public:
    FChartPainter(FSlateWindowElementList& elements,
                  FGeometry const& geometry,
                  ESlateDrawEffect const draw_effect)
        : elements_{elements}
        , geometry_{geometry}
        , draw_effect_{draw_effect} {}

    void box(int32 const layer,
             FVector2f const position,
             FVector2f const size,
             FLinearColor const color) const {
        if (size.X <= 0.0f || size.Y <= 0.0f) {
            return;
        }

        auto const paint_geometry{geometry_.ToPaintGeometry(size, FSlateLayoutTransform{position})};
        FSlateDrawElement::MakeBox(elements_,
                                   layer,
                                   paint_geometry,
                                   FCoreStyle::Get().GetBrush("WhiteBrush"),
                                   draw_effect_,
                                   color);
    }

    void visible_box(int32 const layer,
                     FVector2f const position,
                     FVector2f const size,
                     FLinearColor const color) const {
        if (color.A <= 0.0f) {
            return;
        }
        box(layer, position, size, color);
    }

    void label(int32 const layer,
               FVector2f const position,
               FVector2f const size,
               FText const& text,
               FSlateFontInfo const& font,
               FLinearColor const color) const {
        if (size.X <= 0.0f || size.Y <= 0.0f || text.IsEmpty()) {
            return;
        }

        auto const paint_geometry{geometry_.ToPaintGeometry(size, FSlateLayoutTransform{position})};
        elements_.PushClip(FSlateClippingZone{paint_geometry});
        FSlateDrawElement::MakeText(
            elements_, layer, paint_geometry, text, font, draw_effect_, color);
        elements_.PopClip();
    }

  private:
    FSlateWindowElementList& elements_;
    FGeometry const& geometry_;
    ESlateDrawEffect draw_effect_;
};
}
