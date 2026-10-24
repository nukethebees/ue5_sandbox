#pragma once

#include <concepts>
#include <cstddef>
#include <type_traits>

namespace ioj {
template <typename Source>
concept IndexedComponents = requires(Source const& value) {
    requires std::integral<std::remove_cv_t<decltype(Source::NumComponents)>>;
    typename std::integral_constant<std::size_t, Source::NumComponents>;
    requires (Source::NumComponents > 0);
    value[0];
};

template <typename Source, typename T>
concept CoordinateMembers = requires(Source const& value) {
    { value.x } -> std::convertible_to<T>;
    { value.y } -> std::convertible_to<T>;
    { value.z } -> std::convertible_to<T>;
    { value.w } -> std::convertible_to<T>;
};

template <typename Source, typename T>
concept CoordinateAccessors = requires(Source const& value) {
    { value.x() } -> std::convertible_to<T>;
    { value.y() } -> std::convertible_to<T>;
    { value.z() } -> std::convertible_to<T>;
    { value.w() } -> std::convertible_to<T>;
};

template <typename Source, typename T>
concept UppercaseCoordinateMembers = requires(Source const& value) {
    { value.X } -> std::convertible_to<T>;
    { value.Y } -> std::convertible_to<T>;
    { value.Z } -> std::convertible_to<T>;
    { value.W } -> std::convertible_to<T>;
};

template <typename Source, typename T>
concept UppercaseCoordinateAccessors = requires(Source const& value) {
    { value.X() } -> std::convertible_to<T>;
    { value.Y() } -> std::convertible_to<T>;
    { value.Z() } -> std::convertible_to<T>;
    { value.W() } -> std::convertible_to<T>;
};

template <typename Source, typename T>
concept EulerMembers = requires(Source const& value) {
    { value.pitch } -> std::convertible_to<T>;
    { value.yaw } -> std::convertible_to<T>;
    { value.roll } -> std::convertible_to<T>;
};

template <typename Source, typename T>
concept EulerAccessors = requires(Source const& value) {
    { value.pitch() } -> std::convertible_to<T>;
    { value.yaw() } -> std::convertible_to<T>;
    { value.roll() } -> std::convertible_to<T>;
};

template <typename Source, typename T>
concept UppercaseEulerMembers = requires(Source const& value) {
    { value.Pitch } -> std::convertible_to<T>;
    { value.Yaw } -> std::convertible_to<T>;
    { value.Roll } -> std::convertible_to<T>;
};

template <typename Source, typename T>
concept UppercaseEulerAccessors = requires(Source const& value) {
    { value.Pitch() } -> std::convertible_to<T>;
    { value.Yaw() } -> std::convertible_to<T>;
    { value.Roll() } -> std::convertible_to<T>;
};

}
