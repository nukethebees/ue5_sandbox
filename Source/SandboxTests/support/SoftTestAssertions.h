#pragma once

#include "TestDisplayTraits.h"
#include "TestEqualityTraits.h"

#include <SandboxCore/array_utils.h>
#include <SandboxCore/vector_concepts.h>
#include <SandboxCore/vector_traits.h>

#include <CoreMinimal.h>
#include <GameFramework/Actor.h>
#include <Misc/AutomationTest.h>

#include <functional>
#include <type_traits>

namespace ml {
struct FSoftTestAssertions {
    void store_result(bool result) noexcept;

    /* ---------------------------------------------------------------------------- */
    // Equal
    /* ---------------------------------------------------------------------------- */
    template <typename T, SoftTestDescription Description>
    bool are_equal(T const& exp,
                   T const& got,
                   Description const& description,
                   int32 const i = INDEX_NONE) {
        using Traits = TestEqualityTraits<std::remove_cvref_t<T>>;
        return check_equality<&Traits::is_equal, true>(exp, got, description, i);
    }
    template <typename T, typename Tolerance, SoftTestDescription Description>
    bool are_equal(T const& exp,
                   T const& got,
                   Tolerance const tolerance,
                   Description const& description,
                   int32 const i = INDEX_NONE) {
        using Traits = TestEqualityTraits<std::remove_cvref_t<T>>;
        return check_equality<&Traits::is_equal_with_tolerance, true>(
            exp, got, tolerance, description, i);
    }

    template <typename T, typename Tolerance, SoftTestDescription Description>
    bool not_equal(T const& a,
                   T const& b,
                   Tolerance const tolerance,
                   Description const& description,
                   int32 const i = INDEX_NONE) {
        using Traits = TestEqualityTraits<std::remove_cvref_t<T>>;
        return check_equality<&Traits::is_equal_with_tolerance, false>(
            a, b, tolerance, description, i);
    }

    template <typename T, SoftTestDescription Description>
    bool not_equal(T const& not_exp,
                   T const& got,
                   Description const& description,
                   int32 const i = INDEX_NONE) {
        using Traits = TestEqualityTraits<std::remove_cvref_t<T>>;
        return check_equality<&Traits::is_equal, false>(not_exp, got, description, i);
    }

    /* ---------------------------------------------------------------------------- */
    // All equal
    /* ---------------------------------------------------------------------------- */
    template <typename T, SoftTestDescription Description>
    bool all_equal(T const& exp, T const& got, Description const& description) {
        using Element = std::remove_cvref_t<decltype(exp[0])>;
        return all_equal_impl<&TestEqualityTraits<Element>::is_equal>(exp, got, description);
    }

    template <typename T, typename Tolerance, SoftTestDescription Description>
    bool all_equal(T const& exp,
                   T const& got,
                   Tolerance const tolerance,
                   Description const& description) {
        using Element = std::remove_cvref_t<decltype(exp[0])>;
        return all_equal_impl<&TestEqualityTraits<Element>::is_equal_with_tolerance>(
            exp, got, tolerance, description);
    }

    /* ---------------------------------------------------------------------------- */
    // Zero
    /* ---------------------------------------------------------------------------- */
    template <std::floating_point T, SoftTestDescription Description>
    bool is_zero(T const got,
                 T const tolerance,
                 Description const& description,
                 int32 const i = INDEX_NONE) {
        return are_equal(T{0}, got, tolerance, description, i);
    }
    template <std::floating_point T, SoftTestDescription Description>
    bool not_zero(T const got,
                  T const tolerance,
                  Description const& description,
                  int32 const i = INDEX_NONE) {
        return not_equal(T{0}, got, tolerance, description, i);
    }

    /* ---------------------------------------------------------------------------- */
    // Vector
    /* ---------------------------------------------------------------------------- */
    template <IsUnrealVector T, SoftTestDescription Description>
    bool dist_zero(T const& lhs,
                   T const& rhs,
                   VectorTraits<T>::Element const tolerance,
                   Description const& description,
                   int32 const i = INDEX_NONE) {
        auto const dist{T::Dist(lhs, rhs)};
        auto const result{FMath::IsNearlyZero(dist, tolerance)};
        store_result(result);
        if (should_display_result(result)) {
            reset_message(i);
            message += format_message(TEXT(R"({0} 
Check non-zero dist:
    Distance : {1}
    From     : {2}
    To       : {3} 
    Tolerance: {4})"),
                                      description,
                                      dist,
                                      lhs,
                                      rhs,
                                      tolerance);
            display_result(result, message);
        }

        return result;
    }
    template <IsUnrealVector T, SoftTestDescription Description>
    bool not_dist_zero(T const& lhs,
                       T const& rhs,
                       VectorTraits<T>::Element const tolerance,
                       Description const& description,
                       int32 const i = INDEX_NONE) {
        auto const dist{T::Dist(lhs, rhs)};
        auto const result{!FMath::IsNearlyZero(dist, tolerance)};
        store_result(result);
        if (should_display_result(result)) {
            reset_message(i);
            message += format_message(TEXT(R"({0} 
Check non-zero dist:
    Distance : {1}
    From     : {2}
    To       : {3} 
    Tolerance: {4})"),
                                      description,
                                      dist,
                                      lhs,
                                      rhs,
                                      tolerance);
            display_result(result, message);
        }

        return result;
    }

    template <IsUnrealVector T, SoftTestDescription Description>
    bool dist_greater_than(T const& lhs,
                           T const& rhs,
                           VectorTraits<T>::Element const threshold,
                           Description const& description,
                           int32 const i = INDEX_NONE) {
        auto const dist{T::Dist(lhs, rhs)};
        auto const result{dist > threshold};
        store_result(result);
        if (should_display_result(result)) {
            reset_message(i);
            message += format_message(TEXT(R"({0} 
Check dist > {1}:
    Distance : {2}
    From     : {3}
    To       : {4})"),
                                      description,
                                      threshold,
                                      dist,
                                      lhs,
                                      rhs);
            display_result(result, message);
        }

        return result;
    }

    /* ---------------------------------------------------------------------------- */
    // Comparison
    /* ---------------------------------------------------------------------------- */
    template <typename T, SoftTestDescription Description>
    bool is_greater_than(T const& lhs,
                         T const& rhs,
                         Description const& description,
                         int32 const i = INDEX_NONE) {
        return compare<std::greater<>{}>(lhs, rhs, TEXT(">"), description, i);
    }

    template <typename T, SoftTestDescription Description>
    bool is_less_than(T const& lhs,
                      T const& rhs,
                      Description const& description,
                      int32 const i = INDEX_NONE) {
        return compare<std::less<>{}>(lhs, rhs, TEXT("<"), description, i);
    }

    template <typename T, SoftTestDescription Description>
    bool is_less_equal_than(T const& lhs,
                            T const& rhs,
                            Description const& description,
                            int32 const i = INDEX_NONE) {
        return compare<std::less_equal<>{}>(lhs, rhs, TEXT("<="), description, i);
    }

    /* ---------------------------------------------------------------------------- */
    // Bool
    /* ---------------------------------------------------------------------------- */
    template <SoftTestDescription Description>
    bool is_true(bool result, Description const& description, int32 const i = INDEX_NONE) {
        store_result(result);
        if (should_display_result(result)) {
            reset_message(i);
            message += format_message(TEXT("{0} ({1})"), description, result);
            display_result(result, message);
        }
        return result;
    }

    /* ---------------------------------------------------------------------------- */
    // Pointer
    /* ---------------------------------------------------------------------------- */
    template <SoftTestDescription Description>
    bool not_nullptr(void const* ptr, Description const& description) {
        return is_true(ptr != nullptr, description);
    }
    template <SoftTestDescription Description>
    bool is_valid(AActor const* ptr, Description const& description) {
        return is_true(IsValid(ptr), description);
    }

    void reset();

    FAutomationTestBase* test_runner{nullptr};
    bool log_successful_assertions{false};
    bool all_passed{true};
  private:
    void display_result(bool passed, FString const& msg);
    bool should_display_result(bool passed) const noexcept {
        return !passed || log_successful_assertions;
    }
    void reset_message(int32 i = INDEX_NONE);

    template <typename... Args>
    static auto format_message(TCHAR const* const format, Args const&... args) -> FString {
        FStringFormatOrderedArguments format_args;
        format_args.Reserve(sizeof...(args));
        (format_args.Emplace(make_test_display_arg(args)), ...);
        return FString::Format(format, format_args);
    }

    template <auto is_equal, bool expect_equal, typename T, SoftTestDescription Description>
    bool check_equality(T const& exp, T const& got, Description const& description, int32 const i) {
        auto const result{is_equal(exp, got) == expect_equal};
        store_result(result);

        if (should_display_result(result)) {
            reset_message(i);
            if constexpr (expect_equal) {
                message += format_message(TEXT("{0} (Exp {1}, Got {2})"), description, exp, got);
            } else {
                message += format_message(TEXT("{0} (Expect {1} != {2})"), description, exp, got);
            }
            display_result(result, message);
        }

        return result;
    }

    template <auto is_equal,
              bool expect_equal,
              typename T,
              typename Tolerance,
              SoftTestDescription Description>
    bool check_equality(T const& exp,
                        T const& got,
                        Tolerance const tolerance,
                        Description const& description,
                        int32 const i) {
        auto const result{is_equal(exp, got, tolerance) == expect_equal};
        store_result(result);

        if (should_display_result(result)) {
            reset_message(i);
            if constexpr (expect_equal) {
                message += format_message(TEXT("{0} (Exp {1}, Got {2} (tolerance={3}))"),
                                          description,
                                          exp,
                                          got,
                                          tolerance);
            } else {
                message += format_message(TEXT("{0} (Expect {1} != {2}, tolerance={3})"),
                                          description,
                                          exp,
                                          got,
                                          tolerance);
            }
            display_result(result, message);
        }

        return result;
    }

    template <auto is_equal, typename T, SoftTestDescription Description>
    bool all_equal_impl(T const& exp, T const& got, Description const& description) {
        indices.Reset();

        auto const n_exp{ml::num(exp)};
        auto const n_got{ml::num(got)};
        if (n_exp != n_got) {
            store_result(false);
            message = format_message(
                TEXT("{0} (Expected {1} elements, got {2})"), description, n_exp, n_got);
            display_result(false, message);
            return false;
        }

        for (int32 i{0}; i < n_exp; ++i) {
            if (!is_equal(exp[i], got[i])) {
                indices.Add(i);
            }
        }

        return display_all_equal_result(exp, got, description);
    }

    template <auto is_equal, typename T, typename Tolerance, SoftTestDescription Description>
    bool all_equal_impl(T const& exp,
                        T const& got,
                        Tolerance const tolerance,
                        Description const& description) {
        indices.Reset();

        auto const n_exp{ml::num(exp)};
        auto const n_got{ml::num(got)};
        if (n_exp != n_got) {
            store_result(false);
            message = format_message(
                TEXT("{0} (Expected {1} elements, got {2})"), description, n_exp, n_got);
            display_result(false, message);
            return false;
        }

        for (int32 i{0}; i < n_exp; ++i) {
            if (!is_equal(exp[i], got[i], tolerance)) {
                indices.Add(i);
            }
        }

        return display_all_equal_result(exp, got, description);
    }

    template <typename T, SoftTestDescription Description>
    bool display_all_equal_result(T const& exp, T const& got, Description const& description) {

        auto const result{indices.IsEmpty()};
        store_result(result);
        if (result) {
            if (log_successful_assertions) {
                message = format_message(TEXT("{0}"), description);
                display_result(true, message);
            }
            return true;
        }

        message =
            format_message(TEXT("{0} ({1} mismatched elements):"), description, indices.Num());
        auto const num_indices{indices.Num()};
        for (int32 i{0}; i < num_indices; ++i) {
            auto const index{indices[i]};
            message +=
                format_message(TEXT("\n[{0}] (Exp {1}, Got {2})"), index, exp[index], got[index]);
        }
        display_result(false, message);

        return false;
    }

    template <auto comparison, typename T, SoftTestDescription Description>
    bool compare(T const& lhs,
                 T const& rhs,
                 TCHAR const* const operator_text,
                 Description const& description,
                 int32 const i = INDEX_NONE) {
        auto const result{comparison(lhs, rhs)};
        store_result(result);

        if (should_display_result(result)) {
            reset_message(i);
            message += format_message(
                TEXT("{0} (Expect: {1} {2} {3})"), description, lhs, operator_text, rhs);
            display_result(result, message);
        }

        return result;
    }

    TArray<int32> indices;
    FString message;
};

#define SANDBOX_TESTS_ASSERT_ALL_PASSED(CHECKS_INSTANCE)                     \
    do {                                                                     \
        ASSERT_THAT(IsTrue(CHECKS_INSTANCE.all_passed, TEXT("all_passed"))); \
    } while (0)
}
