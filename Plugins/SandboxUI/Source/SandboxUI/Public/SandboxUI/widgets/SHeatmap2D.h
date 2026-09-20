#pragma once

#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Fonts/SlateFontInfo.h"
#include "Layout/Margin.h"
#include "Math/Color.h"
#include "Math/Vector2D.h"
#include "Rendering/RenderingCommon.h"
#include "Templates/UniquePtr.h"
#include "Widgets/SLeafWidget.h"

struct SANDBOXUI_API FHeatmapGrid {
    int32 columns{0};
    int32 rows{0};
    TArray<float> values;
};

struct SANDBOXUI_API FHeatmapValueRange {
    float minimum{0.0f};
    float maximum{1.0f};
};

struct SANDBOXUI_API FHeatmapDomain {
    float minimum_x{0.0f};
    float maximum_x{1.0f};
    float minimum_y{0.0f};
    float maximum_y{1.0f};
};

struct SANDBOXUI_API FHeatmapColorStop {
    float position{0.0f};
    FLinearColor color{FLinearColor::White};
};

struct SANDBOXUI_API FHeatmap2DStyle {
    FHeatmap2DStyle();

    FVector2f desired_size{320.0f, 320.0f};
    FMargin chart_padding{12.0f};
    FLinearColor background_color{FLinearColor::Transparent};
    TArray<FHeatmapColorStop> color_stops;
    FLinearColor axis_color{0.5f, 0.52f, 0.56f, 0.9f};
    float axis_thickness{1.0f};
    FSlateFontInfo label_font;
    FLinearColor label_color{0.75f, 0.77f, 0.8f, 1.0f};
    float x_label_area_height{20.0f};
    float y_label_area_width{36.0f};
    bool show_axes{true};
};

class SANDBOXUI_API SHeatmap2D : public SLeafWidget {
  public:
    SLATE_BEGIN_ARGS(SHeatmap2D) {}
    SLATE_ARGUMENT(FHeatmapValueRange, ValueRange)
    SLATE_ARGUMENT(FHeatmapDomain, Domain)
    SLATE_ARGUMENT(FHeatmap2DStyle, Style)
    SLATE_END_ARGS()

    SHeatmap2D();
    ~SHeatmap2D() override;

    void Construct(FArguments const& args);

    [[nodiscard]] bool set_grid(FHeatmapGrid grid);
    void clear_grid();
    [[nodiscard]] bool set_value_range(FHeatmapValueRange range);
    [[nodiscard]] bool set_domain(FHeatmapDomain domain);
    [[nodiscard]] bool set_style(FHeatmap2DStyle style);

    auto get_grid() const -> FHeatmapGrid;
    auto get_value_range() const noexcept -> FHeatmapValueRange;
    auto get_domain() const noexcept -> FHeatmapDomain;
    auto get_style() const noexcept -> FHeatmap2DStyle const& { return style_; }

    FVector2D ComputeDesiredSize(float layout_scale_multiplier) const override;
    int32 OnPaint(FPaintArgs const& args,
                  FGeometry const& allotted_geometry,
                  FSlateRect const& culling_rect,
                  FSlateWindowElementList& out_draw_elements,
                  int32 layer_id,
                  FWidgetStyle const& widget_style,
                  bool parent_enabled) const override;
  private:
    struct FRenderCache;
    struct FData;

    static bool is_valid_style(FHeatmap2DStyle const& style);
    void invalidate_heatmap_cache(bool color_lut_changed);

    TUniquePtr<FData> data_;
    FHeatmap2DStyle style_;
    TUniquePtr<FRenderCache> render_cache_;
};
