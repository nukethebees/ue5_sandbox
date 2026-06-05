#pragma once

#include "container_traits.h"

#include <HAL/Platform.h>

#include <concepts>

namespace ml {
template <typename T>
concept HasNumReturningInt32 = requires(T const& value) {
    { NumTraits<T>::num(value) } -> std::same_as<int32>;
};

template <typename... Ts>
concept AllHaveNumReturningInt32 = (HasNumReturningInt32<Ts> && ...);
}
