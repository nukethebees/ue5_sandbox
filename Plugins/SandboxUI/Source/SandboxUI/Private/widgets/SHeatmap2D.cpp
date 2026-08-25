#include "SandboxUI/widgets/SHeatmap2D.h"

#include "Application/SlateApplicationBase.h"
#include "Internationalization/Text.h"
#include "Rendering/DrawElementTypes.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"
#include "Textures/SlateShaderResource.h"

namespace {
int32 constexpr color_lut_entry_count{256};
int32 constexpr maximum_cells_per_batch{16384};

auto is_finite_heatmap_vector(FVector2f const value) -> bool {
    return FMath::IsFinite(value.X) && FMath::IsFinite(value.Y);
}

auto is_finite_heatmap_color(FLinearColor const& color) -> bool {
    return FMath::IsFinite(color.R) && FMath::IsFinite(color.G) && FMath::IsFinite(color.B) &&
           FMath::IsFinite(color.A);
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
    };

    TArray<FLinearColor> color_lut;
    TArray<FHeatmapCellGeometry> cells;
    TArray<FHeatmapMeshBatch> local_batches;
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

auto is_valid_heatmap_grid(FHeatmapGrid const& grid) -> bool {
    if (grid.columns == 0 && grid.rows == 0) {
        return grid.values.IsEmpty();
    }
    if (grid.columns <= 0 || grid.rows <= 0) {
        return false;
    }

    auto const cell_count{static_cast<int64>(grid.columns) * static_cast<int64>(grid.rows)};
    return cell_count <= MAX_int32 && cell_count == grid.values.Num();
}

auto is_valid_heatmap_value_range(FHeatmapValueRange const& range) -> bool {
    return FMath::IsFinite(range.minimum) && FMath::IsFinite(range.maximum) &&
           range.maximum > range.minimum;
}

auto is_valid_heatmap_domain(FHeatmapDomain const& domain) -> bool {
    return FMath::IsFinite(domain.minimum_x) && FMath::IsFinite(domain.maximum_x) &&
           FMath::IsFinite(domain.minimum_y) && FMath::IsFinite(domain.maximum_y) &&
           domain.maximum_x > domain.minimum_x && domain.maximum_y > domain.minimum_y;
}

auto is_valid_heatmap_color_stops(TConstArrayView<FHeatmapColorStop> const stops) -> bool {
    if (stops.Num() < 2 || stops[0].position != 0.0f || stops.Last().position != 1.0f) {
        return false;
    }

    auto previous_position{stops[0].position};
    if (!is_finite_heatmap_color(stops[0].color)) {
        return false;
    }
    auto const stop_count{stops.Num()};
    for (int32 stop_index{1}; stop_index < stop_count; ++stop_index) {
        auto const& stop{stops[stop_index]};
        if (!FMath::IsFinite(stop.position) || stop.position <= previous_position ||
            stop.position > 1.0f || !is_finite_heatmap_color(stop.color)) {
            return false;
        }
        previous_position = stop.position;
    }
    return true;
}

auto make_heatmap_plot_layout(FVector2f const widget_size, FHeatmap2DStyle const& style)
    -> FHeatmapPlotLayout {
    auto const available_width{
        FMath::Max(widget_size.X - style.chart_padding.Left - style.chart_padding.Right, 0.0f)};
    auto const available_height{
        FMath::Max(widget_size.Y - style.chart_padding.Top - style.chart_padding.Bottom, 0.0f)};

    FHeatmapPlotLayout layout;
    if (!style.show_axes) {
        layout.plot_origin = {style.chart_padding.Left, style.chart_padding.Top};
        layout.plot_size = {available_width, available_height};
        return layout;
    }

    layout.x_label_area_height = FMath::Min(style.x_label_area_height, available_height);
    layout.y_label_area_width = FMath::Min(style.y_label_area_width, available_width);
    layout.plot_origin = {style.chart_padding.Left + layout.y_label_area_width +
                              style.axis_thickness,
                          style.chart_padding.Top};
    layout.plot_size = {
        FMath::Max(available_width - layout.y_label_area_width - style.axis_thickness, 0.0f),
        FMath::Max(available_height - layout.x_label_area_height - style.axis_thickness, 0.0f)};
    return layout;
}

auto build_heatmap_color_lut(TConstArrayView<FHeatmapColorStop> const stops,
                             int32 const entry_count) -> TArray<FLinearColor> {
    TArray<FLinearColor> color_lut;
    if (!is_valid_heatmap_color_stops(stops) || entry_count < 2) {
        return color_lut;
    }

    color_lut.Reserve(entry_count);
    int32 upper_stop_index{1};
    for (int32 entry_index{0}; entry_index < entry_count; ++entry_index) {
        auto const position{static_cast<float>(entry_index) / static_cast<float>(entry_count - 1)};
        while (upper_stop_index < stops.Num() - 1 && position > stops[upper_stop_index].position) {
            ++upper_stop_index;
        }

        auto const& lower{stops[upper_stop_index - 1]};
        auto const& upper{stops[upper_stop_index]};
        auto const alpha{(position - lower.position) / (upper.position - lower.position)};
        color_lut.Add(lower.color + (upper.color - lower.color) * alpha);
    }
    return color_lut;
}

auto build_heatmap_cell_geometry(FHeatmapGrid const& grid,
                                 FHeatmapValueRange const& range,
                                 TConstArrayView<FLinearColor> const color_lut,
                                 FVector2f const plot_size) -> TArray<FHeatmapCellGeometry> {
    TArray<FHeatmapCellGeometry> cells;
    if (!is_valid_heatmap_grid(grid) || grid.values.IsEmpty() ||
        !is_valid_heatmap_value_range(range) || color_lut.IsEmpty() ||
        !is_finite_heatmap_vector(plot_size) || plot_size.X <= 0.0f || plot_size.Y <= 0.0f) {
        return cells;
    }

    cells.Reserve(grid.values.Num());
    auto const cell_size{FVector2f{plot_size.X / static_cast<float>(grid.columns),
                                   plot_size.Y / static_cast<float>(grid.rows)}};
    auto const range_size{range.maximum - range.minimum};
    auto const lut_max_index{color_lut.Num() - 1};
    auto const cell_count{grid.values.Num()};
    for (int32 cell_index{0}; cell_index < cell_count; ++cell_index) {
        auto const value{grid.values[cell_index]};
        if (!FMath::IsFinite(value) || value <= range.minimum) {
            continue;
        }

        auto const normalized{FMath::Clamp((value - range.minimum) / range_size, 0.0f, 1.0f)};
        auto const lut_index{FMath::Clamp(
            FMath::RoundToInt(normalized * static_cast<float>(lut_max_index)), 0, lut_max_index)};
        auto const column{cell_index % grid.columns};
        auto const row{cell_index / grid.columns};
        cells.Add({.cell_index = cell_index,
                   .column = column,
                   .row = row,
                   .position = {static_cast<float>(column) * cell_size.X,
                                plot_size.Y - static_cast<float>(row + 1) * cell_size.Y},
                   .size = cell_size,
                   .color = color_lut[lut_index]});
    }
    return cells;
}

auto build_heatmap_mesh_batches(TConstArrayView<FHeatmapCellGeometry> const cells,
                                FVector2f const plot_origin) -> TArray<FHeatmapMeshBatch> {
    TArray<FHeatmapMeshBatch> batches;
    auto const cell_count{cells.Num()};
    if (cell_count == 0 || !is_finite_heatmap_vector(plot_origin)) {
        return batches;
    }

    auto const batch_count{FMath::DivideAndRoundUp(cell_count, maximum_cells_per_batch)};
    batches.Reserve(batch_count);
    for (int32 batch_index{0}; batch_index < batch_count; ++batch_index) {
        auto const first_cell{batch_index * maximum_cells_per_batch};
        auto const cells_in_batch{FMath::Min(maximum_cells_per_batch, cell_count - first_cell)};
        auto& batch{batches.AddDefaulted_GetRef()};
        batch.vertices.Reserve(cells_in_batch * 4);
        batch.indices.Reserve(cells_in_batch * 6);

        for (int32 local_cell_index{0}; local_cell_index < cells_in_batch; ++local_cell_index) {
            auto const& cell{cells[first_cell + local_cell_index]};
            auto const top_left{plot_origin + cell.position};
            auto const bottom_right{top_left + cell.size};
            auto const base_vertex{static_cast<SlateIndex>(batch.vertices.Num())};
            batch.vertices.Add(
                {.position = top_left, .texture_coordinate = {0.0f, 0.0f}, .color = cell.color});
            batch.vertices.Add({.position = {bottom_right.X, top_left.Y},
                                .texture_coordinate = {1.0f, 0.0f},
                                .color = cell.color});
            batch.vertices.Add({.position = bottom_right,
                                .texture_coordinate = {1.0f, 1.0f},
                                .color = cell.color});
            batch.vertices.Add({.position = {top_left.X, bottom_right.Y},
                                .texture_coordinate = {0.0f, 1.0f},
                                .color = cell.color});
            batch.indices.Append({base_vertex,
                                  static_cast<SlateIndex>(base_vertex + 1),
                                  static_cast<SlateIndex>(base_vertex + 2),
                                  base_vertex,
                                  static_cast<SlateIndex>(base_vertex + 2),
                                  static_cast<SlateIndex>(base_vertex + 3)});
        }
    }
    return batches;
}

SHeatmap2D::SHeatmap2D()
    : render_cache_{MakeUnique<FRenderCache>()} {}

SHeatmap2D::~SHeatmap2D() = default;

void SHeatmap2D::Construct(FArguments const& args) {
    value_range_ = args._ValueRange;
    if (!is_valid_heatmap_value_range(value_range_)) {
        value_range_ = FHeatmapValueRange{};
    }

    domain_ = args._Domain;
    if (!is_valid_heatmap_domain(domain_)) {
        domain_ = FHeatmapDomain{};
    }

    style_ = args._Style;
    if (!is_valid_style(style_)) {
        style_ = FHeatmap2DStyle{};
    }
}

bool SHeatmap2D::set_grid(FHeatmapGrid grid) {
    if (!is_valid_heatmap_grid(grid)) {
        return false;
    }

    grid_ = MoveTemp(grid);
    invalidate_heatmap_cache(false);
    Invalidate(EInvalidateWidgetReason::Paint);
    return true;
}

void SHeatmap2D::clear_grid() {
    if (grid_.values.IsEmpty()) {
        return;
    }

    grid_ = FHeatmapGrid{};
    invalidate_heatmap_cache(false);
    Invalidate(EInvalidateWidgetReason::Paint);
}

bool SHeatmap2D::set_value_range(FHeatmapValueRange const range) {
    if (!is_valid_heatmap_value_range(range)) {
        return false;
    }

    value_range_ = range;
    invalidate_heatmap_cache(false);
    Invalidate(EInvalidateWidgetReason::Paint);
    return true;
}

bool SHeatmap2D::set_domain(FHeatmapDomain const domain) {
    if (!is_valid_heatmap_domain(domain)) {
        return false;
    }

    domain_ = domain;
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
    auto const layout{make_heatmap_plot_layout(widget_size, style_)};
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
        cache.color_lut = build_heatmap_color_lut(style_.color_stops, color_lut_entry_count);
        cache.color_lut_dirty = false;
        cache.local_geometry_dirty = true;
    }
    if (cache.local_geometry_dirty || cache.plot_origin != layout.plot_origin ||
        cache.plot_size != layout.plot_size) {
        cache.cells =
            build_heatmap_cell_geometry(grid_, value_range_, cache.color_lut, layout.plot_size);
        cache.local_batches = build_heatmap_mesh_batches(cache.cells, layout.plot_origin);
        cache.plot_origin = layout.plot_origin;
        cache.plot_size = layout.plot_size;
        cache.local_geometry_dirty = false;
        cache.transformed_geometry_valid = false;
    }

    auto const heatmap_layer{layer_id + 1};
    if (!cache.local_batches.IsEmpty() && FSlateApplicationBase::IsInitialized()) {
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
                    cache.transformed_batches.SetNum(cache.local_batches.Num());
                    auto const batch_count{cache.local_batches.Num()};
                    for (int32 batch_index{0}; batch_index < batch_count; ++batch_index) {
                        auto const& local_batch{cache.local_batches[batch_index]};
                        auto& transformed_vertices{cache.transformed_batches[batch_index].vertices};
                        transformed_vertices.Reset(local_batch.vertices.Num());
                        transformed_vertices.Reserve(local_batch.vertices.Num());
                        for (auto const& vertex : local_batch.vertices) {
                            auto const texture_coordinate{resource_proxy->StartUV +
                                                          vertex.texture_coordinate *
                                                              resource_proxy->SizeUV};
                            auto const color{(vertex.color * inherited_tint).ToFColor(true)};
                            transformed_vertices.Add(
                                FSlateVertex::Make<ESlateVertexRounding::Disabled>(
                                    render_transform, vertex.position, texture_coordinate, color));
                        }
                    }
                    cache.render_transform = render_transform;
                    cache.inherited_tint = inherited_tint;
                    cache.transformed_geometry_valid = true;
                }

                auto const clip_geometry{allotted_geometry.ToPaintGeometry(
                    layout.plot_size, FSlateLayoutTransform{layout.plot_origin})};
                out_draw_elements.PushClip(FSlateClippingZone{clip_geometry});
                auto const batch_count{cache.local_batches.Num()};
                for (int32 batch_index{0}; batch_index < batch_count; ++batch_index) {
                    FSlateDrawElement::MakeCustomVerts(
                        out_draw_elements,
                        heatmap_layer,
                        resource_handle,
                        cache.transformed_batches[batch_index].vertices,
                        cache.local_batches[batch_index].indices,
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
    auto const axis_origin{
        FVector2f{layout.plot_origin.X - style_.axis_thickness, layout.plot_origin.Y}};
    draw_heatmap_box(out_draw_elements,
                     axis_layer,
                     allotted_geometry,
                     axis_origin,
                     {style_.axis_thickness, layout.plot_size.Y + style_.axis_thickness},
                     draw_effect,
                     axis_tint);
    draw_heatmap_box(out_draw_elements,
                     axis_layer,
                     allotted_geometry,
                     {axis_origin.X, axis_origin.Y + layout.plot_size.Y},
                     {layout.plot_size.X + style_.axis_thickness, style_.axis_thickness},
                     draw_effect,
                     axis_tint);

    auto const text_layer{axis_layer + 1};
    auto const label_tint{style_.label_color * inherited_tint};
    auto const x_label_y{layout.plot_origin.Y + layout.plot_size.Y + style_.axis_thickness};
    auto const half_plot_width{layout.plot_size.X * 0.5f};
    draw_heatmap_label(out_draw_elements,
                       text_layer,
                       allotted_geometry,
                       {layout.plot_origin.X, x_label_y},
                       {half_plot_width, layout.x_label_area_height},
                       FText::AsNumber(domain_.minimum_x),
                       style_.label_font,
                       draw_effect,
                       label_tint);
    draw_heatmap_label(out_draw_elements,
                       text_layer,
                       allotted_geometry,
                       {layout.plot_origin.X + half_plot_width, x_label_y},
                       {half_plot_width, layout.x_label_area_height},
                       FText::AsNumber(domain_.maximum_x),
                       style_.label_font,
                       draw_effect,
                       label_tint);

    auto const half_plot_height{layout.plot_size.Y * 0.5f};
    auto const y_label_x{style_.chart_padding.Left};
    draw_heatmap_label(out_draw_elements,
                       text_layer,
                       allotted_geometry,
                       {y_label_x, layout.plot_origin.Y},
                       {layout.y_label_area_width, half_plot_height},
                       FText::AsNumber(domain_.maximum_y),
                       style_.label_font,
                       draw_effect,
                       label_tint);
    draw_heatmap_label(out_draw_elements,
                       text_layer,
                       allotted_geometry,
                       {y_label_x, layout.plot_origin.Y + half_plot_height},
                       {layout.y_label_area_width, half_plot_height},
                       FText::AsNumber(domain_.minimum_y),
                       style_.label_font,
                       draw_effect,
                       label_tint);
    return text_layer;
}

bool SHeatmap2D::is_valid_style(FHeatmap2DStyle const& style) {
    return is_finite_heatmap_vector(style.desired_size) && style.desired_size.X >= 0.0f &&
           style.desired_size.Y >= 0.0f && FMath::IsFinite(style.chart_padding.Left) &&
           style.chart_padding.Left >= 0.0f && FMath::IsFinite(style.chart_padding.Right) &&
           style.chart_padding.Right >= 0.0f && FMath::IsFinite(style.chart_padding.Top) &&
           style.chart_padding.Top >= 0.0f && FMath::IsFinite(style.chart_padding.Bottom) &&
           style.chart_padding.Bottom >= 0.0f && is_finite_heatmap_color(style.background_color) &&
           is_valid_heatmap_color_stops(style.color_stops) &&
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
