#pragma once

#include "SandboxISMCInstanceChunkWriter.h"

#include "Containers/Array.h"

namespace sandbox_ismc_tests {
inline auto decode_position(FSandboxISMCRenderInstance const& instance) -> FVector3f {
    return FVector3f{static_cast<float>(instance.position[0]),
                     static_cast<float>(instance.position[1]),
                     static_cast<float>(instance.position[2])} *
           ioj::ismc::position_quantum;
}

template <typename Fill>
void write_transforms(FSandboxISMCInstanceChunkWriter& chunk, int32 const count, Fill&& fill) {
    TArray<FVector3f, TInlineAllocator<1024>> positions;
    TArray<FQuat4f, TInlineAllocator<1024>> rotations;
    positions.SetNumUninitialized(chunk.num());
    rotations.SetNumUninitialized(chunk.num());
    for (int32 index{}; index < count; ++index) {
        fill(index, positions[index], rotations[index]);
    }
    chunk.set_transforms<ESandboxISMCBoundsMode::Calculate>(positions, rotations);
}

template <ESandboxISMCBoundsMode Mode = ESandboxISMCBoundsMode::Calculate>
void write_transform(FSandboxISMCInstanceChunkWriter& chunk,
                     FVector3f const position = FVector3f::ZeroVector,
                     FQuat4f const rotation = FQuat4f::Identity) {
    FVector3f const positions[]{position};
    FQuat4f const rotations[]{rotation};
    chunk.set_transforms<Mode>(positions, rotations);
}
}
