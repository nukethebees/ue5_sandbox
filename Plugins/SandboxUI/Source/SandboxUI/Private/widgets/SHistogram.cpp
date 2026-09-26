#include "SandboxUI/widgets/SHistogram.h"

#include <sandbox/core/ui/chart_layout.h>
#include <sandbox/core/ui/histogram.h>

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

}

struct SHistogram::FData {
    ml::ui::histogram::Data native;
};

FHistogramStyle::FHistogramStyle()
    : label_font{FCoreStyle::GetDefaultFontStyle("Regular", 8)}
    , empty_text{NSLOCTEXT("SandboxUI", "HistogramEmpty", "No interval data")} {}

SHistogram::SHistogram()
    : data_{MakeUnique<FData>()} {}

SHistogram::~SHistogram() = default;

void SHistogram::Construct(FArguments const& args) {
    style_ = args._Style;
    if (!is_valid_style(style_)) {
        style_ = FHistogramStyle{};
    }

    if (!data_->native.set_configuration(
            args._DomainMinimum, args._DomainMaximum, args._BinCount)) {
        verify(data_->native.set_configuration(0.0f, 1.0f, 10));
    }
}

void SHistogram::set_samples(TArray<float> samples) {
    std::vector<float> native_samples;
    native_samples.reserve(static_cast<std::size_t>(samples.Num()));
    for (auto const sample : samples) {
        native_samples.push_back(sample);
    }
    data_->native.set_samples(std::move(native_samples));
    Invalidate(EInvalidateWidgetReason::Paint);
}

void SHistogram::clear_samples() {
    if (data_->native.samples().empty()) {
        return;
    }

    data_->native.clear_samples();
    Invalidate(EInvalidateWidgetReason::Paint);
}

bool SHistogram::set_bin_configuration(float const domain_minimum,
                                       float const domain_maximum,
                                       int32 const bin_count) {
    if (!data_->native.set_configuration(domain_minimum, domain_maximum, bin_count)) {
        return false;
    }
    Invalidate(EInvalidateWidgetReason::Paint);
    return true;
}

auto SHistogram::get_samples() const noexcept -> TConstArrayView<float> {
    auto const samples{data_->native.samples()};
    return {samples.data(), static_cast<int32>(samples.size())};
}

auto SHistogram::get_bins() const noexcept -> TConstArrayView<int32> {
    auto const bins{data_->native.bins()};
    return {bins.data(), static_cast<int32>(bins.size())};
}

auto SHistogram::get_domain_minimum() const noexcept -> float {
    return data_->native.domain_minimum();
}

auto SHistogram::get_domain_maximum() const noexcept -> float {
    return data_->native.domain_maximum();
}

auto SHistogram::get_bin_count() const noexcept -> int32 {
    return data_->native.bin_count();
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
    auto const histogram_geometry{ml::ui::histogram::build_geometry(
        data_->native.bins(), {plot_size.X, plot_size.Y}, style_.bar_gap)};
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
                plot_origin + FVector2f{bar.position.x, bar.position.y},
                FVector2f{bar.size.x, bar.size.y},
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
        !ml::ui::histogram::is_valid_configuration(data_->native.domain_minimum(),
                                                   data_->native.domain_maximum(),
                                                   data_->native.bin_count())) {
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
                         FText::AsNumber(data_->native.domain_minimum()),
                         style_.label_font,
                         draw_effect,
                         label_tint);
    draw_histogram_label(out_draw_elements,
                         axis_layer,
                         allotted_geometry,
                         {plot_origin.X + label_width, label_y},
                         {label_width, label_height},
                         FText::AsNumber(data_->native.domain_maximum()),
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
    auto const local{FVector2f{geometry.AbsoluteToLocal(event.GetScreenSpacePosition())}};
    auto const native_hovered{ml::ui::histogram::hit_test_bin({local.X, local.Y},
                                                              {plot_origin.X, plot_origin.Y},
                                                              {plot_size.X, plot_size.Y},
                                                              data_->native.bin_count())};
    auto const hovered{native_hovered.value_or(INDEX_NONE)};
    if (hovered != hovered_bin_) {
        hovered_bin_ = hovered;
        auto const bins{data_->native.bins()};
        auto const range{ml::ui::histogram::bin_range(data_->native.domain_minimum(),
                                                      data_->native.domain_maximum(),
                                                      data_->native.bin_count(),
                                                      hovered_bin_)};
        if (range && hovered_bin_ < static_cast<int32>(bins.size())) {
            SetToolTipText(FText::FromString(FString::Printf(TEXT("%.5g – %.5g\n%d intervals"),
                                                             range->minimum,
                                                             range->maximum,
                                                             bins[hovered_bin_])));
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
