#include "SandboxUI/widgets/SHeatmap2D.h"

#include "Application/SlateApplicationBase.h"
#include "Internationalization/Text.h"
#include "Rendering/DrawElementTypes.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"
#include "Textures/SlateShaderResource.h"
#include "WidgetMath.h"

namespace {
int32 constexpr color_lut_entry_count{256};

auto is_finite_heatmap_color(FLinearColor const& color) -> bool {
    return FMath::IsFinite(color.R) && FMath::IsFinite(color.G) && FMath::IsFinite(color.B) &&
           FMath::IsFinite(color.A);
}

auto to_native(FLinearColor const color) -> ml::ui::Color4f {
    return {color.R, color.G, color.B, color.A};
}

auto to_unreal(ml::ui::Vector2f const value) -> FVector2f {
    return {value.x, value.y};
}

auto native_color_stops(TConstArrayView<FHeatmapColorStop> const stops)
    -> std::vector<ml::ui::heatmap_2d::ColorStop> {
    std::vector<ml::ui::heatmap_2d::ColorStop> result;
    result.reserve(static_cast<std::size_t>(stops.Num()));
    for (auto const& stop : stops) {
        result.push_back({stop.position, to_native(stop.color)});
    }
    return result;
}

auto native_layout_settings(FHeatmap2DStyle const& style) -> ml::ui::heatmap_2d::LayoutSettings {
    return {.padding = {style.chart_padding.Left,
                        style.chart_padding.Top,
                        style.chart_padding.Right,
                        style.chart_padding.Bottom},
            .axis_thickness = style.axis_thickness,
            .x_label_area_height = style.x_label_area_height,
            .y_label_area_width = style.y_label_area_width,
            .show_axes = style.show_axes};
}

void draw_heatmap_box(FSlateWindowElementList& out_draw_elements,
                      int32 const layer_id,
                      FGeometry const& geometry,
                      FVector2f const position,
                      FVector2f const size,
                      ESlateDrawEffect const draw_effect,
                      FLinearColor const color) {
    if (size.X <= 0.0f || size.Y <= 0.0f || color.A <= 0.0f) {
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

void draw_heatmap_label(FSlateWindowElementList& out_draw_elements,
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

struct SHeatmap2D::FRenderCache {
    struct FTransformedBatch {
        TArray<FSlateVertex> vertices;
        TArray<SlateIndex> indices;
    };

    std::vector<ml::ui::Color4f> color_lut;
    std::vector<ml::ui::heatmap_2d::CellGeometry> cells;
    std::vector<ml::ui::heatmap_2d::MeshBatch> local_batches;
    TArray<FTransformedBatch> transformed_batches;
    FVector2f plot_origin{FVector2f::ZeroVector};
    FVector2f plot_size{FVector2f::ZeroVector};
    FSlateRenderTransform render_transform;
    FLinearColor inherited_tint{FLinearColor::White};
    bool color_lut_dirty{true};
    bool local_geometry_dirty{true};
    bool transformed_geometry_valid{false};
};

FHeatmap2DStyle::FHeatmap2DStyle()
    : label_font{FCoreStyle::GetDefaultFontStyle("Regular", 8)} {
    color_stops = {
        {.position = 0.0f, .color = {0.02f, 0.08f, 0.3f, 0.35f}},
        {.position = 0.33f, .color = {0.0f, 0.8f, 1.0f, 0.75f}},
        {.position = 0.66f, .color = {1.0f, 0.85f, 0.0f, 0.9f}},
        {.position = 1.0f, .color = {1.0f, 0.05f, 0.0f, 1.0f}},
    };
}

SHeatmap2D::SHeatmap2D()
    : render_cache_{MakeUnique<FRenderCache>()} {}

SHeatmap2D::~SHeatmap2D() = default;

void SHeatmap2D::Construct(FArguments const& args) {
    static_cast<void>(data_.set_value_range(args._ValueRange));
    static_cast<void>(data_.set_domain(args._Domain));

    style_ = args._Style;
    if (!is_valid_style(style_)) {
        style_ = FHeatmap2DStyle{};
    }
}

bool SHeatmap2D::set_grid(FHeatmapGrid grid) {
    if (!data_.set_grid(MoveTemp(grid))) {
        return false;
    }
    invalidate_heatmap_cache(false);
    Invalidate(EInvalidateWidgetReason::Paint);
    return true;
}

void SHeatmap2D::clear_grid() {
    if (data_.grid().values.empty()) {
        return;
    }

    data_.clear_grid();
    invalidate_heatmap_cache(false);
    Invalidate(EInvalidateWidgetReason::Paint);
}

bool SHeatmap2D::set_value_range(FHeatmapValueRange const range) {
    if (!data_.set_value_range(range)) {
        return false;
    }
    invalidate_heatmap_cache(false);
    Invalidate(EInvalidateWidgetReason::Paint);
    return true;
}

bool SHeatmap2D::set_domain(FHeatmapDomain const domain) {
    if (!data_.set_domain(domain)) {
        return false;
    }
    Invalidate(EInvalidateWidgetReason::Paint);
    return true;
}

bool SHeatmap2D::set_style(FHeatmap2DStyle style) {
    if (!is_valid_style(style)) {
        return false;
    }

    style_ = MoveTemp(style);
    invalidate_heatmap_cache(true);
    Invalidate(EInvalidateWidgetReason::LayoutAndVolatility);
    return true;
}

FVector2D SHeatmap2D::ComputeDesiredSize(float) const {
    return FVector2D{style_.desired_size};
}

int32 SHeatmap2D::OnPaint(FPaintArgs const&,
                          FGeometry const& allotted_geometry,
                          FSlateRect const&,
                          FSlateWindowElementList& out_draw_elements,
                          int32 const layer_id,
                          FWidgetStyle const& widget_style,
                          bool const parent_enabled) const {
    auto const widget_size{FVector2f{allotted_geometry.GetLocalSize()}};
    auto const layout{ml::ui::heatmap_2d::make_plot_layout({widget_size.X, widget_size.Y},
                                                           native_layout_settings(style_))};
    auto const plot_origin{to_unreal(layout.plot_origin)};
    auto const plot_size{to_unreal(layout.plot_size)};
    auto const enabled{ShouldBeEnabled(parent_enabled)};
    auto const draw_effect{enabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect};
    auto const inherited_tint{widget_style.GetColorAndOpacityTint()};

    draw_heatmap_box(out_draw_elements,
                     layer_id,
                     allotted_geometry,
                     FVector2f::ZeroVector,
                     widget_size,
                     draw_effect,
                     style_.background_color * inherited_tint);

    auto& cache{*render_cache_};
    if (cache.color_lut_dirty) {
        cache.color_lut = ml::ui::heatmap_2d::build_color_lut(
            native_color_stops(style_.color_stops), color_lut_entry_count);
        cache.color_lut_dirty = false;
        cache.local_geometry_dirty = true;
    }
    if (cache.local_geometry_dirty || cache.plot_origin != plot_origin ||
        cache.plot_size != plot_size) {
        cache.cells = ml::ui::heatmap_2d::build_cell_geometry(
            data_.grid(), data_.value_range(), cache.color_lut, layout.plot_size);
        cache.local_batches =
            ml::ui::heatmap_2d::build_mesh_batches(cache.cells, layout.plot_origin);
        cache.transformed_batches.SetNum(static_cast<int32>(cache.local_batches.size()));
        auto const batch_count{static_cast<int32>(cache.local_batches.size())};
        for (int32 batch_index{}; batch_index < batch_count; ++batch_index) {
            auto const& source_indices{cache.local_batches[batch_index].indices};
            auto& indices{cache.transformed_batches[batch_index].indices};
            indices.Reset(static_cast<int32>(source_indices.size()));
            for (auto const index : source_indices) {
                indices.Add(static_cast<SlateIndex>(index));
            }
        }
        cache.plot_origin = plot_origin;
        cache.plot_size = plot_size;
        cache.local_geometry_dirty = false;
        cache.transformed_geometry_valid = false;
    }

    auto const heatmap_layer{layer_id + 1};
    if (!cache.local_batches.empty() && FSlateApplicationBase::IsInitialized()) {
        auto const& white_brush{*FCoreStyle::Get().GetBrush("GenericWhiteBox")};
        auto* const renderer{FSlateApplicationBase::Get().GetRenderer()};
        if (ensureMsgf(renderer != nullptr, TEXT("Heatmap rendering requires a Slate renderer."))) {
            auto const resource_handle{renderer->GetResourceHandle(white_brush)};
            auto const* const resource_proxy{resource_handle.GetResourceProxy()};
            if (ensureMsgf(resource_proxy != nullptr,
                           TEXT("Heatmap rendering requires a valid white-brush resource."))) {
                auto const render_transform{allotted_geometry.GetAccumulatedRenderTransform()};
                if (!cache.transformed_geometry_valid ||
                    cache.render_transform != render_transform ||
                    cache.inherited_tint != inherited_tint) {
                    auto const batch_count{static_cast<int32>(cache.local_batches.size())};
                    for (int32 batch_index{0}; batch_index < batch_count; ++batch_index) {
                        auto const& local_batch{cache.local_batches[batch_index]};
                        auto& transformed_vertices{cache.transformed_batches[batch_index].vertices};
                        transformed_vertices.Reset(static_cast<int32>(local_batch.vertices.size()));
                        transformed_vertices.Reserve(
                            static_cast<int32>(local_batch.vertices.size()));
                        for (auto const& vertex : local_batch.vertices) {
                            auto const texture_coordinate{resource_proxy->StartUV +
                                                          to_unreal(vertex.texture_coordinate) *
                                                              resource_proxy->SizeUV};
                            auto const native_color{vertex.color};
                            auto const color{(FLinearColor{native_color.r,
                                                           native_color.g,
                                                           native_color.b,
                                                           native_color.a} *
                                              inherited_tint)
                                                 .ToFColor(true)};
                            transformed_vertices.Add(
                                FSlateVertex::Make<ESlateVertexRounding::Disabled>(
                                    render_transform,
                                    to_unreal(vertex.position),
                                    texture_coordinate,
                                    color));
                        }
                    }
                    cache.render_transform = render_transform;
                    cache.inherited_tint = inherited_tint;
                    cache.transformed_geometry_valid = true;
                }

                auto const clip_geometry{allotted_geometry.ToPaintGeometry(
                    plot_size, FSlateLayoutTransform{plot_origin})};
                out_draw_elements.PushClip(FSlateClippingZone{clip_geometry});
                auto const batch_count{static_cast<int32>(cache.local_batches.size())};
                for (int32 batch_index{0}; batch_index < batch_count; ++batch_index) {
                    FSlateDrawElement::MakeCustomVerts(
                        out_draw_elements,
                        heatmap_layer,
                        resource_handle,
                        cache.transformed_batches[batch_index].vertices,
                        cache.transformed_batches[batch_index].indices,
                        nullptr,
                        0,
                        0,
                        draw_effect);
                }
                out_draw_elements.PopClip();
            }
        }
    }

    if (!style_.show_axes) {
        return heatmap_layer;
    }

    auto const axis_layer{heatmap_layer + 1};
    auto const axis_tint{style_.axis_color * inherited_tint};
    auto const axis_origin{FVector2f{plot_origin.X - style_.axis_thickness, plot_origin.Y}};
    draw_heatmap_box(out_draw_elements,
                     axis_layer,
                     allotted_geometry,
                     axis_origin,
                     {style_.axis_thickness, plot_size.Y + style_.axis_thickness},
                     draw_effect,
                     axis_tint);
    draw_heatmap_box(out_draw_elements,
                     axis_layer,
                     allotted_geometry,
                     {axis_origin.X, axis_origin.Y + plot_size.Y},
                     {plot_size.X + style_.axis_thickness, style_.axis_thickness},
                     draw_effect,
                     axis_tint);

    auto const text_layer{axis_layer + 1};
    auto const label_tint{style_.label_color * inherited_tint};
    auto const x_label_y{plot_origin.Y + plot_size.Y + style_.axis_thickness};
    auto const half_plot_width{plot_size.X * 0.5f};
    draw_heatmap_label(out_draw_elements,
                       text_layer,
                       allotted_geometry,
                       {plot_origin.X, x_label_y},
                       {half_plot_width, layout.x_label_area_height},
                       FText::AsNumber(data_.domain().minimum_x),
                       style_.label_font,
                       draw_effect,
                       label_tint);
    draw_heatmap_label(out_draw_elements,
                       text_layer,
                       allotted_geometry,
                       {plot_origin.X + half_plot_width, x_label_y},
                       {half_plot_width, layout.x_label_area_height},
                       FText::AsNumber(data_.domain().maximum_x),
                       style_.label_font,
                       draw_effect,
                       label_tint);

    auto const half_plot_height{plot_size.Y * 0.5f};
    auto const y_label_x{style_.chart_padding.Left};
    draw_heatmap_label(out_draw_elements,
                       text_layer,
                       allotted_geometry,
                       {y_label_x, plot_origin.Y},
                       {layout.y_label_area_width, half_plot_height},
                       FText::AsNumber(data_.domain().maximum_y),
                       style_.label_font,
                       draw_effect,
                       label_tint);
    draw_heatmap_label(out_draw_elements,
                       text_layer,
                       allotted_geometry,
                       {y_label_x, plot_origin.Y + half_plot_height},
                       {layout.y_label_area_width, half_plot_height},
                       FText::AsNumber(data_.domain().minimum_y),
                       style_.label_font,
                       draw_effect,
                       label_tint);
    return text_layer;
}

bool SHeatmap2D::is_valid_style(FHeatmap2DStyle const& style) {
    return SandboxUI::Widgets::is_finite_vector(style.desired_size) &&
           style.desired_size.X >= 0.0f && style.desired_size.Y >= 0.0f &&
           FMath::IsFinite(style.chart_padding.Left) && style.chart_padding.Left >= 0.0f &&
           FMath::IsFinite(style.chart_padding.Right) && style.chart_padding.Right >= 0.0f &&
           FMath::IsFinite(style.chart_padding.Top) && style.chart_padding.Top >= 0.0f &&
           FMath::IsFinite(style.chart_padding.Bottom) && style.chart_padding.Bottom >= 0.0f &&
           is_finite_heatmap_color(style.background_color) &&
           ml::ui::heatmap_2d::is_valid_color_stops(native_color_stops(style.color_stops)) &&
           is_finite_heatmap_color(style.axis_color) && FMath::IsFinite(style.axis_thickness) &&
           style.axis_thickness > 0.0f && is_finite_heatmap_color(style.label_color) &&
           FMath::IsFinite(style.x_label_area_height) && style.x_label_area_height >= 0.0f &&
           FMath::IsFinite(style.y_label_area_width) && style.y_label_area_width >= 0.0f;
}

void SHeatmap2D::invalidate_heatmap_cache(bool const color_lut_changed) {
    render_cache_->color_lut_dirty |= color_lut_changed;
    render_cache_->local_geometry_dirty = true;
    render_cache_->transformed_geometry_valid = false;
}
