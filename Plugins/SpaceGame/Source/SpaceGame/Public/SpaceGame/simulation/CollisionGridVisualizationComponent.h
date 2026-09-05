#pragma once

#include <Components/PrimitiveComponent.h>

#include "CollisionGridVisualizationComponent.generated.h"

struct FTestEntityRegistry;
struct FCollisionGridConfig;

namespace ml::ioj {
struct FCollisionSystem;
}

UCLASS(ClassGroup = (Rendering))
class SPACEGAME_API UCollisionGridVisualizationComponent final : public UPrimitiveComponent {
    GENERATED_BODY()
  public:
    UCollisionGridVisualizationComponent();

    void configure(FCollisionGridConfig const& config);
    void configure_collision_bounds(bool visible, float max_draw_distance);
    void update_collision_bounds(FTestEntityRegistry const& entity_registry,
                                 ml::ioj::FCollisionSystem const& collision_system);
    void clear_collision_bounds();
    void clear();

    auto CreateSceneProxy() -> FPrimitiveSceneProxy* override;
    auto CalcBounds(FTransform const& local_to_world) const -> FBoxSphereBounds override;
    void SendRenderDynamicData_Concurrent() override;
  private:
    TArray<FBox3f> entity_bounds_;
    TArray<FBox3f> static_bounds_;
    FIntVector3 grid_dimensions_{FIntVector3::ZeroValue};
    FVector3f cell_size_{FVector3f::ZeroVector};
    FLinearColor line_colour_{0.f, 1.f, 1.f, 1.f};
    float line_thickness_{1.f};
    float collision_bounds_max_draw_distance_{200000.f};
    bool show_grid_{false};
    bool show_collision_bounds_{false};
};
