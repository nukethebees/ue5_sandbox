#include "SpaceGame/simulation/CollisionGridVisualizationComponent.h"

#include <SpaceGame/simulation/SpaceGameLevelConfig.h>

#include <Engine/EngineTypes.h>
#include <MeshElementCollector.h>
#include <PrimitiveDrawInterface.h>
#include <PrimitiveSceneProxy.h>
#include <PrimitiveViewRelevance.h>
#include <SceneView.h>

namespace {
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
                                      float const line_thickness)
        : FPrimitiveSceneProxy{&component}
        , line_colour_{line_colour}
        , line_thickness_{line_thickness} {
        build_lines(grid_dimensions, cell_size);
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
        auto const line_count{lines_.Num()};
        auto const translucent{line_colour_.A < 1.f};

        for (int32 view_index{0}; view_index < view_count; ++view_index) {
            if ((visibility_map & (1u << view_index)) == 0) {
                continue;
            }

            auto const* const view{views[view_index]};
            auto* const primitive_draw_interface{collector.GetPDI(view_index)};
            auto const depth_priority_group{GetDepthPriorityGroup(view)};
            primitive_draw_interface->AddReserveLines(
                depth_priority_group, line_count, false, line_thickness_ > 0.f);

            for (auto const& line : lines_) {
                if (translucent) {
                    primitive_draw_interface->DrawTranslucentLine(line.start,
                                                                  line.end,
                                                                  line_colour_,
                                                                  depth_priority_group,
                                                                  line_thickness_,
                                                                  0.f,
                                                                  true);
                } else {
                    primitive_draw_interface->DrawLine(line.start,
                                                       line.end,
                                                       line_colour_,
                                                       depth_priority_group,
                                                       line_thickness_,
                                                       0.f,
                                                       true);
                }
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
        return FPrimitiveSceneProxy::GetAllocatedSize() + lines_.GetAllocatedSize();
    }
  private:
    void build_lines(FIntVector3 const grid_dimensions, FVector3f const cell_size) {
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
                lines_.Emplace(FGridLine{
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
                lines_.Emplace(FGridLine{
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
                lines_.Emplace(FGridLine{
                    .start = FVector{x_position, y_position, min.Z},
                    .end = FVector{x_position, y_position, max.Z},
                });
            }
        }
    }

    TArray<FGridLine> lines_;
    FLinearColor line_colour_{0.f, 1.f, 1.f, 1.f};
    float line_thickness_{1.f};
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

void UCollisionGridVisualizationComponent::clear() {
    grid_dimensions_ = FIntVector3::ZeroValue;
    show_grid_ = false;

    MarkRenderStateDirty();
}

auto UCollisionGridVisualizationComponent::CreateSceneProxy() -> FPrimitiveSceneProxy* {
    if (!show_grid_ || grid_dimensions_.X <= 0 || grid_dimensions_.Y <= 0 ||
        grid_dimensions_.Z <= 0) {
        return nullptr;
    }

    return new FCollisionGridSceneProxy{
        *this, grid_dimensions_, cell_size_, line_colour_, line_thickness_};
}

auto UCollisionGridVisualizationComponent::CalcBounds(FTransform const&) const -> FBoxSphereBounds {
    FVector const half_extent{
        static_cast<double>(grid_dimensions_.X) * cell_size_.X * 0.5,
        static_cast<double>(grid_dimensions_.Y) * cell_size_.Y * 0.5,
        static_cast<double>(grid_dimensions_.Z) * cell_size_.Z * 0.5,
    };
    return FBoxSphereBounds{FVector::ZeroVector, half_extent, half_extent.Size()};
}
