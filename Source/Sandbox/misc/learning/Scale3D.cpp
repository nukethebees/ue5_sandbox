#include "Scale3D.h"

#include "CoreMinimal.h"

FScale3D::FScale3D(FVector non_uniform) noexcept
    : use_uniform_scale{false}
    , non_uniform_scale{non_uniform}
    , uniform_scale{} {}

FScale3D::FScale3D(float uniform) noexcept
    : use_uniform_scale{true}
    , non_uniform_scale{}
    , uniform_scale{uniform} {}

auto FScale3D::get() const -> FVector {
    if (use_uniform_scale) {
        return {uniform_scale, uniform_scale, uniform_scale};
    } else {
        return non_uniform_scale;
    }
}
