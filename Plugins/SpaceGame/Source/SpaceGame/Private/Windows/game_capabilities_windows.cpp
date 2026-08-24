#include "HAL/Platform.h"

#if PLATFORM_WINDOWS

#include "game_capabilities_windows.h"

#include "Windows/AllowWindowsPlatformTypes.h"

#include <windows.h>

#include "Windows/HideWindowsPlatformTypes.h"

namespace ml::ioj::detail {
auto query_windows_platform_capabilities() -> FGameCapabilities {
    FGameCapabilities capabilities;

    if (GetLargePageMinimum() == 0) {
        return capabilities;
    }

    HANDLE process_token{nullptr};
    if (!OpenProcessToken(
            GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &process_token)) {
        return capabilities;
    }

    LUID privilege_id{};
    if (!LookupPrivilegeValue(nullptr, SE_LOCK_MEMORY_NAME, &privilege_id)) {
        CloseHandle(process_token);
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

    capabilities.supports_large_pages = adjusted != 0 && error == ERROR_SUCCESS;
    return capabilities;
}
}

#endif
