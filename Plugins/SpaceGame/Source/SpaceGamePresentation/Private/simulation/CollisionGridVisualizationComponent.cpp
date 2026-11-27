#include "SpaceGamePresentation/simulation/CollisionGridVisualizationComponent.h"

#include <ioj/levels/collision_grid_geometry.h>

#include <SandboxCoreEngine/VectorConversion.h>

#include <Engine/EngineTypes.h>
#include <MeshElementCollector.h>
#include <PrimitiveDrawingUtils.h>
#include <PrimitiveDrawInterface.h>
#include <PrimitiveSceneProxy.h>
#include <PrimitiveViewRelevance.h>
#include <RenderingThread.h>
#include <SceneView.h>

#include <utility>

namespace {
constexpr FLinearColor entity_bounds_colour{0.f, 1.f, 0.f, 1.f};
constexpr FLinearColor static_bounds_colour{1.f, 0.4f, 0.f, 1.f};

class FCollisionGridSceneProxy final : public FPrimitiveSceneProxy {
  public:
    explicit FCollisionGridSceneProxy(ioj::UCollisionGridVisualizationComponent const& component,
                                      ioj::sim::collision::CollisionGridSpec const grid,
                                      FLinearColor const line_colour,
                                      float const line_thickness,
                                      TConstArrayView<FBox3f> const entity_bounds,
                                      TConstArrayView<FBox3f> const static_bounds,
                                      float const collision_bounds_max_draw_distance,
                                      bool const show_grid,
                                      bool const show_collision_bounds)
        : FPrimitiveSceneProxy{&component}
        , entity_bounds_{entity_bounds}
        , static_bounds_{static_bounds}
        , line_colour_{line_colour}
        , line_thickness_{line_thickness}
        , collision_bounds_max_draw_distance_{collision_bounds_max_draw_distance}
        , show_collision_bounds_{show_collision_bounds} {
        if (show_grid) {
            grid_lines_ = ioj::make_collision_grid_lines(grid);
        }
        bWillEverBeLit = false;
    }

    auto GetTypeHash() const -> SIZE_T override {
        static size_t unique_pointer;
        return reinterpret_cast<size_t>(&unique_pointer);
    }

    void GetDynamicMeshElements(TArray<FSceneView const*> const& views,
                                FSceneViewFamily const&,
                                uint32 const visibility_map,
                                FMeshElementCollector& collector) const override {
        auto const view_count{views.Num()};
        auto const maximum_line_count{grid_lines_.Num() +
                                      12 * (entity_bounds_.Num() + static_bounds_.Num())};

        for (int32 view_index{0}; view_index < view_count; ++view_index) {
            if ((visibility_map & (1u << view_index)) == 0) {
                continue;
            }

            auto const* const view{views[view_index]};
            auto* const primitive_draw_interface{collector.GetPDI(view_index)};
            auto const depth_priority_group{GetDepthPriorityGroup(view)};
            primitive_draw_interface->AddReserveLines(
                depth_priority_group, maximum_line_count, false, line_thickness_ > 0.f);

            ioj::draw_collision_grid_lines(*primitive_draw_interface,
                                           grid_lines_,
                                           line_colour_,
                                           line_thickness_,
                                           depth_priority_group);

            if (show_collision_bounds_) {
                draw_bounds(*primitive_draw_interface,
                            *view,
                            entity_bounds_,
                            entity_bounds_colour,
                            depth_priority_group);
                draw_bounds(*primitive_draw_interface,
                            *view,
                            static_bounds_,
                            static_bounds_colour,
                            depth_priority_group);
            }
        }
    }

    auto GetViewRelevance(FSceneView const* const view) const -> FPrimitiveViewRelevance override {
        FPrimitiveViewRelevance relevance;
        relevance.bDrawRelevance = IsShown(view);
        relevance.bDynamicRelevance = true;
        relevance.bShadowRelevance = false;
        relevance.bEditorPrimitiveRelevance = UseEditorCompositing(view);
        return relevance;
    }

    auto GetMemoryFootprint() const -> uint32 override {
        return sizeof(*this) + GetAllocatedSize();
    }

    auto GetAllocatedSize() const -> uint32 {
        return FPrimitiveSceneProxy::GetAllocatedSize() + grid_lines_.GetAllocatedSize() +
               entity_bounds_.GetAllocatedSize() + static_bounds_.GetAllocatedSize();
    }

    void update_entity_bounds_render_thread(TArray<FBox3f> entity_bounds) {
        check(IsInRenderingThread());
        entity_bounds_ = MoveTemp(entity_bounds);
    }
  private:
    auto is_visible(FBox3f const& bounds, FSceneView const& view) const -> bool {
        FVector const centre{bounds.GetCenter()};
        FVector const extent{bounds.GetExtent()};
        if (!view.ViewFrustum.IntersectBox(centre, extent)) {
            return false;
        }

        if (collision_bounds_max_draw_distance_ <= 0.f) {
            return true;
        }

        auto const& view_origin{view.ViewMatrices.GetViewOrigin()};
        FVector const closest_point{
            FMath::Clamp(view_origin.X,
                         static_cast<double>(bounds.Min.X),
                         static_cast<double>(bounds.Max.X)),
            FMath::Clamp(view_origin.Y,
                         static_cast<double>(bounds.Min.Y),
                         static_cast<double>(bounds.Max.Y)),
            FMath::Clamp(view_origin.Z,
                         static_cast<double>(bounds.Min.Z),
                         static_cast<double>(bounds.Max.Z)),
        };
        auto const max_distance_squared{
            FMath::Square(static_cast<double>(collision_bounds_max_draw_distance_))};
        return FVector::DistSquared(view_origin, closest_point) <= max_distance_squared;
    }

    void draw_bounds(FPrimitiveDrawInterface& primitive_draw_interface,
                     FSceneView const& view,
                     TConstArrayView<FBox3f> const bounds,
                     FLinearColor const colour,
                     uint8 const depth_priority_group) const {
        for (auto const& box : bounds) {
            if (is_visible(box, view)) {
                DrawWireBox(&primitive_draw_interface,
                            FBox{box},
                            colour,
                            depth_priority_group,
                            line_thickness_,
                            0.f,
                            true);
            }
        }
    }

    TArray<ioj::FCollisionGridLine> grid_lines_;
    TArray<FBox3f> entity_bounds_;
    TArray<FBox3f> static_bounds_;
    FLinearColor line_colour_{0.f, 1.f, 1.f, 1.f};
    float line_thickness_{1.f};
    float collision_bounds_max_draw_distance_{200000.f};
    bool show_collision_bounds_{false};
};
}

namespace ioj {
auto make_collision_grid_lines(sim::collision::CollisionGridSpec const grid)
    -> TArray<FCollisionGridLine> {
    TArray<FCollisionGridLine> lines;
    auto const min{-FVector{to_unreal(sim::collision::grid_half_size(grid))}};
    for (int axis{}; axis < 3; ++axis) {
        auto const u{(axis + 1) % 3};
        auto const v{(axis + 2) % 3};
        auto const width{grid.dimensions[u]};
        auto const height{grid.dimensions[v]};
        auto const add_line{[&](int column, int row) {
            auto start{min};
            start[u] += static_cast<double>(column) * grid.cell_dimensions[u];
            start[v] += static_cast<double>(row) * grid.cell_dimensions[v];
            auto end{start};
            end[axis] = -min[axis];
            lines.Add({start, end});
        }};
        for (int column{}; column <= width; ++column) {
            add_line(column, 0);
            add_line(column, height);
        }
        for (int row{1}; row < height; ++row) {
            add_line(0, row);
            add_line(width, row);
        }
    }
    return lines;
}
void draw_collision_grid_lines(FPrimitiveDrawInterface& pdi,
                               TConstArrayView<FCollisionGridLine> const lines,
                               FLinearColor const colour,
                               float const thickness,
                               uint8 const depth_priority) {
    for (auto const& line : lines) {
        if (colour.A < 1.f) {
            pdi.DrawTranslucentLine(
                line.start, line.end, colour, depth_priority, thickness, 0.f, true);
        } else {
            pdi.DrawLine(line.start, line.end, colour, depth_priority, thickness, 0.f, true);
        }
    }
}
UCollisionGridVisualizationComponent::UCollisionGridVisualizationComponent() {
    PrimaryComponentTick.bCanEverTick = false;
    SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SetGenerateOverlapEvents(false);
    SetCastShadow(false);
}

void UCollisionGridVisualizationComponent::configure(
    TOptional<FCollisionGridVisualizationSettings> settings) {
    if (settings) {
        grid_ = settings->grid;
        line_colour_ = settings->line_colour;
        line_thickness_ = settings->line_thickness;
    } else {
        grid_ = {};
    }
    show_grid_ = settings && settings->visibility.grid;
    show_collision_bounds_ = settings && settings->visibility.bounds;
    if (!show_collision_bounds_) {
        entity_bounds_.Reset();
    }
    UpdateBounds();

    MarkRenderStateDirty();
}

void UCollisionGridVisualizationComponent::update_entity_bounds(
    presentation::world_aabbs::ConstView const entity_aabbs) {
    if (!show_collision_bounds_) {
        return;
    }

    auto const entity_count{static_cast<int32>(entity_aabbs.min_points.num())};
    auto const entity_min_points{entity_aabbs.min_points};
    auto const entity_max_points{entity_aabbs.max_points};
    entity_bounds_.Reset();
    entity_bounds_.Reserve(entity_count);
    for (int32 i{}; i < entity_count; ++i) {
        entity_bounds_.Emplace(to_unreal(entity_min_points[i]), to_unreal(entity_max_points[i]));
    }

    MarkRenderDynamicDataDirty();
}

void UCollisionGridVisualizationComponent::set_static_bounds(
    presentation::world_aabbs::ConstView const static_aabbs) {
    auto const static_count{static_cast<int32>(static_aabbs.min_points.num())};
    auto const static_min_points{static_aabbs.min_points};
    auto const static_max_points{static_aabbs.max_points};
    static_bounds_.SetNumUninitialized(static_count, EAllowShrinking::No);
    for (int32 i{}; i < static_count; ++i) {
        static_bounds_[i] =
            FBox3f{to_unreal(static_min_points[i]), to_unreal(static_max_points[i])};
    }

    MarkRenderStateDirty();
}

void UCollisionGridVisualizationComponent::clear() {
    static_bounds_.Reset();
    configure(NullOpt);
}

auto UCollisionGridVisualizationComponent::CreateSceneProxy() -> FPrimitiveSceneProxy* {
    auto const has_valid_grid{sim::collision::is_configured(grid_)};
    if (!has_valid_grid || (!show_grid_ && !show_collision_bounds_)) {
        return nullptr;
    }

    return new FCollisionGridSceneProxy{*this,
                                        grid_,
                                        line_colour_,
                                        line_thickness_,
                                        entity_bounds_,
                                        static_bounds_,
                                        collision_bounds_max_draw_distance_,
                                        show_grid_,
                                        show_collision_bounds_};
}

auto UCollisionGridVisualizationComponent::CalcBounds(FTransform const&) const -> FBoxSphereBounds {
    FVector const half_extent{to_unreal(sim::collision::grid_half_size(grid_))};
    return FBoxSphereBounds{FVector::ZeroVector, half_extent, half_extent.Size()};
}

void UCollisionGridVisualizationComponent::SendRenderDynamicData_Concurrent() {
    Super::SendRenderDynamicData_Concurrent();

    if (SceneProxy == nullptr) {
        return;
    }

    auto* const scene_proxy{static_cast<FCollisionGridSceneProxy*>(SceneProxy)};
    TArray<FBox3f> entity_bounds_snapshot{entity_bounds_};
    ENQUEUE_RENDER_COMMAND(UpdateCollisionVisualizationBounds)
    ([scene_proxy,
      entity_bounds = MoveTemp(entity_bounds_snapshot)](FRHICommandListImmediate&) mutable {
        scene_proxy->update_entity_bounds_render_thread(MoveTemp(entity_bounds));
    });
}

} // namespace ioj
