#include <SandboxCore/block_allocator.h>

#include "CoreMinimal.h"
#include "TestHarness.h"

#include <cstdint>
#include <type_traits>

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"

#include <windows.h>

#include "Windows/HideWindowsPlatformTypes.h"
#endif

namespace {
struct alignas(64) FOverAlignedValue {
    uint64 value{0};
};
}

TEST_CASE("SandboxCore.FBlockAllocator.Allocate returns a typed non-null block") {
    ml::FBlockAllocator allocator{ml::EBlockAllocationMode::Unreal};

    int32* const block{allocator.allocate<int32>(1)};

    CHECK(block != nullptr);
}

TEST_CASE("SandboxCore.FBlockAllocator.Allocated typed memory can be written and read") {
    ml::FBlockAllocator allocator{ml::EBlockAllocationMode::Unreal};

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
    ml::FBlockAllocator allocator{ml::EBlockAllocationMode::Unreal};

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
    ml::FBlockAllocator allocator{ml::EBlockAllocationMode::Unreal};

    int32* const first_block{allocator.allocate<int32>(1)};
    float* const second_block{allocator.allocate<float>(1)};

    *first_block = 10;
    *second_block = 20.0f;

    CHECK(*first_block == 10);
    CHECK(*second_block == 20.0f);
}

TEST_CASE("SandboxCore.FBlockAllocator.Allocates over-aligned storage") {
    ml::FBlockAllocator allocator{ml::EBlockAllocationMode::Unreal};

    FOverAlignedValue* const block{allocator.allocate<FOverAlignedValue>(1)};

    auto const address{reinterpret_cast<uintptr_t>(block)};
    CHECK(block != nullptr);
    CHECK(address % alignof(FOverAlignedValue) == 0);
}

TEST_CASE("SandboxCore.FBlockAllocator.Destruction frees owned blocks") {
    {
        ml::FBlockAllocator allocator{ml::EBlockAllocationMode::Unreal};

        CHECK(allocator.allocate<uint8>(8) != nullptr);
        CHECK(allocator.allocate<uint8>(16) != nullptr);
        CHECK(allocator.allocate<uint8>(32) != nullptr);
    }

    ml::FBlockAllocator subsequent_allocator{ml::EBlockAllocationMode::Unreal};
    CHECK(subsequent_allocator.allocate<uint8>(8) != nullptr);
}

TEST_CASE("SandboxCore.FBlockAllocator.Supports its fixed allocation capacity") {
    ml::FBlockAllocator allocator{ml::EBlockAllocationMode::Unreal};

    for (int32 i{0}; i < 8; ++i) {
        CHECK(allocator.allocate<uint8>(1) != nullptr);
    }
}

TEST_CASE("SandboxCore.FBlockAllocator.Is not copyable") {
    static_assert(!std::is_copy_constructible_v<ml::FBlockAllocator>);
    static_assert(!std::is_copy_assignable_v<ml::FBlockAllocator>);
}

TEST_CASE("SandboxCore.FBlockAllocator.Retains its allocation mode") {
    ml::FBlockAllocator allocator{ml::EBlockAllocationMode::Unreal};

    CHECK(allocator.allocation_mode() == ml::EBlockAllocationMode::Unreal);
}

TEST_CASE("SandboxCore.FBlockAllocator.Move transfers allocation mode and ownership") {
    ml::FBlockAllocator source{ml::EBlockAllocationMode::Unreal};
    CHECK(source.allocate<uint8>(8) != nullptr);

    ml::FBlockAllocator moved{MoveTemp(source)};
    CHECK(moved.allocation_mode() == ml::EBlockAllocationMode::Unreal);

    ml::FBlockAllocator destination{ml::EBlockAllocationMode::VirtualAlloc2};
    destination = MoveTemp(moved);

    CHECK(destination.allocation_mode() == ml::EBlockAllocationMode::Unreal);
    CHECK(destination.allocate<uint8>(8) != nullptr);
}

#if PLATFORM_WINDOWS
TEST_CASE("SandboxCore.FBlockAllocator.VirtualAlloc2 allocates typed writable memory") {
    ml::FBlockAllocator allocator{ml::EBlockAllocationMode::VirtualAlloc2};

    int32* const block{allocator.allocate<int32>(3)};
    block[0] = 10;
    block[1] = 20;
    block[2] = 30;

    CHECK(block != nullptr);
    CHECK(block[0] == 10);
    CHECK(block[1] == 20);
    CHECK(block[2] == 30);
}

TEST_CASE("SandboxCore.FBlockAllocator.VirtualAlloc2 allocations coexist") {
    ml::FBlockAllocator allocator{ml::EBlockAllocationMode::VirtualAlloc2};

    uint8* const first_block{allocator.allocate<uint8>(1)};
    uint64* const second_block{allocator.allocate<uint64>(1)};
    *first_block = 10;
    *second_block = 20;

    CHECK(*first_block == 10);
    CHECK(*second_block == 20);
}

TEST_CASE("SandboxCore.FBlockAllocator.VirtualAlloc2 preserves requested alignment") {
    ml::FBlockAllocator allocator{ml::EBlockAllocationMode::VirtualAlloc2};

    FOverAlignedValue* const block{allocator.allocate<FOverAlignedValue>(1)};
    auto const address{reinterpret_cast<uintptr_t>(block)};

    CHECK(address % alignof(FOverAlignedValue) == 0);
}

TEST_CASE("SandboxCore.FBlockAllocator.VirtualAlloc2 allocations are committed virtual memory") {
    ml::FBlockAllocator allocator{ml::EBlockAllocationMode::VirtualAlloc2};
    void* const block{allocator.allocate<uint8>(1)};
    MEMORY_BASIC_INFORMATION memory_info{};

    CHECK(VirtualQuery(block, &memory_info, sizeof(memory_info)) == sizeof(memory_info));
    CHECK(memory_info.State == MEM_COMMIT);
    CHECK(memory_info.AllocationBase == block);
}

TEST_CASE("SandboxCore.FBlockAllocator.VirtualAlloc2 destruction releases owned blocks") {
    void* block{nullptr};

    {
        ml::FBlockAllocator allocator{ml::EBlockAllocationMode::VirtualAlloc2};
        block = allocator.allocate<uint8>(1);
    }

    MEMORY_BASIC_INFORMATION memory_info{};
    CHECK(VirtualQuery(block, &memory_info, sizeof(memory_info)) == sizeof(memory_info));
    CHECK(memory_info.State == MEM_FREE);
}
#endif
