#pragma once

#include <sandbox/core/compact_vector_view.h>
#include <sandbox/core/rotator_soa_view.h>
#include <SandboxCore/single_allocation/runtime.h>

#include <Containers/ArrayView.h>

namespace ml::soa {

template <typename T>
using Vector2View = soa_storage_detail::VectorView<T, 2, TArrayView, int32>;
template <typename T>
using Vector2ConstView = Vector2View<T const>;
template <typename T>
using Vector3View = soa_storage_detail::VectorView<T, 3, TArrayView, int32>;
template <typename T>
using Vector3ConstView = Vector3View<T const>;

template <typename T>
using RotatorSoAView = soa_storage_detail::RotatorSoAView<T, TArrayView, int32>;
template <typename T>
using RotatorSoAConstView = RotatorSoAView<T const>;

}
