#pragma once

namespace ml::detail {
template <typename... Arrays>
auto all_num_equal(int32 const count, Arrays const&... arrays) -> bool {
    return ((arrays.Num() == count) && ...);
}
}
