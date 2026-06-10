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

// Reserve
template <typename T>
struct ReserveTraits;

template <typename T>
    requires requires(T& t) {
        { t.Reserve(0) } -> std::convertible_to<void>;
    }
struct ReserveTraits<T> {
    static auto reserve(T& value, int32 count) -> void { value.Reserve(count); }
};

template <typename T>
    requires requires(T& t) {
        { t.reserve(0) } -> std::convertible_to<void>;
    }
struct ReserveTraits<T> {
    static auto reserve(T& value, int32 count) -> void { value.reserve(count); }
};

// AddUninitialized
template <typename T>
struct AddUninitialisedTraits;

template <typename T>
    requires requires(T& t) {
        { t.AddUninitialized(0) } -> std::convertible_to<int32>;
    }
struct AddUninitialisedTraits<T> {
    static auto add_uninitialised(T& value, int32 count) -> void { value.AddUninitialized(count); }
};

template <typename T>
    requires requires(T& t) {
        { t.add_uninitialized(0) } -> std::convertible_to<void>;
    }
struct AddUninitialisedTraits<T> {
    static auto add_uninitialised(T& value, int32 count) -> void { value.add_uninitialized(count); }
};

// RemoveAtSwap
template <typename T>
struct RemoveAtSwapTraits;

template <typename T>
    requires requires(T& t) {
        { t.RemoveAtSwap(0, 0, EAllowShrinking::No) } -> std::convertible_to<void>;
    }
struct RemoveAtSwapTraits<T> {
    static auto remove_at_swap(T& value, int32 index, int32 count, EAllowShrinking as) -> void {
        value.RemoveAtSwap(index, count, as);
    }
};

template <typename T>
    requires requires(T& t) {
        { t.remove_at_swap(0, 0, EAllowShrinking::No) } -> std::convertible_to<void>;
    }
struct RemoveAtSwapTraits<T> {
    static auto remove_at_swap(T& value, int32 index, int32 count, EAllowShrinking as) -> void {
        value.remove_at_swap(index, count, as);
    }
};
}
