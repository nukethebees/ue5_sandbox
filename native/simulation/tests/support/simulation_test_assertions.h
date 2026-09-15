#pragma once
#include <cmath>
#include <gtest/gtest.h>
#include <sandbox/core/math_types.h>
#include <source_location>
#include <string>

namespace ioj::sim::tests {
inline auto distance(Vector3f a, Vector3f b) -> float {
    return HMM_LenV3(a - b);
}
inline auto distance(ml::Vector3d a, ml::Vector3d b) -> double {
    return (a - b).size();
}
inline auto expect_true(bool condition,
                        std::string const& description,
                        int index = -1,
                        std::source_location site = std::source_location::current()) -> bool {
    if (!condition) {
        ADD_FAILURE_AT(site.file_name(), static_cast<int>(site.line()))
            << description << " [index " << index << "]";
    }
    return condition;
}
inline auto expect_false(bool condition,
                         std::string const& description,
                         int index = -1,
                         std::source_location site = std::source_location::current()) -> bool {
    return expect_true(!condition, description, index, site);
}
template <typename A, typename B>
auto expect_equal(A const& a,
                  B const& b,
                  std::string const& description,
                  int index = -1,
                  std::source_location site = std::source_location::current()) -> bool {
    if (a == b) {
        return true;
    }
    return expect_true(false,
                       description + " lhs " + ::testing::PrintToString(a) + ", rhs " +
                           ::testing::PrintToString(b),
                       index,
                       site);
}
template <typename A, typename B, typename T>
auto expect_equal(A a,
                  B b,
                  T tolerance,
                  std::string const& description,
                  int index = -1,
                  std::source_location site = std::source_location::current()) -> bool {
    return expect_true(std::abs(a - b) <= tolerance, description, index, site);
}
template <typename A, typename B>
auto expect_not_equal(A const& a,
                      B const& b,
                      std::string const& description,
                      int index = -1,
                      std::source_location site = std::source_location::current()) -> bool {
    return expect_true(a != b, description, index, site);
}
template <typename A, typename B>
auto expect_greater(A a,
                    B b,
                    std::string const& description,
                    int index = -1,
                    std::source_location site = std::source_location::current()) -> bool {
    return expect_true(a > b, description, index, site);
}
template <typename A, typename B>
auto expect_less_equal(A a,
                       B b,
                       std::string const& description,
                       int index = -1,
                       std::source_location site = std::source_location::current()) -> bool {
    return expect_true(a <= b, description, index, site);
}
template <typename T>
auto expect_not_null(T* pointer,
                     std::string const& description,
                     std::source_location site = std::source_location::current()) -> bool {
    return expect_true(pointer != nullptr, description, -1, site);
}
template <typename V, typename T>
auto expect_distance_near(V a,
                          V b,
                          T tolerance,
                          std::string const& description,
                          int index = -1,
                          std::source_location site = std::source_location::current()) -> bool {
    return expect_true(distance(a, b) <= tolerance, description, index, site);
}
template <typename V, typename T>
auto expect_distance_not_near(V a,
                              V b,
                              T tolerance,
                              std::string const& description,
                              int index = -1,
                              std::source_location site = std::source_location::current()) -> bool {
    return expect_true(distance(a, b) > tolerance, description, index, site);
}
template <typename V, typename T>
auto expect_distance_greater(V a,
                             V b,
                             T tolerance,
                             std::string const& description,
                             int index = -1,
                             std::source_location site = std::source_location::current()) -> bool {
    return expect_true(distance(a, b) > tolerance, description, index, site);
}
}
