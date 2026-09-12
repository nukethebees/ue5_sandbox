#pragma once

#include <sandbox/core/frame_array.h>

#include <Containers/ArrayView.h>

namespace ml {
template <typename T>
class TFrameArray : public FrameArray<T> {
    using Base = FrameArray<T>;
  public:
    using Base::Base;

    void reserve(int32 const capacity) { Base::reserve(capacity); }

    operator TArrayView<T>() noexcept { return view(); }
    operator TConstArrayView<T>() const noexcept { return view(); }

    auto view() noexcept -> TArrayView<T> { return {this->data(), this->num()}; }
    auto view() const noexcept -> TConstArrayView<T> { return {this->data(), this->num()}; }
};
}
