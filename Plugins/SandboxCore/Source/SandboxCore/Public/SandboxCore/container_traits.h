#pragma once

#include "HAL/Platform.h"

#include <concepts>

class UInstancedStaticMeshComponent;

namespace ml {
// Num
template <typename T>
struct NumTraits;

template <typename T>
    requires requires(T const& t) {
        { t.Num() } -> std::convertible_to<int32>;
    }
struct NumTraits<T> {
    static auto num(T const& value) -> int32 { return value.Num(); }
};

template <typename T>
    requires requires(T const& t) {
        { t.num() } -> std::convertible_to<int32>;
    }
struct NumTraits<T> {
    static auto num(T const& value) -> int32 { return value.num(); }
};

template <>
struct SANDBOXCORE_API NumTraits<UInstancedStaticMeshComponent> {
    static auto num(UInstancedStaticMeshComponent const& value) -> int32;
};

template <>
struct NumTraits<int32> {
    static auto num(int32 const value) noexcept -> int32 { return value; }
};

// Reset
template <typename T>
struct ResetTraits;

template <typename T>
    requires requires(T& t) {
        { t.Reset() } -> std::convertible_to<void>;
    }
struct ResetTraits<T> {
    static auto reset(T& value) -> void { value.Reset(); }
};

template <typename T>
    requires requires(T& t) {
        { t.reset() } -> std::convertible_to<void>;
    }
struct ResetTraits<T> {
    static auto reset(T& value) -> void { return value.reset(); }
};
}
