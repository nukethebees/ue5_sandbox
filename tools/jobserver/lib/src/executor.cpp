#include "jobserver/executor.hpp"

#include <Windows.h>

#include <algorithm>

namespace jobserver {
namespace executor_detail {
struct Handle {
    HANDLE value{};
    ~Handle() {
        if (value && value != INVALID_HANDLE_VALUE) {
            CloseHandle(value);
        }
    }
};
auto quote(std::wstring const& argument) -> std::wstring {
    std::wstring result{L"\""};
    std::size_t slashes{};
    for (auto const ch : argument) {
        if (ch == L'\\') {
            ++slashes;
            continue;
        }
        result.append(ch == L'"' ? slashes * 2 + 1 : slashes, L'\\');
        result.push_back(ch);
        slashes = 0;
    }
    result.append(slashes * 2, L'\\');
    return result + L'"';
}
auto environment(Grant const& grant, std::vector<GateClaim> const& gates) -> std::vector<wchar_t> {
    std::vector<std::wstring> entries;
    auto const own_machine{
        std::ranges::any_of(gates, [](GateClaim const& gate) { return gate.name == "machine"; })};
    auto const inherited{GetEnvironmentStringsW()};
    if (!inherited) {
        return {};
    }
    for (auto entry{inherited}; *entry; entry += wcslen(entry) + 1) {
        auto prefix = [&](wchar_t const* name) {
            return _wcsnicmp(entry, name, wcslen(name)) == 0;
        };
        if (!prefix(L"NUKETHEBEES_JOBSERVER_LEASE=") &&
            !(own_machine && prefix(L"NUKETHEBEES_JOBSERVER_MACHINE_MODE="))) {
            entries.emplace_back(entry);
        }
    }
    FreeEnvironmentStringsW(inherited);
    entries.push_back(L"NUKETHEBEES_JOBSERVER_LEASE=" + std::to_wstring(grant.lease.value));
    for (auto const& gate : gates) {
        if (gate.name == "machine") {
            entries.push_back(std::wstring{L"NUKETHEBEES_JOBSERVER_MACHINE_MODE="} +
                              (gate.mode == LeaseMode::exclusive ? L"exclusive" : L"shared"));
        }
    }
    std::ranges::sort(entries, [](std::wstring const& a, std::wstring const& b) {
        return _wcsicmp(a.c_str(), b.c_str()) < 0;
    });
    std::vector<wchar_t> block;
    for (auto const& entry : entries) {
        block.insert(block.end(), entry.begin(), entry.end());
        block.push_back(L'\0');
    }
    block.push_back(L'\0');
    return block;
}
}
auto resolve_executable(std::filesystem::path const& path) -> std::filesystem::path {
    if (path.is_absolute()) {
        return path;
    }
    wchar_t buffer[32768]{};
    auto const length{SearchPathW(nullptr, path.c_str(), L".exe", 32768, buffer, nullptr)};
    return length && length < 32768 ? std::filesystem::path{buffer} : path;
}
auto powershell_command(std::string const& text, std::filesystem::path cwd) -> Command {
    return {resolve_executable(L"pwsh.exe"),
            {"-NoLogo", "-NoProfile", "-NonInteractive", "-Command", text},
            std::move(cwd)};
}
LocalExecutor::LocalExecutor() {
    job_ = CreateJobObjectW(nullptr, nullptr);
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (job_ && !SetInformationJobObject(
                    job_, JobObjectExtendedLimitInformation, &limits, sizeof(limits))) {
        CloseHandle(job_);
        job_ = nullptr;
    }
}
LocalExecutor::~LocalExecutor() {
    if (job_) {
        CloseHandle(job_);
    }
}
auto LocalExecutor::run(Command const& command,
                        Session& session,
                        Grant const& grant,
                        std::vector<GateClaim> const& gates,
                        OutputFiles const* const output) -> std::expected<int, Error> {
    if (!job_) {
        return std::unexpected(
            Error{"containment_failed", "Cannot create broker session Job Object"});
    }
    executor_detail::Handle input, standard_output, standard_error;
    SECURITY_ATTRIBUTES inherit{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    auto duplicate = [](HANDLE handle) {
        HANDLE result{};
        if (handle && handle != INVALID_HANDLE_VALUE) {
            DuplicateHandle(GetCurrentProcess(),
                            handle,
                            GetCurrentProcess(),
                            &result,
                            0,
                            TRUE,
                            DUPLICATE_SAME_ACCESS);
        }
        return result;
    };
    if (output) {
        input.value = CreateFileW(L"NUL",
                                  GENERIC_READ,
                                  FILE_SHARE_READ | FILE_SHARE_WRITE,
                                  &inherit,
                                  OPEN_EXISTING,
                                  0,
                                  nullptr);
        standard_output.value = CreateFileW(output->standard_output.c_str(),
                                            GENERIC_WRITE,
                                            FILE_SHARE_READ | FILE_SHARE_WRITE,
                                            &inherit,
                                            CREATE_ALWAYS,
                                            0,
                                            nullptr);
        standard_error.value = CreateFileW(output->standard_error.c_str(),
                                           GENERIC_WRITE,
                                           FILE_SHARE_READ | FILE_SHARE_WRITE,
                                           &inherit,
                                           CREATE_ALWAYS,
                                           0,
                                           nullptr);
    } else {
        input.value = duplicate(GetStdHandle(STD_INPUT_HANDLE));
        standard_output.value = duplicate(GetStdHandle(STD_OUTPUT_HANDLE));
        standard_error.value = duplicate(GetStdHandle(STD_ERROR_HANDLE));
        if (!input.value) {
            input.value = CreateFileW(L"NUL",
                                      GENERIC_READ,
                                      FILE_SHARE_READ | FILE_SHARE_WRITE,
                                      &inherit,
                                      OPEN_EXISTING,
                                      0,
                                      nullptr);
        }
    }
    if (!input.value || input.value == INVALID_HANDLE_VALUE || !standard_output.value ||
        standard_output.value == INVALID_HANDLE_VALUE || !standard_error.value ||
        standard_error.value == INVALID_HANDLE_VALUE) {
        return std::unexpected(Error{"output_failed", "Cannot open command input/output handles"});
    }
    SIZE_T bytes{};
    InitializeProcThreadAttributeList(nullptr, 2, 0, &bytes);
    std::vector<std::byte> storage(bytes);
    auto const attributes{reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(storage.data())};
    if (!InitializeProcThreadAttributeList(attributes, 2, 0, &bytes)) {
        return std::unexpected(Error{"launch_failed", "Cannot initialize process attributes"});
    }
    struct Attributes {
        LPPROC_THREAD_ATTRIBUTE_LIST value;
        ~Attributes() { DeleteProcThreadAttributeList(value); }
    } cleanup{attributes};
    HANDLE handles[]{input.value, standard_output.value, standard_error.value};
    if (!UpdateProcThreadAttribute(attributes,
                                   0,
                                   PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
                                   handles,
                                   sizeof(handles),
                                   nullptr,
                                   nullptr) ||
        !UpdateProcThreadAttribute(
            attributes, 0, PROC_THREAD_ATTRIBUTE_JOB_LIST, &job_, sizeof(job_), nullptr, nullptr)) {
        return std::unexpected(Error{"launch_failed", "Cannot set broker containment attributes"});
    }
    STARTUPINFOEXW startup{};
    startup.StartupInfo.cb = sizeof(startup);
    startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    startup.StartupInfo.hStdInput = input.value;
    startup.StartupInfo.hStdOutput = standard_output.value;
    startup.StartupInfo.hStdError = standard_error.value;
    startup.lpAttributeList = attributes;
    auto const executable{resolve_executable(command.executable)};
    auto line{executor_detail::quote(executable.wstring())};
    for (auto const& argument : command.arguments) {
        line += L" " + executor_detail::quote(path_from_utf8(argument).wstring());
    }
    auto environment{executor_detail::environment(grant, gates)};
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(executable.c_str(),
                        line.data(),
                        nullptr,
                        nullptr,
                        TRUE,
                        CREATE_SUSPENDED | CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT |
                            EXTENDED_STARTUPINFO_PRESENT,
                        environment.data(),
                        command.working_directory.empty() ? nullptr
                                                          : command.working_directory.c_str(),
                        &startup.StartupInfo,
                        &process)) {
        return std::unexpected(Error{
            "launch_failed", "Local CreateProcess failed: " + std::to_string(GetLastError())});
    }
    executor_detail::Handle root{process.hProcess}, thread{process.hThread};
    if (auto started{session.started(grant, process.dwProcessId)}; !started) {
        TerminateJobObject(job_, 1);
        return std::unexpected(started.error());
    }
    if (ResumeThread(thread.value) == MAXDWORD) {
        TerminateJobObject(job_, 1);
        return std::unexpected(Error{"launch_failed", "Cannot resume local command"});
    }
    HANDLE const wait[]{root.value, session.lost_event()};
    auto const result{WaitForMultipleObjects(2, wait, FALSE, INFINITE)};
    if (result != WAIT_OBJECT_0) {
        TerminateJobObject(job_, 1);
        return std::unexpected(session.failure());
    }
    DWORD exit_code{};
    if (!GetExitCodeProcess(root.value, &exit_code)) {
        return std::unexpected(Error{"wait_failed", "Cannot read root exit code"});
    }
    return static_cast<int>(exit_code);
}
}
