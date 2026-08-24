#include "SandboxUI/widgets/SGraphPlot.h"

#include "Rendering/DrawElementTypes.h"
#include "Styling/CoreStyle.h"

namespace {
void draw_graph_plot_box(FSlateWindowElementList& out_draw_elements,
                         int32 layer_id,
                         FGeometry const& geometry,
                         FVector2f position,
                         FVector2f size,
                         FLinearColor color) {
    if (size.X <= 0.0f || size.Y <= 0.0f) {
        return;
    }

    auto const paint_geometry{geometry.ToPaintGeometry(size, FSlateLayoutTransform(position))};
    FSlateDrawElement::MakeBox(out_draw_elements,
                               layer_id,
                               paint_geometry,
                               FCoreStyle::Get().GetBrush("WhiteBrush"),
                               ESlateDrawEffect::None,
                               color);
}
} // namespace

FGraphPlotStyle::FGraphPlotStyle()
    : label_font{FCoreStyle::GetDefaultFontStyle("Regular", 8)} {}

void SGraphPlot::Construct(FArguments const& args) {
    style_ = args._Style;
    (void)cache_.set_axis_settings(args._XAxis, args._YAxis);
    SetCanTick(true);
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

void SGraphPlot::set_style(FGraphPlotStyle style) {
    style_ = MoveTemp(style);
    ticks_dirty_ = true;
    Invalidate(EInvalidateWidgetReason::LayoutAndVolatility);
}

FVector2D SGraphPlot::ComputeDesiredSize(float) const {
    return FVector2D{style_.desired_size};
}

void SGraphPlot::Tick(FGeometry const& allotted_geometry,
                      double const current_time,
                      float const delta_time) {
    SLeafWidget::Tick(allotted_geometry, current_time, delta_time);

    auto const local_size{FVector2f{allotted_geometry.GetLocalSize()}};
    plot_origin_ = {style_.left_margin, style_.top_margin};
    plot_size_ = {FMath::Max(0.0f, local_size.X - style_.left_margin - style_.right_margin),
                  FMath::Max(0.0f, local_size.Y - style_.top_margin - style_.bottom_margin)};

    auto const cache_rebuilt{cache_.update(plot_size_)};
    if (cache_rebuilt || ticks_dirty_) {
        rebuild_ticks();
        ticks_dirty_ = false;
        Invalidate(EInvalidateWidgetReason::Paint);
    }
}

int32 SGraphPlot::OnPaint(FPaintArgs const&,
                          FGeometry const& allotted_geometry,
                          FSlateRect const&,
                          FSlateWindowElementList& out_draw_elements,
                          int32 layer_id,
                          FWidgetStyle const&,
                          bool) const {
    auto const local_size{FVector2f{allotted_geometry.GetLocalSize()}};
    draw_graph_plot_box(out_draw_elements,
                        layer_id,
                        allotted_geometry,
                        FVector2f::ZeroVector,
                        local_size,
                        style_.background_color);
    draw_graph_plot_box(out_draw_elements,
                        layer_id,
                        allotted_geometry,
                        plot_origin_,
                        plot_size_,
                        style_.plot_color);

    auto const grid_layer{layer_id + 1};
    for (auto const& tick : x_ticks_) {
        draw_graph_plot_box(out_draw_elements,
                            grid_layer,
                            allotted_geometry,
                            plot_origin_ + FVector2f{tick.position, 0.0f},
                            FVector2f{1.0f, plot_size_.Y},
                            style_.grid_color);
    }
    for (auto const& tick : y_ticks_) {
        draw_graph_plot_box(out_draw_elements,
                            grid_layer,
                            allotted_geometry,
                            plot_origin_ + FVector2f{0.0f, tick.position},
                            FVector2f{plot_size_.X, 1.0f},
                            style_.grid_color);
    }

    auto const axis_layer{grid_layer + 1};
    draw_graph_plot_box(out_draw_elements,
                        axis_layer,
                        allotted_geometry,
                        plot_origin_,
                        FVector2f{plot_size_.X, 1.0f},
                        style_.axis_color);
    draw_graph_plot_box(out_draw_elements,
                        axis_layer,
                        allotted_geometry,
                        plot_origin_ + FVector2f{0.0f, plot_size_.Y - 1.0f},
                        FVector2f{plot_size_.X, 1.0f},
                        style_.axis_color);
    draw_graph_plot_box(out_draw_elements,
                        axis_layer,
                        allotted_geometry,
                        plot_origin_,
                        FVector2f{1.0f, plot_size_.Y},
                        style_.axis_color);

    auto const x_range{cache_.get_x_range()};
    if (x_range.min < 0.0 && x_range.max > 0.0) {
        auto const x{static_cast<float>(-x_range.min / (x_range.max - x_range.min)) * plot_size_.X};
        draw_graph_plot_box(out_draw_elements,
                            axis_layer,
                            allotted_geometry,
                            plot_origin_ + FVector2f{x, 0.0f},
                            FVector2f{1.0f, plot_size_.Y},
                            style_.axis_color);
    }
    auto const y_range{cache_.get_y_range()};
    if (y_range.min < 0.0 && y_range.max > 0.0) {
        auto const y{static_cast<float>(y_range.max / (y_range.max - y_range.min)) * plot_size_.Y};
        draw_graph_plot_box(out_draw_elements,
                            axis_layer,
                            allotted_geometry,
                            plot_origin_ + FVector2f{0.0f, y},
                            FVector2f{plot_size_.X, 1.0f},
                            style_.axis_color);
    }
    draw_graph_plot_box(out_draw_elements,
                        axis_layer,
                        allotted_geometry,
                        plot_origin_ + FVector2f{plot_size_.X - 1.0f, 0.0f},
                        FVector2f{1.0f, plot_size_.Y},
                        style_.axis_color);

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
                                         ESlateDrawEffect::None,
                                         series.style.color,
                                         series.style.antialias,
                                         series.style.thickness);
        } else if (series.render_points.Num() == 1) {
            auto const point{series.render_points[0]};
            draw_graph_plot_box(out_draw_elements,
                                series_layer,
                                allotted_geometry,
                                plot_origin_ + point - FVector2f{1.5f, 1.5f},
                                FVector2f{3.0f, 3.0f},
                                series.style.color);
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
                                    ESlateDrawEffect::None,
                                    style_.label_color);
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
                                    ESlateDrawEffect::None,
                                    style_.label_color);
    }

    if (style_.show_legend) {
        float legend_y{plot_origin_.Y + 4.0f};
        for (auto const& series : cache_.get_series()) {
            auto const paint_geometry{allotted_geometry.ToPaintGeometry(
                FVector2f{120.0f, 16.0f},
                FSlateLayoutTransform{FVector2f{plot_origin_.X + 6.0f, legend_y}})};
            FSlateDrawElement::MakeText(out_draw_elements,
                                        text_layer,
                                        paint_geometry,
                                        series.name,
                                        style_.label_font,
                                        ESlateDrawEffect::None,
                                        series.style.color);
            legend_y += 14.0f;
        }
    }

    return text_layer;
}

void SGraphPlot::rebuild_ticks() {
    build_ticks(cache_.get_x_range(), plot_size_.X, style_.target_x_ticks, false, x_ticks_);
    build_ticks(cache_.get_y_range(), plot_size_.Y, style_.target_y_ticks, true, y_ticks_);
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
