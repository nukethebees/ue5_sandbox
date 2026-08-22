#include "SandboxCore/array_utils.h"

#include "CoreMinimal.h"
#include "TestHarness.h"

TEST_CASE("SandboxCore.Array.is_sorted_desc.Empty array is sorted", "[SandboxCore][Array][is_sorted_desc]") {
    TArray<int32> empty;
    REQUIRE(ml::is_sorted_desc(empty));
}

TEST_CASE("SandboxCore.Array.is_sorted_desc.Single element array is sorted", "[SandboxCore][Array][is_sorted_desc]") {
    TArray<int32> array{1};
    REQUIRE(ml::is_sorted_desc(array));
}

TEST_CASE("SandboxCore.Array.is_sorted_desc.Ascending numbers", "[SandboxCore][Array][is_sorted_desc]") {
    TArray<int32> array{};
    array.Reserve(10);

    array.Add(1);
    array.Add(2);
    array.Add(3);
    REQUIRE(!ml::is_sorted_desc(array));
}

TEST_CASE("SandboxCore.Array.is_sorted_desc.Ascending negative numbers", "[SandboxCore][Array][is_sorted_desc]") {
    TArray<int32> array{};
    array.Reserve(10);

    array.Add(-3);
    array.Add(-2);
    array.Add(-1);

    REQUIRE(!ml::is_sorted_desc(array));
}

TEST_CASE("SandboxCore.Array.is_sorted_desc.single zero", "[SandboxCore][Array][is_sorted_desc]") {
    TArray<int32> array{};
    array.Reserve(10);

    array.Add(0);
    REQUIRE(ml::is_sorted_desc(array));
}

TEST_CASE("SandboxCore.Array.is_sorted_desc.Two zeros", "[SandboxCore][Array][is_sorted_desc]") {
    TArray<int32> array{};
    array.Reserve(10);

    array.Add(0);
    array.Add(0);
    REQUIRE(ml::is_sorted_desc(array));
}

TEST_CASE("SandboxCore.Array.is_sorted_desc.Descending positive values", "[SandboxCore][Array][is_sorted_desc]") {
    TArray<int32> array{};
    array.Reserve(10);

    array.Add(3);
    array.Add(2);
    array.Add(1);
    REQUIRE(ml::is_sorted_desc(array));
}
