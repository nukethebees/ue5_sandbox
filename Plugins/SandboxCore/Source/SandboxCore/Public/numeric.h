#pragma once

#include <concepts>

namespace ml {
template <typename T>
concept Numeric = std::integral<T> || std::floating_point<T>;
}