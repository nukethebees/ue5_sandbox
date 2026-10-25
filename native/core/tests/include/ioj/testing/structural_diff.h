#pragma once

#include <ioj/structural_diff.h>

#include <gtest/gtest.h>

namespace ioj::testing {
template <typename T>
auto structurally_equal(char const* expected_expression,
                        char const* actual_expression,
                        T const& expected,
                        T const& actual) -> ::testing::AssertionResult {
    DiffReport differences;
    differences.diff(std::string_view{actual_expression}, expected, actual);
    if (differences.empty()) {
        return ::testing::AssertionSuccess();
    }

    return ::testing::AssertionFailure()
        << "Comparing " << actual_expression << " with " << expected_expression << ":\n"
        << differences.message();
}
}
