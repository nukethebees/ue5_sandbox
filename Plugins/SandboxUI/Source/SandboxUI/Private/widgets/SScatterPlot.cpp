#include "SandboxUI/widgets/SScatterPlot.h"

#include "Internationalization/Text.h"
#include "Rendering/DrawElementTypes.h"
#include "Styling/CoreStyle.h"

namespace {
void draw_scatter_plot_box(FSlateWindowElementList& out_draw_elements,
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

void draw_scatter_plot_label(FSlateWindowElementList& out_draw_elements,
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

auto is_finite_scatter_plot_vector(FVector2f const value) -> bool {
    return FMath::IsFinite(value.X) && FMath::IsFinite(value.Y);
}
}

FScatterPlotStyle::FScatterPlotStyle()
    : label_font{FCoreStyle::GetDefaultFontStyle("Regular", 8)} {}

auto is_valid_scatter_plot_domain(FScatterPlotDomain const& domain) -> bool {
    return FMath::IsFinite(domain.minimum_x) && FMath::IsFinite(domain.maximum_x) &&
           FMath::IsFinite(domain.minimum_y) && FMath::IsFinite(domain.maximum_y) &&
           domain.maximum_x > domain.minimum_x && domain.maximum_y > domain.minimum_y;
}

auto scatter_plot_to_local(FVector2f const point,
                           FScatterPlotDomain const& domain,
                           FVector2f const plot_size) -> FVector2f {
    if (!is_valid_scatter_plot_domain(domain) || !is_finite_scatter_plot_vector(point) ||
        !is_finite_scatter_plot_vector(plot_size) || plot_size.X <= 0.0f || plot_size.Y <= 0.0f) {
        return FVector2f::ZeroVector;
    }

    auto const normalized_x{(point.X - domain.minimum_x) / (domain.maximum_x - domain.minimum_x)};
    auto const normalized_y{(point.Y - domain.minimum_y) / (domain.maximum_y - domain.minimum_y)};
    return {normalized_x * plot_size.X, (1.0f - normalized_y) * plot_size.Y};
}

auto build_scatter_plot_geometry(TConstArrayView<FScatterPlotPoint> const points,
                                 FScatterPlotDomain const& domain,
                                 FVector2f const plot_size,
                                 FVector2f const point_size) -> TArray<FScatterPlotPointGeometry> {
    TArray<FScatterPlotPointGeometry> geometry;
    if (!is_valid_scatter_plot_domain(domain) || !is_finite_scatter_plot_vector(plot_size) ||
        plot_size.X <= 0.0f || plot_size.Y <= 0.0f || !is_finite_scatter_plot_vector(point_size) ||
        point_size.X <= 0.0f || point_size.Y <= 0.0f) {
        return geometry;
    }

    geometry.Reserve(points.Num());
    auto const half_point_size{point_size * 0.5f};
    auto const point_count{points.Num()};
    for (int32 point_index{0}; point_index < point_count; ++point_index) {
        auto const& point{points[point_index]};
        if (!is_finite_scatter_plot_vector(point.position) || point.position.X < domain.minimum_x ||
            point.position.X > domain.maximum_x || point.position.Y < domain.minimum_y ||
            point.position.Y > domain.maximum_y) {
            continue;
        }

        auto const local_centre{scatter_plot_to_local(point.position, domain, plot_size)};
        geometry.Add({.point_index = point_index,
                      .position = local_centre - half_point_size,
                      .size = point_size,
                      .color = point.color});
    }
    return geometry;
}

void SScatterPlot::Construct(FArguments const& args) {
    domain_ = args._Domain;
    if (!is_valid_scatter_plot_domain(domain_)) {
        domain_ = FScatterPlotDomain{};
    }

    style_ = args._Style;
    if (!is_valid_style(style_)) {
        style_ = FScatterPlotStyle{};
    }
}

void SScatterPlot::set_points(TArray<FScatterPlotPoint> points) {
    points_ = MoveTemp(points);
    Invalidate(EInvalidateWidgetReason::Paint);
}

void SScatterPlot::clear_points() {
    if (points_.IsEmpty()) {
        return;
    }

    points_.Reset();
    Invalidate(EInvalidateWidgetReason::Paint);
}

bool SScatterPlot::set_domain(FScatterPlotDomain domain) {
    if (!is_valid_scatter_plot_domain(domain)) {
        return false;
    }

    domain_ = domain;
    Invalidate(EInvalidateWidgetReason::Paint);
    return true;
}

bool SScatterPlot::set_style(FScatterPlotStyle style) {
    if (!is_valid_style(style)) {
        return false;
    }

    style_ = MoveTemp(style);
    Invalidate(EInvalidateWidgetReason::LayoutAndVolatility);
    return true;
}

FVector2D SScatterPlot::ComputeDesiredSize(float) const {
    return FVector2D{style_.desired_size};
}

int32 SScatterPlot::OnPaint(FPaintArgs const&,
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
    auto const x_label_height{FMath::Min(style_.x_label_area_height, available_height)};
    auto const y_label_width{FMath::Min(style_.y_label_area_width, available_width)};
    auto const plot_size{
        FVector2f{FMath::Max(available_width - y_label_width - style_.axis_thickness, 0.0f),
                  FMath::Max(available_height - x_label_height - style_.axis_thickness, 0.0f)}};
    auto const plot_origin{
        FVector2f{style_.chart_padding.Left + y_label_width + style_.axis_thickness,
                  style_.chart_padding.Top}};

    auto const enabled{ShouldBeEnabled(parent_enabled)};
    auto const draw_effect{enabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect};
    auto const inherited_tint{widget_style.GetColorAndOpacityTint()};
    auto const point_geometry{
        build_scatter_plot_geometry(points_, domain_, plot_size, style_.point_size)};
    auto const point_layer{layer_id};
    if (plot_size.X > 0.0f && plot_size.Y > 0.0f) {
        auto const clip_geometry{
            allotted_geometry.ToPaintGeometry(plot_size, FSlateLayoutTransform{plot_origin})};
        out_draw_elements.PushClip(FSlateClippingZone{clip_geometry});
        for (auto const& point : point_geometry) {
            draw_scatter_plot_box(out_draw_elements,
                                  point_layer,
                                  allotted_geometry,
                                  plot_origin + point.position,
                                  point.size,
                                  draw_effect,
                                  point.color * inherited_tint);
        }
        out_draw_elements.PopClip();
    }

    auto const axis_layer{point_layer + 1};
    auto const axis_tint{style_.axis_color * inherited_tint};
    auto const axis_origin{FVector2f{plot_origin.X - style_.axis_thickness, plot_origin.Y}};
    draw_scatter_plot_box(out_draw_elements,
                          axis_layer,
                          allotted_geometry,
                          axis_origin,
                          {style_.axis_thickness, plot_size.Y + style_.axis_thickness},
                          draw_effect,
                          axis_tint);
    draw_scatter_plot_box(out_draw_elements,
                          axis_layer,
                          allotted_geometry,
                          {axis_origin.X, axis_origin.Y + plot_size.Y},
                          {plot_size.X + style_.axis_thickness, style_.axis_thickness},
                          draw_effect,
                          axis_tint);

    if (!is_valid_scatter_plot_domain(domain_)) {
        return axis_layer;
    }

    auto const label_tint{style_.label_color * inherited_tint};
    auto const x_label_y{plot_origin.Y + plot_size.Y + style_.axis_thickness};
    auto const half_plot_width{plot_size.X * 0.5f};
    draw_scatter_plot_label(out_draw_elements,
                            axis_layer,
                            allotted_geometry,
                            {plot_origin.X, x_label_y},
                            {half_plot_width, x_label_height},
                            FText::AsNumber(domain_.minimum_x),
                            style_.label_font,
                            draw_effect,
                            label_tint);
    draw_scatter_plot_label(out_draw_elements,
                            axis_layer,
                            allotted_geometry,
                            {plot_origin.X + half_plot_width, x_label_y},
                            {half_plot_width, x_label_height},
                            FText::AsNumber(domain_.maximum_x),
                            style_.label_font,
                            draw_effect,
                            label_tint);

    auto const half_plot_height{plot_size.Y * 0.5f};
    auto const y_label_x{style_.chart_padding.Left};
    draw_scatter_plot_label(out_draw_elements,
                            axis_layer,
                            allotted_geometry,
                            {y_label_x, plot_origin.Y},
                            {y_label_width, half_plot_height},
                            FText::AsNumber(domain_.maximum_y),
                            style_.label_font,
                            draw_effect,
                            label_tint);
    draw_scatter_plot_label(out_draw_elements,
                            axis_layer,
                            allotted_geometry,
                            {y_label_x, plot_origin.Y + half_plot_height},
                            {y_label_width, half_plot_height},
                            FText::AsNumber(domain_.minimum_y),
                            style_.label_font,
                            draw_effect,
                            label_tint);
    return axis_layer;
}

bool SScatterPlot::is_valid_style(FScatterPlotStyle const& style) {
    return is_finite_scatter_plot_vector(style.desired_size) && style.desired_size.X >= 0.0f &&
           style.desired_size.Y >= 0.0f && FMath::IsFinite(style.chart_padding.Left) &&
           style.chart_padding.Left >= 0.0f && FMath::IsFinite(style.chart_padding.Right) &&
           style.chart_padding.Right >= 0.0f && FMath::IsFinite(style.chart_padding.Top) &&
           style.chart_padding.Top >= 0.0f && FMath::IsFinite(style.chart_padding.Bottom) &&
           style.chart_padding.Bottom >= 0.0f && is_finite_scatter_plot_vector(style.point_size) &&
           style.point_size.X > 0.0f && style.point_size.Y > 0.0f &&
           FMath::IsFinite(style.axis_thickness) && style.axis_thickness > 0.0f &&
           FMath::IsFinite(style.x_label_area_height) && style.x_label_area_height >= 0.0f &&
           FMath::IsFinite(style.y_label_area_width) && style.y_label_area_width >= 0.0f;
}
