#pragma once

#include <SandboxCore/soa_vectors.h>

#include <HAL/Platform.h>
#include <Math/MathFwd.h>

namespace ml {
void SANDBOXCORE_API assign_from(FVectors3f& dst, FVectors3f const& src);
inline void
    assign_from(FVectors3f& dst, int32 const dst_i, FVectors3f const& src, int32 const src_i) {
    dst.xs[dst_i] = src.xs[src_i];
    dst.ys[dst_i] = src.ys[src_i];
    dst.zs[dst_i] = src.zs[src_i];
}

inline void assign(FVectors3f& vector, int32 const i, float const value) {
    vector.xs[i] = value;
    vector.ys[i] = value;
    vector.zs[i] = value;
}
inline void assign(FVectors3f& vector, int32 const i, FVector const& value) {
    vector.xs[i] = static_cast<float>(value.X);
    vector.ys[i] = static_cast<float>(value.Y);
    vector.zs[i] = static_cast<float>(value.Z);
}

void SANDBOXCORE_API multiply_in_place(FVectors3f& dst, float const value);

inline auto get_fvector(FVectors3f const& src, int32 i) -> FVector {
    return {src.xs[i], src.ys[i], src.zs[i]};
}

inline auto scaled_fvector(FVectors3f const& vectors, int32 const i, float const scale) -> FVector {
    return FVector{
        vectors.xs[i] * scale,
        vectors.ys[i] * scale,
        vectors.zs[i] * scale,
    };
}
}

inline auto operator*=(FVectors3f& vectors, float const scale) -> FVectors3f& {
    ml::multiply_in_place(vectors, scale);
    return vectors;
}
