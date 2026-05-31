#pragma once

#include "CoreMinimal.h"

#include <concepts>

namespace ml {
template <typename T>
concept HasNumReturningInt32 = requires(T const& value) {
    { value.Num() } -> std::convertible_to<int32>;
};

template <typename... Ts>
concept AllHaveNumReturningInt32 = (HasNumReturningInt32<Ts> && ...);
}
