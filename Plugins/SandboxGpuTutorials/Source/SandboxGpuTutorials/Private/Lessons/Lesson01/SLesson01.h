#pragma once

#include "TutorialMaterialResource.h"

#include "Widgets/SCompoundWidget.h"
#include "Widgets/SLeafWidget.h"

class SLesson01SlateGradient : public SLeafWidget {
  public:
    using ThisClass = SLesson01SlateGradient;

    SLATE_BEGIN_ARGS(SLesson01SlateGradient) {}
    SLATE_END_ARGS()

    void Construct(FArguments const&) {}

    auto OnPaint(FPaintArgs const& args,
                 FGeometry const& allotted_geometry,
                 FSlateRect const& culling_rect,
                 FSlateWindowElementList& out_draw_elements,
                 int32 layer_id,
                 FWidgetStyle const& widget_style,
                 bool parent_enabled) const -> int32 override;

    auto ComputeDesiredSize(float layout_scale_multiplier) const -> FVector2D override;
};

class SLesson01 : public SCompoundWidget {
  public:
    using ThisClass = SLesson01;

    SLATE_BEGIN_ARGS(SLesson01) {}
    SLATE_END_ARGS()

    void Construct(FArguments const& args);
  private:
    FTutorialMaterialResource gradient_material_;
};
