#pragma once

#include <concepts>
#include <cstddef>

namespace ioj {
template <typename Source, typename T, std::size_t N>
concept CoordinateMembers = requires(Source const& value) {
    requires (N > 0 && N <= 4);
    { value.x } -> std::convertible_to<T>;
    requires (N <= 1 || requires {
        { value.y } -> std::convertible_to<T>;
    });
    requires (N <= 2 || requires {
        { value.z } -> std::convertible_to<T>;
    });
    requires (N <= 3 || requires {
        { value.w } -> std::convertible_to<T>;
    });
};

template <typename Source, typename T, std::size_t N>
concept CoordinateAccessors = requires(Source const& value) {
    requires (N > 0 && N <= 4);
    { value.x() } -> std::convertible_to<T>;
    requires (N <= 1 || requires {
        { value.y() } -> std::convertible_to<T>;
    });
    requires (N <= 2 || requires {
        { value.z() } -> std::convertible_to<T>;
    });
    requires (N <= 3 || requires {
        { value.w() } -> std::convertible_to<T>;
    });
};

template <typename Source, typename T, std::size_t N>
concept UppercaseCoordinateMembers = requires(Source const& value) {
    requires (N > 0 && N <= 4);
    { value.X } -> std::convertible_to<T>;
    requires (N <= 1 || requires {
        { value.Y } -> std::convertible_to<T>;
    });
    requires (N <= 2 || requires {
        { value.Z } -> std::convertible_to<T>;
    });
    requires (N <= 3 || requires {
        { value.W } -> std::convertible_to<T>;
    });
};

template <typename Source, typename T, std::size_t N>
concept UppercaseCoordinateAccessors = requires(Source const& value) {
    requires (N > 0 && N <= 4);
    { value.X() } -> std::convertible_to<T>;
    requires (N <= 1 || requires {
        { value.Y() } -> std::convertible_to<T>;
    });
    requires (N <= 2 || requires {
        { value.Z() } -> std::convertible_to<T>;
    });
    requires (N <= 3 || requires {
        { value.W() } -> std::convertible_to<T>;
    });
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
