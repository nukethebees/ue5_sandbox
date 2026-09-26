#pragma once

#include "Containers/AllowShrinking.h"
#include "CoreMinimal.h"

template <typename SizeType>
auto DefaultCalculateSlackGrow(SizeType const requested, SizeType const current, SIZE_T, bool)
    -> SizeType {
    return current == 0 ? requested : requested + requested * 3 / 8 + 16;
}
