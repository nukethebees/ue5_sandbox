#pragma once

#include <HAL/Platform.h>

#include <type_traits>

namespace ml::view {
auto slice(auto& soa_view, int32 offset, int32 count) {
    using T = std::remove_cvref_t<decltype(soa_view)>;

    return soa_view.apply_arrays(
        [=](auto&... subviews) { return T{subviews.Slice(offset, count)...}; });
}

auto right(auto& soa_view, int32 count) {
    using T = std::remove_cvref_t<decltype(soa_view)>;

    return soa_view.apply_arrays([=](auto&... subviews) { return T{subviews.Right(count)...}; });
}
}
