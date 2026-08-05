#pragma once

#include "soa_vectors.h"

#include <concepts>
#include <type_traits>

namespace ml {
template <typename T>
concept is_vec3f = std::same_as<std::remove_cvref_t<T>, FVectors3f> ||
                   std::same_as<std::remove_cvref_t<T>, TVectors3View<float>> ||
                   std::same_as<std::remove_cvref_t<T>, TVectors3View<float const>>;

template <typename T>
concept is_mutable_vec3f =
    (std::same_as<T, FVectors3f&> || std::same_as<std::remove_cvref_t<T>, TVectors3View<float>>) &&
    requires(T&& value) {
        { value.xs.GetData() } -> std::convertible_to<float*>;
        { value.ys.GetData() } -> std::convertible_to<float*>;
        { value.zs.GetData() } -> std::convertible_to<float*>;
    };

template <typename T>
concept is_readable_vec3f = is_vec3f<T> && requires(T const& value) {
    { value.xs.GetData() } -> std::convertible_to<float const*>;
    { value.ys.GetData() } -> std::convertible_to<float const*>;
    { value.zs.GetData() } -> std::convertible_to<float const*>;
};
}
