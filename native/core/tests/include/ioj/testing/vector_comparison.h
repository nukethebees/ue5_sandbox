#pragma once

#include <ioj/vector_value_math.h>

#include <gtest/gtest.h>

namespace ioj::testing {
template <std::floating_point T, std::size_t N>
auto floating_point_equal(Vector<T, N> const& actual,
                          std::type_identity_t<Vector<T, N>> const& expected)
    -> ::testing::AssertionResult {
    for (std::size_t component{}; component < N; ++component) {
        if (!::testing::internal::FloatingPoint<T>{actual[component]}.AlmostEquals(
                ::testing::internal::FloatingPoint<T>{expected[component]})) {
            return ::testing::AssertionFailure()
                << "Component " << component << " is not equal within 4 ULPs.\nActual: "
                << ::testing::PrintToString(actual.elements)
                << "\nExpected: " << ::testing::PrintToString(expected.elements);
        }
    }
    return ::testing::AssertionSuccess();
}

template <std::floating_point T, std::size_t N>
void expect_approximately_equal(Vector<T, N> const& actual,
                                std::type_identity_t<Vector<T, N>> const& expected,
                                std::type_identity_t<T> const tolerance) {
    if (almost_equal(actual, expected, tolerance)) {
        return;
    }

    for (std::size_t component{}; component < N; ++component) {
        SCOPED_TRACE(component);
        EXPECT_NEAR(actual[component], expected[component], tolerance);
    }
}
}
