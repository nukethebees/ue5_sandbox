#pragma once

#include <Components/PrimitiveComponent.h>

#include "CollisionGridVisualizationComponent.generated.h"

namespace ioj::sim::collision {
struct CollisionSystem;
}

struct FCollisionGridVisualizationSettings {
    FIntVector3 dimensions{FIntVector3::ZeroValue};
    FVector3f cell_size{FVector3f::ZeroVector};
    FLinearColor line_colour{0.f, 1.f, 1.f, 1.f};
    float line_thickness{1.f};
    bool show_grid{false};
};

UCLASS(ClassGroup = (Rendering))
class SPACEGAMEPRESENTATION_API UCollisionGridVisualizationComponent final
    : public UPrimitiveComponent {
    GENERATED_BODY()
  public:
    UCollisionGridVisualizationComponent();

    void configure(TOptional<FCollisionGridVisualizationSettings> settings);
    void update_collision_bounds(::ioj::sim::collision::CollisionSystem const* collision_system);
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

    UPROPERTY(EditAnywhere, Category = "Collision|Visualization")
    bool show_collision_bounds_{false};

    UPROPERTY(EditAnywhere,
              Category = "Collision|Visualization",
              meta = (ClampMin = "0.0", Units = "cm", EditCondition = "show_collision_bounds_"))
    float collision_bounds_max_draw_distance_{200000.f};

    float applied_collision_bounds_max_draw_distance_{200000.f};
    bool show_grid_{false};
    bool collision_bounds_visible_{false};
};
