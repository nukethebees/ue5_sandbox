#include "SandboxUI/widgets/SRadar2D.h"

#include "Brushes/SlateColorBrush.h"
#include "Rendering/DrawElementTypes.h"

#include <sandbox/core/ui/radar_2d.h>

namespace {
auto to_native(FVectors2f const& positions) -> ml::ui::radar_2d::Positions {
    ml::ui::radar_2d::Positions result;
    result.xs.reserve(static_cast<std::size_t>(positions.xs.Num()));
    result.ys.reserve(static_cast<std::size_t>(positions.ys.Num()));
    for (auto const value : positions.xs) {
        result.xs.push_back(value);
    }
    for (auto const value : positions.ys) {
        result.ys.push_back(value);
    }
    return result;
}

auto to_unreal(ml::ui::radar_2d::Positions const& positions) -> FVectors2f {
    FVectors2f result;
    result.xs.Reserve(static_cast<int32>(positions.xs.size()));
    result.ys.Reserve(static_cast<int32>(positions.ys.size()));
    for (auto const value : positions.xs) {
        result.xs.Add(value);
    }
    for (auto const value : positions.ys) {
        result.ys.Add(value);
    }
    return result;
}
}

struct SRadar2D::FData {
    ml::ui::radar_2d::Data native;
};

FRadar2DContactStyle::FRadar2DContactStyle()
    : brush{FSlateColorBrush{FLinearColor::White}} {}

FRadar2DPresentation::FRadar2DPresentation()
    : background_brush{FSlateColorBrush{FLinearColor::White}} {}

SRadar2D::SRadar2D()
    : data_{MakeUnique<FData>()} {}

SRadar2D::~SRadar2D() = default;

void SRadar2D::Construct(FArguments const& args) {
    presentation_ = args._Presentation;
    if (!is_valid_presentation(presentation_)) {
        presentation_ = FRadar2DPresentation{};
    }
    if (FMath::IsFinite(args._Range) && args._Range > 0.0f &&
        args._Range != data_->native.range()) {
        static_cast<void>(data_->native.set_range(args._Range));
    }
}

bool SRadar2D::set_range(float const range) {
    if (!data_->native.set_range(range)) {
        return false;
    }
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
    std::vector<ml::ui::radar_2d::Positions> positions;
    positions.reserve(static_cast<std::size_t>(buckets.Num()));
    for (auto const& bucket : buckets) {
        if (!is_valid_style(bucket.style)) {
            return false;
        }
        auto native_positions{to_native(bucket.positions)};
        if (!native_positions.is_valid()) {
            return false;
        }
        positions.push_back(std::move(native_positions));
    }

    TArray<FRadar2DContactStyle> styles;
    styles.Reserve(buckets.Num());
    for (auto& bucket : buckets) {
        styles.Add(MoveTemp(bucket.style));
    }

    if (!data_->native.set_buckets(std::move(positions))) {
        return false;
    }
    styles_ = MoveTemp(styles);
    check_invariants();
    Invalidate(EInvalidateWidgetReason::Paint);
    return true;
}

auto SRadar2D::add_style(FRadar2DContactStyle style) -> int32 {
    if (!is_valid_style(style)) {
        return INDEX_NONE;
    }

    auto const style_index{styles_.Add(MoveTemp(style))};
    auto const bucket_index{data_->native.add_bucket()};
    check(style_index == bucket_index);
    check_invariants();
    Invalidate(EInvalidateWidgetReason::Paint);
    return style_index;
}

bool SRadar2D::set_style(int32 const style_index, FRadar2DContactStyle style) {
    check_invariants();
    if (!styles_.IsValidIndex(style_index)) {
        return false;
    }
    if (!is_valid_style(style)) {
        return false;
    }

    styles_[style_index] = MoveTemp(style);
    Invalidate(EInvalidateWidgetReason::Paint);
    return true;
}

bool SRadar2D::set_positions(int32 const style_index, FVectors2f positions) {
    check_invariants();
    if (!data_->native.set_positions(style_index, to_native(positions))) {
        return false;
    }
    check_invariants();
    Invalidate(EInvalidateWidgetReason::Paint);
    return true;
}

bool SRadar2D::clear_positions(int32 const style_index) {
    check_invariants();
    if (!data_->native.clear_positions(style_index)) {
        return false;
    }
    check_invariants();
    Invalidate(EInvalidateWidgetReason::Paint);
    return true;
}

void SRadar2D::clear_positions() {
    check_invariants();
    bool changed{false};
    for (auto const& positions : data_->native.buckets()) {
        changed |= !positions.empty();
    }
    if (changed) {
        data_->native.clear_positions();
        check_invariants();
        Invalidate(EInvalidateWidgetReason::Paint);
    }
}

void SRadar2D::clear_styles() {
    check_invariants();
    if (styles_.IsEmpty()) {
        return;
    }

    styles_.Reset();
    data_->native.clear_buckets();
    check_invariants();
    Invalidate(EInvalidateWidgetReason::Paint);
}

auto SRadar2D::get_range() const noexcept -> float {
    return data_->native.range();
}

auto SRadar2D::get_buckets() const -> TArray<FRadar2DStyleBucket> {
    check_invariants();
    auto const native_buckets{data_->native.buckets()};
    TArray<FRadar2DStyleBucket> result;
    result.Reserve(styles_.Num());
    for (int32 index{}; index < styles_.Num(); ++index) {
        result.Add({.style = styles_[index],
                    .positions = to_unreal(native_buckets[static_cast<std::size_t>(index)])});
    }
    return result;
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
    check_invariants();
    auto const widget_size{FVector2f{allotted_geometry.GetLocalSize()}};
    auto const layout{
        ml::ui::radar_2d::make_layout({widget_size.X, widget_size.Y}, data_->native.range())};
    if (layout.size.x <= 0.0f) {
        return layer_id;
    }
    auto const layout_origin{FVector2f{layout.origin.x, layout.origin.y}};
    auto const layout_size{FVector2f{layout.size.x, layout.size.y}};

    auto const enabled{ShouldBeEnabled(parent_enabled)};
    auto const draw_effect{enabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect};
    auto const inherited_tint{widget_style.GetColorAndOpacityTint()};
    auto const radar_geometry{
        allotted_geometry.ToPaintGeometry(layout_size, FSlateLayoutTransform{layout_origin})};
    FSlateDrawElement::MakeBox(out_draw_elements,
                               layer_id,
                               radar_geometry,
                               &presentation_.background_brush,
                               draw_effect,
                               presentation_.background_tint * inherited_tint);

    auto const contact_layer{layer_id + 1};
    auto const radar_max{layout_origin + layout_size};
    out_draw_elements.PushClip(FSlateClippingZone{radar_geometry});
    auto const positions_buckets{data_->native.buckets()};
    auto const bucket_count{static_cast<int32>(positions_buckets.size())};
    for (int32 bucket_index{}; bucket_index < bucket_count; ++bucket_index) {
        auto const& positions{positions_buckets[bucket_index]};
        auto const contact_count{static_cast<int32>(positions.size())};
        if (contact_count == 0) {
            continue;
        }

        auto const& style{styles_[bucket_index]};
        auto const half_size{style.rendered_size * 0.5f};
        auto const tint{style.tint * inherited_tint};
        auto const* const xs{positions.xs.data()};
        auto const* const ys{positions.ys.data()};
        for (int32 contact_index{0}; contact_index < contact_count; ++contact_index) {
            auto const native_centre{
                ml::ui::radar_2d::to_local({xs[contact_index], ys[contact_index]}, layout)};
            auto const local_centre{FVector2f{native_centre.x, native_centre.y}};
            auto const top_left{local_centre - half_size};
            auto const bottom_right{local_centre + half_size};
            if (bottom_right.X <= layout_origin.X || bottom_right.Y <= layout_origin.Y ||
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
    for (auto& style : styles_) {
        style.brush.AddReferencedObjects(collector);
    }
}

FString SRadar2D::GetReferencerName() const {
    return TEXT("SRadar2D");
}

bool SRadar2D::is_valid_style(FRadar2DContactStyle const& style) {
    return ml::ui::radar_2d::is_valid_extent({style.rendered_size.X, style.rendered_size.Y});
}

bool SRadar2D::is_valid_presentation(FRadar2DPresentation const& presentation) {
    return FMath::IsFinite(presentation.desired_size.X) &&
           FMath::IsFinite(presentation.desired_size.Y) && presentation.desired_size.X >= 0.0f &&
           presentation.desired_size.Y >= 0.0f;
}

void SRadar2D::check_invariants() const {
    auto const buckets{data_->native.buckets()};
    check(static_cast<int32>(buckets.size()) == styles_.Num());
    for (auto const& positions : buckets) {
        check(positions.is_valid());
    }
}
