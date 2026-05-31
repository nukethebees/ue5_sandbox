#pragma once

#include "SandboxCore/vector_traits.h"

#include "CoreMinimal.h"

#include <concepts>

namespace ml {
template <typename T>
concept HasSizeSquared = requires(T const& value) {
    { value.SizeSquared() } -> std::convertible_to<ml::VectorElementT<T>>;
};
}
