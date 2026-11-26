#pragma once

#include <ioj/sim/collision/world_aabbs_view.h>
#include <SpaceGamePresentation/simulation/CollisionOverlay.h>

#include <Components/PrimitiveComponent.h>

#include "CollisionGridVisualizationComponent.generated.h"

struct FCollisionGridVisualizationSettings {
    ioj::sim::collision::CollisionGridSpec grid{};
    FLinearColor line_colour{0.f, 1.f, 1.f, 1.f};
    float line_thickness{1.f};
    ioj::FCollisionOverlayVisibility visibility;
};

namespace ioj {
UCLASS(ClassGroup = (Rendering))
class SPACEGAMEPRESENTATION_API UCollisionGridVisualizationComponent final
    : public UPrimitiveComponent {
    GENERATED_BODY()
  public:
    UCollisionGridVisualizationComponent();

    void configure(TOptional<FCollisionGridVisualizationSettings> settings);
    void update_entity_bounds(sim::collision::world_aabbs::ConstView entity_aabbs);
    void set_static_bounds(sim::collision::world_aabbs::ConstView static_aabbs);
    void clear();

    auto CreateSceneProxy() -> FPrimitiveSceneProxy* override;
    auto CalcBounds(FTransform const& local_to_world) const -> FBoxSphereBounds override;
    void SendRenderDynamicData_Concurrent() override;
  private:
    TArray<FBox3f> entity_bounds_;
    TArray<FBox3f> static_bounds_;
    sim::collision::CollisionGridSpec grid_{};
    FLinearColor line_colour_{0.f, 1.f, 1.f, 1.f};
    float line_thickness_{1.f};

    bool show_collision_bounds_{false};

    UPROPERTY(EditAnywhere,
              Category = "Collision|Visualization",
              meta = (ClampMin = "0.0", Units = "cm"))
    float collision_bounds_max_draw_distance_{200000.f};

    bool show_grid_{false};
};

} // namespace ioj
