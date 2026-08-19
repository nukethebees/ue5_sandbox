#pragma once

#include <Containers/StaticArray.h>
#include <HAL/Platform.h>
#include <Misc/AssertionMacros.h>

namespace ml {
struct FLoopBounds {
    int32 begin;
    int32 end;
};

inline auto make_rotated_loop_bounds(int32 const begin, int32 const end, int32 const offset)
    -> TStaticArray<FLoopBounds, 2> {
    check(begin >= 0);
    check(end >= begin);
    check(offset >= 0);

    if (begin == end) {
        return TStaticArray<FLoopBounds, 2>{
            FLoopBounds{begin, begin},
            FLoopBounds{begin, begin},
        };
    }

    int32 const count{end - begin};
    int32 const normalized_offset{offset % count};
    int32 const pivot{begin + normalized_offset};

    return TStaticArray<FLoopBounds, 2>{FLoopBounds{pivot, end}, FLoopBounds{begin, pivot}};
}
}
