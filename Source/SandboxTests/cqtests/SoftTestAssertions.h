#pragma once

#include "lex_to_string.h"

#include <SandboxCore/array_utils.h>

#include <CoreMinimal.h>
#include <Misc/AutomationTest.h>

class AActor;

namespace ml {
struct FSoftTestAssertions {
    static constexpr auto to_string(bool b) -> TCHAR const* {
        return b ? TEXT("true") : TEXT("false");
    }

    void display_result(bool const passed, FString const& msg);

    template <typename T>
        requires requires(T const& t) {
            { LexToString(t) } -> std::convertible_to<FString>;
        }
    static auto to_string(T const& value) -> FString {
        return LexToString(value);
    }

    auto start_msg(int32 i = INDEX_NONE) const -> FString;
    void store_result(bool result) noexcept;

    template <typename T>
    bool are_equal(T const& exp, T const& got, FString const description) {
        auto const result{exp == got};
        store_result(result);

        auto msg{FString::Printf(
            TEXT("%s (Exp %s, Got %s)"), *description, *to_string(exp), *to_string(got))};

        display_result(result, msg);

        return result;
    }
    template <typename T>
    bool not_equal(T const& not_exp,
                   T const& got,
                   FString const description,
                   int32 const i = INDEX_NONE) {
        auto const result{not_exp != got};
        store_result(result);

        FString msg{start_msg(i)};
        msg += FString::Printf(
            TEXT("%s (%s vs %s)"), *description, *to_string(not_exp), *to_string(got));

        display_result(result, msg);

        return result;
    }

    bool is_true(bool result, FString const description, int32 const i = INDEX_NONE) {
        store_result(result);

        FString msg{start_msg(i)};
        msg += FString::Printf(TEXT("%s (%s)"), *description, to_string(result));

        display_result(result, msg);

        return result;
    }

    template <typename T>
    bool all_equal(T const& exp, T const& got, FString const description) {
        auto const n_exp{ml::num(exp)};
        auto const n_got{ml::num(got)};

        if (!are_equal(n_exp, n_got, description)) { return false; }

        for (int32 i{0}; i < n_exp; ++i) {
            are_equal(exp[i], got[i], FString::Printf(TEXT("[%d] %s"), i, *description));
        }

        return true;
    }

    bool not_nullptr(void* ptr, FString const description);
    bool is_valid(AActor* ptr, FString const description);

    FAutomationTestBase* test_runner{nullptr};
    bool log_successful_assertions{false};
    bool all_passed{true};
};

#define SANDBOX_TESTS_ASSERT_ALL_PASSED(CHECKS_INSTANCE)                     \
    do {                                                                     \
        ASSERT_THAT(IsTrue(CHECKS_INSTANCE.all_passed, TEXT("all_passed"))); \
    } while (0)
}
