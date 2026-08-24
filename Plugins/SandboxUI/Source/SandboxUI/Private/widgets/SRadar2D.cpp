#include "SandboxUI/widgets/SRadar2D.h"

#include "Brushes/SlateColorBrush.h"
#include "Rendering/DrawElementTypes.h"

FRadar2DContactStyle::FRadar2DContactStyle()
    : brush{FSlateColorBrush{FLinearColor::White}} {}

FRadar2DPresentation::FRadar2DPresentation()
    : background_brush{FSlateColorBrush{FLinearColor::White}} {}

auto make_radar_2d_layout(FVector2f const widget_size, float const range) -> FRadar2DLayout {
    auto const width{FMath::Max(widget_size.X, 0.0f)};
    auto const height{FMath::Max(widget_size.Y, 0.0f)};
    auto const side{FMath::Min(width, height)};
    auto const origin{FVector2f{(width - side) * 0.5f, (height - side) * 0.5f}};
    auto const size{FVector2f{side, side}};
    auto const centre{origin + size * 0.5f};
    auto const valid_range{FMath::IsFinite(range) && range > 0.0f};
    auto const pixels_per_unit{valid_range ? side / (range * 2.0f) : 0.0f};
    return {.origin = origin, .size = size, .centre = centre, .pixels_per_unit = pixels_per_unit};
}

auto radar_to_local(FVector2f const radar_position, FRadar2DLayout const& layout) -> FVector2f {
    return layout.centre + FVector2f{radar_position.X, -radar_position.Y} * layout.pixels_per_unit;
}

void SRadar2D::Construct(FArguments const& args) {
    presentation_ = args._Presentation;
    if (!is_valid_presentation(presentation_)) {
        presentation_ = FRadar2DPresentation{};
    }
    if (!set_range(args._Range)) {
        range_ = 1.0f;
    }
}

bool SRadar2D::set_range(float const range) {
    if (!FMath::IsFinite(range) || range <= 0.0f) {
        return false;
    }
    if (range_ == range) {
        return false;
    }

    range_ = range;
    Invalidate(EInvalidateWidgetReason::Paint);
    return true;
}

bool SRadar2D::set_presentation(FRadar2DPresentation presentation) {
    if (!is_valid_presentation(presentation)) {
        return false;
    }

    presentation_ = MoveTemp(presentation);
    Invalidate(EInvalidateWidgetReason::LayoutAndVolatility);
    return true;
}

bool SRadar2D::set_buckets(TArray<FRadar2DStyleBucket> buckets) {
    for (auto const& bucket : buckets) {
        if (!is_valid_style(bucket.style)) {
            return false;
        }
        if (!has_valid_array_sizes(bucket.positions)) {
            return false;
        }
    }

    buckets_ = MoveTemp(buckets);
    Invalidate(EInvalidateWidgetReason::Paint);
    return true;
}

auto SRadar2D::add_style(FRadar2DContactStyle style) -> int32 {
    if (!is_valid_style(style)) {
        return INDEX_NONE;
    }

    auto const style_index{buckets_.Add({.style = MoveTemp(style)})};
    Invalidate(EInvalidateWidgetReason::Paint);
    return style_index;
}

bool SRadar2D::set_style(int32 const style_index, FRadar2DContactStyle style) {
    if (!buckets_.IsValidIndex(style_index)) {
        return false;
    }
    if (!is_valid_style(style)) {
        return false;
    }

    buckets_[style_index].style = MoveTemp(style);
    Invalidate(EInvalidateWidgetReason::Paint);
    return true;
}

bool SRadar2D::set_positions(int32 const style_index, FVectors2f positions) {
    if (!buckets_.IsValidIndex(style_index)) {
        return false;
    }
    if (!has_valid_array_sizes(positions)) {
        return false;
    }

    buckets_[style_index].positions = MoveTemp(positions);
    Invalidate(EInvalidateWidgetReason::Paint);
    return true;
}

bool SRadar2D::clear_positions(int32 const style_index) {
    if (!buckets_.IsValidIndex(style_index)) {
        return false;
    }
    if (buckets_[style_index].positions.is_empty()) {
        return false;
    }

    buckets_[style_index].positions.reset();
    Invalidate(EInvalidateWidgetReason::Paint);
    return true;
}

void SRadar2D::clear_positions() {
    bool changed{false};
    for (auto& bucket : buckets_) {
        changed |= !bucket.positions.is_empty();
        bucket.positions.reset();
    }
    if (changed) {
        Invalidate(EInvalidateWidgetReason::Paint);
    }
}

void SRadar2D::clear_styles() {
    if (buckets_.IsEmpty()) {
        return;
    }

    buckets_.Reset();
    Invalidate(EInvalidateWidgetReason::Paint);
}

FVector2D SRadar2D::ComputeDesiredSize(float) const {
    return FVector2D{presentation_.desired_size};
}

int32 SRadar2D::OnPaint(FPaintArgs const&,
                        FGeometry const& allotted_geometry,
                        FSlateRect const&,
                        FSlateWindowElementList& out_draw_elements,
                        int32 const layer_id,
                        FWidgetStyle const& widget_style,
                        bool const parent_enabled) const {
    auto const widget_size{FVector2f{allotted_geometry.GetLocalSize()}};
    auto const layout{make_radar_2d_layout(widget_size, range_)};
    if (layout.size.X <= 0.0f) {
        return layer_id;
    }

    auto const enabled{ShouldBeEnabled(parent_enabled)};
    auto const draw_effect{enabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect};
    auto const inherited_tint{widget_style.GetColorAndOpacityTint()};
    auto const radar_geometry{
        allotted_geometry.ToPaintGeometry(layout.size, FSlateLayoutTransform{layout.origin})};
    FSlateDrawElement::MakeBox(out_draw_elements,
                               layer_id,
                               radar_geometry,
                               &presentation_.background_brush,
                               draw_effect,
                               presentation_.background_tint * inherited_tint);

    auto const contact_layer{layer_id + 1};
    auto const radar_max{layout.origin + layout.size};
    out_draw_elements.PushClip(FSlateClippingZone{radar_geometry});
    for (auto const& bucket : buckets_) {
        auto const contact_count{bucket.positions.num()};
        if (contact_count == 0) {
            continue;
        }

        auto const& style{bucket.style};
        auto const half_size{style.rendered_size * 0.5f};
        auto const tint{style.tint * inherited_tint};
        auto const* const xs{bucket.positions.xs.GetData()};
        auto const* const ys{bucket.positions.ys.GetData()};
        for (int32 contact_index{0}; contact_index < contact_count; ++contact_index) {
            auto const local_centre{
                radar_to_local(FVector2f{xs[contact_index], ys[contact_index]}, layout)};
            auto const top_left{local_centre - half_size};
            auto const bottom_right{local_centre + half_size};
            if (bottom_right.X <= layout.origin.X || bottom_right.Y <= layout.origin.Y ||
                top_left.X >= radar_max.X || top_left.Y >= radar_max.Y) {
                continue;
            }

            auto const contact_geometry{allotted_geometry.ToPaintGeometry(
                style.rendered_size, FSlateLayoutTransform{top_left})};
            FSlateDrawElement::MakeBox(out_draw_elements,
                                       contact_layer,
                                       contact_geometry,
                                       &style.brush,
                                       draw_effect,
                                       tint);
        }
    }
    out_draw_elements.PopClip();
    return contact_layer;
}

void SRadar2D::AddReferencedObjects(FReferenceCollector& collector) {
    presentation_.background_brush.AddReferencedObjects(collector);
    for (auto& bucket : buckets_) {
        bucket.style.brush.AddReferencedObjects(collector);
    }
}

FString SRadar2D::GetReferencerName() const {
    return TEXT("SRadar2D");
}

bool SRadar2D::is_valid_style(FRadar2DContactStyle const& style) {
    return FMath::IsFinite(style.rendered_size.X) && FMath::IsFinite(style.rendered_size.Y) &&
           style.rendered_size.X > 0.0f && style.rendered_size.Y > 0.0f;
}

bool SRadar2D::is_valid_presentation(FRadar2DPresentation const& presentation) {
    return FMath::IsFinite(presentation.desired_size.X) &&
           FMath::IsFinite(presentation.desired_size.Y) && presentation.desired_size.X >= 0.0f &&
           presentation.desired_size.Y >= 0.0f;
}

bool SRadar2D::has_valid_array_sizes(FVectors2f const& positions) {
    return positions.xs.Num() == positions.ys.Num();
}
