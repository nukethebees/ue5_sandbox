#pragma once

#include <concepts>

namespace ml {
template <typename T>
concept is_numeric = std::floating_point<T> || (std::integral<T> && !std::same_as<T, bool>);
}
