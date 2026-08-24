#pragma once

#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Fonts/SlateFontInfo.h"
#include "Layout/Margin.h"
#include "Math/Color.h"
#include "Math/Vector2D.h"
#include "Widgets/SLeafWidget.h"

struct SANDBOXUI_API FScatterPlotPoint {
    FVector2f position{FVector2f::ZeroVector};
    FLinearColor color{FLinearColor::White};
};

struct SANDBOXUI_API FScatterPlotDomain {
    float minimum_x{0.0f};
    float maximum_x{1.0f};
    float minimum_y{0.0f};
    float maximum_y{1.0f};
};

struct SANDBOXUI_API FScatterPlotStyle {
    FScatterPlotStyle();

    FVector2f desired_size{320.0f, 200.0f};
    FMargin chart_padding{12.0f};
    FVector2f point_size{5.0f, 5.0f};
    FLinearColor axis_color{0.5f, 0.52f, 0.56f, 0.9f};
    float axis_thickness{1.0f};
    FSlateFontInfo label_font;
    FLinearColor label_color{0.75f, 0.77f, 0.8f, 1.0f};
    float x_label_area_height{20.0f};
    float y_label_area_width{36.0f};
};

struct SANDBOXUI_API FScatterPlotPointGeometry {
    int32 point_index{INDEX_NONE};
    FVector2f position{FVector2f::ZeroVector};
    FVector2f size{FVector2f::ZeroVector};
    FLinearColor color{FLinearColor::White};
};

SANDBOXUI_API auto is_valid_scatter_plot_domain(FScatterPlotDomain const& domain) -> bool;
SANDBOXUI_API auto scatter_plot_to_local(FVector2f point,
                                         FScatterPlotDomain const& domain,
                                         FVector2f plot_size) -> FVector2f;
SANDBOXUI_API auto build_scatter_plot_geometry(TConstArrayView<FScatterPlotPoint> points,
                                               FScatterPlotDomain const& domain,
                                               FVector2f plot_size,
                                               FVector2f point_size)
    -> TArray<FScatterPlotPointGeometry>;

class SANDBOXUI_API SScatterPlot : public SLeafWidget {
  public:
    SLATE_BEGIN_ARGS(SScatterPlot) {}
    SLATE_ARGUMENT(FScatterPlotDomain, Domain)
    SLATE_ARGUMENT(FScatterPlotStyle, Style)
    SLATE_END_ARGS()

    void Construct(FArguments const& args);

    void set_points(TArray<FScatterPlotPoint> points);
    void clear_points();
    [[nodiscard]] bool set_domain(FScatterPlotDomain domain);
    [[nodiscard]] bool set_style(FScatterPlotStyle style);

    auto get_points() const noexcept -> TConstArrayView<FScatterPlotPoint> { return points_; }
    auto get_domain() const noexcept -> FScatterPlotDomain const& { return domain_; }
    auto get_style() const noexcept -> FScatterPlotStyle const& { return style_; }

    FVector2D ComputeDesiredSize(float layout_scale_multiplier) const override;
    int32 OnPaint(FPaintArgs const& args,
                  FGeometry const& allotted_geometry,
                  FSlateRect const& culling_rect,
                  FSlateWindowElementList& out_draw_elements,
                  int32 layer_id,
                  FWidgetStyle const& widget_style,
                  bool parent_enabled) const override;
  private:
    static bool is_valid_style(FScatterPlotStyle const& style);

    TArray<FScatterPlotPoint> points_;
    FScatterPlotDomain domain_;
    FScatterPlotStyle style_;
};
