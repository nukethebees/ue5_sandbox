#pragma once

#include <utility>

namespace ml {

template <auto Fn, typename... Ts>
constexpr void invoke_on_all(Ts&&... values) {
    ((void)Fn(std::forward<Ts>(values)), ...);
}

template <typename Fn, typename... Ts>
constexpr void invoke_on_all(Fn&& fn, Ts&&... values) {
    ((void)std::forward<Fn>(fn)(std::forward<Ts>(values)), ...);
}

} // namespace ml
