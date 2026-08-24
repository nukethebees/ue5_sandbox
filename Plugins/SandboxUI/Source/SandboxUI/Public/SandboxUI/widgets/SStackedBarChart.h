#pragma once

#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Fonts/SlateFontInfo.h"
#include "Internationalization/Text.h"
#include "Layout/Margin.h"
#include "Math/Color.h"
#include "Math/Vector2D.h"
#include "Widgets/SLeafWidget.h"

struct SANDBOXUI_API FStackedBarSegment {
    float value{0.0f};
    FLinearColor color{FLinearColor::White};
};

struct SANDBOXUI_API FStackedBar {
    FText label;
    TArray<FStackedBarSegment> segments;
};

struct SANDBOXUI_API FStackedBarChartStyle {
    FStackedBarChartStyle();

    FVector2f desired_size{320.0f, 200.0f};
    FMargin chart_padding{12.0f};
    float bar_gap{8.0f};
    FLinearColor axis_color{0.5f, 0.52f, 0.56f, 0.9f};
    float axis_thickness{1.0f};
    FSlateFontInfo label_font;
    FLinearColor label_color{0.75f, 0.77f, 0.8f, 1.0f};
    float label_area_height{20.0f};
};

struct SANDBOXUI_API FStackedBarSegmentGeometry {
    int32 bar_index{INDEX_NONE};
    int32 segment_index{INDEX_NONE};
    FVector2f position{FVector2f::ZeroVector};
    FVector2f size{FVector2f::ZeroVector};
    FLinearColor color{FLinearColor::White};
};

struct SANDBOXUI_API FStackedBarChartGeometry {
    TArray<FStackedBarSegmentGeometry> segments;
    float maximum_total{0.0f};
    float slot_width{0.0f};
    float bar_width{0.0f};
};

SANDBOXUI_API auto stacked_bar_total(FStackedBar const& bar) -> float;
SANDBOXUI_API auto maximum_stacked_bar_total(TConstArrayView<FStackedBar> bars) -> float;
SANDBOXUI_API auto build_stacked_bar_chart_geometry(TConstArrayView<FStackedBar> bars,
                                                    FVector2f plot_size,
                                                    float bar_gap) -> FStackedBarChartGeometry;

class SANDBOXUI_API SStackedBarChart : public SLeafWidget {
  public:
    SLATE_BEGIN_ARGS(SStackedBarChart) {}
    SLATE_ARGUMENT(FStackedBarChartStyle, Style)
    SLATE_END_ARGS()

    void Construct(FArguments const& args);

    void set_bars(TArray<FStackedBar> bars);
    void clear_bars();
    [[nodiscard]] bool set_style(FStackedBarChartStyle style);

    auto get_bars() const noexcept -> TConstArrayView<FStackedBar> { return bars_; }
    auto get_style() const noexcept -> FStackedBarChartStyle const& { return style_; }

    FVector2D ComputeDesiredSize(float layout_scale_multiplier) const override;
    int32 OnPaint(FPaintArgs const& args,
                  FGeometry const& allotted_geometry,
                  FSlateRect const& culling_rect,
                  FSlateWindowElementList& out_draw_elements,
                  int32 layer_id,
                  FWidgetStyle const& widget_style,
                  bool parent_enabled) const override;
  private:
    static bool is_valid_style(FStackedBarChartStyle const& style);

    TArray<FStackedBar> bars_;
    FStackedBarChartStyle style_;
};
