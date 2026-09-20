#pragma once

#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Fonts/SlateFontInfo.h"
#include "Layout/Margin.h"
#include "Math/Color.h"
#include "Math/Vector2D.h"
#include "Templates/UniquePtr.h"
#include "Widgets/SLeafWidget.h"

struct SANDBOXUI_API FHistogramStyle {
    FHistogramStyle();

    FVector2f desired_size{320.0f, 200.0f};
    FMargin chart_padding{12.0f};
    float bar_gap{2.0f};
    FLinearColor bar_color{0.2f, 0.55f, 0.9f, 1.0f};
    FLinearColor hovered_bar_color{0.45f, 0.75f, 1.0f, 1.0f};
    FLinearColor background_color{0.008f, 0.01f, 0.015f, 0.9f};
    FLinearColor plot_color{0.015f, 0.02f, 0.03f, 1.0f};
    FLinearColor axis_color{0.5f, 0.52f, 0.56f, 0.9f};
    float axis_thickness{1.0f};
    FSlateFontInfo label_font;
    FLinearColor label_color{0.75f, 0.77f, 0.8f, 1.0f};
    float label_area_height{20.0f};
    FText empty_text;
};

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

    SHistogram();
    ~SHistogram() override;

    void Construct(FArguments const& args);

    void set_samples(TArray<float> samples);
    void clear_samples();
    bool set_bin_configuration(float domain_minimum, float domain_maximum, int32 bin_count);
    [[nodiscard]] bool set_style(FHistogramStyle style);

    auto get_samples() const noexcept -> TConstArrayView<float>;
    auto get_bins() const noexcept -> TConstArrayView<int32>;
    auto get_domain_minimum() const noexcept -> float;
    auto get_domain_maximum() const noexcept -> float;
    auto get_bin_count() const noexcept -> int32;
    auto get_style() const noexcept -> FHistogramStyle const& { return style_; }

    FVector2D ComputeDesiredSize(float layout_scale_multiplier) const override;
    int32 OnPaint(FPaintArgs const& args,
                  FGeometry const& allotted_geometry,
                  FSlateRect const& culling_rect,
                  FSlateWindowElementList& out_draw_elements,
                  int32 layer_id,
                  FWidgetStyle const& widget_style,
                  bool parent_enabled) const override;
    auto OnMouseMove(FGeometry const& geometry, FPointerEvent const& event) -> FReply override;
    void OnMouseLeave(FPointerEvent const& event) override;
    [[nodiscard]] auto get_hovered_bin() const noexcept -> int32 { return hovered_bin_; }
  private:
    struct FData;

    static bool is_valid_style(FHistogramStyle const& style);
    TUniquePtr<FData> data_;
    FHistogramStyle style_;
    int32 hovered_bin_{INDEX_NONE};
};
