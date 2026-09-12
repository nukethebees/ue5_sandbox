#pragma once

#include "Containers/ArrayView.h"

namespace ml {

inline void fill_indices(TArrayView<int32> const indices) {
    auto const count{indices.Num()};
    for (int32 index{}; index < count; ++index) {
        indices[index] = index;
    }
}

} // namespace ml
