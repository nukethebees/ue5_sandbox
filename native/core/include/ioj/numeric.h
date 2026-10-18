#pragma once

#include <concepts>

namespace ioj {
template <typename T>
concept Numeric = std::integral<T> || std::floating_point<T>;
}
