#pragma once

#include "HAL/Platform.h"

class UInstancedStaticMeshComponent;

namespace ml {
template <typename T>
struct NumTraits {
    static auto num(T const& value) -> int32 { return value.Num(); }
};

template <>
struct SANDBOXCORE_API NumTraits<UInstancedStaticMeshComponent> {
    static auto num(UInstancedStaticMeshComponent const& value) -> int32;
};

template <>
struct NumTraits<int32> {
    static auto num(int32 const value) noexcept -> int32 { return value; }
};
}
