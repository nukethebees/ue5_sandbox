#pragma once

#include <ioj/vector_value_math.h>

#include <gtest/gtest.h>

namespace ioj::testing {
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
