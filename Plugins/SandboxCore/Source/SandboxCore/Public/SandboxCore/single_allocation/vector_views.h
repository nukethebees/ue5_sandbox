#pragma once

#include <SandboxCore/compact_vector_view.h>
#include <SandboxCore/single_allocation/runtime.h>

#include <Containers/ArrayView.h>

namespace ml::soa {

template <typename T>
using Vector2View = soa_storage_detail::VectorView<T, 2, TArrayView, soa_storage::require>;
template <typename T>
using Vector2ConstView = Vector2View<T const>;
template <typename T>
using Vector3View = soa_storage_detail::VectorView<T, 3, TArrayView, soa_storage::require>;
template <typename T>
using Vector3ConstView = Vector3View<T const>;

}
