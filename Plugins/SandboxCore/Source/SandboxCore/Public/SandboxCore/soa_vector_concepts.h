#pragma once

namespace ml {
template <typename T>
concept is_readable_vec3f = requires(T const& value) {
    value.num();
    value.xs.GetData();
    value.ys.GetData();
    value.zs.GetData();
};

template <typename T>
concept is_mutable_vec3f = is_readable_vec3f<T> && requires(T&& value) {
    value.xs.GetData();
    value.ys.GetData();
    value.zs.GetData();
};
} // namespace ml
