#pragma once

#include "CoreMinimal.h"

#include <initializer_list>

namespace ml {

inline void fatal_if_nums_not_equal(std::initializer_list<int32> const nums) {
    if (nums.size() < 2) {
        return;
    }
    auto const expected{*nums.begin()};
    for (auto const value : nums) {
        check(value == expected);
    }
}

} // namespace ml
