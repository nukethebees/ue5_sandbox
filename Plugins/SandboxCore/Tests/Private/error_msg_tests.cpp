#include <SandboxCore/error_msg.h>

#include "TestHarness.h"

TEST_CASE("SandboxCore.ErrorMsg.IsFalseWhenEmpty") {
    ml::FErrorMsg error_msg;

    CHECK_FALSE(static_cast<bool>(error_msg));
}

TEST_CASE("SandboxCore.ErrorMsg.IsTrueWhenMessageIsPresent") {
    ml::FErrorMsg error_msg;
    error_msg.message = TEXT("error");

    CHECK(static_cast<bool>(error_msg));
}
