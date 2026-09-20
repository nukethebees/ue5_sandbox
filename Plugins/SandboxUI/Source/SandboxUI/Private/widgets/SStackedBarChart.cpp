#include "SandboxUI/widgets/SStackedBarChart.h"

#include "Rendering/DrawElementTypes.h"
#include "Styling/CoreStyle.h"

#include <sandbox/core/ui/chart_layout.h>
#include <sandbox/core/ui/stacked_bar_chart.h>

namespace {
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

struct SStackedBarChart::FData {
    ml::ui::stacked_bar_chart::Data native;
};

FStackedBarChartStyle::FStackedBarChartStyle()
    : label_font{FCoreStyle::GetDefaultFontStyle("Regular", 8)} {}

SStackedBarChart::SStackedBarChart()
    : data_{MakeUnique<FData>()} {}

SStackedBarChart::~SStackedBarChart() = default;

void SStackedBarChart::Construct(FArguments const& args) {
    style_ = args._Style;
    if (!is_valid_style(style_)) {
        style_ = FStackedBarChartStyle{};
    }
}

void SStackedBarChart::set_bars(TArray<FStackedBar> bars) {
    std::vector<ml::ui::stacked_bar_chart::Bar> native_bars;
    native_bars.reserve(static_cast<std::size_t>(bars.Num()));
    TArray<FText> labels;
    labels.Reserve(bars.Num());
    TArray<TArray<FLinearColor>> segment_colors;
    segment_colors.Reserve(bars.Num());
    for (auto& bar : bars) {
        auto& values{native_bars.emplace_back()};
        values.reserve(static_cast<std::size_t>(bar.segments.Num()));
        auto& colors{segment_colors.AddDefaulted_GetRef()};
        colors.Reserve(bar.segments.Num());
        for (auto const& segment : bar.segments) {
            values.push_back(segment.value);
            colors.Add(segment.color);
        }
        labels.Add(MoveTemp(bar.label));
    }

    data_->native.set_bars(std::move(native_bars));
    labels_ = MoveTemp(labels);
    segment_colors_ = MoveTemp(segment_colors);
    check_invariants();
    Invalidate(EInvalidateWidgetReason::Paint);
}

void SStackedBarChart::clear_bars() {
    check_invariants();
    if (data_->native.bars().empty()) {
        return;
    }

    data_->native.clear_bars();
    labels_.Reset();
    segment_colors_.Reset();
    check_invariants();
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

auto SStackedBarChart::get_bars() const -> TArray<FStackedBar> {
    check_invariants();
    auto const native_bars{data_->native.bars()};
    TArray<FStackedBar> result;
    result.Reserve(labels_.Num());
    for (int32 bar_index{}; bar_index < labels_.Num(); ++bar_index) {
        auto& bar{result.AddDefaulted_GetRef()};
        bar.label = labels_[bar_index];
        auto const& values{native_bars[static_cast<std::size_t>(bar_index)]};
        auto const& colors{segment_colors_[bar_index]};
        bar.segments.Reserve(colors.Num());
        for (int32 segment_index{}; segment_index < colors.Num(); ++segment_index) {
            bar.segments.Add({.value = values[static_cast<std::size_t>(segment_index)],
                              .color = colors[segment_index]});
        }
    }
    return result;
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
    check_invariants();
    auto const widget_size{FVector2f{allotted_geometry.GetLocalSize()}};
    auto const native_layout{
        ml::ui::chart_layout::make_layout({widget_size.X, widget_size.Y},
                                          {.padding = {style_.chart_padding.Left,
                                                       style_.chart_padding.Top,
                                                       style_.chart_padding.Right,
                                                       style_.chart_padding.Bottom},
                                           .axis_thickness = style_.axis_thickness,
                                           .label_area_height = style_.label_area_height})};
    auto const plot_size{FVector2f{native_layout.plot_size.x, native_layout.plot_size.y}};
    auto const plot_origin{FVector2f{native_layout.plot_origin.x, native_layout.plot_origin.y}};
    auto const label_height{native_layout.label_area_height};

    auto const enabled{ShouldBeEnabled(parent_enabled)};
    auto const draw_effect{enabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect};
    auto const inherited_tint{widget_style.GetColorAndOpacityTint()};
    auto const chart_geometry{ml::ui::stacked_bar_chart::build_geometry(
        data_->native.bars(), {plot_size.X, plot_size.Y}, style_.bar_gap)};
    auto const segment_layer{layer_id};
    if (plot_size.X > 0.0f && plot_size.Y > 0.0f) {
        auto const clip_geometry{
            allotted_geometry.ToPaintGeometry(plot_size, FSlateLayoutTransform{plot_origin})};
        out_draw_elements.PushClip(FSlateClippingZone{clip_geometry});
        for (auto const& segment : chart_geometry.segments) {
            draw_stacked_bar_box(out_draw_elements,
                                 segment_layer,
                                 allotted_geometry,
                                 plot_origin + FVector2f{segment.position.x, segment.position.y},
                                 FVector2f{segment.size.x, segment.size.y},
                                 draw_effect,
                                 segment_colors_[segment.bar_index][segment.segment_index] *
                                     inherited_tint);
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
    auto const bar_count{labels_.Num()};
    for (int32 bar_index{0}; bar_index < bar_count; ++bar_index) {
        auto const& label{labels_[bar_index]};
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

void SStackedBarChart::check_invariants() const {
    auto const bars{data_->native.bars()};
    check(static_cast<int32>(bars.size()) == labels_.Num());
    check(labels_.Num() == segment_colors_.Num());
    for (int32 bar_index{}; bar_index < labels_.Num(); ++bar_index) {
        check(static_cast<int32>(bars[static_cast<std::size_t>(bar_index)].size()) ==
              segment_colors_[bar_index].Num());
    }
}
