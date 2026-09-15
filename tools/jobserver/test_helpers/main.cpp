#include <Windows.h>

#include <chrono>
#include <cstdlib>
#include <cwchar>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>

namespace {
auto narrow(std::wstring const& text) -> std::string {
    if (text.empty()) {
        return {};
    }
    auto const size{WideCharToMultiByte(
        CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr)};
    std::string result(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8,
                        0,
                        text.data(),
                        static_cast<int>(text.size()),
                        result.data(),
                        size,
                        nullptr,
                        nullptr);
    return result;
}
}

auto wmain(int argc, wchar_t** argv) -> int {
    if (argc < 2) {
        return 2;
    }
    auto const mode{std::wstring{argv[1]}};
    if (mode == L"check-environment") {
        wchar_t value[256]{};
        if (GetEnvironmentVariableW(L"JOBSERVER_ENV_ADD", value, 256) == 0 ||
            std::wstring{value} != L"added") {
            return 10;
        }
        if (GetEnvironmentVariableW(L"JOBSERVER_ENV_REPLACE", value, 256) == 0 ||
            std::wstring{value} != L"replaced") {
            return 11;
        }
        if (GetEnvironmentVariableW(L"JOBSERVER_ENV_REMOVE", value, 256) != 0) {
            return 12;
        }
        if (GetEnvironmentVariableW(L"NUKETHEBEES_JOBSERVER_JOB", value, 256) == 0 ||
            std::wstring{value} == L"spoofed") {
            return 13;
        }
        return 0;
    }
    if (mode == L"exit") {
        return argc >= 3 ? _wtoi(argv[2]) : 0;
    }
    if (mode == L"sleep") {
        auto const milliseconds{argc >= 3 ? _wtoi(argv[2]) : 100};
        std::this_thread::sleep_for(std::chrono::milliseconds{milliseconds});
        return 0;
    }
    if (mode == L"ready-sleep" && argc == 3) {
        {
            std::ofstream ready{std::filesystem::path{argv[2]}};
            ready << GetCurrentProcessId();
        }
        std::this_thread::sleep_for(std::chrono::seconds{60});
        return 0;
    }
    if (mode == L"output") {
        auto const count{argc >= 3 ? _wtoi(argv[2]) : 3};
        for (auto index{0}; index != count; ++index) {
            std::cout << "stdout " << index << '\n' << std::flush;
            std::cerr << "stderr " << index << '\n' << std::flush;
            std::this_thread::sleep_for(std::chrono::milliseconds{20});
        }
        return 0;
    }
    if (mode == L"arguments") {
        for (auto index{2}; index < argc; ++index) {
            auto const argument{narrow(argv[index])};
            std::cout << argument.size() << ':' << argument << '\n';
        }
        return 0;
    }
    if (mode == L"environment") {
        if (argc < 3) {
            return 2;
        }
        wchar_t* value{};
        std::size_t size{};
        if (_wdupenv_s(&value, &size, argv[2]) != 0 || value == nullptr) {
            return 4;
        }
        std::cout << narrow(value);
        std::free(value);
        return 0;
    }
    if (mode == L"working-directory") {
        std::cout << narrow(std::filesystem::current_path().wstring());
        return 0;
    }
    if (mode == L"require-no-console") {
        return GetConsoleWindow() == nullptr ? 0 : 5;
    }
    if (mode == L"large-output") {
        auto const size{argc >= 3 ? _wtoi(argv[2]) : 65'536};
        std::string output(static_cast<std::size_t>(size), 'x');
        std::cout.write(output.data(), static_cast<std::streamsize>(output.size()));
        return 0;
    }
    if (mode == L"binary-output") {
        std::string const output{"a\0b\xff", 4};
        std::cout.write(output.data(), static_cast<std::streamsize>(output.size()));
        return 0;
    }
    if (mode == L"pause-output") {
        std::cout << "before\n" << std::flush;
        std::this_thread::sleep_for(std::chrono::milliseconds{500});
        std::cout << "after\n" << std::flush;
        return 0;
    }
    if (mode == L"output-after") {
        auto const milliseconds{argc >= 3 ? _wtoi(argv[2]) : 100};
        std::this_thread::sleep_for(std::chrono::milliseconds{milliseconds});
        std::cout << "descendant output\n" << std::flush;
        return 0;
    }
    if (mode == L"marker-after") {
        if (argc < 4) {
            return 2;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds{_wtoi(argv[3])});
        std::ofstream marker{std::filesystem::path{argv[2]}};
        marker << "created";
        return 0;
    }
    if (mode == L"crash") {
        std::abort();
    }
    if (mode == L"spawn") {
        STARTUPINFOW startup{};
        startup.cb = sizeof(STARTUPINFOW);
        PROCESS_INFORMATION child{};
        std::wstring command{L"jobserver-test-helper.exe sleep 60000"};
        if (!CreateProcessW(nullptr,
                            command.data(),
                            nullptr,
                            nullptr,
                            FALSE,
                            0,
                            nullptr,
                            nullptr,
                            &startup,
                            &child)) {
            return 3;
        }
        CloseHandle(child.hThread);
        CloseHandle(child.hProcess);
        return 0;
    }
    if (mode == L"spawn-output") {
        STARTUPINFOW startup{};
        startup.cb = sizeof(STARTUPINFOW);
        PROCESS_INFORMATION child{};
        std::wstring command{L"jobserver-test-helper.exe output-after 150"};
        if (!CreateProcessW(nullptr,
                            command.data(),
                            nullptr,
                            nullptr,
                            TRUE,
                            0,
                            nullptr,
                            nullptr,
                            &startup,
                            &child)) {
            return 3;
        }
        CloseHandle(child.hThread);
        CloseHandle(child.hProcess);
        return 0;
    }
    if (mode == L"spawn-marker") {
        if (argc < 4) {
            return 2;
        }
        STARTUPINFOW startup{};
        startup.cb = sizeof(STARTUPINFOW);
        PROCESS_INFORMATION child{};
        auto command{L"jobserver-test-helper.exe marker-after \"" +
                     std::filesystem::path{argv[2]}.wstring() + L"\" " + argv[3]};
        if (!CreateProcessW(nullptr,
                            command.data(),
                            nullptr,
                            nullptr,
                            TRUE,
                            0,
                            nullptr,
                            nullptr,
                            &startup,
                            &child)) {
            return 3;
        }
        CloseHandle(child.hThread);
        CloseHandle(child.hProcess);
        return 0;
    }
    return 2;
}
