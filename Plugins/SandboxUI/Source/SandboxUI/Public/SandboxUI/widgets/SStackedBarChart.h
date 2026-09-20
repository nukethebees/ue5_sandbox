#pragma once

#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Fonts/SlateFontInfo.h"
#include "Internationalization/Text.h"
#include "Layout/Margin.h"
#include "Math/Color.h"
#include "Math/Vector2D.h"
#include "Templates/UniquePtr.h"
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

class SANDBOXUI_API SStackedBarChart : public SLeafWidget {
  public:
    SLATE_BEGIN_ARGS(SStackedBarChart) {}
    SLATE_ARGUMENT(FStackedBarChartStyle, Style)
    SLATE_END_ARGS()

    SStackedBarChart();
    ~SStackedBarChart() override;

    void Construct(FArguments const& args);

    void set_bars(TArray<FStackedBar> bars);
    void clear_bars();
    [[nodiscard]] bool set_style(FStackedBarChartStyle style);

    auto get_bars() const -> TArray<FStackedBar>;
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
    struct FData;

    static bool is_valid_style(FStackedBarChartStyle const& style);
    void check_invariants() const;

    TUniquePtr<FData> data_;
    TArray<FText> labels_;
    TArray<TArray<FLinearColor>> segment_colors_;
    FStackedBarChartStyle style_;
};
