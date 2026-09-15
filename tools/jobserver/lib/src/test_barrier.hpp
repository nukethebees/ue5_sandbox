#pragma once

#include <Windows.h>

#include <atomic>
#include <iterator>
#include <string_view>

namespace jobserver {
inline void test_barrier(char const* const phase) {
    char configured[128]{};
    auto const length{GetEnvironmentVariableA("NUKETHEBEES_JOBSERVER_TEST_BARRIER",
                                              configured,
                                              static_cast<DWORD>(std::size(configured)))};
    if (length == 0 || length >= std::size(configured) || std::string_view{configured} != phase) {
        return;
    }
    static std::atomic<bool> fired{};
    if (fired.exchange(true)) {
        return;
    }

    wchar_t reached_name[256]{};
    wchar_t release_name[256]{};
    if (GetEnvironmentVariableW(L"NUKETHEBEES_JOBSERVER_TEST_BARRIER_REACHED",
                                reached_name,
                                static_cast<DWORD>(std::size(reached_name))) == 0 ||
        GetEnvironmentVariableW(L"NUKETHEBEES_JOBSERVER_TEST_BARRIER_RELEASE",
                                release_name,
                                static_cast<DWORD>(std::size(release_name))) == 0) {
        return;
    }
    auto const reached{OpenEventW(EVENT_MODIFY_STATE, FALSE, reached_name)};
    auto const release{OpenEventW(SYNCHRONIZE, FALSE, release_name)};
    if (reached != nullptr && release != nullptr) {
        SetEvent(reached);
        WaitForSingleObject(release, INFINITE);
    }
    if (reached != nullptr) {
        CloseHandle(reached);
    }
    if (release != nullptr) {
        CloseHandle(release);
    }
}
}
