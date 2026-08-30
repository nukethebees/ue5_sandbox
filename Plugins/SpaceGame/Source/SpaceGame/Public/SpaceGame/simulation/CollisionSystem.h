#pragma once

#include <SGCollision/world_aabbs.h>

#include <CoreMinimal.h>

namespace ml::ioj {
struct SPACEGAME_API FCollisionSystem {
  public:
    FCollisionSystem() = default;

    void update();

    auto get_grid_dims() const noexcept -> FIntVector3 { return grid_dims_; }
    void set_grid_dims(FIntVector3 const grid_dims) noexcept { grid_dims_ = grid_dims; }

    auto get_cell_dims() const noexcept -> FVector3f { return cell_dims_; }
    void set_cell_dims(FVector3f const cell_dims) noexcept { cell_dims_ = cell_dims; }
  private:
    FIntVector3 grid_dims_{FIntVector3::ZeroValue};
    FVector3f cell_dims_{FVector3f::ZeroVector};
};
}
