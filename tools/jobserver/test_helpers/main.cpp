#include <Windows.h>

#include <filesystem>
#include <fstream>
#include <string>

auto wmain(int argc, wchar_t** argv) -> int {
    if (argc < 2) {
        return 2;
    }
    std::wstring const mode{argv[1]};
    if (mode == L"exit") {
        return argc > 2 ? _wtoi(argv[2]) : 0;
    }
    if (mode == L"child" && argc == 3) {
        {
            std::ofstream file{std::filesystem::path{argv[2]}};
            file << GetCurrentProcessId();
        }
        Sleep(60000);
        return 0;
    }
    if (mode == L"tree" && argc == 4) {
        wchar_t executable[32768]{};
        GetModuleFileNameW(nullptr, executable, 32768);
        auto line{L"\"" + std::wstring{executable} + L"\" child \"" + argv[2] + L"\""};
        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        PROCESS_INFORMATION process{};
        if (!CreateProcessW(executable,
                            line.data(),
                            nullptr,
                            nullptr,
                            FALSE,
                            CREATE_NO_WINDOW,
                            nullptr,
                            nullptr,
                            &startup,
                            &process)) {
            return 3;
        }
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
        auto const deadline{GetTickCount64() + 5000};
        while (!std::filesystem::exists(argv[2]) && GetTickCount64() < deadline) {
            Sleep(5);
        }
        if (std::wstring{argv[3]} == L"stay") {
            Sleep(60000);
        }
        return 0;
    }
    return 2;
}
