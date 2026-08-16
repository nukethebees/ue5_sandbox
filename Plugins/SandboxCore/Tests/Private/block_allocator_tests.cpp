#include <SandboxCore/block_allocator.h>

#include "CoreMinimal.h"
#include "TestHarness.h"

#include <cstdint>
#include <type_traits>

namespace {
struct alignas(64) FOverAlignedValue {
    uint64 value{0};
};
}

TEST_CASE("SandboxCore.FBlockAllocator.Allocate returns a typed non-null block") {
    ml::FBlockAllocator allocator{};

    int32* const block{allocator.allocate<int32>(1)};

    CHECK(block != nullptr);
}

TEST_CASE("SandboxCore.FBlockAllocator.Allocated typed memory can be written and read") {
    ml::FBlockAllocator allocator{};

    int32* const block{allocator.allocate<int32>(4)};
    block[0] = 17;
    block[1] = 34;
    block[2] = 51;
    block[3] = 68;

    CHECK(block[0] == 17);
    CHECK(block[1] == 34);
    CHECK(block[2] == 51);
    CHECK(block[3] == 68);
}

TEST_CASE("SandboxCore.FBlockAllocator.Allocates different element sizes") {
    ml::FBlockAllocator allocator{};

    uint8* const small_elements{allocator.allocate<uint8>(2)};
    uint64* const large_elements{allocator.allocate<uint64>(2)};

    small_elements[0] = 10;
    small_elements[1] = 20;
    large_elements[0] = 30;
    large_elements[1] = 40;

    CHECK(small_elements[0] == 10);
    CHECK(small_elements[1] == 20);
    CHECK(large_elements[0] == 30);
    CHECK(large_elements[1] == 40);
}

TEST_CASE("SandboxCore.FBlockAllocator.Allocates independent typed blocks") {
    ml::FBlockAllocator allocator{};

    int32* const first_block{allocator.allocate<int32>(1)};
    float* const second_block{allocator.allocate<float>(1)};

    *first_block = 10;
    *second_block = 20.0f;

    CHECK(*first_block == 10);
    CHECK(*second_block == 20.0f);
}

TEST_CASE("SandboxCore.FBlockAllocator.Allocates over-aligned storage") {
    ml::FBlockAllocator allocator{};

    FOverAlignedValue* const block{allocator.allocate<FOverAlignedValue>(1)};

    auto const address{reinterpret_cast<uintptr_t>(block)};
    CHECK(block != nullptr);
    CHECK(address % alignof(FOverAlignedValue) == 0);
}

TEST_CASE("SandboxCore.FBlockAllocator.Destruction frees owned blocks") {
    {
        ml::FBlockAllocator allocator{};

        CHECK(allocator.allocate<uint8>(8) != nullptr);
        CHECK(allocator.allocate<uint8>(16) != nullptr);
        CHECK(allocator.allocate<uint8>(32) != nullptr);
    }

    ml::FBlockAllocator subsequent_allocator{};
    CHECK(subsequent_allocator.allocate<uint8>(8) != nullptr);
}

TEST_CASE("SandboxCore.FBlockAllocator.Supports its fixed allocation capacity") {
    ml::FBlockAllocator allocator{};

    for (int32 i{0}; i < 8; ++i) {
        CHECK(allocator.allocate<uint8>(1) != nullptr);
    }
}

TEST_CASE("SandboxCore.FBlockAllocator.Is not copyable") {
    static_assert(!std::is_copy_constructible_v<ml::FBlockAllocator>);
    static_assert(!std::is_copy_assignable_v<ml::FBlockAllocator>);
}
