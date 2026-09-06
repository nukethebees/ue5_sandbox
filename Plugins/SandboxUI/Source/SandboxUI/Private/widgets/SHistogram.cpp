#include "SandboxUI/widgets/SHistogram.h"

#include "Internationalization/Text.h"
#include "Rendering/DrawElementTypes.h"
#include "Styling/CoreStyle.h"

namespace {
void draw_histogram_box(FSlateWindowElementList& out_draw_elements,
                        int32 const layer_id,
                        FGeometry const& geometry,
                        FVector2f const position,
                        FVector2f const size,
                        ESlateDrawEffect const draw_effect,
                        FLinearColor const color) {
    if (size.X <= 0.0f || size.Y <= 0.0f) {
        return;
    }

    auto const paint_geometry{geometry.ToPaintGeometry(size, FSlateLayoutTransform{position})};
    FSlateDrawElement::MakeBox(out_draw_elements,
                               layer_id,
                               paint_geometry,
                               FCoreStyle::Get().GetBrush("WhiteBrush"),
                               draw_effect,
                               color);
}

void draw_histogram_label(FSlateWindowElementList& out_draw_elements,
                          int32 const layer_id,
                          FGeometry const& geometry,
                          FVector2f const position,
                          FVector2f const size,
                          FText const& label,
                          FSlateFontInfo const& font,
                          ESlateDrawEffect const draw_effect,
                          FLinearColor const color) {
    if (size.X <= 0.0f || size.Y <= 0.0f || label.IsEmpty()) {
        return;
    }

    auto const label_geometry{geometry.ToPaintGeometry(size, FSlateLayoutTransform{position})};
    out_draw_elements.PushClip(FSlateClippingZone{label_geometry});
    FSlateDrawElement::MakeText(
        out_draw_elements, layer_id, label_geometry, label, font, draw_effect, color);
    out_draw_elements.PopClip();
}

auto has_valid_domain(float const minimum, float const maximum) -> bool {
    return FMath::IsFinite(minimum) && FMath::IsFinite(maximum) && maximum > minimum;
}
}

FHistogramStyle::FHistogramStyle()
    : label_font{FCoreStyle::GetDefaultFontStyle("Regular", 8)}
    , empty_text{NSLOCTEXT("SandboxUI", "HistogramEmpty", "No interval data")} {}

auto build_histogram_bins(TConstArrayView<float> const samples,
                          float const domain_minimum,
                          float const domain_maximum,
                          int32 const bin_count) -> TArray<int32> {
    TArray<int32> bins;
    if (!has_valid_domain(domain_minimum, domain_maximum) || bin_count <= 0) {
        return bins;
    }

    bins.Init(0, bin_count);
    auto const domain_size{domain_maximum - domain_minimum};
    for (auto const sample : samples) {
        if (!FMath::IsFinite(sample) || sample < domain_minimum || sample > domain_maximum) {
            continue;
        }

        auto const bin_index{
            sample == domain_maximum
                ? bin_count - 1
                : FMath::Clamp(FMath::FloorToInt((sample - domain_minimum) / domain_size *
                                                 static_cast<float>(bin_count)),
                               0,
                               bin_count - 1)};
        ++bins[bin_index];
    }
    return bins;
}

auto maximum_histogram_bin_count(TConstArrayView<int32> const bins) -> int32 {
    int32 maximum_count{0};
    for (auto const count : bins) {
        maximum_count = FMath::Max(maximum_count, count);
    }
    return maximum_count;
}

auto build_histogram_geometry(TConstArrayView<int32> const bins,
                              FVector2f const plot_size,
                              float const bar_gap) -> FHistogramGeometry {
    FHistogramGeometry geometry;
    geometry.maximum_count = maximum_histogram_bin_count(bins);

    auto const bin_count{bins.Num()};
    auto const width{FMath::Max(plot_size.X, 0.0f)};
    auto const height{FMath::Max(plot_size.Y, 0.0f)};
    if (bin_count == 0 || width <= 0.0f || height <= 0.0f) {
        return geometry;
    }

    geometry.slot_width = width / static_cast<float>(bin_count);
    geometry.bar_width = FMath::Max(geometry.slot_width - FMath::Max(bar_gap, 0.0f), 0.0f);
    if (geometry.maximum_count <= 0 || geometry.bar_width <= 0.0f) {
        return geometry;
    }

    auto const pixels_per_sample{height / static_cast<float>(geometry.maximum_count)};
    for (int32 bin_index{0}; bin_index < bin_count; ++bin_index) {
        auto const count{bins[bin_index]};
        if (count <= 0) {
            continue;
        }

        auto const bar_height{static_cast<float>(count) * pixels_per_sample};
        auto const x{static_cast<float>(bin_index) * geometry.slot_width +
                     (geometry.slot_width - geometry.bar_width) * 0.5f};
        geometry.bars.Add({.bin_index = bin_index,
                           .count = count,
                           .position = {x, height - bar_height},
                           .size = {geometry.bar_width, bar_height}});
    }
    return geometry;
}

auto hit_test_histogram_bin(FVector2f const point,
                            FVector2f const plot_origin,
                            FVector2f const plot_size,
                            int32 const bin_count) -> int32 {
    if (bin_count <= 0 || plot_size.X <= 0.0f || plot_size.Y <= 0.0f || point.X < plot_origin.X ||
        point.Y < plot_origin.Y || point.X >= plot_origin.X + plot_size.X ||
        point.Y >= plot_origin.Y + plot_size.Y) {
        return INDEX_NONE;
    }
    return FMath::Clamp(
        FMath::FloorToInt((point.X - plot_origin.X) / plot_size.X * bin_count), 0, bin_count - 1);
}

void SHistogram::Construct(FArguments const& args) {
    style_ = args._Style;
    if (!is_valid_style(style_)) {
        style_ = FHistogramStyle{};
    }

    if (has_valid_domain(args._DomainMinimum, args._DomainMaximum) && args._BinCount > 0) {
        domain_minimum_ = args._DomainMinimum;
        domain_maximum_ = args._DomainMaximum;
        bin_count_ = args._BinCount;
    }
    rebuild_bins();
}

void SHistogram::set_samples(TArray<float> samples) {
    samples_ = MoveTemp(samples);
    rebuild_bins();
    Invalidate(EInvalidateWidgetReason::Paint);
}

void SHistogram::clear_samples() {
    if (samples_.IsEmpty()) {
        return;
    }

    samples_.Reset();
    rebuild_bins();
    Invalidate(EInvalidateWidgetReason::Paint);
}

bool SHistogram::set_bin_configuration(float const domain_minimum,
                                       float const domain_maximum,
                                       int32 const bin_count) {
    if (!has_valid_domain(domain_minimum, domain_maximum) || bin_count <= 0) {
        return false;
    }
    domain_minimum_ = domain_minimum;
    domain_maximum_ = domain_maximum;
    bin_count_ = bin_count;
    rebuild_bins();
    Invalidate(EInvalidateWidgetReason::Paint);
    return true;
}

bool SHistogram::set_style(FHistogramStyle style) {
    if (!is_valid_style(style)) {
        return false;
    }

    style_ = MoveTemp(style);
    Invalidate(EInvalidateWidgetReason::LayoutAndVolatility);
    return true;
}

FVector2D SHistogram::ComputeDesiredSize(float) const {
    return FVector2D{style_.desired_size};
}

int32 SHistogram::OnPaint(FPaintArgs const&,
                          FGeometry const& allotted_geometry,
                          FSlateRect const&,
                          FSlateWindowElementList& out_draw_elements,
                          int32 const layer_id,
                          FWidgetStyle const& widget_style,
                          bool const parent_enabled) const {
    auto const widget_size{FVector2f{allotted_geometry.GetLocalSize()}};
    auto const available_width{
        FMath::Max(widget_size.X - style_.chart_padding.Left - style_.chart_padding.Right, 0.0f)};
    auto const available_height{
        FMath::Max(widget_size.Y - style_.chart_padding.Top - style_.chart_padding.Bottom, 0.0f)};
    auto const label_height{FMath::Min(style_.label_area_height, available_height)};
    auto const plot_size{
        FVector2f{FMath::Max(available_width - style_.axis_thickness, 0.0f),
                  FMath::Max(available_height - label_height - style_.axis_thickness, 0.0f)}};
    auto const plot_origin{
        FVector2f{style_.chart_padding.Left + style_.axis_thickness, style_.chart_padding.Top}};

    auto const enabled{ShouldBeEnabled(parent_enabled)};
    auto const draw_effect{enabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect};
    auto const inherited_tint{widget_style.GetColorAndOpacityTint()};
    auto const histogram_geometry{build_histogram_geometry(bins_, plot_size, style_.bar_gap)};
    draw_histogram_box(out_draw_elements,
                       layer_id,
                       allotted_geometry,
                       FVector2f::ZeroVector,
                       widget_size,
                       draw_effect,
                       style_.background_color * inherited_tint);
    draw_histogram_box(out_draw_elements,
                       layer_id + 1,
                       allotted_geometry,
                       plot_origin,
                       plot_size,
                       draw_effect,
                       style_.plot_color * inherited_tint);
    auto const bar_layer{layer_id + 2};
    if (plot_size.X > 0.0f && plot_size.Y > 0.0f) {
        auto const clip_geometry{
            allotted_geometry.ToPaintGeometry(plot_size, FSlateLayoutTransform{plot_origin})};
        out_draw_elements.PushClip(FSlateClippingZone{clip_geometry});
        for (auto const& bar : histogram_geometry.bars) {
            draw_histogram_box(
                out_draw_elements,
                bar_layer,
                allotted_geometry,
                plot_origin + bar.position,
                bar.size,
                draw_effect,
                (bar.bin_index == hovered_bin_ ? style_.hovered_bar_color : style_.bar_color) *
                    inherited_tint);
        }
        out_draw_elements.PopClip();
    }

    auto const axis_layer{bar_layer + 1};
    auto const axis_tint{style_.axis_color * inherited_tint};
    auto const axis_origin{FVector2f{style_.chart_padding.Left, style_.chart_padding.Top}};
    draw_histogram_box(out_draw_elements,
                       axis_layer,
                       allotted_geometry,
                       axis_origin,
                       {style_.axis_thickness, plot_size.Y + style_.axis_thickness},
                       draw_effect,
                       axis_tint);
    draw_histogram_box(out_draw_elements,
                       axis_layer,
                       allotted_geometry,
                       {axis_origin.X, axis_origin.Y + plot_size.Y},
                       {plot_size.X + style_.axis_thickness, style_.axis_thickness},
                       draw_effect,
                       axis_tint);

    if (label_height <= 0.0f || plot_size.X <= 0.0f ||
        !has_valid_domain(domain_minimum_, domain_maximum_)) {
        return axis_layer;
    }

    auto const label_y{plot_origin.Y + plot_size.Y + style_.axis_thickness};
    auto const label_tint{style_.label_color * inherited_tint};
    auto const label_width{plot_size.X * 0.5f};
    draw_histogram_label(out_draw_elements,
                         axis_layer,
                         allotted_geometry,
                         {plot_origin.X, label_y},
                         {label_width, label_height},
                         FText::AsNumber(domain_minimum_),
                         style_.label_font,
                         draw_effect,
                         label_tint);
    draw_histogram_label(out_draw_elements,
                         axis_layer,
                         allotted_geometry,
                         {plot_origin.X + label_width, label_y},
                         {label_width, label_height},
                         FText::AsNumber(domain_maximum_),
                         style_.label_font,
                         draw_effect,
                         label_tint);
    draw_histogram_label(out_draw_elements,
                         axis_layer,
                         allotted_geometry,
                         {plot_origin.X + 3.0f, plot_origin.Y + 2.0f},
                         {80.0f, 16.0f},
                         FText::AsNumber(histogram_geometry.maximum_count),
                         style_.label_font,
                         draw_effect,
                         label_tint);
    draw_histogram_label(out_draw_elements,
                         axis_layer,
                         allotted_geometry,
                         {plot_origin.X + 3.0f, plot_origin.Y + plot_size.Y - 16.0f},
                         {80.0f, 16.0f},
                         FText::AsNumber(0),
                         style_.label_font,
                         draw_effect,
                         label_tint);
    if (histogram_geometry.maximum_count == 0 && !style_.empty_text.IsEmpty()) {
        draw_histogram_label(out_draw_elements,
                             axis_layer,
                             allotted_geometry,
                             plot_origin + FVector2f{8.0f, plot_size.Y * 0.5f - 8.0f},
                             {FMath::Max(0.0f, plot_size.X - 16.0f), 18.0f},
                             style_.empty_text,
                             style_.label_font,
                             draw_effect,
                             label_tint);
    }
    return axis_layer;
}

auto SHistogram::OnMouseMove(FGeometry const& geometry, FPointerEvent const& event) -> FReply {
    auto const widget_size{FVector2f{geometry.GetLocalSize()}};
    auto const available_width{
        FMath::Max(widget_size.X - style_.chart_padding.Left - style_.chart_padding.Right, 0.0f)};
    auto const available_height{
        FMath::Max(widget_size.Y - style_.chart_padding.Top - style_.chart_padding.Bottom, 0.0f)};
    auto const label_height{FMath::Min(style_.label_area_height, available_height)};
    auto const plot_size{
        FVector2f{FMath::Max(available_width - style_.axis_thickness, 0.0f),
                  FMath::Max(available_height - label_height - style_.axis_thickness, 0.0f)}};
    auto const plot_origin{
        FVector2f{style_.chart_padding.Left + style_.axis_thickness, style_.chart_padding.Top}};
    auto const local{FVector2f{geometry.AbsoluteToLocal(event.GetScreenSpacePosition())}};
    auto const hovered{hit_test_histogram_bin(local, plot_origin, plot_size, bin_count_)};
    if (hovered != hovered_bin_) {
        hovered_bin_ = hovered;
        if (hovered_bin_ != INDEX_NONE && bins_.IsValidIndex(hovered_bin_)) {
            auto const width{(domain_maximum_ - domain_minimum_) / bin_count_};
            auto const minimum{domain_minimum_ + width * hovered_bin_};
            auto const maximum{minimum + width};
            SetToolTipText(FText::FromString(FString::Printf(
                TEXT("%.5g – %.5g\n%d intervals"), minimum, maximum, bins_[hovered_bin_])));
        } else {
            SetToolTipText(FText::GetEmpty());
        }
        Invalidate(EInvalidateWidgetReason::Paint);
    }
    return FReply::Handled();
}

void SHistogram::OnMouseLeave(FPointerEvent const& event) {
    SLeafWidget::OnMouseLeave(event);
    hovered_bin_ = INDEX_NONE;
    SetToolTipText(FText::GetEmpty());
    Invalidate(EInvalidateWidgetReason::Paint);
}

bool SHistogram::is_valid_style(FHistogramStyle const& style) {
    return FMath::IsFinite(style.desired_size.X) && FMath::IsFinite(style.desired_size.Y) &&
           style.desired_size.X >= 0.0f && style.desired_size.Y >= 0.0f &&
           FMath::IsFinite(style.chart_padding.Left) && style.chart_padding.Left >= 0.0f &&
           FMath::IsFinite(style.chart_padding.Right) && style.chart_padding.Right >= 0.0f &&
           FMath::IsFinite(style.chart_padding.Top) && style.chart_padding.Top >= 0.0f &&
           FMath::IsFinite(style.chart_padding.Bottom) && style.chart_padding.Bottom >= 0.0f &&
           FMath::IsFinite(style.bar_gap) && style.bar_gap >= 0.0f &&
           FMath::IsFinite(style.axis_thickness) && style.axis_thickness > 0.0f &&
           FMath::IsFinite(style.label_area_height) && style.label_area_height >= 0.0f;
}

void SHistogram::rebuild_bins() {
    bins_ = build_histogram_bins(samples_, domain_minimum_, domain_maximum_, bin_count_);
}
