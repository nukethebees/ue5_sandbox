#pragma once

#include <CoreMinimal.h>

namespace ml {
template <typename T>
struct TestEqualityTraits {
    static bool is_equal(T const& expected, T const& actual) { return expected == actual; }
};

template <>
struct TestEqualityTraits<float> {
    static bool is_equal(float const expected, float const actual) { return expected == actual; }

    static bool
        is_equal_with_tolerance(float const expected, float const actual, float const tolerance) {
        return FMath::IsNearlyEqual(expected, actual, tolerance);
    }
};

template <>
struct TestEqualityTraits<double> {
    static bool is_equal(double const expected, double const actual) { return expected == actual; }

    static bool is_equal_with_tolerance(double const expected,
                                        double const actual,
                                        double const tolerance) {
        return FMath::IsNearlyEqual(expected, actual, tolerance);
    }
};

template <>
struct TestEqualityTraits<FVector2D> {
    static bool is_equal(FVector2D const& expected, FVector2D const& actual) {
        return expected == actual;
    }

    static bool is_equal_with_tolerance(FVector2D const& expected,
                                        FVector2D const& actual,
                                        double const tolerance) {
        return expected.Equals(actual, tolerance);
    }
};

template <>
struct TestEqualityTraits<FVector3d> {
    static bool is_equal(FVector3d const& expected, FVector3d const& actual) {
        return expected == actual;
    }

    static bool is_equal_with_tolerance(FVector3d const& expected,
                                        FVector3d const& actual,
                                        double const tolerance) {
        return expected.Equals(actual, tolerance);
    }
};
}
