#include "SoftTestAssertions.h"

#include <CoreMinimal.h>

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

void FSoftTestAssertions::reset() {
    *this = {};
}
}
