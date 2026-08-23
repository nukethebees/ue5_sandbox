#pragma once

#include "SandboxCore/graph_plot.h"
#include "Fonts/SlateFontInfo.h"
#include "Widgets/SLeafWidget.h"

struct SANDBOXCOREENGINE_API FGraphPlotStyle {
    FGraphPlotStyle();

    FVector2f desired_size{320.0f, 180.0f};
    float left_margin{54.0f};
    float right_margin{12.0f};
    float top_margin{12.0f};
    float bottom_margin{26.0f};
    FLinearColor background_color{0.008f, 0.01f, 0.015f, 0.9f};
    FLinearColor plot_color{0.015f, 0.02f, 0.03f, 1.0f};
    FLinearColor grid_color{0.18f, 0.2f, 0.24f, 0.45f};
    FLinearColor axis_color{0.5f, 0.52f, 0.56f, 0.9f};
    FLinearColor label_color{0.75f, 0.77f, 0.8f, 1.0f};
    FSlateFontInfo label_font;
    int32 target_x_ticks{6};
    int32 target_y_ticks{5};
    bool show_legend{true};
};

/** Lightweight, non-owning Slate line graph. The caller must keep supplied sample arrays alive. */
class SANDBOXCOREENGINE_API SGraphPlot : public SLeafWidget {
  public:
    SLATE_BEGIN_ARGS(SGraphPlot) {}
    SLATE_ARGUMENT(FGraphPlotStyle, Style)
    SLATE_ARGUMENT(FGraphAxisSettings, XAxis)
    SLATE_ARGUMENT(FGraphAxisSettings, YAxis)
    SLATE_END_ARGS()

    void Construct(FArguments const& args);

    [[nodiscard]] bool set_series(TConstArrayView<FGraphSeriesView> series, uint64 data_revision);
    [[nodiscard]] bool set_axis_settings(FGraphAxisSettings x_axis, FGraphAxisSettings y_axis);
    void set_style(FGraphPlotStyle style);

    auto get_cache_stats() const noexcept -> FGraphCacheStats const& { return cache_.get_stats(); }

    FVector2D ComputeDesiredSize(float layout_scale_multiplier) const override;
    void Tick(FGeometry const& allotted_geometry, double current_time, float delta_time) override;
    int32 OnPaint(FPaintArgs const& args,
                  FGeometry const& allotted_geometry,
                  FSlateRect const& culling_rect,
                  FSlateWindowElementList& out_draw_elements,
                  int32 layer_id,
                  FWidgetStyle const& widget_style,
                  bool parent_enabled) const override;
  private:
    struct FTick {
        FText label;
        float position{0.0f};
    };

    void rebuild_ticks();
    static void build_ticks(
        FGraphRange range, float extent, int32 target_count, bool invert, TArray<FTick>& out_ticks);

    FGraphRenderCache cache_;
    FGraphPlotStyle style_;
    TArray<FTick> x_ticks_;
    TArray<FTick> y_ticks_;
    FVector2f plot_origin_{0.0f, 0.0f};
    FVector2f plot_size_{0.0f, 0.0f};
    bool ticks_dirty_{true};
};
