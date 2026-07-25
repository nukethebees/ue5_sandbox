#include "SoftTestAssertions.h"

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
}
