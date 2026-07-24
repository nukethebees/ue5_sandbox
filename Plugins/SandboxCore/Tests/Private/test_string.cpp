#include <SandboxCore/string.h>

#include "CoreMinimal.h"
#include "TestHarness.h"

namespace {
void test_fn(FString const& input, FString const& expected_output) {
    REQUIRE(ml::without_class_prefix(input) == expected_output);
}
}

TEST_CASE("SandboxCore.String.without_class_prefix.Empty string") {
    test_fn(TEXT(""), TEXT(""));
}

TEST_CASE("SandboxCore.String.without_class_prefix.Foo prefix") {
    test_fn(TEXT("Foo::bar"), TEXT("bar"));
}

TEST_CASE("SandboxCore.String.without_class_prefix.Foo") {
    test_fn(TEXT("Foo"), TEXT("Foo"));
}