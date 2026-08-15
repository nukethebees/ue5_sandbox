#pragma once

#include "container_ops.h"

#include <Containers/AllowShrinking.h>
#include <HAL/Platform.h>
#include <Math/UnrealMathUtility.h>
#include "Containers/Array.h"
#include "Containers/ArrayView.h"

#include <concepts>
#include <type_traits>

namespace ml::kernel {
// -------------------------------------------------------------------------------------------------
// Assignment
// -------------------------------------------------------------------------------------------------
template <typename T>
void assign_from(T* const RESTRICT dst_x,
                 T* const RESTRICT dst_y,
                 T* const RESTRICT dst_z,
                 T const* const RESTRICT src_x,
                 T const* const RESTRICT src_y,
                 T const* const RESTRICT src_z,
                 int32 const count) {
    for (int32 i{0}; i < count; ++i) {
        dst_x[i] = src_x[i];
        dst_y[i] = src_y[i];
        dst_z[i] = src_z[i];
    }
}

template <typename T>
void fill(T* values, T const value, int32 const count) {
    for (int32 i{0}; i < count; ++i) {
        values[i] = value;
    }
}
template <typename T>
void fill(T* RESTRICT xs, T* RESTRICT ys, T* RESTRICT zs, T const value, int32 const count) {
    for (int32 i{0}; i < count; ++i) {
        xs[i] = value;
        ys[i] = value;
        zs[i] = value;
    }
}

// -------------------------------------------------------------------------------------------------
// Comparison
// -------------------------------------------------------------------------------------------------
template <std::floating_point T>
auto almost_equal(T const* const lhs,
                  T const* const rhs,
                  int32 const count,
                  T const tolerance = static_cast<T>(KINDA_SMALL_NUMBER)) -> bool {
    check(lhs != nullptr);
    check(rhs != nullptr);
    check(count >= 0);

    for (int32 i{0}; i < count; ++i) {
        if (!FMath::IsNearlyEqual(lhs[i], rhs[i], tolerance)) {
            return false;
        }
    }

    return true;
}
}

namespace ml {
template <std::floating_point T>
[[nodiscard]]
auto almost_equal(TConstArrayView<T> const lhs,
                  TConstArrayView<T> const rhs,
                  T const tolerance = static_cast<T>(KINDA_SMALL_NUMBER)) -> bool {
    if (lhs.Num() != rhs.Num()) {
        return false;
    }

    if (lhs.IsEmpty()) {
        return true;
    }

    return ml::kernel::almost_equal(lhs.GetData(), rhs.GetData(), lhs.Num(), tolerance);
}

template <SupportsNum... Arrays>
auto all_num_equal_to(int32 const count, Arrays const&... arrays) -> bool {
    return ((num(arrays) == count) && ...);
}

// Use other to guarantee two arrays
template <typename Array, typename Other, typename... Rest>
    requires (SupportsNum<Array> && SupportsNum<Other> && (SupportsNum<Rest> && ...))
auto all_num_equal(Array const& array, Other const& other, Rest const&... rest) -> bool {
    return all_num_equal_to(ml::num(array), other, rest...);
}

auto SANDBOXCORE_API is_sorted_desc(TConstArrayView<int32> const xs) -> bool;

template <typename... TArrays>
void remove_at_swap_many_sorted_desc(TConstArrayView<int32> const indices, TArrays&... arrays) {
    check(ml::is_sorted_desc(indices));

    auto const n{indices.Num()};
    if (n < 1) {
        return;
    }

    int32 last_handled{INDEX_NONE};
    for (auto const index : indices) {
        if (index == last_handled) {
            continue;
        }
        ((RemoveAtSwapTraits<TArrays>::remove_at_swap(arrays, index, 1, EAllowShrinking::No)), ...);
        last_handled = index;
    }
}

template <typename Container, typename T>
    requires requires(Container& container) {
        { container.GetData() } -> std::same_as<T*>;
        { container.Num() } -> std::same_as<int32>;
    }
void fill(Container&& values, T const value) {
    ml::kernel::fill(values.GetData(), value, values.Num());
}

inline void fill_indices(TArrayView<int32> const indices) {
    auto const n{indices.Num()};
    for (int32 i{}; i < n; ++i) {
        indices[i] = i;
    }
}

template <typename Container, typename T>
    requires requires(Container& values) {
        { values.GetData() } -> std::same_as<T*>;
        { values.Num() } -> std::same_as<int32>;
    }
void fill_first(Container&& values, T const value, int32 const count) {
    check(count >= 0);
    check(count <= values.Num());
    fill(TArrayView<T>{values}.Left(count), value);
}

template <typename Container, typename T>
    requires requires(Container& values) {
        { values.GetData() } -> std::same_as<T*>;
        { values.Num() } -> std::same_as<int32>;
    }
void fill_last(Container&& values, T const value, int32 const count) {
    check(count >= 0);
    check(count <= values.Num());
    fill(TArrayView<T>{values}.Right(count), value);
}

template <typename T>
void append_n(TArray<T>& values, T const value, int32 const count) {
    values.AddUninitialized(count);
    fill_last(values, value, count);
}

template <typename KeysType, typename SearchKeysType>
    requires requires(KeysType const& k, SearchKeysType const& s) {
        s.Num();
        k.IndexOfByKey(s[0]);
    }
void collect_valid_indices_by_key(KeysType const& keys,
                                  SearchKeysType const& search_keys,
                                  TArray<int32>& out_indices) {
    out_indices.SetNumUninitialized(search_keys.Num(), EAllowShrinking::No);

    int32 n{0};

    for (auto const& search_key : search_keys) {
        auto const index{keys.IndexOfByKey(search_key)};

        if (index == INDEX_NONE) {
            continue;
        }

        out_indices[n] = index;
        ++n;
    }

    out_indices.SetNum(n, EAllowShrinking::No);
}
}
