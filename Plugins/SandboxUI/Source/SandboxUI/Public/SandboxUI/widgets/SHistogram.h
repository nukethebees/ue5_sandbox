#pragma once

#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Fonts/SlateFontInfo.h"
#include "Layout/Margin.h"
#include "Math/Color.h"
#include "Math/Vector2D.h"
#include "Widgets/SLeafWidget.h"

struct SANDBOXUI_API FHistogramStyle {
    FHistogramStyle();

    FVector2f desired_size{320.0f, 200.0f};
    FMargin chart_padding{12.0f};
    float bar_gap{2.0f};
    FLinearColor bar_color{0.2f, 0.55f, 0.9f, 1.0f};
    FLinearColor axis_color{0.5f, 0.52f, 0.56f, 0.9f};
    float axis_thickness{1.0f};
    FSlateFontInfo label_font;
    FLinearColor label_color{0.75f, 0.77f, 0.8f, 1.0f};
    float label_area_height{20.0f};
};

struct SANDBOXUI_API FHistogramBarGeometry {
    int32 bin_index{INDEX_NONE};
    int32 count{0};
    FVector2f position{FVector2f::ZeroVector};
    FVector2f size{FVector2f::ZeroVector};
};

struct SANDBOXUI_API FHistogramGeometry {
    TArray<FHistogramBarGeometry> bars;
    int32 maximum_count{0};
    float slot_width{0.0f};
    float bar_width{0.0f};
};

SANDBOXUI_API auto build_histogram_bins(TConstArrayView<float> samples,
                                        float domain_minimum,
                                        float domain_maximum,
                                        int32 bin_count) -> TArray<int32>;
SANDBOXUI_API auto maximum_histogram_bin_count(TConstArrayView<int32> bins) -> int32;
SANDBOXUI_API auto build_histogram_geometry(TConstArrayView<int32> bins,
                                            FVector2f plot_size,
                                            float bar_gap) -> FHistogramGeometry;

class SANDBOXUI_API SHistogram : public SLeafWidget {
  public:
    SLATE_BEGIN_ARGS(SHistogram)
        : _DomainMinimum(0.0f)
        , _DomainMaximum(1.0f)
        , _BinCount(10) {}
    SLATE_ARGUMENT(FHistogramStyle, Style)
    SLATE_ARGUMENT(float, DomainMinimum)
    SLATE_ARGUMENT(float, DomainMaximum)
    SLATE_ARGUMENT(int32, BinCount)
    SLATE_END_ARGS()

    void Construct(FArguments const& args);

    void set_samples(TArray<float> samples);
    void clear_samples();
    void set_bin_configuration(float domain_minimum, float domain_maximum, int32 bin_count);
    [[nodiscard]] bool set_style(FHistogramStyle style);

    auto get_samples() const noexcept -> TConstArrayView<float> { return samples_; }
    auto get_bins() const noexcept -> TConstArrayView<int32> { return bins_; }
    auto get_domain_minimum() const noexcept -> float { return domain_minimum_; }
    auto get_domain_maximum() const noexcept -> float { return domain_maximum_; }
    auto get_bin_count() const noexcept -> int32 { return bin_count_; }
    auto get_style() const noexcept -> FHistogramStyle const& { return style_; }

    FVector2D ComputeDesiredSize(float layout_scale_multiplier) const override;
    int32 OnPaint(FPaintArgs const& args,
                  FGeometry const& allotted_geometry,
                  FSlateRect const& culling_rect,
                  FSlateWindowElementList& out_draw_elements,
                  int32 layer_id,
                  FWidgetStyle const& widget_style,
                  bool parent_enabled) const override;
  private:
    static bool is_valid_style(FHistogramStyle const& style);
    void rebuild_bins();

    TArray<float> samples_;
    TArray<int32> bins_;
    float domain_minimum_{0.0f};
    float domain_maximum_{1.0f};
    int32 bin_count_{10};
    FHistogramStyle style_;
};
