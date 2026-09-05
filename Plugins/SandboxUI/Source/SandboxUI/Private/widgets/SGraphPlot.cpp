#include "SandboxUI/widgets/SGraphPlot.h"

#include "Rendering/DrawElementTypes.h"
#include "Styling/CoreStyle.h"

namespace {
void draw_graph_plot_box(FSlateWindowElementList& out_draw_elements,
                         int32 layer_id,
                         FGeometry const& geometry,
                         FVector2f position,
                         FVector2f size,
                         ESlateDrawEffect draw_effect,
                         FLinearColor color) {
    if (size.X <= 0.0f || size.Y <= 0.0f) {
        return;
    }

    auto const paint_geometry{geometry.ToPaintGeometry(size, FSlateLayoutTransform(position))};
    FSlateDrawElement::MakeBox(out_draw_elements,
                               layer_id,
                               paint_geometry,
                               FCoreStyle::Get().GetBrush("WhiteBrush"),
                               draw_effect,
                               color);
}
} // namespace

FGraphPlotStyle::FGraphPlotStyle()
    : label_font{FCoreStyle::GetDefaultFontStyle("Regular", 8)}
    , empty_text{NSLOCTEXT("SandboxUI", "GraphPlotEmpty", "No data")} {}

void SGraphPlot::Construct(FArguments const& args) {
    style_ = args._Style;
    if (!is_valid_style(style_)) {
        style_ = FGraphPlotStyle{};
    }
    (void)cache_.set_axis_settings(args._XAxis, args._YAxis);
}

void SGraphPlot::set_series(TArray<FGraphSeries> series) {
    series_ = MoveTemp(series);
    refresh_cache_series();
    Invalidate(EInvalidateWidgetReason::Paint);
}

void SGraphPlot::clear_series() {
    if (series_.IsEmpty()) {
        return;
    }

    series_.Reset();
    refresh_cache_series();
    Invalidate(EInvalidateWidgetReason::Paint);
}

bool SGraphPlot::set_axis_settings(FGraphAxisSettings const x_axis,
                                   FGraphAxisSettings const y_axis) {
    auto const changed{cache_.set_axis_settings(x_axis, y_axis)};
    if (changed) {
        Invalidate(EInvalidateWidgetReason::Paint);
    }
    return changed;
}

bool SGraphPlot::set_style(FGraphPlotStyle style) {
    if (!is_valid_style(style)) {
        return false;
    }

    style_ = MoveTemp(style);
    ticks_dirty_ = true;
    Invalidate(EInvalidateWidgetReason::LayoutAndVolatility);
    return true;
}

FVector2D SGraphPlot::ComputeDesiredSize(float) const {
    return FVector2D{style_.desired_size};
}

void SGraphPlot::update_layout(FVector2f const local_size) const {
    plot_origin_ = {style_.left_margin, style_.top_margin};
    plot_size_ = {FMath::Max(0.0f, local_size.X - style_.left_margin - style_.right_margin),
                  FMath::Max(0.0f, local_size.Y - style_.top_margin - style_.bottom_margin)};

    auto const cache_rebuilt{cache_.update(plot_size_)};
    if (cache_rebuilt || ticks_dirty_) {
        rebuild_ticks();
        ticks_dirty_ = false;
    }
}

int32 SGraphPlot::OnPaint(FPaintArgs const&,
                          FGeometry const& allotted_geometry,
                          FSlateRect const&,
                          FSlateWindowElementList& out_draw_elements,
                          int32 layer_id,
                          FWidgetStyle const& widget_style,
                          bool const parent_enabled) const {
    auto const local_size{FVector2f{allotted_geometry.GetLocalSize()}};
    update_layout(local_size);

    auto const enabled{ShouldBeEnabled(parent_enabled)};
    auto const draw_effect{enabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect};
    auto const inherited_tint{widget_style.GetColorAndOpacityTint()};
    auto const widget_geometry{
        allotted_geometry.ToPaintGeometry(local_size, FSlateLayoutTransform{})};
    out_draw_elements.PushClip(FSlateClippingZone{widget_geometry});

    draw_graph_plot_box(out_draw_elements,
                        layer_id,
                        allotted_geometry,
                        FVector2f::ZeroVector,
                        local_size,
                        draw_effect,
                        style_.background_color * inherited_tint);
    draw_graph_plot_box(out_draw_elements,
                        layer_id,
                        allotted_geometry,
                        plot_origin_,
                        plot_size_,
                        draw_effect,
                        style_.plot_color * inherited_tint);

    auto const grid_layer{layer_id + 1};
    for (auto const& tick : x_ticks_) {
        draw_graph_plot_box(out_draw_elements,
                            grid_layer,
                            allotted_geometry,
                            plot_origin_ + FVector2f{tick.position, 0.0f},
                            FVector2f{1.0f, plot_size_.Y},
                            draw_effect,
                            style_.grid_color * inherited_tint);
    }
    for (auto const& tick : y_ticks_) {
        draw_graph_plot_box(out_draw_elements,
                            grid_layer,
                            allotted_geometry,
                            plot_origin_ + FVector2f{0.0f, tick.position},
                            FVector2f{plot_size_.X, 1.0f},
                            draw_effect,
                            style_.grid_color * inherited_tint);
    }

    auto const axis_layer{grid_layer + 1};
    draw_graph_plot_box(out_draw_elements,
                        axis_layer,
                        allotted_geometry,
                        plot_origin_,
                        FVector2f{plot_size_.X, 1.0f},
                        draw_effect,
                        style_.axis_color * inherited_tint);
    draw_graph_plot_box(out_draw_elements,
                        axis_layer,
                        allotted_geometry,
                        plot_origin_ + FVector2f{0.0f, plot_size_.Y - 1.0f},
                        FVector2f{plot_size_.X, 1.0f},
                        draw_effect,
                        style_.axis_color * inherited_tint);
    draw_graph_plot_box(out_draw_elements,
                        axis_layer,
                        allotted_geometry,
                        plot_origin_,
                        FVector2f{1.0f, plot_size_.Y},
                        draw_effect,
                        style_.axis_color * inherited_tint);

    auto const x_range{cache_.get_x_range()};
    if (x_range.min < 0.0 && x_range.max > 0.0) {
        auto const x{static_cast<float>(-x_range.min / (x_range.max - x_range.min)) * plot_size_.X};
        draw_graph_plot_box(out_draw_elements,
                            axis_layer,
                            allotted_geometry,
                            plot_origin_ + FVector2f{x, 0.0f},
                            FVector2f{1.0f, plot_size_.Y},
                            draw_effect,
                            style_.axis_color * inherited_tint);
    }
    auto const y_range{cache_.get_y_range()};
    if (y_range.min < 0.0 && y_range.max > 0.0) {
        auto const y{static_cast<float>(y_range.max / (y_range.max - y_range.min)) * plot_size_.Y};
        draw_graph_plot_box(out_draw_elements,
                            axis_layer,
                            allotted_geometry,
                            plot_origin_ + FVector2f{0.0f, y},
                            FVector2f{plot_size_.X, 1.0f},
                            draw_effect,
                            style_.axis_color * inherited_tint);
    }
    draw_graph_plot_box(out_draw_elements,
                        axis_layer,
                        allotted_geometry,
                        plot_origin_ + FVector2f{plot_size_.X - 1.0f, 0.0f},
                        FVector2f{1.0f, plot_size_.Y},
                        draw_effect,
                        style_.axis_color * inherited_tint);

    auto const plot_geometry{
        allotted_geometry.ToPaintGeometry(plot_size_, FSlateLayoutTransform(plot_origin_))};
    auto const series_layer{axis_layer + 1};
    out_draw_elements.PushClip(FSlateClippingZone{plot_geometry});
    for (auto const& series : cache_.get_series()) {
        if (series.render_points.Num() >= 2) {
            FSlateDrawElement::MakeLines(out_draw_elements,
                                         series_layer,
                                         plot_geometry,
                                         series.render_points,
                                         draw_effect,
                                         series.style.color * inherited_tint,
                                         series.style.antialias,
                                         series.style.thickness);
        } else if (series.render_points.Num() == 1) {
            auto const point{series.render_points[0]};
            draw_graph_plot_box(out_draw_elements,
                                series_layer,
                                allotted_geometry,
                                plot_origin_ + point - FVector2f{1.5f, 1.5f},
                                FVector2f{3.0f, 3.0f},
                                draw_effect,
                                series.style.color * inherited_tint);
        }
    }
    out_draw_elements.PopClip();

    auto const text_layer{series_layer + 1};
    for (auto const& tick : x_ticks_) {
        auto const paint_geometry{allotted_geometry.ToPaintGeometry(
            FVector2f{48.0f, style_.bottom_margin},
            FSlateLayoutTransform{plot_origin_ +
                                  FVector2f{tick.position - 24.0f, plot_size_.Y + 3.0f}})};
        FSlateDrawElement::MakeText(out_draw_elements,
                                    text_layer,
                                    paint_geometry,
                                    tick.label,
                                    style_.label_font,
                                    draw_effect,
                                    style_.label_color * inherited_tint);
    }
    for (auto const& tick : y_ticks_) {
        auto const paint_geometry{allotted_geometry.ToPaintGeometry(
            FVector2f{style_.left_margin - 5.0f, 18.0f},
            FSlateLayoutTransform{FVector2f{2.0f, plot_origin_.Y + tick.position - 7.0f}})};
        FSlateDrawElement::MakeText(out_draw_elements,
                                    text_layer,
                                    paint_geometry,
                                    tick.label,
                                    style_.label_font,
                                    draw_effect,
                                    style_.label_color * inherited_tint);
    }

    bool has_render_points{false};
    for (auto const& series : cache_.get_series()) {
        if (!series.render_points.IsEmpty()) {
            has_render_points = true;
            break;
        }
    }

    if (has_render_points && style_.show_legend && plot_size_.X >= 100.0f &&
        plot_size_.Y >= 32.0f) {
        float legend_y{plot_origin_.Y + 4.0f};
        auto const legend_bottom{plot_origin_.Y + plot_size_.Y - 4.0f};
        for (auto const& series : cache_.get_series()) {
            if (series.name.IsEmpty()) {
                continue;
            }
            if (legend_y + 14.0f > legend_bottom) {
                break;
            }

            auto const paint_geometry{allotted_geometry.ToPaintGeometry(
                FVector2f{120.0f, 16.0f},
                FSlateLayoutTransform{FVector2f{plot_origin_.X + 6.0f, legend_y}})};
            FSlateDrawElement::MakeText(out_draw_elements,
                                        text_layer,
                                        paint_geometry,
                                        series.name,
                                        style_.label_font,
                                        draw_effect,
                                        series.style.color * inherited_tint);
            legend_y += 14.0f;
        }
    }

    if (!has_render_points && !style_.empty_text.IsEmpty() && plot_size_.X > 0.0f &&
        plot_size_.Y > 0.0f) {
        auto const empty_geometry{allotted_geometry.ToPaintGeometry(
            FVector2f{FMath::Max(plot_size_.X - 12.0f, 0.0f), 18.0f},
            FSlateLayoutTransform{plot_origin_ + FVector2f{6.0f, plot_size_.Y * 0.5f - 9.0f}})};
        FSlateDrawElement::MakeText(out_draw_elements,
                                    text_layer,
                                    empty_geometry,
                                    style_.empty_text,
                                    style_.label_font,
                                    draw_effect,
                                    style_.label_color * inherited_tint);
    }

    out_draw_elements.PopClip();

    return text_layer;
}

void SGraphPlot::rebuild_ticks() const {
    build_ticks(cache_.get_x_range(), plot_size_.X, style_.target_x_ticks, false, x_ticks_);
    build_ticks(cache_.get_y_range(), plot_size_.Y, style_.target_y_ticks, true, y_ticks_);
}

bool SGraphPlot::is_valid_style(FGraphPlotStyle const& style) {
    return FMath::IsFinite(style.desired_size.X) && FMath::IsFinite(style.desired_size.Y) &&
           style.desired_size.X >= 0.0f && style.desired_size.Y >= 0.0f &&
           FMath::IsFinite(style.left_margin) && style.left_margin >= 0.0f &&
           FMath::IsFinite(style.right_margin) && style.right_margin >= 0.0f &&
           FMath::IsFinite(style.top_margin) && style.top_margin >= 0.0f &&
           FMath::IsFinite(style.bottom_margin) && style.bottom_margin >= 0.0f &&
           style.target_x_ticks >= 0 && style.target_y_ticks >= 0;
}

void SGraphPlot::refresh_cache_series() {
    TArray<FGraphSeriesView> series_views;
    series_views.Reserve(series_.Num());
    for (auto const& series : series_) {
        series_views.Add(
            {.name = series.name, .x = series.x, .y = series.y, .style = series.style});
    }

    ++data_revision_;
    (void)cache_.set_series(series_views, data_revision_);
}

void SGraphPlot::build_ticks(FGraphRange const range,
                             float const extent,
                             int32 const target_count,
                             bool const invert,
                             TArray<FTick>& out_ticks) {
    out_ticks.Reset();
    if (extent <= 0.0f || target_count <= 0 || range.max <= range.min) {
        return;
    }

    auto const raw_step{(range.max - range.min) / FMath::Max(1, target_count)};
    auto const exponent{FMath::FloorToDouble(FMath::LogX(10.0, raw_step))};
    auto const magnitude{FMath::Pow(10.0, exponent)};
    auto const normalised{raw_step / magnitude};
    auto const step_multiplier{normalised <= 1.0   ? 1.0
                               : normalised <= 2.0 ? 2.0
                               : normalised <= 5.0 ? 5.0
                                                   : 10.0};
    auto const step{step_multiplier * magnitude};
    auto const first{FMath::CeilToDouble(range.min / step) * step};
    auto const span{range.max - range.min};

    out_ticks.Reserve(target_count + 2);
    for (int32 i = 0; i < 64; ++i) {
        auto value{first + static_cast<double>(i) * step};
        if (value > range.max + step * 1e-6) {
            break;
        }
        if (FMath::Abs(value) < step * 1e-9) {
            value = 0.0;
        }

        auto alpha{static_cast<float>((value - range.min) / span)};
        if (invert) {
            alpha = 1.0f - alpha;
        }
        out_ticks.Add({FText::FromString(FString::Printf(TEXT("%.5g"), value)), alpha * extent});
    }
}
