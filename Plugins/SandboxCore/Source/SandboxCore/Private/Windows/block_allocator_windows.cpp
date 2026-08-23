#include "block_allocator_windows.h"

#include <SandboxCore/log_categories.h>

#include <Misc/AssertionMacros.h>

#include "Windows/AllowWindowsPlatformTypes.h"

#include <memoryapi.h>
#include <windows.h>

#include "Windows/HideWindowsPlatformTypes.h"

#include <limits>

extern "C" {
    PVOID
    WINAPI
    VirtualAlloc2(_In_opt_ HANDLE Process,
                  _In_opt_ PVOID BaseAddress,
                  _In_ SIZE_T Size,
                  _In_ ULONG AllocationType,
                  _In_ ULONG PageProtection,
                  _Inout_updates_opt_(ParameterCount) MEM_EXTENDED_PARAMETER* ExtendedParameters,
                  _In_ ULONG ParameterCount);
}

namespace ml::detail {
namespace {
constexpr SIZE_T huge_page_bytes{SIZE_T{1} << 30};

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

auto lock_memory_privilege() -> FPrivilegeResult const& {
    static FPrivilegeResult const result{enable_lock_memory_privilege()};
    return result;
}

auto round_up_to_huge_page(SIZE_T const bytes) -> SIZE_T {
    checkf(bytes <= std::numeric_limits<SIZE_T>::max() - (huge_page_bytes - 1),
           TEXT("Block allocation is too large to round to a 1 GiB page boundary."));
    return (bytes + huge_page_bytes - 1) & ~(huge_page_bytes - 1);
}

auto allocate_default_pages(SIZE_T const bytes) -> void* {
    constexpr HANDLE process{nullptr};
    constexpr PVOID base_address{nullptr};
    constexpr ULONG allocation_type{MEM_RESERVE | MEM_COMMIT};
    constexpr ULONG page_protection{PAGE_READWRITE};

    return VirtualAlloc2(
        process, base_address, bytes, allocation_type, page_protection, nullptr, 0);
}

auto allocate_huge_pages(SIZE_T const bytes) -> void* {
    constexpr HANDLE process{nullptr};
    constexpr PVOID base_address{nullptr};
    constexpr ULONG allocation_type{MEM_RESERVE | MEM_COMMIT | MEM_LARGE_PAGES};
    constexpr ULONG page_protection{PAGE_READWRITE};

    MEM_EXTENDED_PARAMETER parameter{};
    parameter.Type = MemExtendedParameterAttributeFlags;
    parameter.ULong64 = MEM_EXTENDED_PARAMETER_NONPAGED_HUGE;

    SIZE_T const rounded_bytes{round_up_to_huge_page(bytes)};
    return VirtualAlloc2(
        process, base_address, rounded_bytes, allocation_type, page_protection, &parameter, 1);
}

void report_huge_page_failure(EVirtualAlloc2PageMode const page_mode,
                              TCHAR const* const reason,
                              DWORD const error) {
    if (page_mode == EVirtualAlloc2PageMode::PreferHugePages) {
        UE_LOG(LogSandboxCore,
               Warning,
               TEXT("VirtualAlloc2 could not allocate 1 GiB pages (%s, error %lu); falling back "
                    "to ordinary pages."),
               reason,
               error);
        return;
    }

    UE_LOG(LogSandboxCore,
           Error,
           TEXT("VirtualAlloc2 could not allocate required 1 GiB pages (%s, error %lu)."),
           reason,
           error);
    checkf(false,
           TEXT("VirtualAlloc2 could not allocate required 1 GiB pages (%s, error %lu)."),
           reason,
           error);
}
}

auto allocate_virtual_alloc2(SIZE_T const bytes,
                             SIZE_T const alignment,
                             EVirtualAlloc2PageMode const page_mode) -> void* {
    static_cast<void>(alignment);

    void* data{nullptr};
    if (page_mode != EVirtualAlloc2PageMode::Default) {
        FPrivilegeResult const& privilege{lock_memory_privilege()};
        if (privilege.enabled) {
            data = allocate_huge_pages(bytes);
            if (!data) {
                report_huge_page_failure(page_mode, TEXT("allocation failed"), GetLastError());
            }
        } else {
            report_huge_page_failure(
                page_mode, TEXT("SeLockMemoryPrivilege is unavailable"), privilege.error);
        }
    }

    if (!data && page_mode != EVirtualAlloc2PageMode::RequireHugePages) {
        data = allocate_default_pages(bytes);
    }

    if (!data) {
        DWORD const error{GetLastError()};
        UE_LOG(LogSandboxCore, Error, TEXT("VirtualAlloc2 failed: %lu"), error);
        checkf(false, TEXT("VirtualAlloc2 failed: %lu"), error);
    }

    return data;
}

void free_virtual_alloc2(void* const data) {
    check(VirtualFree(data, 0, MEM_RELEASE) != 0);
}
}
