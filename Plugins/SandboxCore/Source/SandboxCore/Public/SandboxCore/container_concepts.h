#pragma once

#include "container_traits.h"

#include <HAL/Platform.h>

#include <concepts>

namespace ml {
// num
template <typename T>
concept SupportsNumTraits = requires(T const& value) {
    { NumTraits<T>::num(value) } -> std::same_as<int32>;
};

template <typename... Ts>
concept AllSupportNumTraits = (SupportsNumTraits<Ts> && ...);

// reset
template <typename T>
concept SupportsResetTraits = requires(T& value) {
    { ResetTraits<T>::reset(value) } -> std::same_as<void>;
};

template <typename... Ts>
concept AllSupportResetTraits = (SupportsResetTraits<Ts> && ...);

// reserve
template <typename T>
concept SupportsReserveTraits = requires(T& value, int32 count) {
    { ReserveTraits<T>::reserve(value, count) } -> std::same_as<void>;
};
}
