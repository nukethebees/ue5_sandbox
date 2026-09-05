#include "ArrayKernels.h"

#include <cstddef>

template <typename T, std::size_t Count>
auto test_size() -> bool {
    constexpr auto storage_count{Count == 0 ? 1 : Count};
    T lhs[storage_count]{};
    T rhs[storage_count]{};
    T out[storage_count]{};
    for (std::size_t index{}; index < Count; ++index) {
        lhs[index] = static_cast<T>(index + 1);
        rhs[index] = static_cast<T>(index + 2);
    }

    auto const count{static_cast<int32>(Count)};
    ml::multiply(TConstArrayView<T>{lhs, count},
                 TConstArrayView<T>{rhs, count},
                 TArrayView<T>{out, count});
    for (std::size_t index{}; index < Count; ++index) {
        if (out[index] != lhs[index] * rhs[index]) {
            return false;
        }
    }

    ml::multiply(TConstArrayView<T>{lhs, count}, static_cast<T>(3), TArrayView<T>{out, count});
    ml::multiply_in_place(TArrayView<T>{lhs, count}, static_cast<T>(3));
    for (std::size_t index{}; index < Count; ++index) {
        if (out[index] != lhs[index]) {
            return false;
        }
    }
    return true;
}

auto main() -> int {
    auto const passed = test_size<int32, 0>() && test_size<int32, 1>() &&
                        test_size<int32, 7>() && test_size<int32, 8>() &&
                        test_size<int32, 9>() && test_size<float, 31>() &&
                        test_size<double, 9>();
    return passed ? 0 : 1;
}
