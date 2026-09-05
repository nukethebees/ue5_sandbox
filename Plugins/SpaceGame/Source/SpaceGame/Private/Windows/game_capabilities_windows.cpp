#include "HAL/Platform.h"

#if PLATFORM_WINDOWS

#include "game_capabilities_windows.h"

#include "SpaceGame/support/logging/SandboxLogCategories.h"

#include "Windows/AllowWindowsPlatformTypes.h"

#include <windows.h>

#include "Windows/HideWindowsPlatformTypes.h"

namespace ml::ioj::detail {
auto query_windows_platform_capabilities() -> FWindowsGameCapabilities {
    FWindowsGameCapabilities capabilities;

    auto const large_page_minimum{GetLargePageMinimum()};
    if (large_page_minimum == 0) {
        capabilities.large_page_access_status = ELargePageAccessStatus::Unsupported;
        return capabilities;
    }
    capabilities.large_page_minimum_bytes = static_cast<uint64>(large_page_minimum);

    HANDLE process_token{nullptr};
    if (!OpenProcessToken(
            GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &process_token)) {
        UE_LOG(LogSandboxSubsystem,
               Warning,
               TEXT("Could not open the process token while querying large-page access: %lu."),
               GetLastError());
        return capabilities;
    }

    LUID privilege_id{};
    if (!LookupPrivilegeValue(nullptr, SE_LOCK_MEMORY_NAME, &privilege_id)) {
        auto const error{GetLastError()};
        CloseHandle(process_token);
        UE_LOG(LogSandboxSubsystem,
               Warning,
               TEXT("Could not look up the large-page privilege while querying capabilities: "
                    "%lu."),
               error);
        return capabilities;
    }

    TOKEN_PRIVILEGES privileges{};
    privileges.PrivilegeCount = 1;
    privileges.Privileges[0].Luid = privilege_id;
    privileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    SetLastError(ERROR_SUCCESS);
    BOOL const adjusted{AdjustTokenPrivileges(process_token, 0, &privileges, 0, nullptr, nullptr)};
    DWORD const error{GetLastError()};
    CloseHandle(process_token);

    if (adjusted == 0) {
        UE_LOG(LogSandboxSubsystem,
               Warning,
               TEXT("Could not adjust the process token while querying large-page access: %lu."),
               error);
        return capabilities;
    }
    if (error == ERROR_NOT_ALL_ASSIGNED) {
        capabilities.large_page_access_status = ELargePageAccessStatus::PrivilegeUnavailable;
        return capabilities;
    }
    if (error != ERROR_SUCCESS) {
        UE_LOG(LogSandboxSubsystem,
               Warning,
               TEXT("Unexpected result while enabling large-page access: %lu."),
               error);
        return capabilities;
    }

    capabilities.large_page_access_status = ELargePageAccessStatus::Enabled;
    return capabilities;
}
}

#endif
