#pragma once

#include <HAL/Platform.h>
#include <Math/MathFwd.h>

struct FVectors3f;

namespace ml {
void assign_from(FVectors3f& dst, FVectors3f const& src);

void multiply_in_place(FVectors3f& dst, float const value);

auto get_fvector(FVectors3f const& src, int32 i) -> FVector {
    return {src.xs[i], src.ys[i], src.zs[i]};
}

auto scaled_fvector(FVectors3f const& vectors, int32 const i, float const scale) -> FVector {
    return FVector{
        vectors.xs[i] * scale,
        vectors.ys[i] * scale,
        vectors.zs[i] * scale,
    };
}
}

auto operator*=(FVectors3f& vectors, float const scale) -> FVectors3f& {
    ml::multiply_in_place(vectors, scale);
    return vectors;
}
