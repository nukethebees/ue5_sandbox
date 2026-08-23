#include <SandboxCore/block_allocator.h>

#include "CoreMinimal.h"
#include "TestHarness.h"

#include <cstdint>
#include <type_traits>

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

namespace {
struct alignas(64) FOverAlignedValue {
    uint64 value{0};
};

template <typename Test>
void for_each_standard_allocator_configuration(Test&& test) {
    auto run{[&](char const* const name, ml::EBlockAllocationMode const allocation_mode, ml::EVirtualAlloc2PageMode const page_mode) {
        SECTION(name) {
            test(allocation_mode, page_mode);
        }
    }};

    run("Unreal", ml::EBlockAllocationMode::Unreal, ml::EVirtualAlloc2PageMode::Default);
#if PLATFORM_WINDOWS
    run("VirtualAlloc2 default pages", ml::EBlockAllocationMode::VirtualAlloc2, ml::EVirtualAlloc2PageMode::Default);
#endif
}

template <typename Test>
void for_each_allocator_configuration(Test&& test) {
    for_each_standard_allocator_configuration(test);
#if PLATFORM_WINDOWS
    auto run{[&](char const* const name, ml::EVirtualAlloc2PageMode const page_mode) {
        SECTION(name) {
            test(ml::EBlockAllocationMode::VirtualAlloc2, page_mode);
        }
    }};

    run("VirtualAlloc2 preferred huge pages", ml::EVirtualAlloc2PageMode::PreferHugePages);
    run("VirtualAlloc2 required huge pages", ml::EVirtualAlloc2PageMode::RequireHugePages);
#endif
}

#if PLATFORM_WINDOWS
struct FPrivilegeResult {
    bool enabled{false};
    DWORD error{ERROR_SUCCESS};
};

auto enable_lock_memory_privilege() -> FPrivilegeResult {
    HANDLE token{nullptr};
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token)) {
        return {.enabled = false, .error = GetLastError()};
    }

    LUID privilege_id{};
    if (!LookupPrivilegeValue(nullptr, SE_LOCK_MEMORY_NAME, &privilege_id)) {
        DWORD const error{GetLastError()};
        CloseHandle(token);
        return {.enabled = false, .error = error};
    }

    TOKEN_PRIVILEGES privileges{};
    privileges.PrivilegeCount = 1;
    privileges.Privileges[0].Luid = privilege_id;
    privileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    SetLastError(ERROR_SUCCESS);
    BOOL const adjusted{AdjustTokenPrivileges(token, 0, &privileges, 0, nullptr, nullptr)};
    DWORD const error{GetLastError()};
    CloseHandle(token);

    return {.enabled = adjusted != 0 && error == ERROR_SUCCESS, .error = error};
}
#endif
}

TEST_CASE("SandboxCore.FBlockAllocator.Allocated typed memory is writable") {
    for_each_standard_allocator_configuration([](auto const allocation_mode, auto const page_mode) {
        ml::FBlockAllocator allocator{allocation_mode, page_mode};
        uint8* const small_elements{allocator.allocate<uint8>(2)};
        uint64* const large_elements{allocator.allocate<uint64>(2)};
        small_elements[0] = 10;
        small_elements[1] = 20;
        large_elements[0] = 30;
        large_elements[1] = 40;

        CHECK(small_elements != nullptr);
        CHECK(large_elements != nullptr);
        CHECK(small_elements[0] == 10);
        CHECK(small_elements[1] == 20);
        CHECK(large_elements[0] == 30);
        CHECK(large_elements[1] == 40);
    });
}

TEST_CASE("SandboxCore.FBlockAllocator.Allocates independent typed blocks") {
    for_each_standard_allocator_configuration([](auto const allocation_mode, auto const page_mode) {
        ml::FBlockAllocator allocator{allocation_mode, page_mode};
        int32* const first_block{allocator.allocate<int32>(1)};
        float* const second_block{allocator.allocate<float>(1)};
        *first_block = 10;
        *second_block = 20.0f;

        CHECK(*first_block == 10);
        CHECK(*second_block == 20.0f);
    });
}

TEST_CASE("SandboxCore.FBlockAllocator.Allocates over-aligned storage") {
    for_each_standard_allocator_configuration([](auto const allocation_mode, auto const page_mode) {
        ml::FBlockAllocator allocator{allocation_mode, page_mode};
        FOverAlignedValue* const block{allocator.allocate<FOverAlignedValue>(1)};
        auto const address{reinterpret_cast<uintptr_t>(block)};

        CHECK(block != nullptr);
        CHECK(address % alignof(FOverAlignedValue) == 0);
    });
}

TEST_CASE("SandboxCore.FBlockAllocator.Supports its fixed allocation capacity") {
    for_each_standard_allocator_configuration([](auto const allocation_mode, auto const page_mode) {
        ml::FBlockAllocator allocator{allocation_mode, page_mode};
        for (int32 i{0}; i < 8; ++i) {
            CHECK(allocator.allocate<uint8>(1) != nullptr);
        }
    });
}

TEST_CASE("SandboxCore.FBlockAllocator.Destruction frees owned blocks") {
    for_each_standard_allocator_configuration([](auto const allocation_mode, auto const page_mode) {
        {
            ml::FBlockAllocator allocator{allocation_mode, page_mode};
            CHECK(allocator.allocate<uint8>(8) != nullptr);
            CHECK(allocator.allocate<uint8>(16) != nullptr);
            CHECK(allocator.allocate<uint8>(32) != nullptr);
        }

        ml::FBlockAllocator subsequent_allocator{allocation_mode, page_mode};
        CHECK(subsequent_allocator.allocate<uint8>(8) != nullptr);
    });
}

TEST_CASE("SandboxCore.FBlockAllocator.Is not copyable") {
    static_assert(!std::is_copy_constructible_v<ml::FBlockAllocator>);
    static_assert(!std::is_copy_assignable_v<ml::FBlockAllocator>);
}

TEST_CASE("SandboxCore.FBlockAllocator.Retains its configuration") {
    for_each_allocator_configuration([](auto const allocation_mode, auto const page_mode) {
        ml::FBlockAllocator allocator{allocation_mode, page_mode};
        CHECK(allocator.allocation_mode() == allocation_mode);
        CHECK(allocator.virtual_alloc2_page_mode() == page_mode);
    });
}

TEST_CASE("SandboxCore.FBlockAllocator.Move transfers configuration") {
    for_each_allocator_configuration([](auto const allocation_mode, auto const page_mode) {
        ml::FBlockAllocator source{allocation_mode, page_mode};

        ml::FBlockAllocator moved{MoveTemp(source)};
        CHECK(moved.allocation_mode() == allocation_mode);
        CHECK(moved.virtual_alloc2_page_mode() == page_mode);

        ml::FBlockAllocator destination{ml::EBlockAllocationMode::Unreal};
        destination = MoveTemp(moved);
        CHECK(destination.allocation_mode() == allocation_mode);
        CHECK(destination.virtual_alloc2_page_mode() == page_mode);
    });
}

TEST_CASE("SandboxCore.FBlockAllocator.Move transfers ownership") {
    for_each_standard_allocator_configuration([](auto const allocation_mode, auto const page_mode) {
        ml::FBlockAllocator source{allocation_mode, page_mode};
        CHECK(source.allocate<uint8>(8) != nullptr);

        ml::FBlockAllocator destination{ml::EBlockAllocationMode::Unreal};
        destination = MoveTemp(source);
        CHECK(destination.allocate<uint8>(8) != nullptr);
    });
}

#if PLATFORM_WINDOWS
TEST_CASE("SandboxCore.FBlockAllocator.VirtualAlloc2 allocations are committed and released") {
    void* block{nullptr};
    {
        ml::FBlockAllocator allocator{ml::EBlockAllocationMode::VirtualAlloc2};
        block = allocator.allocate<uint8>(1);

        MEMORY_BASIC_INFORMATION memory_info{};
        CHECK(VirtualQuery(block, &memory_info, sizeof(memory_info)) == sizeof(memory_info));
        CHECK(memory_info.State == MEM_COMMIT);
        CHECK(memory_info.AllocationBase == block);
    }

    MEMORY_BASIC_INFORMATION memory_info{};
    CHECK(VirtualQuery(block, &memory_info, sizeof(memory_info)) == sizeof(memory_info));
    CHECK(memory_info.State == MEM_FREE);
}

TEST_CASE("SandboxCore.FBlockAllocator.VirtualAlloc2 allocates real 1 GiB pages") {
    FPrivilegeResult const privilege{enable_lock_memory_privilege()};
    INFO("Grant this account 'Lock pages in memory' in Local Security Policy, then sign out and "
         "back in. Windows error: "
         << privilege.error);
    REQUIRE(privilege.enabled);

    auto run{[](ml::EVirtualAlloc2PageMode const page_mode) {
        constexpr SIZE_T huge_page_bytes{SIZE_T{1} << 30};
        ml::FBlockAllocator allocator{ml::EBlockAllocationMode::VirtualAlloc2, page_mode};
        uint8* const block{allocator.allocate<uint8>(1)};
        block[0] = 42;

        MEMORY_BASIC_INFORMATION memory_info{};
        REQUIRE(VirtualQuery(block, &memory_info, sizeof(memory_info)) == sizeof(memory_info));
        CHECK(block[0] == 42);
        CHECK(memory_info.State == MEM_COMMIT);
        CHECK(memory_info.AllocationBase == block);
        CHECK(memory_info.RegionSize == huge_page_bytes);
    }};

    SECTION("preferred huge pages") {
        run(ml::EVirtualAlloc2PageMode::PreferHugePages);
    }
    SECTION("required huge pages") {
        run(ml::EVirtualAlloc2PageMode::RequireHugePages);
    }
}
#endif
