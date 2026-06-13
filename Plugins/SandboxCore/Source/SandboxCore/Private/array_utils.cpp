#include "SandboxCore/array_utils.h"

namespace ml {
auto is_sorted_desc(TConstArrayView<int32> const xs) -> bool {
    auto const n{xs.Num()};

    for (int32 i{1}; i < n; ++i) {
        if (xs[i - 1] < xs[i]) {
            return false;
        }
    }

    return true;
}
}
