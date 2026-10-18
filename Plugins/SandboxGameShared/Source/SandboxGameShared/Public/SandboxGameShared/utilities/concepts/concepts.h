#pragma once

#include <concepts>

namespace ioj {
template <typename T>
concept is_numeric = std::floating_point<T> || (std::integral<T> && !std::same_as<T, bool>);
}
