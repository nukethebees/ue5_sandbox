#include "SoftTestAssertions.h"

#include <CoreMinimal.h>
#include <GameFramework/Actor.h>

namespace ml {
void FSoftTestAssertions::display_result(bool const passed, FString const& msg) {
    if (!passed) {
        test_runner->AddError(msg);
    } else if (log_successful_assertions) {
        test_runner->AddInfo(msg);
    }
}

void FSoftTestAssertions::reset_message(int32 const i) {
    message.Reset();
    if (i != INDEX_NONE) {
        message.Appendf(TEXT("[%d] "), i);
    }
}

void FSoftTestAssertions::store_result(bool const result) noexcept {
    all_passed &= result;
}

bool FSoftTestAssertions::is_true(bool result, FString const& description, int32 const i) {
    store_result(result);

    reset_message(i);
    message.Appendf(TEXT("%s (%s)"), *description, to_string(result));
    display_result(result, message);

    return result;
}

bool FSoftTestAssertions::not_nullptr(void const* ptr, FString const& description) {
    return is_true(ptr != nullptr, description);
}

bool FSoftTestAssertions::is_valid(AActor* ptr, FString const& description) {
    return is_true(IsValid(ptr), description);
}

void FSoftTestAssertions::reset() {
    *this = {};
}
}
