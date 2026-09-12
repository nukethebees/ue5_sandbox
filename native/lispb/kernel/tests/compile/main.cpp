#include "ArrayKernels.h"

#include <cmath>
#include <cstddef>
#include <string_view>

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
    ml::multiply(
        TConstArrayView<T>{lhs, count}, TConstArrayView<T>{rhs, count}, TArrayView<T>{out, count});
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

    ml::divide_in_place(TArrayView<T>{lhs, count}, static_cast<T>(3));
    ml::add_in_place(TArrayView<T>{lhs, count}, static_cast<T>(5));
    ml::subtract_in_place(TArrayView<T>{lhs, count}, static_cast<T>(5));
    for (std::size_t index{}; index < Count; ++index) {
        if (lhs[index] != static_cast<T>(index + 1)) {
            return false;
        }
    }
    return true;
}

template <typename T>
auto test_constants() -> bool {
    T input[1]{static_cast<T>(1)};
    T out[1]{};
    auto const input_view{TConstArrayView<T>{input, 1}};
    auto const out_view{TArrayView<T>{out, 1}};

    ml::add_nan(input_view, out_view);
    if (!std::isnan(out[0])) {
        return false;
    }
    ml::add_infinity(input_view, out_view);
    if (!std::isinf(out[0]) || out[0] < static_cast<T>(0)) {
        return false;
    }
    ml::add_negative_infinity(input_view, out_view);
    return std::isinf(out[0]) && out[0] < static_cast<T>(0);
}

void run_unequal_lengths() {
    float lhs[2]{};
    float rhs[1]{};
    float out[2]{};
    ml::multiply(
        TConstArrayView<float>{lhs, 2}, TConstArrayView<float>{rhs, 1}, TArrayView<float>{out, 2});
}

void run_overlapping_output() {
    float storage[3]{};
    ml::multiply(TConstArrayView<float>{storage, 2}, 2.0f, TArrayView<float>{storage + 1, 2});
}

void run_overlapping_in_place() {
    float storage[3]{};
    ml::multiply_in_place(TArrayView<float>{storage, 2}, TConstArrayView<float>{storage + 1, 2});
}

auto main(int const argc, char const* const* argv) -> int {
    if (argc == 2) {
        auto const invariant{std::string_view{argv[1]}};
        if (invariant == "unequal-lengths") {
            run_unequal_lengths();
        } else if (invariant == "overlapping-output") {
            run_overlapping_output();
        } else if (invariant == "overlapping-in-place") {
            run_overlapping_in_place();
        } else {
            return 2;
        }
        return 0;
    }

    auto const passed = test_size<int32, 0>() && test_size<int32, 1>() && test_size<int32, 7>() &&
                        test_size<int32, 8>() && test_size<int32, 9>() && test_size<float, 31>() &&
                        test_size<double, 9>() && test_constants<float>() &&
                        test_constants<double>();
    return passed ? 0 : 1;
}
