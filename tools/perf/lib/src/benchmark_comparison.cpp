#include <sandbox/perf/benchmark_comparison.hpp>

#include <jobserver/client.hpp>

#include <CLI/CLI.hpp>
#include <Windows.h>
#include <winsock2.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstddef>
#include <cstdio>
#include <expected>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <mutex>
#include <optional>
#include <ranges>
#include <regex>
#include <sstream>
#include <string_view>
#include <thread>
#include <utility>

namespace sandbox::perf {
namespace {
constexpr std::string_view benchmark_executable{"native-simulation-benchmark.exe"};
constexpr std::string_view capture_executable{"tracy-capture.exe"};
constexpr std::string_view csvexport_executable{"tracy-csvexport.exe"};
constexpr std::string_view profiler_ready_message{"native-simulation-benchmark: profiler-ready"};

auto lower(std::string value) -> std::string {
    std::ranges::transform(value, value.begin(), [](unsigned char const character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

auto status_text(ComparisonStatus const status) -> std::string_view {
    switch (status) {
        case ComparisonStatus::complete:
            return "complete";
        case ComparisonStatus::complete_with_warnings:
            return "complete_with_warnings";
        case ComparisonStatus::incomparable:
            return "incomparable";
        case ComparisonStatus::failed:
            return "failed";
    }
    return "failed";
}

auto is_positive_finite(double const value) -> bool {
    return value > 0.0 && std::isfinite(value);
}

auto format_number(double const value) -> std::string {
    std::ostringstream output;
    output << std::setprecision(17) << value;
    return output.str();
}

auto absolute_path(std::filesystem::path const& path) -> std::filesystem::path {
    std::error_code error;
    auto resolved{std::filesystem::weakly_canonical(path, error)};
    if (!error) {
        return resolved;
    }
    return std::filesystem::absolute(path);
}

auto binary_path(CompareOptions const& options,
                 std::string_view const preset,
                 std::string_view const executable) -> std::filesystem::path {
    return options.root / "out" / "build" / preset / "bin" / executable;
}

void require_file(std::filesystem::path const& path, std::string_view const description) {
    if (!std::filesystem::is_regular_file(path)) {
        throw PipelineError{"missing_executable",
                            std::string{description} + " was not built: " + path.string()};
    }
}

auto quote_argument(std::wstring const& argument) -> std::wstring {
    if (!argument.empty() && argument.find_first_of(L" \t\"") == std::wstring::npos) {
        return argument;
    }
    std::wstring result{L"\""};
    std::size_t backslashes{};
    for (auto const character : argument) {
        if (character == L'\\') {
            ++backslashes;
        } else if (character == L'\"') {
            result.append(backslashes * 2 + 1, L'\\');
            result.push_back(character);
            backslashes = 0;
        } else {
            result.append(backslashes, L'\\');
            result.push_back(character);
            backslashes = 0;
        }
    }
    result.append(backslashes * 2, L'\\');
    result.push_back(L'\"');
    return result;
}

auto command_line(std::filesystem::path const& executable,
                  std::vector<std::string> const& arguments) -> std::wstring {
    std::wstring result{quote_argument(executable.wstring())};
    for (auto const& argument : arguments) {
        result.push_back(L' ');
        result += quote_argument(std::filesystem::path{argument}.wstring());
    }
    return result;
}

class Handle final {
  public:
    Handle() = default;
    explicit Handle(HANDLE const value)
        : value_{value} {}
    Handle(Handle&& other) noexcept
        : value_{std::exchange(other.value_, nullptr)} {}
    auto operator=(Handle&& other) noexcept -> Handle& {
        if (this != &other) {
            reset(std::exchange(other.value_, nullptr));
        }
        return *this;
    }
    Handle(Handle const&) = delete;
    auto operator=(Handle const&) -> Handle& = delete;
    ~Handle() { reset(); }

    [[nodiscard]] auto get() const -> HANDLE { return value_; }
    void reset(HANDLE const value = nullptr) {
        if (value_ != nullptr && value_ != INVALID_HANDLE_VALUE) {
            CloseHandle(value_);
        }
        value_ = value;
    }
  private:
    HANDLE value_{};
};

class Socket final {
  public:
    explicit Socket(SOCKET const value)
        : value_{value} {}
    Socket(Socket const&) = delete;
    auto operator=(Socket const&) -> Socket& = delete;
    ~Socket() { reset(); }

    [[nodiscard]] auto get() const -> SOCKET { return value_; }
    void reset(SOCKET const value = INVALID_SOCKET) {
        if (value_ != INVALID_SOCKET) {
            closesocket(value_);
        }
        value_ = value;
    }
  private:
    SOCKET value_{INVALID_SOCKET};
};

auto make_environment(std::map<std::wstring, std::wstring, std::less<>> const& changes)
    -> std::vector<wchar_t> {
    std::map<std::wstring, std::wstring, std::less<>> values;
    LPWCH const inherited{GetEnvironmentStringsW()};
    if (inherited == nullptr) {
        throw PipelineError{"operating_system_error", "Could not read process environment"};
    }
    for (auto const* cursor{inherited}; *cursor != L'\0';) {
        std::wstring entry{cursor};
        cursor += entry.size() + 1;
        auto const separator{entry.find(L'=')};
        if (separator != std::wstring::npos && separator != 0) {
            values.insert_or_assign(entry.substr(0, separator), entry.substr(separator + 1));
        }
    }
    FreeEnvironmentStringsW(inherited);
    for (auto const& [name, value] : changes) {
        values.insert_or_assign(name, value);
    }

    std::vector<wchar_t> result;
    for (auto const& [name, value] : values) {
        std::wstring const wide_name{name.begin(), name.end()};
        result.insert(result.end(), wide_name.begin(), wide_name.end());
        result.push_back(L'=');
        result.insert(result.end(), value.begin(), value.end());
        result.push_back(L'\0');
    }
    result.push_back(L'\0');
    return result;
}

struct Process {
    Handle handle{};
    Handle stdout_read{};
    Handle stderr_read{};
};

struct Pipe {
    Handle read{};
    Handle write{};
};

auto create_pipe(std::string_view const description) -> Pipe {
    SECURITY_ATTRIBUTES security{.nLength = sizeof(SECURITY_ATTRIBUTES),
                                 .lpSecurityDescriptor = nullptr,
                                 .bInheritHandle = TRUE};
    HANDLE read{};
    HANDLE write{};
    if (!CreatePipe(&read, &write, &security, 0)) {
        throw PipelineError{"operating_system_error",
                            "Could not create " + std::string{description}};
    }

    Pipe pipe{.read = Handle{read}, .write = Handle{write}};
    if (!SetHandleInformation(pipe.read.get(), HANDLE_FLAG_INHERIT, 0)) {
        throw PipelineError{"operating_system_error",
                            "Could not configure " + std::string{description}};
    }
    return pipe;
}

auto create_process(std::filesystem::path const& executable,
                    std::vector<std::string> const& arguments,
                    std::filesystem::path const& working_directory,
                    std::map<std::wstring, std::wstring, std::less<>> const& environment,
                    bool const capture_stdout,
                    bool const capture_stderr) -> Process {
    Handle stdout_read;
    Handle stdout_write;
    Handle stderr_read;
    Handle stderr_write;
    if (capture_stdout) {
        auto pipe{create_pipe("benchmark stdout pipe")};
        stdout_read = std::move(pipe.read);
        stdout_write = std::move(pipe.write);
    }
    if (capture_stderr) {
        auto pipe{create_pipe("benchmark stderr pipe")};
        stderr_read = std::move(pipe.read);
        stderr_write = std::move(pipe.write);
    }

    STARTUPINFOW startup{};
    startup.cb = sizeof(STARTUPINFOW);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    startup.hStdOutput = capture_stdout ? stdout_write.get() : GetStdHandle(STD_OUTPUT_HANDLE);
    startup.hStdError = capture_stderr ? stderr_write.get() : GetStdHandle(STD_ERROR_HANDLE);
    PROCESS_INFORMATION information{};
    auto command{command_line(executable, arguments)};
    auto environment_block{make_environment(environment)};
    if (!CreateProcessW(executable.c_str(),
                        command.data(),
                        nullptr,
                        nullptr,
                        TRUE,
                        CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT,
                        environment_block.data(),
                        working_directory.c_str(),
                        &startup,
                        &information)) {
        throw PipelineError{"operating_system_error", "Could not launch: " + executable.string()};
    }
    CloseHandle(information.hThread);
    return {.handle = Handle{information.hProcess},
            .stdout_read = std::move(stdout_read),
            .stderr_read = std::move(stderr_read)};
}

auto wait_for_process(Process const& process, double const timeout_seconds) -> std::optional<int> {
    auto const milliseconds{static_cast<DWORD>(std::min(
        timeout_seconds * 1000.0, static_cast<double>(std::numeric_limits<DWORD>::max() - 1)))};
    auto const result{WaitForSingleObject(process.handle.get(), milliseconds)};
    if (result == WAIT_TIMEOUT) {
        return std::nullopt;
    }
    if (result != WAIT_OBJECT_0) {
        throw PipelineError{"operating_system_error", "Could not wait for child process"};
    }
    DWORD exit_code{};
    if (!GetExitCodeProcess(process.handle.get(), &exit_code)) {
        throw PipelineError{"operating_system_error", "Could not read child process exit code"};
    }
    return static_cast<int>(exit_code);
}

void stop_process(Process const& process) {
    if (process.handle.get() == nullptr ||
        WaitForSingleObject(process.handle.get(), 0) != WAIT_TIMEOUT) {
        return;
    }
    TerminateProcess(process.handle.get(), 1);
    WaitForSingleObject(process.handle.get(), 5'000);
}

void read_pipe(HANDLE const handle, std::function<void(std::string_view)> const& consume) {
    std::array<char, 4096> buffer{};
    for (;;) {
        DWORD read{};
        if (!ReadFile(handle, buffer.data(), static_cast<DWORD>(buffer.size()), &read, nullptr)) {
            auto const error{GetLastError()};
            if (error == ERROR_BROKEN_PIPE) {
                return;
            }
            return;
        }
        if (read == 0) {
            return;
        }
        consume(std::string_view{buffer.data(), read});
    }
}

class CapturedProcess final {
  public:
    using PipeConsumer = std::function<void(std::string_view)>;

    CapturedProcess(Process process, PipeConsumer stdout_consumer, PipeConsumer stderr_consumer)
        : process_{std::move(process)} {
        try {
            stdout_reader_ = std::thread{[this, consume = std::move(stdout_consumer)] {
                read_pipe(process_.stdout_read.get(), consume);
            }};
            stderr_reader_ = std::thread{[this, consume = std::move(stderr_consumer)] {
                read_pipe(process_.stderr_read.get(), consume);
            }};
        } catch (...) {
            stop_and_join();
            throw;
        }
    }

    CapturedProcess(CapturedProcess const&) = delete;
    auto operator=(CapturedProcess const&) -> CapturedProcess& = delete;
    ~CapturedProcess() { stop_and_join(); }

    [[nodiscard]] auto process() const -> Process const& { return process_; }
    void stop() noexcept { stop_process(process_); }
    void join_readers() noexcept {
        if (stdout_reader_.joinable()) {
            stdout_reader_.join();
        }
        if (stderr_reader_.joinable()) {
            stderr_reader_.join();
        }
    }
  private:
    void stop_and_join() noexcept {
        stop();
        join_readers();
    }

    Process process_{};
    std::thread stdout_reader_{};
    std::thread stderr_reader_{};
};

auto run_command(std::filesystem::path const& executable,
                 std::vector<std::string> const& arguments,
                 std::filesystem::path const& working_directory) -> int {
    std::cout << "> " << executable.string();
    for (auto const& argument : arguments) {
        std::cout << ' ' << argument;
    }
    std::cout << '\n' << std::flush;
    auto process{create_process(executable, arguments, working_directory, {}, false, false)};
    auto const exit_code{wait_for_process(process, 3'600.0)};
    if (!exit_code) {
        stop_process(process);
        throw PipelineError{"build_failed", "command timed out: " + executable.string()};
    }
    return *exit_code;
}

auto capture_command(std::filesystem::path const& executable,
                     std::vector<std::string> const& arguments,
                     std::filesystem::path const& working_directory,
                     double const timeout_seconds) -> std::pair<int, std::string> {
    std::string output;
    std::mutex mutex;
    CapturedProcess process{
        create_process(executable, arguments, working_directory, {}, true, true),
        [&](std::string_view const text) {
            std::scoped_lock const lock{mutex};
            output.append(text);
        },
        [](std::string_view const) {}};
    auto const exit_code{wait_for_process(process.process(), timeout_seconds)};
    if (!exit_code) {
        throw PipelineError{"export_timeout", "command timed out: " + executable.string()};
    }
    process.join_readers();
    return {*exit_code, std::move(output)};
}

auto git_output(CompareOptions const& options, std::vector<std::string> const& arguments)
    -> std::string {
    try {
        auto const [exit_code, output]{capture_command("git", arguments, options.root, 30.0)};
        if (exit_code != 0) {
            return "unknown";
        }
        auto const last{output.find_last_not_of(" \t\r\n")};
        return last == std::string::npos ? "" : output.substr(0, last + 1);
    } catch (PipelineError const&) {
        return "unknown";
    }
}

auto tracy_version(CompareOptions const& options) -> std::string {
    std::ifstream input{options.root / "native" / "third_party" / "tracy" / "public" / "common" /
                        "TracyVersion.hpp"};
    std::string contents{std::istreambuf_iterator<char>{input}, {}};
    std::array<std::string, 3> values{};
    std::array<std::string_view, 3> const names{"Major", "Minor", "Patch"};
    for (std::size_t index{}; index < names.size(); ++index) {
        std::smatch match;
        if (!std::regex_search(
                contents, match, std::regex{std::string{names[index]} + R"(\s*=\s*(\d+))"})) {
            return "unknown";
        }
        values[index] = match[1].str();
    }
    return values[0] + "." + values[1] + "." + values[2];
}

auto utc_timestamp() -> std::string {
    SYSTEMTIME time{};
    GetSystemTime(&time);
    std::ostringstream output;
    output << std::setfill('0') << std::setw(4) << time.wYear << std::setw(2) << time.wMonth
           << std::setw(2) << time.wDay << '-' << std::setw(2) << time.wHour << std::setw(2)
           << time.wMinute << std::setw(2) << time.wSecond << '-' << std::setw(3)
           << time.wMilliseconds << "000Z";
    return output.str();
}

auto iso_utc_timestamp() -> std::string {
    SYSTEMTIME time{};
    GetSystemTime(&time);
    std::ostringstream output;
    output << std::setfill('0') << std::setw(4) << time.wYear << '-' << std::setw(2) << time.wMonth
           << '-' << std::setw(2) << time.wDay << 'T' << std::setw(2) << time.wHour << ':'
           << std::setw(2) << time.wMinute << ':' << std::setw(2) << time.wSecond << '.'
           << std::setw(3) << time.wMilliseconds << "+00:00";
    return output.str();
}

void write_json(std::filesystem::path const& path, Json const& value) {
    std::filesystem::create_directories(path.parent_path());
    auto temporary{path};
    temporary += ".tmp";
    {
        std::ofstream output{temporary, std::ios::binary | std::ios::trunc};
        if (!output) {
            throw PipelineError{"operating_system_error", "Could not write: " + temporary.string()};
        }
        output << value.dump(2) << '\n';
    }
    if (!MoveFileExW(
            temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        throw PipelineError{"operating_system_error", "Could not publish: " + path.string()};
    }
}

auto warning_json(Warning const& warning) -> Json {
    auto result = warning.details;
    result["code"] = warning.code;
    result["severity"] = warning.severity;
    result["message"] = warning.message;
    return result;
}

auto statistics_json(std::optional<ZoneStatistics> const& statistics) -> Json {
    if (!statistics) {
        return nullptr;
    }
    return Json{{"total_ns", statistics->total_nanoseconds},
                {"total_percent", statistics->total_percent},
                {"mean_ns", statistics->mean_nanoseconds},
                {"min_ns", statistics->minimum_nanoseconds},
                {"max_ns", statistics->maximum_nanoseconds},
                {"source_line", statistics->source_line}};
}

auto option_arguments(CompareOptions const& options) -> std::vector<std::string> {
    std::vector<std::string> result{"--root",
                                    options.root.string(),
                                    "--level",
                                    options.level.string(),
                                    "--seconds",
                                    format_number(options.seconds),
                                    "--game-speed",
                                    std::to_string(options.game_speed),
                                    "--a-preset",
                                    options.a_preset,
                                    "--b-preset",
                                    options.b_preset,
                                    "--output-dir",
                                    options.output_directory.string(),
                                    "--connection-timeout-seconds",
                                    format_number(options.connection_timeout_seconds),
                                    "--process-timeout-seconds",
                                    format_number(options.process_timeout_seconds),
                                    "--top",
                                    std::to_string(options.top),
                                    "--skip-build"};
    if (!options.runner_arguments.empty()) {
        result.push_back("--");
        result.insert(
            result.end(), options.runner_arguments.begin(), options.runner_arguments.end());
    }
    return result;
}

auto executable_path() -> std::filesystem::path {
    std::vector<wchar_t> storage(MAX_PATH);
    for (;;) {
        auto const copied{
            GetModuleFileNameW(nullptr, storage.data(), static_cast<DWORD>(storage.size()))};
        if (copied == 0) {
            throw PipelineError{"operating_system_error", "Could not locate comparison executable"};
        }
        if (copied < storage.size() - 1) {
            return std::filesystem::path{std::wstring{storage.data(), copied}};
        }
        storage.resize(storage.size() * 2);
    }
}

void build_prerequisites(CompareOptions const& options) {
    for (auto const& command : std::array<std::vector<std::string>, 2>{
             std::vector<std::string>{"--preset", "tracy-tools"},
             std::vector<std::string>{"--build", "--preset", "tracy-tools"}}) {
        if (run_command("cmake", command, options.root) != 0) {
            throw PipelineError{"build_failed", "command exited unsuccessfully: cmake"};
        }
    }
    for (auto const* preset : {&options.a_preset, &options.b_preset}) {
        if (preset != &options.a_preset && *preset == options.a_preset) {
            continue;
        }
        if (run_command("cmake", {"--preset", *preset}, options.root) != 0 ||
            run_command("cmake",
                        {"--build", "--preset", *preset, "--target", "native-simulation-benchmark"},
                        options.root) != 0) {
            throw PipelineError{"build_failed", "could not build benchmark preset: " + *preset};
        }
    }
}

auto make_manifest(CompareOptions const& options, std::string_view const status) -> Json {
    Json manifest = Json::object();
    manifest["schema_version"] = 1;
    manifest["created_utc"] = iso_utc_timestamp();
    manifest["status"] = status;
    manifest["benchmark"] = Json::object();
    manifest["benchmark"]["arguments"] = Json::object();
    manifest["benchmark"]["arguments"]["level"] = options.level.string();
    manifest["benchmark"]["arguments"]["seconds"] = options.seconds;
    manifest["benchmark"]["arguments"]["game_speed"] = options.game_speed;
    manifest["benchmark"]["arguments"]["additional"] = options.runner_arguments;
    manifest["benchmark"]["effective_argv"] = benchmark_arguments(options);
    manifest["source"] = {{"git_sha", git_output(options, {"rev-parse", "HEAD"})},
                          {"dirty", !git_output(options, {"status", "--porcelain"}).empty()}};
    manifest["tracy"] = {
        {"version", tracy_version(options)},
        {"git_sha", git_output(options, {"-C", "native/third_party/tracy", "rev-parse", "HEAD"})},
        {"capture_executable", binary_path(options, "tracy-tools", capture_executable).string()},
        {"csvexport_executable",
         binary_path(options, "tracy-tools", csvexport_executable).string()}};
    manifest["configurations"] = {
        {"a",
         {{"preset", options.a_preset},
          {"benchmark_executable",
           binary_path(options, options.a_preset, benchmark_executable).string()}}},
        {"b",
         {{"preset", options.b_preset},
          {"benchmark_executable",
           binary_path(options, options.b_preset, benchmark_executable).string()}}}};
    return manifest;
}

void write_failure(CompareOptions const& options, Json& manifest, PipelineError const& error) {
    Warning const warning{.code = error.code(), .severity = "error", .message = error.what()};
    manifest["status"] = "failed";
    manifest["warnings"] = Json::array({warning_json(warning)});
    write_json(options.output_directory / "manifest.json", manifest);
    write_json(options.output_directory / "comparison.json",
               Json{{"schema_version", 1},
                    {"status", "failed"},
                    {"overall", nullptr},
                    {"zones", Json::array()},
                    {"warnings", Json::array({warning_json(warning)})}});
}

auto unused_loopback_port() -> int {
    WSADATA socket_data{};
    if (WSAStartup(MAKEWORD(2, 2), &socket_data) != 0) {
        throw PipelineError{"operating_system_error", "Could not initialize Windows sockets"};
    }
    struct SocketCleanup final {
        ~SocketCleanup() { WSACleanup(); }
    } cleanup;
    Socket listener{socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)};
    if (listener.get() == INVALID_SOCKET) {
        throw PipelineError{"operating_system_error", "Could not allocate Tracy loopback port"};
    }
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = 0;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(listener.get(), reinterpret_cast<sockaddr*>(&address), sizeof(address)) ==
        SOCKET_ERROR) {
        throw PipelineError{"operating_system_error", "Could not bind Tracy loopback port"};
    }
    int length{sizeof(address)};
    if (getsockname(listener.get(), reinterpret_cast<sockaddr*>(&address), &length) ==
        SOCKET_ERROR) {
        throw PipelineError{"operating_system_error", "Could not inspect Tracy loopback port"};
    }
    return ntohs(address.sin_port);
}

auto capture_run(CompareOptions const& options,
                 std::string_view const label,
                 std::string const& preset) -> Json {
    auto const output{options.output_directory / label};
    std::filesystem::create_directories(output);
    auto const capture_path{output / "capture.tracy"};
    auto const benchmark_path{binary_path(options, preset, benchmark_executable)};
    auto const port{unused_loopback_port()};
    std::string stdout_text;
    std::string stderr_text;
    std::string stderr_pending;
    std::mutex output_mutex;
    std::condition_variable ready_changed;
    bool ready{};
    CapturedProcess benchmark{
        create_process(benchmark_path,
                       benchmark_arguments(options),
                       options.root,
                       {{L"TRACY_PORT", std::to_wstring(port)}, {L"TRACY_NO_EXIT", L"1"}},
                       true,
                       true),
        [&](std::string_view const text) {
            std::scoped_lock const lock{output_mutex};
            stdout_text.append(text);
        },
        [&](std::string_view const text) {
            std::scoped_lock const lock{output_mutex};
            stderr_text.append(text);
            stderr_pending.append(text);
            for (;;) {
                auto const newline{stderr_pending.find('\n')};
                if (newline == std::string::npos) {
                    break;
                }
                auto line{stderr_pending.substr(0, newline)};
                stderr_pending.erase(0, newline + 1);
                if (!line.empty() && line.back() == '\r') {
                    line.pop_back();
                }
                if (line == profiler_ready_message) {
                    ready = true;
                    ready_changed.notify_all();
                }
            }
        }};
    std::string capture_log;
    std::mutex capture_mutex;
    std::optional<CapturedProcess> capture;
    try {
        std::unique_lock lock{output_mutex};
        auto const deadline{std::chrono::steady_clock::now() +
                            std::chrono::duration<double>{options.process_timeout_seconds}};
        while (!ready) {
            if (WaitForSingleObject(benchmark.process().handle.get(), 0) == WAIT_OBJECT_0) {
                throw PipelineError{"benchmark_failed",
                                    "benchmark " + std::string{label} +
                                        " exited before requesting a profiler"};
            }
            if (std::chrono::steady_clock::now() >= deadline) {
                throw PipelineError{"benchmark_timeout",
                                    "benchmark " + std::string{label} +
                                        " did not request a profiler in time"};
            }
            ready_changed.wait_for(lock, std::chrono::milliseconds{50});
        }
        lock.unlock();

        capture.emplace(
            create_process(
                binary_path(options, "tracy-tools", capture_executable),
                {"-o", capture_path.string(), "-a", "127.0.0.1", "-p", std::to_string(port)},
                options.root,
                {},
                true,
                true),
            [&](std::string_view const text) {
                std::scoped_lock const lock{capture_mutex};
                capture_log.append(text);
            },
            [&](std::string_view const text) {
                std::scoped_lock const lock{capture_mutex};
                capture_log.append(text);
            });

        auto const benchmark_exit{
            wait_for_process(benchmark.process(), options.process_timeout_seconds)};
        if (!benchmark_exit) {
            throw PipelineError{"benchmark_timeout",
                                "benchmark " + std::string{label} +
                                    " exceeded the process timeout"};
        }
        benchmark.join_readers();
        if (*benchmark_exit != 0) {
            throw PipelineError{"benchmark_failed",
                                "benchmark " + std::string{label} + " exited with " +
                                    std::to_string(*benchmark_exit)};
        }
        Json result;
        try {
            result = Json::parse(stdout_text);
        } catch (Json::parse_error const&) {
            throw PipelineError{"benchmark_result_invalid",
                                "benchmark " + std::string{label} +
                                    " did not emit one JSON result"};
        }
        if (!result.is_object() || !result.contains("environment") ||
            !result["environment"].is_object()) {
            throw PipelineError{"benchmark_result_invalid",
                                "benchmark " + std::string{label} + " environment is missing"};
        }
        if (!result["environment"].value("tracy_enabled", false)) {
            throw PipelineError{"tracy_not_enabled",
                                "benchmark preset " + preset + " does not enable Tracy"};
        }
        write_json(output / "benchmark-result.json", result);

        auto const capture_exit{
            wait_for_process(capture->process(), std::min(options.process_timeout_seconds, 120.0))};
        if (!capture_exit) {
            throw PipelineError{"capture_timeout",
                                "Tracy capture " + std::string{label} + " did not finish"};
        }
        capture->join_readers();
        std::ofstream{output / "capture.log"} << capture_log;
        std::ofstream{output / "benchmark.stderr.log"} << stderr_text;
        if (*capture_exit != 0) {
            throw PipelineError{"capture_failed",
                                "Tracy capture " + std::string{label} + " exited with " +
                                    std::to_string(*capture_exit)};
        }
        if (!std::filesystem::is_regular_file(capture_path) ||
            std::filesystem::file_size(capture_path) == 0) {
            throw PipelineError{"capture_failed",
                                "Tracy capture " + std::string{label} + " was not created"};
        }
        return result;
    } catch (...) {
        benchmark.stop();
        if (capture) {
            capture->stop();
        }
        if (capture) {
            capture->join_readers();
        }
        benchmark.join_readers();
        throw;
    }
}

void export_capture(CompareOptions const& options,
                    std::string_view const label,
                    bool const self_time) {
    auto const output{options.output_directory / label};
    auto const [exit_code, stdout_text]{capture_command(
        binary_path(options, "tracy-tools", csvexport_executable),
        self_time ? std::vector<std::string>{"--self", (output / "capture.tracy").string()}
                  : std::vector<std::string>{(output / "capture.tracy").string()},
        options.root,
        options.process_timeout_seconds)};
    std::ofstream{output / (self_time ? "self.csv" : "inclusive.csv")} << stdout_text;
    if (exit_code != 0) {
        throw PipelineError{"export_failed", "Tracy export " + std::string{label} + " failed"};
    }
}

auto run_comparison(CompareOptions const& options, Json& manifest, std::ostream& output) -> int {
    require_file(options.level, "benchmark level");
    require_file(binary_path(options, "tracy-tools", capture_executable), "tracy-capture");
    require_file(binary_path(options, "tracy-tools", csvexport_executable), "tracy-csvexport");
    require_file(binary_path(options, options.a_preset, benchmark_executable), "benchmark A");
    require_file(binary_path(options, options.b_preset, benchmark_executable), "benchmark B");
    auto const a_result = capture_run(options, "a", options.a_preset);
    auto const b_result = capture_run(options, "b", options.b_preset);
    for (auto const label : {"a", "b"}) {
        export_capture(options, label, false);
        export_capture(options, label, true);
    }
    auto const comparison{compare_results(options, a_result, b_result)};
    auto const serialized = to_json(comparison);
    write_json(options.output_directory / "comparison.json", serialized);
    manifest["status"] = status_text(comparison.status);
    manifest["warnings"] = Json::array();
    for (auto const& warning : comparison.warnings) {
        manifest["warnings"].push_back(warning_json(warning));
    }
    write_json(options.output_directory / "manifest.json", manifest);
    print_summary(comparison, options.output_directory, options.top, output);
    return comparison.status == ComparisonStatus::incomparable ? 2 : 0;
}
} // namespace

PipelineError::PipelineError(std::string code, std::string message)
    : std::runtime_error{std::move(message)}
    , code_{std::move(code)} {}

auto PipelineError::code() const -> std::string const& {
    return code_;
}

auto parse_command_line(int const argc, char const* const* argv) -> CommandLineResult {
    CompareOptions options;
    CLI::App app{"Compare two native benchmark configurations using Tracy captures."};
    app.positionals_at_end(true);
    app.add_option("--root", options.root, "Repository worktree root")->required();
    app.add_option("--level", options.level, "Lisp level file")->required();
    app.add_option("--seconds", options.seconds, "Simulated seconds")->required();
    app.add_option("--game-speed", options.game_speed)->default_val(1);
    app.add_option("--a-preset", options.a_preset)->required();
    app.add_option("--b-preset", options.b_preset)->required();
    app.add_option("--output-dir", options.output_directory);
    app.add_flag("--skip-build", options.skip_build);
    app.add_flag("--jobserver-child", options.jobserver_child)->group("");
    app.add_option("--connection-timeout-seconds", options.connection_timeout_seconds)
        ->default_val(30.0);
    app.add_option("--process-timeout-seconds", options.process_timeout_seconds)
        ->default_val(1800.0);
    app.add_option("--top", options.top)->default_val(5);
    app.add_option("runner_arguments", options.runner_arguments)->expected(0, -1);
    try {
        app.parse(argc, argv);
    } catch (CLI::ParseError const& error) {
        std::ostringstream standard_output;
        std::ostringstream standard_error;
        app.exit(error, standard_output, standard_error);
        return {.standard_output = standard_output.str(),
                .standard_error = standard_error.str(),
                .exit_code = error.get_exit_code() == 0 ? 0 : 2};
    }
    if (!is_positive_finite(options.seconds) ||
        !is_positive_finite(options.connection_timeout_seconds) ||
        !is_positive_finite(options.process_timeout_seconds) || options.game_speed <= 0 ||
        options.top <= 0) {
        return {.standard_error = "comparison options must be positive and finite\n",
                .exit_code = 2};
    }
    std::regex const preset_pattern{"[A-Za-z0-9_.-]+"};
    if (!std::regex_match(options.a_preset, preset_pattern) ||
        !std::regex_match(options.b_preset, preset_pattern)) {
        return {.standard_error = "invalid CMake preset name\n", .exit_code = 2};
    }
    for (auto const& argument : options.runner_arguments) {
        auto const option{argument.substr(0, argument.find('='))};
        if (option == "--level" || option == "--seconds" || option == "--game-speed" ||
            option == "--wait-for-profiler") {
            return {.standard_error = option + " is managed by the comparison tool\n",
                    .exit_code = 2};
        }
    }
    options.root = absolute_path(options.root);
    options.level = absolute_path(options.level);
    options.output_directory_explicit = !options.output_directory.empty();
    if (!options.output_directory_explicit) {
        options.output_directory =
            options.root / ".local" / "benchmarks" / "tracy-comparison" / utc_timestamp();
    }
    options.output_directory = absolute_path(options.output_directory);
    return {.options = std::move(options)};
}

auto benchmark_arguments(CompareOptions const& options) -> std::vector<std::string> {
    std::vector<std::string> result{"--level",
                                    options.level.string(),
                                    "--seconds",
                                    format_number(options.seconds),
                                    "--game-speed",
                                    std::to_string(options.game_speed)};
    result.insert(result.end(), options.runner_arguments.begin(), options.runner_arguments.end());
    result.insert(result.end(),
                  {"--wait-for-profiler", format_number(options.connection_timeout_seconds)});
    return result;
}

auto normalize_source_file(std::string source_file, std::filesystem::path const& root)
    -> std::string {
    std::ranges::replace(source_file, '\\', '/');
    auto root_text{absolute_path(root).generic_string()};
    if (!root_text.empty() && root_text.back() != '/') {
        root_text.push_back('/');
    }
    if (auto const source_lower{lower(source_file)}, root_lower{lower(root_text)};
        source_lower.starts_with(root_lower)) {
        source_file.erase(0, root_text.size());
    }
    return source_file;
}

auto parse_zone_csv(std::filesystem::path const& path, std::filesystem::path const& root)
    -> ParsedZones {
    std::ifstream input{path, std::ios::binary};
    if (!input) {
        throw PipelineError{"csv_parse_failed", "Could not read Tracy CSV: " + path.string()};
    }
    std::string text{std::istreambuf_iterator<char>{input}, {}};
    if (text.starts_with("\xEF\xBB\xBF")) {
        text.erase(0, 3);
    }
    std::vector<std::vector<std::string>> rows;
    std::vector<std::string> row;
    std::string field;
    bool quoted{};
    for (std::size_t index{}; index < text.size(); ++index) {
        auto const character{text[index]};
        if (quoted) {
            if (character == '"' && index + 1 < text.size() && text[index + 1] == '"') {
                field.push_back(character);
                ++index;
            } else if (character == '"') {
                quoted = false;
            } else {
                field.push_back(character);
            }
        } else if (character == '"' && field.empty()) {
            quoted = true;
        } else if (character == ',') {
            row.push_back(std::move(field));
            field.clear();
        } else if (character == '\n') {
            if (!field.empty() && field.back() == '\r') {
                field.pop_back();
            }
            row.push_back(std::move(field));
            field.clear();
            rows.push_back(std::move(row));
            row.clear();
        } else {
            field.push_back(character);
        }
    }
    if (quoted) {
        throw PipelineError{"csv_parse_failed", "malformed Tracy CSV row in " + path.string()};
    }
    if (!field.empty() || !row.empty()) {
        row.push_back(std::move(field));
        rows.push_back(std::move(row));
    }
    if (rows.empty()) {
        throw PipelineError{"csv_parse_failed", "unexpected Tracy CSV header in " + path.string()};
    }
    std::map<std::string, std::size_t, std::less<>> columns;
    for (std::size_t index{}; index < rows.front().size(); ++index) {
        columns.emplace(rows.front()[index], index);
    }
    std::array<std::string_view, 10> const required{"name",
                                                    "src_file",
                                                    "src_line",
                                                    "total_ns",
                                                    "total_perc",
                                                    "counts",
                                                    "mean_ns",
                                                    "min_ns",
                                                    "max_ns",
                                                    "std_ns"};
    for (auto const name : required) {
        if (!columns.contains(name)) {
            throw PipelineError{"csv_parse_failed",
                                "unexpected Tracy CSV header in " + path.string()};
        }
    }
    ParsedZones result;
    std::map<ZoneKey, bool> ambiguous;
    auto parse_integer = [&](std::vector<std::string> const& values, std::string_view name) {
        try {
            std::size_t used{};
            auto const index{columns.at(std::string{name})};
            auto const value{std::stoll(values.at(index), &used)};
            if (used != values.at(index).size()) {
                throw std::invalid_argument{"value"};
            }
            return value;
        } catch (...) {
            throw PipelineError{"csv_parse_failed", "invalid Tracy CSV value in " + path.string()};
        }
    };
    auto parse_double = [&](std::vector<std::string> const& values, std::string_view name) {
        try {
            std::size_t used{};
            auto const index{columns.at(std::string{name})};
            auto const value{std::stod(values.at(index), &used)};
            if (used != values.at(index).size()) {
                throw std::invalid_argument{"value"};
            }
            return value;
        } catch (...) {
            throw PipelineError{"csv_parse_failed", "invalid Tracy CSV value in " + path.string()};
        }
    };
    for (auto const& values : std::views::drop(rows, 1)) {
        if (values.size() != rows.front().size()) {
            throw PipelineError{"csv_parse_failed", "malformed Tracy CSV row in " + path.string()};
        }
        auto const source{normalize_source_file(values.at(columns.at("src_file")), root)};
        ZoneStatistics statistics{.name = values.at(columns.at("name")),
                                  .source_file = source,
                                  .source_line =
                                      static_cast<int>(parse_integer(values, "src_line")),
                                  .total_nanoseconds = parse_integer(values, "total_ns"),
                                  .total_percent = parse_double(values, "total_perc"),
                                  .count = parse_integer(values, "counts"),
                                  .mean_nanoseconds = parse_integer(values, "mean_ns"),
                                  .minimum_nanoseconds = parse_integer(values, "min_ns"),
                                  .maximum_nanoseconds = parse_integer(values, "max_ns")};
        ZoneKey const key{.name = statistics.name, .source_file = lower(statistics.source_file)};
        if (result.zones.contains(key) || ambiguous.contains(key)) {
            result.zones.erase(key);
            ambiguous[key] = true;
            result.warnings.push_back(
                {.code = "ambiguous_zone_identity",
                 .severity = "warning",
                 .message =
                     "multiple Tracy rows share identity " + statistics.name + " in " + source,
                 .details = {{"zone", {{"name", statistics.name}, {"source_file", source}}}}});
        } else {
            result.zones.emplace(key, std::move(statistics));
        }
    }
    return result;
}

auto percentage_delta(double const a, double const b) -> std::optional<double> {
    return a == 0.0 ? std::nullopt : std::optional<double>{100.0 * (b - a) / a};
}

auto compare_results(CompareOptions const& options, Json const& a_result, Json const& b_result)
    -> Comparison {
    auto a_inclusive{
        parse_zone_csv(options.output_directory / "a" / "inclusive.csv", options.root)};
    auto a_self{parse_zone_csv(options.output_directory / "a" / "self.csv", options.root)};
    auto b_inclusive{
        parse_zone_csv(options.output_directory / "b" / "inclusive.csv", options.root)};
    auto b_self{parse_zone_csv(options.output_directory / "b" / "self.csv", options.root)};
    Comparison result{.a_result = a_result, .b_result = b_result};
    for (auto* parsed : {&a_inclusive, &a_self, &b_inclusive, &b_self}) {
        result.warnings.insert(
            result.warnings.end(), parsed->warnings.begin(), parsed->warnings.end());
    }
    if (a_result.value("workload", Json{}) != b_result.value("workload", Json{})) {
        result.warnings.push_back(
            {.code = "workload_mismatch",
             .severity = "error",
             .message = "benchmark results describe different deterministic workloads"});
    }
    if (!a_result.contains("final_state") || !a_result["final_state"].is_object() ||
        !b_result.contains("final_state") || !b_result["final_state"].is_object()) {
        throw PipelineError{"benchmark_result_invalid", "benchmark final state is missing"};
    }
    if (a_result["final_state"] != b_result["final_state"]) {
        result.warnings.push_back(
            {.code = "final_state_mismatch",
             .severity = "error",
             .message = "benchmark results describe different final simulation states",
             .details = {{"a", a_result["final_state"]}, {"b", b_result["final_state"]}}});
    }
    std::map<ZoneKey, bool> keys;
    for (auto const& [key, _] : a_inclusive.zones) {
        keys[key] = true;
    }
    for (auto const& [key, _] : b_inclusive.zones) {
        keys[key] = true;
    }
    for (auto const& [key, _] : keys) {
        auto select = [&](ParsedZones const& zones) -> std::optional<ZoneStatistics> {
            auto found{zones.zones.find(key)};
            return found == zones.zones.end() ? std::nullopt
                                              : std::optional<ZoneStatistics>{found->second};
        };
        ZoneComparison zone{.identity = key,
                            .a_inclusive = select(a_inclusive),
                            .b_inclusive = select(b_inclusive),
                            .a_self = select(a_self),
                            .b_self = select(b_self)};
        auto const present{zone.a_inclusive && zone.b_inclusive && zone.a_self && zone.b_self};
        auto const& representative{
            zone.a_inclusive ? zone.a_inclusive
                             : (zone.b_inclusive ? zone.b_inclusive
                                                 : (zone.a_self ? zone.a_self : zone.b_self))};
        if (!present) {
            auto const missing{!zone.a_inclusive || !zone.a_self ? "a" : "b"};
            result.warnings.push_back(
                {.code = std::string{"missing_zone_"} + missing,
                 .severity = "warning",
                 .message = std::string{"zone is missing from configuration "} +
                            static_cast<char>(std::toupper(missing[0])),
                 .details = {{"zone",
                              {{"name", representative->name},
                               {"source_file", representative->source_file}}}}});
        }
        zone.counts_match = zone.a_inclusive && zone.b_inclusive &&
                            zone.a_inclusive->count == zone.b_inclusive->count;
        if (present && !zone.counts_match) {
            result.warnings.push_back(
                {.code = "invocation_count_mismatch",
                 .severity = "warning",
                 .message =
                     "zone invocation count differs: A=" + std::to_string(zone.a_inclusive->count) +
                     ", B=" + std::to_string(zone.b_inclusive->count),
                 .details = {{"zone",
                              {{"name", representative->name},
                               {"source_file", representative->source_file}}}}});
        }
        if (zone.a_inclusive && zone.b_inclusive) {
            zone.inclusive_delta_percent =
                percentage_delta(static_cast<double>(zone.a_inclusive->total_nanoseconds),
                                 static_cast<double>(zone.b_inclusive->total_nanoseconds));
        }
        if (zone.a_self && zone.b_self) {
            zone.self_delta_percent =
                percentage_delta(static_cast<double>(zone.a_self->total_nanoseconds),
                                 static_cast<double>(zone.b_self->total_nanoseconds));
        }
        zone.comparable = present && zone.counts_match;
        result.zones.push_back(std::move(zone));
    }
    try {
        result.overall_delta_percent =
            percentage_delta(a_result.at("timing").at("mean_tick_microseconds").get<double>(),
                             b_result.at("timing").at("mean_tick_microseconds").get<double>());
    } catch (...) {
        throw PipelineError{"benchmark_result_invalid", "benchmark timing result is missing"};
    }
    auto const has_error{std::ranges::any_of(
        result.warnings, [](Warning const& warning) { return warning.severity == "error"; })};
    result.status = has_error
                      ? ComparisonStatus::incomparable
                      : (result.warnings.empty() ? ComparisonStatus::complete
                                                 : ComparisonStatus::complete_with_warnings);
    return result;
}

auto to_json(Comparison const& comparison) -> Json {
    Json zones = Json::array();
    for (auto const& zone : comparison.zones) {
        Json serialized_zone = Json::object();
        Json identity = Json::object();
        identity["name"] = zone.identity.name;
        identity["source_file"] =
            zone.a_inclusive
                ? zone.a_inclusive->source_file
                : (zone.b_inclusive ? zone.b_inclusive->source_file : zone.identity.source_file);
        serialized_zone["identity"] = std::move(identity);
        Json invocation_count = Json::object();
        invocation_count["a"] = zone.a_inclusive ? Json(zone.a_inclusive->count) : Json(nullptr);
        invocation_count["b"] = zone.b_inclusive ? Json(zone.b_inclusive->count) : Json(nullptr);
        invocation_count["match"] = zone.counts_match;
        serialized_zone["invocation_count"] = std::move(invocation_count);
        Json inclusive = Json::object();
        inclusive["a"] = statistics_json(zone.a_inclusive);
        inclusive["b"] = statistics_json(zone.b_inclusive);
        inclusive["delta_percent"] =
            zone.inclusive_delta_percent ? Json(*zone.inclusive_delta_percent) : Json(nullptr);
        serialized_zone["inclusive"] = std::move(inclusive);
        Json self = Json::object();
        self["a"] = statistics_json(zone.a_self);
        self["b"] = statistics_json(zone.b_self);
        self["delta_percent"] =
            zone.self_delta_percent ? Json(*zone.self_delta_percent) : Json(nullptr);
        serialized_zone["self"] = std::move(self);
        serialized_zone["comparable"] = zone.comparable;
        zones.push_back(std::move(serialized_zone));
    }
    Json warnings = Json::array();
    for (auto const& warning : comparison.warnings) {
        warnings.push_back(warning_json(warning));
    }
    Json result = Json::object();
    result["schema_version"] = 1;
    result["status"] = status_text(comparison.status);
    Json overall = Json::object();
    overall["metric"] = "mean_tick_microseconds";
    overall["a"] = comparison.a_result;
    overall["b"] = comparison.b_result;
    overall["delta_percent"] =
        comparison.overall_delta_percent ? Json(*comparison.overall_delta_percent) : Json(nullptr);
    result["overall"] = std::move(overall);
    result["zones"] = std::move(zones);
    result["warnings"] = std::move(warnings);
    return result;
}

void print_summary(Comparison const& comparison,
                   std::filesystem::path const& output_directory,
                   int const top,
                   std::ostream& output) {
    auto const a_value{comparison.a_result.at("timing").at("mean_tick_microseconds").get<double>()};
    auto const b_value{comparison.b_result.at("timing").at("mean_tick_microseconds").get<double>()};
    auto comparable{std::vector<ZoneComparison const*>{}};
    for (auto const& zone : comparison.zones) {
        if (zone.comparable) {
            comparable.push_back(&zone);
        }
    }
    output << "Tracy comparison: " << status_text(comparison.status) << '\n';
    output << std::fixed << std::setprecision(3) << "Overall mean tick: A " << a_value << " us, B "
           << b_value << " us (";
    if (comparison.overall_delta_percent) {
        output << std::showpos << std::setprecision(2) << *comparison.overall_delta_percent << '%'
               << std::noshowpos;
    } else {
        output << "n/a";
    }
    output << ")\nZones: " << comparable.size() << " comparable, "
           << comparison.zones.size() - comparable.size() << " mismatched\n";
    std::stable_sort(comparable.begin(), comparable.end(), [](auto const* left, auto const* right) {
        return left->b_inclusive->total_nanoseconds - left->a_inclusive->total_nanoseconds >
               right->b_inclusive->total_nanoseconds - right->a_inclusive->total_nanoseconds;
    });
    auto print_group = [&](std::string_view heading, bool const regression) {
        std::vector<ZoneComparison const*> selected;
        if (regression) {
            for (auto const* zone : comparable) {
                if (*zone->inclusive_delta_percent > 0.0 &&
                    static_cast<int>(selected.size()) < top) {
                    selected.push_back(zone);
                }
            }
        } else {
            for (auto iterator{comparable.rbegin()}; iterator != comparable.rend(); ++iterator) {
                auto const* zone{*iterator};
                if (*zone->inclusive_delta_percent < 0.0 &&
                    static_cast<int>(selected.size()) < top) {
                    selected.push_back(zone);
                }
            }
        }
        if (selected.empty()) {
            return;
        }
        output << heading << ":\n";
        for (auto const* zone : selected) {
            auto const delta_ns{zone->b_inclusive->total_nanoseconds -
                                zone->a_inclusive->total_nanoseconds};
            output << std::showpos << std::fixed << std::setprecision(2) << "  " << std::setw(8)
                   << *zone->inclusive_delta_percent << "% " << std::setprecision(3)
                   << std::setw(10) << static_cast<double>(delta_ns) / 1'000'000.0 << " ms  "
                   << std::noshowpos << zone->identity.name << '\n';
        }
    };
    print_group("Largest regressions", true);
    print_group("Largest improvements", false);
    output << "Results: " << output_directory.string()
           << "\nTracy capture A: " << (output_directory / "a" / "capture.tracy").string()
           << "\nTracy capture B: " << (output_directory / "b" / "capture.tracy").string()
           << "\nOpen capture A in tracy-profiler, then use Compare > Open second trace and select "
              "capture B.\n";
}

auto run_application(int const argc,
                     char const* const* argv,
                     std::ostream& standard_output,
                     std::ostream& standard_error) -> int {
    auto parsed{parse_command_line(argc, argv)};
    standard_output << parsed.standard_output;
    standard_error << parsed.standard_error;
    if (!parsed.options) {
        return parsed.exit_code;
    }
    auto const& options{*parsed.options};
    auto const inside_jobserver{options.jobserver_child &&
                                GetEnvironmentVariableW(L"NUKETHEBEES_JOBSERVER_JOB", nullptr, 0) !=
                                    0};
    if (options.output_directory_explicit && !inside_jobserver &&
        std::filesystem::exists(options.output_directory) &&
        !std::filesystem::is_empty(options.output_directory)) {
        standard_error << "Tracy comparison failed: output directory is not empty: "
                       << options.output_directory.string() << '\n';
        return 1;
    }
    std::filesystem::create_directories(options.output_directory);
    auto manifest = make_manifest(options, "preparing");
    write_json(options.output_directory / "manifest.json", manifest);
    try {
        if (options.jobserver_child && !inside_jobserver) {
            throw PipelineError{"invalid_jobserver_child",
                                "Jobserver child marker requires an active job"};
        }
        if (!options.skip_build) {
            build_prerequisites(options);
        }
        if (!inside_jobserver) {
            auto child_arguments{option_arguments(options)};
            child_arguments.insert(child_arguments.begin(), "--jobserver-child");
            jobserver::SubmitRequest request{
                .metadata = {.name = "Tracy native benchmark comparison",
                             .kind = "benchmark",
                             .worktree = options.root},
                .command = {.executable = executable_path(),
                            .arguments = std::move(child_arguments),
                            .working_directory = options.root,
                            .environment = {}},
                .resources = {{.name = "machine", .mode = jobserver::ClaimMode::exclusive},
                              {.name = "benchmark", .mode = jobserver::ClaimMode::exclusive}},
                .timeout = std::nullopt,
                .suspect_after = std::nullopt,
                .disconnect_policy = jobserver::DisconnectPolicy::cancel,
            };
            auto result{jobserver::Client::run(
                request, [](std::string const& stream, std::string const& text) {
                    (stream == "stderr" ? std::cerr : std::cout) << text << std::flush;
                })};
            if (!result) {
                throw PipelineError{"jobserver_failed", result.error().message};
            }
            if (*result != 0 &&
                !std::filesystem::is_regular_file(options.output_directory / "comparison.json")) {
                throw PipelineError{"jobserver_failed",
                                    "jobserver command exited with " + std::to_string(*result)};
            }
            return *result;
        }
        return run_comparison(options, manifest, standard_output);
    } catch (PipelineError const& error) {
        write_failure(options, manifest, error);
        standard_error << "Tracy comparison failed: " << error.what()
                       << "\nResults: " << options.output_directory.string() << '\n';
        return 1;
    } catch (std::filesystem::filesystem_error const& error) {
        PipelineError const wrapped{"operating_system_error", error.what()};
        write_failure(options, manifest, wrapped);
        standard_error << "Tracy comparison failed: " << wrapped.what() << '\n';
        return 1;
    } catch (Json::exception const& error) {
        PipelineError const wrapped{"serialization_error", error.what()};
        write_failure(options, manifest, wrapped);
        standard_error << "Tracy comparison failed: " << wrapped.what() << '\n';
        return 1;
    }
}
} // namespace sandbox::perf
