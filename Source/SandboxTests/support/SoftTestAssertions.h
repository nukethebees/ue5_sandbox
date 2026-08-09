#pragma once

#include "lex_to_string.h"
#include "TestEqualityTraits.h"

#include <Sandbox/utilities/enums.h>

#include <SandboxCore/array_utils.h>
#include <SandboxCore/vector_concepts.h>
#include <SandboxCore/vector_traits.h>

#include <CoreMinimal.h>
#include <Misc/AutomationTest.h>

#include <concepts>
#include <functional>
#include <type_traits>

class AActor;

namespace ml {
struct FSoftTestAssertions {
    static constexpr auto to_string(bool b) -> TCHAR const* {
        return b ? TEXT("true") : TEXT("false");
    }

    static auto to_string(FVector2D const& value) -> FString { return value.ToString(); }
    static auto to_string(FVector3d const& value) -> FString { return value.ToCompactString(); }

    void display_result(bool const passed, FString const& msg);

    template <typename T>
        requires supports_lex_to_string<T> || is_uenum<T>
    static auto to_string(T const& value) -> FString {
        if constexpr (supports_lex_to_string<T>) {
            return LexToString(value);
        } else {
            return ml::to_string_without_type_prefix(value);
        }
    }

    auto start_msg(int32 i = INDEX_NONE) const -> FString;
    void store_result(bool result) noexcept;

    /* ---------------------------------------------------------------------------- */
    // Equal
    /* ---------------------------------------------------------------------------- */
    template <typename T>
    bool are_equal(T const& exp,
                   T const& got,
                   FString const& description,
                   int32 const i = INDEX_NONE) {
        auto const result{exp == got};
        store_result(result);

        FString msg{start_msg(i)};

        msg += FString::Printf(
            TEXT("%s (Exp %s, Got %s)"), *description, *to_string(exp), *to_string(got));

        display_result(result, msg);

        return result;
    }
    template <std::floating_point T>
    bool are_equal(T const exp,
                   T const got,
                   T const tolerance,
                   FString const& description,
                   int32 const i = INDEX_NONE) {
        auto const result{FMath::IsNearlyEqual(exp, got, tolerance)};
        store_result(result);

        FString msg{start_msg(i)};

        msg += FString::Printf(TEXT("%s (Exp %s, Got %s (tolerance=%f))"),
                               *description,
                               *to_string(exp),
                               *to_string(got),
                               tolerance);

        display_result(result, msg);

        return result;
    }

    template <std::floating_point T>
    bool not_equal(T const a,
                   T const b,
                   T const tolerance,
                   FString const& description,
                   int32 const i = INDEX_NONE) {
        auto const result{!FMath::IsNearlyEqual(a, b, tolerance)};
        store_result(result);

        FString msg{start_msg(i)};

        msg += FString::Printf(TEXT("%s (Expect %s != %s, (tolerance=%f)"),
                               *description,
                               *to_string(a),
                               *to_string(b),
                               tolerance);

        display_result(result, msg);

        return result;
    }

    template <typename T>
    bool not_equal(T const& not_exp,
                   T const& got,
                   FString const& description,
                   int32 const i = INDEX_NONE) {
        auto const result{not_exp != got};
        store_result(result);
        display_result(result,
                       FString::Printf(TEXT("%s%s (%s vs %s)"),
                                       *start_msg(i),
                                       *description,
                                       *to_string(not_exp),
                                       *to_string(got)));

        return result;
    }

    /* ---------------------------------------------------------------------------- */
    // All equal
    /* ---------------------------------------------------------------------------- */
    template <typename T>
    bool all_equal(T const& exp, T const& got, FString const& description) {
        using Element = std::remove_cvref_t<decltype(exp[0])>;
        return all_equal_impl<&TestEqualityTraits<Element>::is_equal>(exp, got, description);
    }

    template <typename T, typename Tolerance>
    bool all_equal(T const& exp,
                   T const& got,
                   Tolerance const tolerance,
                   FString const& description) {
        using Element = std::remove_cvref_t<decltype(exp[0])>;
        return all_equal_impl<&TestEqualityTraits<Element>::is_equal_with_tolerance>(
            exp, got, tolerance, description);
    }

    /* ---------------------------------------------------------------------------- */
    // Zero
    /* ---------------------------------------------------------------------------- */
    template <std::floating_point T>
    bool is_zero(T const got,
                 T const tolerance,
                 FString const& description,
                 int32 const i = INDEX_NONE) {
        return are_equal(T{0}, got, tolerance, description, i);
    }
    template <std::floating_point T>
    bool not_zero(T const got,
                  T const tolerance,
                  FString const& description,
                  int32 const i = INDEX_NONE) {
        return not_equal(T{0}, got, tolerance, description, i);
    }

    /* ---------------------------------------------------------------------------- */
    // Vector
    /* ---------------------------------------------------------------------------- */
    template <IsUnrealVector T>
    bool dist_zero(T const& lhs,
                   T const& rhs,
                   VectorTraits<T>::Element const tolerance,
                   FString const& description,
                   int32 const i = INDEX_NONE) {
        auto const dist{T::Dist(lhs, rhs)};
        auto const result{FMath::IsNearlyZero(dist, tolerance)};
        store_result(result);
        display_result(result,
                       FString::Printf(TEXT(R"(%s%s 
Check non-zero dist:
    Distance : %f
    From     : %s
    To       : %s 
    Tolerance: %f)"),
                                       *start_msg(i),
                                       *description,
                                       dist,
                                       *lhs.ToCompactString(),
                                       *rhs.ToCompactString(),
                                       tolerance));

        return result;
    }
    template <IsUnrealVector T>
    bool not_dist_zero(T const& lhs,
                       T const& rhs,
                       VectorTraits<T>::Element const tolerance,
                       FString const& description,
                       int32 const i = INDEX_NONE) {
        auto const dist{T::Dist(lhs, rhs)};
        auto const result{!FMath::IsNearlyZero(dist, tolerance)};
        store_result(result);
        display_result(result,
                       FString::Printf(TEXT(R"(%s%s 
Check non-zero dist:
    Distance : %f
    From     : %s
    To       : %s 
    Tolerance: %f)"),
                                       *start_msg(i),
                                       *description,
                                       dist,
                                       *lhs.ToCompactString(),
                                       *rhs.ToCompactString(),
                                       tolerance));

        return result;
    }

    template <IsUnrealVector T>
    bool dist_greater_than(T const& lhs,
                           T const& rhs,
                           VectorTraits<T>::Element const threshold,
                           FString const& description,
                           int32 const i = INDEX_NONE) {
        auto const dist{T::Dist(lhs, rhs)};
        auto const result{dist > threshold};
        store_result(result);
        display_result(result,
                       FString::Printf(TEXT(R"(%s%s 
Check dist > %f:
    Distance : %f
    From     : %s
    To       : %s)"),
                                       *start_msg(i),
                                       *description,
                                       threshold,
                                       dist,
                                       *lhs.ToCompactString(),
                                       *rhs.ToCompactString()));

        return result;
    }

    /* ---------------------------------------------------------------------------- */
    // Comparison
    /* ---------------------------------------------------------------------------- */
    template <typename T>
    bool is_greater_than(T const& lhs,
                         T const& rhs,
                         FString const& description,
                         int32 const i = INDEX_NONE) {
        return compare<std::greater<>{}>(lhs, rhs, TEXT(">"), description, i);
    }

    template <typename T>
    bool is_less_than(T const& lhs,
                      T const& rhs,
                      FString const& description,
                      int32 const i = INDEX_NONE) {
        return compare<std::less<>{}>(lhs, rhs, TEXT("<"), description, i);
    }

    template <typename T>
    bool is_less_equal_than(T const& lhs,
                            T const& rhs,
                            FString const& description,
                            int32 const i = INDEX_NONE) {
        return compare<std::less_equal<>{}>(lhs, rhs, TEXT("<="), description, i);
    }

    /* ---------------------------------------------------------------------------- */
    // Bool
    /* ---------------------------------------------------------------------------- */
    bool is_true(bool result, FString const& description, int32 const i = INDEX_NONE);

    /* ---------------------------------------------------------------------------- */
    // Pointer
    /* ---------------------------------------------------------------------------- */
    bool not_nullptr(void const* ptr, FString const& description);
    bool is_valid(AActor* ptr, FString const& description);

    void reset();

    FAutomationTestBase* test_runner{nullptr};
    bool log_successful_assertions{false};
    bool all_passed{true};
  private:
    template <auto is_equal, typename T>
    bool all_equal_impl(T const& exp, T const& got, FString const& description) {
        indices.Reset();
        message.Reset();

        auto const n_exp{ml::num(exp)};
        auto const n_got{ml::num(got)};
        if (n_exp != n_got) {
            store_result(false);
            message.Appendf(TEXT("%s (Expected %d elements, got %d)"), *description, n_exp, n_got);
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

    template <auto is_equal, typename T, typename Tolerance>
    bool all_equal_impl(T const& exp,
                        T const& got,
                        Tolerance const tolerance,
                        FString const& description) {
        indices.Reset();
        message.Reset();

        auto const n_exp{ml::num(exp)};
        auto const n_got{ml::num(got)};
        if (n_exp != n_got) {
            store_result(false);
            message.Appendf(TEXT("%s (Expected %d elements, got %d)"), *description, n_exp, n_got);
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

    template <typename T>
    bool display_all_equal_result(T const& exp, T const& got, FString const& description) {

        auto const result{indices.IsEmpty()};
        store_result(result);
        if (result) {
            display_result(true, description);
            return true;
        }

        message.Appendf(TEXT("%s (%d mismatched elements):"), *description, indices.Num());
        auto const num_indices{indices.Num()};
        for (int32 i{0}; i < num_indices; ++i) {
            auto const index{indices[i]};
            message.Appendf(TEXT("\n[%d] (Exp %s, Got %s)"),
                            index,
                            *to_string(exp[index]),
                            *to_string(got[index]));
        }
        display_result(false, message);

        return false;
    }

    template <auto comparison, typename T>
    bool compare(T const& lhs,
                 T const& rhs,
                 TCHAR const* const operator_text,
                 FString const& description,
                 int32 const i = INDEX_NONE) {
        auto const result{comparison(lhs, rhs)};
        store_result(result);

        display_result(result,
                       FString::Printf(TEXT("%s%s (Expect: %s %s %s)"),
                                       *start_msg(i),
                                       *description,
                                       *to_string(lhs),
                                       operator_text,
                                       *to_string(rhs)));

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
