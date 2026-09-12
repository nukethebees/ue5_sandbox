#pragma once

#include "Containers/AllowShrinking.h"
#include "Containers/Array.h"

namespace ml {

template <typename T>
auto num(T const& values) -> int32 {
    if constexpr (requires { values.Num(); }) {
        return values.Num();
    } else {
        return values.num();
    }
}

template <typename T>
void reset(T& values) {
    if constexpr (requires { values.Reset(); }) {
        values.Reset();
    } else {
        values.reset();
    }
}

template <typename T>
void reserve(T& values, int32 const count) {
    if constexpr (requires { values.Reserve(count); }) {
        values.Reserve(count);
    } else {
        values.reserve(count);
    }
}

template <typename T>
void add_uninitialised(T& values, int32 const count) {
    if constexpr (requires { values.AddUninitialized(count); }) {
        values.AddUninitialized(count);
    } else {
        values.add_uninitialised(count);
    }
}

template <typename T>
void add_defaulted(T& values, int32 const count) {
    if constexpr (requires { values.AddDefaulted(count); }) {
        values.AddDefaulted(count);
    } else {
        values.add_defaulted(count);
    }
}

template <typename T>
void remove_at_swap(T& values,
                    int32 const index,
                    int32 const count,
                    EAllowShrinking const allow_shrinking) {
    if constexpr (requires { values.RemoveAtSwap(index, count, allow_shrinking); }) {
        values.RemoveAtSwap(index, count, allow_shrinking);
    } else {
        values.remove_at_swap(index, count, allow_shrinking);
    }
}

template <typename T>
void set_num(T& values, int32 const count, EAllowShrinking const allow_shrinking) {
    if constexpr (requires { values.SetNum(count, allow_shrinking); }) {
        values.SetNum(count, allow_shrinking);
    } else {
        values.set_num(count, allow_shrinking);
    }
}

template <typename Destination, typename Source>
void copy_element(Destination& destination,
                  int32 const destination_index,
                  Source const& source,
                  int32 const source_index) {
    if constexpr (requires { destination[destination_index] = source[source_index]; }) {
        destination[destination_index] = source[source_index];
    } else {
        destination.copy_element(destination_index, source, source_index);
    }
}

template <typename Destination, typename Source>
void copy_elements(Destination& destination,
                   int32 const destination_index,
                   Source const& source,
                   int32 const source_index,
                   int32 const count) {
    if constexpr (requires {
                      destination.copy_elements(destination_index, source, source_index, count);
                  }) {
        destination.copy_elements(destination_index, source, source_index, count);
    } else {
        for (int32 offset{}; offset < count; ++offset) {
            copy_element(destination, destination_index + offset, source, source_index + offset);
        }
    }
}

template <typename Destination, typename Source>
void append_from(Destination& destination, Source const& source) {
    if constexpr (requires { destination.Append(source); }) {
        destination.Append(source);
    } else {
        destination.append_from(source);
    }
}

} // namespace ml
