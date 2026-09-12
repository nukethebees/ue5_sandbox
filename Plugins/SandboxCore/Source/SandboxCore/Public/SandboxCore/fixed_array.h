#pragma once

#include <sandbox/core/fixed_array.h>

#include <Containers/ArrayView.h>
#include <HAL/Platform.h>

namespace ml {
template <typename T, int32 N>
class TFixedArray : public FixedArray<T, N> {
    using Base = FixedArray<T, N>;
  public:
    using Base::Base;

    operator TArrayView<T>() { return {this->data(), this->num()}; }
    operator TConstArrayView<T>() const { return {this->data(), this->num()}; }

    auto capacity_view() noexcept -> TArrayView<T> { return {this->data(), this->capacity()}; }
};
}
