#pragma once

#include "container_traits.h"

#include <HAL/Platform.h>

#include <concepts>

namespace ml {
// num
template <typename T>
concept SupportsNum = requires(T const& value) {
    { NumTraits<T>::num(value) } -> std::same_as<int32>;
};

// reset
template <typename T>
concept SupportsReset = requires(T& value) {
    { ResetTraits<T>::reset(value) } -> std::same_as<void>;
};

// reserve
template <typename T>
concept SupportsReserve = requires(T& value, int32 count) {
    { ReserveTraits<T>::reserve(value, count) } -> std::same_as<void>;
};

// add_uninitialized
template <typename T>
concept SupportsAddUninitialised = requires(T& value, int32 count) {
    { AddUninitialisedTraits<T>::add_uninitialised(value, count) } -> std::same_as<void>;
};

// add_defaulted
template <typename T>
concept SupportsAddDefaulted = requires(T& value, int32 count) {
    { AddDefaultedTraits<T>::add_defaulted(value, count) } -> std::same_as<void>;
};

// remove_at_swap
template <typename T>
concept SupportsRemoveAtSwap = requires(T& value, int32 index, int32 count, EAllowShrinking as) {
    { RemoveAtSwapTraits<T>::remove_at_swap(value, index, count, as) } -> std::same_as<void>;
};
}
