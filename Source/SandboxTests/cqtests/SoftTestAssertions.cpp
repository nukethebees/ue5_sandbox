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

auto FSoftTestAssertions::start_msg(int32 const i) const -> FString {
    if (i != INDEX_NONE) { return FString::Printf(TEXT("[%d] "), i); }
    return {};
}

void FSoftTestAssertions::store_result(bool const result) noexcept {
    all_passed &= result;
}

bool FSoftTestAssertions::not_nullptr(void* ptr, FString const description) {
    return is_true(ptr != nullptr, description);
}

bool FSoftTestAssertions::is_valid(AActor* ptr, FString const description) {
    return is_true(IsValid(ptr), description);
}
}
