#pragma once

#include "SandboxCore/vector_concepts.h"

#include <type_traits>
#include <utility>

namespace ml {

template <IsUnrealVector T>
struct VectorTraits {
    using Element = std::remove_cvref_t<decltype(std::declval<T>().X)>;
};

template <typename T>
using VectorElementT = typename VectorTraits<std::remove_cvref_t<T>>::Element;
}
