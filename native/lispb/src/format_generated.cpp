#include "format_generated.h"

#include <atomic>
#include <chrono>
#include <fstream>
#include <stdexcept>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#else
#include <cerrno>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace codegen::detail {
namespace {

class FormatTemporary {
  public:
    FormatTemporary() {
        static std::atomic<unsigned> sequence{};
        auto const root{std::filesystem::temp_directory_path()};
        auto const stamp{std::chrono::steady_clock::now().time_since_epoch().count()};
        do {
            directory_ =
                root / ("lispb-format-" + std::to_string(stamp) + "-" + std::to_string(sequence++));
        } while (!std::filesystem::create_directory(directory_));
    }
    ~FormatTemporary() {
        std::error_code ignored;
        std::filesystem::remove(path(), ignored);
        std::filesystem::remove(directory_, ignored);
    }
    FormatTemporary(FormatTemporary const&) = delete;
    auto operator=(FormatTemporary const&) -> FormatTemporary& = delete;
    auto path() const -> std::filesystem::path { return directory_ / "generated.cpp"; }
  private:
    std::filesystem::path directory_;
};

auto style_path(std::filesystem::path const& destination) -> std::filesystem::path {
    auto directory{std::filesystem::absolute(destination).parent_path()};
    while (!directory.empty()) {
        auto const candidate{directory / ".clang-format"};
        if (std::filesystem::is_regular_file(candidate)) {
            return candidate;
        }
        auto const parent{directory.parent_path()};
        if (parent == directory) {
            break;
        }
        directory = parent;
    }
    throw std::runtime_error{"No .clang-format found for generated file: " + destination.string()};
}

#if defined(_WIN32)
auto quote_argument(std::wstring const& argument) -> std::wstring {
    std::wstring result{L"\""};
    std::size_t backslashes{};
    for (wchar_t const character : argument) {
        if (character == L'\\') {
            ++backslashes;
            continue;
        }
        result.append(character == L'"' ? backslashes * 2 + 1 : backslashes, L'\\');
        result += character;
        backslashes = 0;
    }
    result.append(backslashes * 2, L'\\');
    result += L'"';
    return result;
}
#endif

void run_formatter(std::filesystem::path const& input, std::filesystem::path const& style) {
    std::filesystem::path const executable{SANDBOX_CLANG_FORMAT_EXECUTABLE};
#if defined(_WIN32)
    auto command{quote_argument(executable.wstring()) + L" -i " +
                 quote_argument(L"--style=file:" + style.wstring()) + L" " +
                 quote_argument(input.wstring())};
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(executable.c_str(),
                        command.data(),
                        nullptr,
                        nullptr,
                        false,
                        CREATE_NO_WINDOW,
                        nullptr,
                        nullptr,
                        &startup,
                        &process)) {
        throw std::runtime_error{"Cannot launch clang-format: " + executable.string()};
    }
    auto const wait_result{WaitForSingleObject(process.hProcess, INFINITE)};
    DWORD exit_code{};
    auto const have_exit_code{GetExitCodeProcess(process.hProcess, &exit_code)};
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    if (wait_result != WAIT_OBJECT_0 || !have_exit_code || exit_code != 0) {
        throw std::runtime_error{"clang-format failed for generated output"};
    }
#else
    auto const executable_string{executable.string()};
    auto const style_argument{"--style=file:" + style.string()};
    auto const input_string{input.string()};
    auto const child{fork()};
    if (child == -1) {
        throw std::runtime_error{"Cannot launch clang-format"};
    }
    if (child == 0) {
        execl(executable_string.c_str(),
              executable_string.c_str(),
              "-i",
              style_argument.c_str(),
              input_string.c_str(),
              nullptr);
        _exit(127);
    }
    int status{};
    pid_t waited;
    do {
        waited = waitpid(child, &status, 0);
    } while (waited == -1 && errno == EINTR);
    if (waited == -1 || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        throw std::runtime_error{"clang-format failed for generated output"};
    }
#endif
}

}

auto format_generated(std::string const& content, std::filesystem::path const& destination)
    -> std::string {
    auto const style{style_path(destination)};
    FormatTemporary temporary;
    {
        std::ofstream output{temporary.path(), std::ios::binary};
        output.write(content.data(), static_cast<std::streamsize>(content.size()));
        output.close();
        if (!output) {
            throw std::runtime_error{"Cannot write temporary clang-format input"};
        }
    }
    run_formatter(temporary.path(), style);
    std::ifstream input{temporary.path(), std::ios::binary};
    if (!input) {
        throw std::runtime_error{"Cannot read formatted generated output"};
    }
    return std::string{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}

}
