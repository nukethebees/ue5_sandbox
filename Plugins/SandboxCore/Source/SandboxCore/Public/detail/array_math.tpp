#pragma once

#include <SandboxCore/Public/numeric.h>

#include "CoreMinimal.h"

namespace ml::detail {
template <ml::Numeric T>
void subtract_in_place(T* data, T const value, int32 const count) noexcept {
    for (int32 i{0}; i < count; ++i) {
        data[i] -= value;
    }
}
}

namespace ml {
template <ml::Numeric T>
void subtract_in_place(TArrayView<T> data, T const value) noexcept {
    ml::detail::subtract_in_place(data.GetData(), value, data.Num());
}
}
