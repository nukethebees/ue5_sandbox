#pragma once

#include "container_concepts.h"
#include "container_traits.h"

#include "Containers/Array.h"
#include "Containers/ArrayView.h"

namespace ml::kernel {
template <typename T>
void fill(T* values, T const value, int32 const count) {
    for (int32 i{0}; i < count; ++i) {
        values[i] = value;
    }
}
}

namespace ml {
template <SupportsNumTraits T>
auto num(T const& value) -> int32 {
    return NumTraits<T>::num(value);
}

template <typename... Arrays>
    requires ml::AllSupportNumTraits<Arrays...>
auto all_num_equal_to(int32 const count, Arrays const&... arrays) -> bool {
    return ((num(arrays) == count) && ...);
}

// Use other to guarantee two arrays
template <typename Array, typename Other, typename... Rest>
    requires ml::AllSupportNumTraits<Array, Other, Rest...>
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

    for (int32 i{0}; i < n; ++i) {
        auto const index{indices[i]};
        ((arrays.RemoveAtSwap(index, 1, EAllowShrinking::No)), ...);
    }
}

template <typename... Arrays>
    requires ml::AllSupportResetTraits<Arrays...>
auto reset_arrays(Arrays&... arrays) -> void {
    return (ResetTraits<Arrays>::reset(arrays), ...);
}

template <SupportsReserveTraits Array>
auto reserve(Array& array, int32 count) -> void {
    ReserveTraits<Array>::reserve(array, count);
}

template <SupportsAddUninitialisedTraits Array>
auto add_uninitialised(Array& array, int32 count) -> void {
    AddUninitialisedTraits<Array>::add_uninitialised(array, count);
}

template <typename T>
void fill(TArrayView<T> values, T const value) {
    ml::kernel::fill(values.GetData(), value, values.Num());
}
template <typename T>
void fill(TArray<T>& values, T const value) {
    fill(TArrayView<T>{values}, value);
}

template <typename T>
void append_n(TArray<T>& values, T const value, int32 const count) {
    auto const old_num{values.Num()};

    values.AddUninitialized(count);

    fill(TArrayView<T>{values}.Right(count), value);
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
