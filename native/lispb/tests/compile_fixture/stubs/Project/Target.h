#pragma once

#include "CoreMinimal.h"

struct FTarget {
    auto get(int32 const offset = 0) const -> int32 { return value + offset; }
    void set(int32 const new_value) { value = new_value; }
    void clear() { value = 0; }

    int32 value{};
};
