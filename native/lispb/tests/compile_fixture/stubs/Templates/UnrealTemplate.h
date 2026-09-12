#pragma once

#include <type_traits>

template <typename T>
constexpr auto MoveTemp(T&& value) noexcept -> std::remove_reference_t<T>&& {
    return static_cast<std::remove_reference_t<T>&&>(value);
}
