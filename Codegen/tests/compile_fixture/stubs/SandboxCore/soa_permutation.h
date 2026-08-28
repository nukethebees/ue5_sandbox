#pragma once

#include "Containers/Array.h"
#include "Containers/ArrayView.h"

namespace ml {

template <typename T>
void apply_permutation(TArray<T>&, TArrayView<int32>) {}

} // namespace ml
