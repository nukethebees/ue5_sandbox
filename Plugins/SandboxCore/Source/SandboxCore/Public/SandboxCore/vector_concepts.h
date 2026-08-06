#pragma once

#include "CoreMinimal.h"

#include <concepts>
#include <type_traits>

namespace ml {
template <typename T>
concept IsUnrealVector = requires(std::remove_cvref_t<T> const& value) {
    requires std::floating_point<std::remove_cvref_t<decltype(value.X)>>;
    {
        std::remove_cvref_t<T>::Dist(value, value)
    } -> std::convertible_to<std::remove_cvref_t<decltype(value.X)>>;
    { value.ToCompactString() } -> std::convertible_to<FString>;
};

template <typename T>
concept HasSizeSquared = requires(std::remove_cvref_t<T> const& value) {
    { value.SizeSquared() } -> std::convertible_to<std::remove_cvref_t<decltype(value.X)>>;
};
}
