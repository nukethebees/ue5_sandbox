#pragma once

#include <CoreMinimal.h>

namespace ml {
template <typename TestRunnerType>
struct FSoftTestAssertions {
    inline static auto to_string(bool b) -> TCHAR const* {
        return b ? TEXT("true") : TEXT("false");
    }

    void display_result(bool const passed, FString const& msg) {
        if (!passed) {
            test_runner->AddError(msg);
        } else if (log_successful_assertions) {
            test_runner->AddInfo(msg);
        }
    }

    void store_result(bool const result) noexcept { all_passed &= result; }

    template <typename T>
    bool are_equal(T const& exp, T const& got, FString const description) {
        auto const result{exp == got};
        store_result(result);

        auto const exp_str{LexToString(exp)};
        auto const got_str{LexToString(got)};
        auto msg{FString::Printf(TEXT("%s (Exp %s, Got %s)"), *description, *exp_str, *got_str)};

        display_result(result, msg);

        return result;
    }
    bool is_true(bool result, FString const description) {
        store_result(result);

        auto msg{FString::Printf(TEXT("%s (%s)"), *description, to_string(result))};

        display_result(result, msg);

        return result;
    }

    TestRunnerType* test_runner{nullptr};
    bool log_successful_assertions{false};
    bool all_passed{true};
};
}
