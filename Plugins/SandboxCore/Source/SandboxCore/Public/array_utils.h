#pragma once

#include "container_concepts.h"

#include "CoreMinimal.h"

namespace ml {
template <typename... Arrays>
    requires ml::AllHaveNumReturningInt32<Arrays...>
auto all_num_equal_to(int32 const count, Arrays const&... arrays) -> bool {
    return ((arrays.Num() == count) && ...);
}

// Use other to guarantee two arrays
template <typename Array, typename Other, typename... Rest>
    requires ml::AllHaveNumReturningInt32<Array, Other, Rest...>
auto all_num_equal(Array const& array, Other const& other, Rest const&... rest) -> bool {
    return all_num_equal_to(array.Num(), other, rest...);
}
}
