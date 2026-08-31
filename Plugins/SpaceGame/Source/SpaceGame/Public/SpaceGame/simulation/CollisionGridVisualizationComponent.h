#pragma once

#include <Components/PrimitiveComponent.h>

#include "CollisionGridVisualizationComponent.generated.h"

struct FCollisionGridConfig;

UCLASS(ClassGroup = (Rendering))
class SPACEGAME_API UCollisionGridVisualizationComponent final : public UPrimitiveComponent {
    GENERATED_BODY()
  public:
    UCollisionGridVisualizationComponent();

    void configure(FCollisionGridConfig const& config);
    void clear();

    auto CreateSceneProxy() -> FPrimitiveSceneProxy* override;
    auto CalcBounds(FTransform const& local_to_world) const -> FBoxSphereBounds override;
  private:
    FIntVector3 grid_dimensions_{FIntVector3::ZeroValue};
    FVector3f cell_size_{FVector3f::ZeroVector};
    FLinearColor line_colour_{0.f, 1.f, 1.f, 1.f};
    float line_thickness_{1.f};
    bool show_grid_{false};
};
