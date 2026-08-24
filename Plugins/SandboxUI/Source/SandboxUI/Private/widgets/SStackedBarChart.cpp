#include "SandboxUI/widgets/SStackedBarChart.h"

#include "Rendering/DrawElementTypes.h"
#include "Styling/CoreStyle.h"

namespace {
auto positive_value(float const value) -> float {
    return FMath::IsFinite(value) && value > 0.0f ? value : 0.0f;
}

void draw_stacked_bar_box(FSlateWindowElementList& out_draw_elements,
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
}

FStackedBarChartStyle::FStackedBarChartStyle()
    : label_font{FCoreStyle::GetDefaultFontStyle("Regular", 8)} {}

auto stacked_bar_total(FStackedBar const& bar) -> float {
    float total{0.0f};
    for (auto const& segment : bar.segments) {
        total += positive_value(segment.value);
    }
    return total;
}

auto maximum_stacked_bar_total(TConstArrayView<FStackedBar> const bars) -> float {
    float maximum_total{0.0f};
    for (auto const& bar : bars) {
        maximum_total = FMath::Max(maximum_total, stacked_bar_total(bar));
    }
    return maximum_total;
}

auto build_stacked_bar_chart_geometry(TConstArrayView<FStackedBar> const bars,
                                      FVector2f const plot_size,
                                      float const bar_gap) -> FStackedBarChartGeometry {
    FStackedBarChartGeometry geometry;
    geometry.maximum_total = maximum_stacked_bar_total(bars);

    auto const bar_count{bars.Num()};
    auto const width{FMath::Max(plot_size.X, 0.0f)};
    auto const height{FMath::Max(plot_size.Y, 0.0f)};
    if (bar_count == 0 || width <= 0.0f || height <= 0.0f) {
        return geometry;
    }

    geometry.slot_width = width / static_cast<float>(bar_count);
    geometry.bar_width = FMath::Max(geometry.slot_width - FMath::Max(bar_gap, 0.0f), 0.0f);
    if (geometry.maximum_total <= 0.0f || geometry.bar_width <= 0.0f) {
        return geometry;
    }

    auto const pixels_per_value{height / geometry.maximum_total};
    for (int32 bar_index{0}; bar_index < bar_count; ++bar_index) {
        auto const& bar{bars[bar_index]};
        auto const x{static_cast<float>(bar_index) * geometry.slot_width +
                     (geometry.slot_width - geometry.bar_width) * 0.5f};
        float segment_bottom{height};
        auto const segment_count{bar.segments.Num()};
        for (int32 segment_index{0}; segment_index < segment_count; ++segment_index) {
            auto const& segment{bar.segments[segment_index]};
            auto const value{positive_value(segment.value)};
            if (value <= 0.0f) {
                continue;
            }

            auto const unclamped_top{segment_bottom - value * pixels_per_value};
            auto const segment_top{FMath::Max(unclamped_top, 0.0f)};
            auto const segment_height{segment_bottom - segment_top};
            geometry.segments.Add({.bar_index = bar_index,
                                   .segment_index = segment_index,
                                   .position = {x, segment_top},
                                   .size = {geometry.bar_width, segment_height},
                                   .color = segment.color});
            segment_bottom = segment_top;
        }
    }
    return geometry;
}

void SStackedBarChart::Construct(FArguments const& args) {
    style_ = args._Style;
    if (!is_valid_style(style_)) {
        style_ = FStackedBarChartStyle{};
    }
}

void SStackedBarChart::set_bars(TArray<FStackedBar> bars) {
    bars_ = MoveTemp(bars);
    Invalidate(EInvalidateWidgetReason::Paint);
}

void SStackedBarChart::clear_bars() {
    if (bars_.IsEmpty()) {
        return;
    }

    bars_.Reset();
    Invalidate(EInvalidateWidgetReason::Paint);
}

bool SStackedBarChart::set_style(FStackedBarChartStyle style) {
    if (!is_valid_style(style)) {
        return false;
    }

    style_ = MoveTemp(style);
    Invalidate(EInvalidateWidgetReason::LayoutAndVolatility);
    return true;
}

FVector2D SStackedBarChart::ComputeDesiredSize(float) const {
    return FVector2D{style_.desired_size};
}

int32 SStackedBarChart::OnPaint(FPaintArgs const&,
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
    auto const chart_geometry{build_stacked_bar_chart_geometry(bars_, plot_size, style_.bar_gap)};
    auto const segment_layer{layer_id};
    if (plot_size.X > 0.0f && plot_size.Y > 0.0f) {
        auto const clip_geometry{
            allotted_geometry.ToPaintGeometry(plot_size, FSlateLayoutTransform{plot_origin})};
        out_draw_elements.PushClip(FSlateClippingZone{clip_geometry});
        for (auto const& segment : chart_geometry.segments) {
            draw_stacked_bar_box(out_draw_elements,
                                 segment_layer,
                                 allotted_geometry,
                                 plot_origin + segment.position,
                                 segment.size,
                                 draw_effect,
                                 segment.color * inherited_tint);
        }
        out_draw_elements.PopClip();
    }

    auto const axis_layer{segment_layer + 1};
    auto const axis_tint{style_.axis_color * inherited_tint};
    auto const axis_origin{FVector2f{style_.chart_padding.Left, style_.chart_padding.Top}};
    draw_stacked_bar_box(out_draw_elements,
                         axis_layer,
                         allotted_geometry,
                         axis_origin,
                         {style_.axis_thickness, plot_size.Y + style_.axis_thickness},
                         draw_effect,
                         axis_tint);
    draw_stacked_bar_box(out_draw_elements,
                         axis_layer,
                         allotted_geometry,
                         {axis_origin.X, axis_origin.Y + plot_size.Y},
                         {plot_size.X + style_.axis_thickness, style_.axis_thickness},
                         draw_effect,
                         axis_tint);

    if (label_height <= 0.0f || chart_geometry.slot_width <= 0.0f) {
        return axis_layer;
    }

    auto const label_y{plot_origin.Y + plot_size.Y + style_.axis_thickness};
    auto const label_tint{style_.label_color * inherited_tint};
    auto const bar_count{bars_.Num()};
    for (int32 bar_index{0}; bar_index < bar_count; ++bar_index) {
        auto const& label{bars_[bar_index].label};
        if (label.IsEmpty()) {
            continue;
        }

        auto const label_position{FVector2f{
            plot_origin.X + static_cast<float>(bar_index) * chart_geometry.slot_width, label_y}};
        auto const label_size{FVector2f{chart_geometry.slot_width, label_height}};
        auto const label_geometry{
            allotted_geometry.ToPaintGeometry(label_size, FSlateLayoutTransform{label_position})};
        out_draw_elements.PushClip(FSlateClippingZone{label_geometry});
        FSlateDrawElement::MakeText(out_draw_elements,
                                    axis_layer,
                                    label_geometry,
                                    label,
                                    style_.label_font,
                                    draw_effect,
                                    label_tint);
        out_draw_elements.PopClip();
    }
    return axis_layer;
}

bool SStackedBarChart::is_valid_style(FStackedBarChartStyle const& style) {
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
