#include "SpaceGame/simulation/CollisionGridVisualizationComponent.h"

#include <SpaceGame/entities/TestEntityRegistry.h>
#include <SpaceGame/simulation/CollisionSystem.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>

#include <Engine/EngineTypes.h>
#include <MeshElementCollector.h>
#include <PrimitiveDrawInterface.h>
#include <PrimitiveSceneProxy.h>
#include <PrimitiveViewRelevance.h>
#include <RenderingThread.h>
#include <SceneView.h>

#include <utility>

namespace {
constexpr FLinearColor entity_bounds_colour{0.f, 1.f, 0.f, 1.f};
constexpr FLinearColor static_bounds_colour{1.f, 0.4f, 0.f, 1.f};

struct FGridLine {
    FVector start{FVector::ZeroVector};
    FVector end{FVector::ZeroVector};
};

class FCollisionGridSceneProxy final : public FPrimitiveSceneProxy {
  public:
    explicit FCollisionGridSceneProxy(UCollisionGridVisualizationComponent const& component,
                                      FIntVector3 const grid_dimensions,
                                      FVector3f const cell_size,
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
            build_grid_lines(grid_dimensions, cell_size);
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

            for (auto const& line : grid_lines_) {
                draw_line(*primitive_draw_interface,
                          line.start,
                          line.end,
                          line_colour_,
                          depth_priority_group);
            }

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

    void update_collision_bounds_render_thread(TArray<FBox3f> entity_bounds,
                                               TArray<FBox3f> static_bounds) {
        check(IsInRenderingThread());
        entity_bounds_ = MoveTemp(entity_bounds);
        static_bounds_ = MoveTemp(static_bounds);
    }
  private:
    void draw_line(FPrimitiveDrawInterface& primitive_draw_interface,
                   FVector const& start,
                   FVector const& end,
                   FLinearColor const colour,
                   uint8 const depth_priority_group) const {
        if (colour.A < 1.f) {
            primitive_draw_interface.DrawTranslucentLine(
                start, end, colour, depth_priority_group, line_thickness_, 0.f, true);
        } else {
            primitive_draw_interface.DrawLine(
                start, end, colour, depth_priority_group, line_thickness_, 0.f, true);
        }
    }

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
        static constexpr uint8 edges[12][2]{
            {0, 1},
            {1, 3},
            {3, 2},
            {2, 0},
            {4, 5},
            {5, 7},
            {7, 6},
            {6, 4},
            {0, 4},
            {1, 5},
            {2, 6},
            {3, 7},
        };

        for (auto const& box : bounds) {
            if (!is_visible(box, view)) {
                continue;
            }

            auto const& min{box.Min};
            auto const& max{box.Max};
            FVector const corners[8]{
                {min.X, min.Y, min.Z},
                {max.X, min.Y, min.Z},
                {min.X, max.Y, min.Z},
                {max.X, max.Y, min.Z},
                {min.X, min.Y, max.Z},
                {max.X, min.Y, max.Z},
                {min.X, max.Y, max.Z},
                {max.X, max.Y, max.Z},
            };
            for (auto const& edge : edges) {
                draw_line(primitive_draw_interface,
                          corners[edge[0]],
                          corners[edge[1]],
                          colour,
                          depth_priority_group);
            }
        }
    }

    void build_grid_lines(FIntVector3 const grid_dimensions, FVector3f const cell_size) {
        auto const grid_size{FVector3f{
            static_cast<float>(grid_dimensions.X) * cell_size.X,
            static_cast<float>(grid_dimensions.Y) * cell_size.Y,
            static_cast<float>(grid_dimensions.Z) * cell_size.Z,
        }};
        auto const min{-0.5f * grid_size};
        auto const max{0.5f * grid_size};

        for (int32 y{0}; y <= grid_dimensions.Y; ++y) {
            for (int32 z{0}; z <= grid_dimensions.Z; ++z) {
                auto const on_surface{y == 0 || y == grid_dimensions.Y || z == 0 ||
                                      z == grid_dimensions.Z};
                if (!on_surface) {
                    continue;
                }

                auto const y_position{min.Y + static_cast<float>(y) * cell_size.Y};
                auto const z_position{min.Z + static_cast<float>(z) * cell_size.Z};
                grid_lines_.Emplace(FGridLine{
                    .start = FVector{min.X, y_position, z_position},
                    .end = FVector{max.X, y_position, z_position},
                });
            }
        }

        for (int32 x{0}; x <= grid_dimensions.X; ++x) {
            for (int32 z{0}; z <= grid_dimensions.Z; ++z) {
                auto const on_surface{x == 0 || x == grid_dimensions.X || z == 0 ||
                                      z == grid_dimensions.Z};
                if (!on_surface) {
                    continue;
                }

                auto const x_position{min.X + static_cast<float>(x) * cell_size.X};
                auto const z_position{min.Z + static_cast<float>(z) * cell_size.Z};
                grid_lines_.Emplace(FGridLine{
                    .start = FVector{x_position, min.Y, z_position},
                    .end = FVector{x_position, max.Y, z_position},
                });
            }
        }

        for (int32 x{0}; x <= grid_dimensions.X; ++x) {
            for (int32 y{0}; y <= grid_dimensions.Y; ++y) {
                auto const on_surface{x == 0 || x == grid_dimensions.X || y == 0 ||
                                      y == grid_dimensions.Y};
                if (!on_surface) {
                    continue;
                }

                auto const x_position{min.X + static_cast<float>(x) * cell_size.X};
                auto const y_position{min.Y + static_cast<float>(y) * cell_size.Y};
                grid_lines_.Emplace(FGridLine{
                    .start = FVector{x_position, y_position, min.Z},
                    .end = FVector{x_position, y_position, max.Z},
                });
            }
        }
    }

    TArray<FGridLine> grid_lines_;
    TArray<FBox3f> entity_bounds_;
    TArray<FBox3f> static_bounds_;
    FLinearColor line_colour_{0.f, 1.f, 1.f, 1.f};
    float line_thickness_{1.f};
    float collision_bounds_max_draw_distance_{200000.f};
    bool show_collision_bounds_{false};
};
}

UCollisionGridVisualizationComponent::UCollisionGridVisualizationComponent() {
    PrimaryComponentTick.bCanEverTick = false;
    SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SetGenerateOverlapEvents(false);
    SetCastShadow(false);
}

void UCollisionGridVisualizationComponent::configure(FCollisionGridConfig const& config) {
    grid_dimensions_ = config.calculate_grid_dimensions();
    cell_size_ = config.cell_size;
    line_colour_ = config.line_colour;
    line_thickness_ = config.line_thickness;
    show_grid_ = config.show_grid;

    MarkRenderStateDirty();
}

void UCollisionGridVisualizationComponent::configure_collision_bounds(
    bool const visible, float const max_draw_distance) {
    auto const clamped_max_draw_distance{FMath::Max(max_draw_distance, 0.f)};
    if (show_collision_bounds_ == visible &&
        collision_bounds_max_draw_distance_ == clamped_max_draw_distance) {
        return;
    }

    show_collision_bounds_ = visible;
    collision_bounds_max_draw_distance_ = clamped_max_draw_distance;
    MarkRenderStateDirty();
}

void UCollisionGridVisualizationComponent::update_collision_bounds(
    FTestEntityRegistry const& entity_registry, ml::ioj::FCollisionSystem const& collision_system) {
    if (!show_collision_bounds_) {
        clear_collision_bounds();
        return;
    }

    auto const& entity_data{entity_registry.get_entity_data()};
    auto const& entity_aabbs{collision_system.get_entity_aabbs()};
    auto const entity_count{entity_data.num()};
    entity_bounds_.Reset();
    entity_bounds_.Reserve(entity_count);
    for (int32 i{}; i < entity_count; ++i) {
        if (entity_data.alive[i] == 0) {
            continue;
        }

        auto const aabb_index{std::to_underlying(entity_data.entity_types[i])};
        auto const centre{entity_data.locations[i] + entity_aabbs.get_centre(aabb_index)};
        auto const half_extent{entity_aabbs.get_half_extents(aabb_index)};
        entity_bounds_.Emplace(centre - half_extent, centre + half_extent);
    }

    auto const& static_aabbs{collision_system.get_uniform_grid().get_static_aabbs()};
    auto const static_count{static_aabbs.num()};
    static_bounds_.SetNumUninitialized(static_count, EAllowShrinking::No);
    for (int32 i{}; i < static_count; ++i) {
        static_bounds_[i] = FBox3f{static_aabbs.mins[i], static_aabbs.maxes[i]};
    }

    if (SceneProxy != nullptr) {
        MarkRenderDynamicDataDirty();
    } else if (IsRegistered() && (!entity_bounds_.IsEmpty() || !static_bounds_.IsEmpty())) {
        MarkRenderStateDirty();
    }
}

void UCollisionGridVisualizationComponent::clear_collision_bounds() {
    if (entity_bounds_.IsEmpty() && static_bounds_.IsEmpty()) {
        return;
    }

    entity_bounds_.Reset();
    static_bounds_.Reset();
    if (SceneProxy != nullptr) {
        if (show_grid_) {
            MarkRenderDynamicDataDirty();
        } else {
            MarkRenderStateDirty();
        }
    }
}

void UCollisionGridVisualizationComponent::clear() {
    entity_bounds_.Reset();
    static_bounds_.Reset();
    grid_dimensions_ = FIntVector3::ZeroValue;
    show_grid_ = false;
    show_collision_bounds_ = false;

    MarkRenderStateDirty();
}

auto UCollisionGridVisualizationComponent::CreateSceneProxy() -> FPrimitiveSceneProxy* {
    auto const has_valid_grid{grid_dimensions_.X > 0 && grid_dimensions_.Y > 0 &&
                              grid_dimensions_.Z > 0};
    auto const has_collision_bounds{show_collision_bounds_ &&
                                    (!entity_bounds_.IsEmpty() || !static_bounds_.IsEmpty())};
    if (!has_valid_grid || (!show_grid_ && !has_collision_bounds)) {
        return nullptr;
    }

    return new FCollisionGridSceneProxy{*this,
                                        grid_dimensions_,
                                        cell_size_,
                                        line_colour_,
                                        line_thickness_,
                                        entity_bounds_,
                                        static_bounds_,
                                        collision_bounds_max_draw_distance_,
                                        show_grid_,
                                        show_collision_bounds_};
}

auto UCollisionGridVisualizationComponent::CalcBounds(FTransform const&) const -> FBoxSphereBounds {
    FVector const half_extent{
        static_cast<double>(grid_dimensions_.X) * cell_size_.X * 0.5,
        static_cast<double>(grid_dimensions_.Y) * cell_size_.Y * 0.5,
        static_cast<double>(grid_dimensions_.Z) * cell_size_.Z * 0.5,
    };
    return FBoxSphereBounds{FVector::ZeroVector, half_extent, half_extent.Size()};
}

void UCollisionGridVisualizationComponent::SendRenderDynamicData_Concurrent() {
    Super::SendRenderDynamicData_Concurrent();

    if (SceneProxy == nullptr) {
        return;
    }

    auto* const scene_proxy{static_cast<FCollisionGridSceneProxy*>(SceneProxy)};
    TArray<FBox3f> entity_bounds_snapshot{entity_bounds_};
    TArray<FBox3f> static_bounds_snapshot{static_bounds_};
    ENQUEUE_RENDER_COMMAND(UpdateCollisionVisualizationBounds)
    ([scene_proxy,
      entity_bounds = MoveTemp(entity_bounds_snapshot),
      static_bounds = MoveTemp(static_bounds_snapshot)](FRHICommandListImmediate&) mutable {
        scene_proxy->update_collision_bounds_render_thread(MoveTemp(entity_bounds),
                                                           MoveTemp(static_bounds));
    });
}
