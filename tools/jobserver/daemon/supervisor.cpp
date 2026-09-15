#include "supervisor.hpp"
#include "environment.hpp"
#include "test_barrier.hpp"

#include <Windows.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cwchar>
#include <thread>
#include <vector>

namespace jobserver {
namespace {
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
            backslashes = 0;
            result.push_back(character);
        }
    }
    result.append(backslashes * 2, L'\\');
    result.push_back(L'\"');
    return result;
}

auto widen(std::string const& text) -> std::wstring {
    if (text.empty()) {
        return {};
    }
    auto const count{MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0)};
    if (count <= 0) {
        return {};
    }
    std::wstring result(static_cast<std::size_t>(count), L'\0');
    MultiByteToWideChar(CP_UTF8,
                        MB_ERR_INVALID_CHARS,
                        text.data(),
                        static_cast<int>(text.size()),
                        result.data(),
                        count);
    return result;
}

auto make_command_line(Command const& command) -> std::wstring {
    std::wstring result{quote_argument(command.executable.wstring())};
    for (auto const& argument : command.arguments) {
        result.push_back(L' ');
        result += quote_argument(widen(argument));
    }
    return result;
}

void close_if_valid(HANDLE const handle) {
    if (handle != nullptr && handle != INVALID_HANDLE_VALUE) {
        CloseHandle(handle);
    }
}

void read_output(HANDLE const pipe,
                 std::string stream,
                 Supervisor::Output const& output,
                 std::atomic<std::int64_t>& last_activity) {
    std::array<char, 4096> buffer{};
    for (;;) {
        DWORD read{};
        if (!ReadFile(pipe, buffer.data(), static_cast<DWORD>(buffer.size()), &read, nullptr) ||
            read == 0) {
            break;
        }
        last_activity.store(std::chrono::steady_clock::now().time_since_epoch().count(),
                            std::memory_order_relaxed);
        test_barrier("during_output");
        output(stream, std::string{buffer.data(), read});
    }
    CloseHandle(pipe);
}
}

Supervisor::~Supervisor() {
    std::scoped_lock const lock{mutex_};
    close_if_valid(static_cast<HANDLE>(job_handle_));
}

auto Supervisor::run(Command const& command,
                     std::optional<std::chrono::milliseconds> const timeout,
                     std::optional<std::chrono::milliseconds> const suspect_after,
                     Output output,
                     Health health,
                     std::function<bool()> connection_alive)
    -> std::expected<ProcessResult, Error> {
    auto environment{detail::make_environment(command)};
    if (!environment) {
        return std::unexpected(environment.error());
    }

    SECURITY_ATTRIBUTES pipe_security{};
    pipe_security.nLength = sizeof(SECURITY_ATTRIBUTES);
    pipe_security.bInheritHandle = TRUE;
    HANDLE stdout_read{};
    HANDLE stdout_write{};
    HANDLE stderr_read{};
    HANDLE stderr_write{};
    if (!CreatePipe(&stdout_read, &stdout_write, &pipe_security, 0) ||
        !CreatePipe(&stderr_read, &stderr_write, &pipe_security, 0)) {
        close_if_valid(stdout_read);
        close_if_valid(stdout_write);
        close_if_valid(stderr_read);
        close_if_valid(stderr_write);
        return std::unexpected(
            Error{"pipe_creation_failed", "Could not create child output pipes"});
    }
    SetHandleInformation(stdout_read, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(stderr_read, HANDLE_FLAG_INHERIT, 0);

    auto const job{CreateJobObjectW(nullptr, nullptr)};
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (job == nullptr ||
        !SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof(limits))) {
        close_if_valid(job);
        close_if_valid(stdout_read);
        close_if_valid(stdout_write);
        close_if_valid(stderr_read);
        close_if_valid(stderr_write);
        return std::unexpected(
            Error{"job_creation_failed", "Could not create the Windows Job Object"});
    }

    test_barrier("before_process_creation");

    SIZE_T attributes_size{};
    InitializeProcThreadAttributeList(nullptr, 1, 0, &attributes_size);
    std::vector<std::byte> attributes_storage(attributes_size);
    auto const attributes{reinterpret_cast<PPROC_THREAD_ATTRIBUTE_LIST>(attributes_storage.data())};
    auto attributes_initialized{
        InitializeProcThreadAttributeList(attributes, 1, 0, &attributes_size) != FALSE};
    HANDLE job_list[]{job};
    if (!attributes_initialized || !UpdateProcThreadAttribute(attributes,
                                                              0,
                                                              PROC_THREAD_ATTRIBUTE_JOB_LIST,
                                                              job_list,
                                                              sizeof(job_list),
                                                              nullptr,
                                                              nullptr)) {
        if (attributes_initialized) {
            DeleteProcThreadAttributeList(attributes);
        }
        close_if_valid(job);
        close_if_valid(stdout_read);
        close_if_valid(stdout_write);
        close_if_valid(stderr_read);
        close_if_valid(stderr_write);
        return std::unexpected(
            Error{"job_assignment_failed", "Could not prepare atomic Job Object assignment"});
    }

    STARTUPINFOEXW startup{};
    startup.StartupInfo.cb = sizeof(STARTUPINFOEXW);
    startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    startup.StartupInfo.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    startup.StartupInfo.hStdOutput = stdout_write;
    startup.StartupInfo.hStdError = stderr_write;
    startup.lpAttributeList = attributes;
    PROCESS_INFORMATION process{};
    auto command_line{make_command_line(command)};
    auto const working_directory{
        command.working_directory.empty() ? nullptr : command.working_directory.c_str()};
    if (!CreateProcessW(command.executable.c_str(),
                        command_line.data(),
                        nullptr,
                        nullptr,
                        TRUE,
                        CREATE_NO_WINDOW | CREATE_SUSPENDED | CREATE_UNICODE_ENVIRONMENT |
                            EXTENDED_STARTUPINFO_PRESENT,
                        environment->data(),
                        working_directory,
                        &startup.StartupInfo,
                        &process)) {
        DeleteProcThreadAttributeList(attributes);
        close_if_valid(job);
        close_if_valid(stdout_read);
        close_if_valid(stdout_write);
        close_if_valid(stderr_read);
        close_if_valid(stderr_write);
        return std::unexpected(
            Error{"process_creation_failed", "Could not create the supervised process"});
    }
    DeleteProcThreadAttributeList(attributes);
    test_barrier("after_process_creation");
    {
        std::scoped_lock const lock{mutex_};
        job_handle_ = job;
        if (cancellation_requested_) {
            TerminateJobObject(job, static_cast<UINT>(termination_exit_code_));
        }
    }
    test_barrier("before_process_resume");
    ResumeThread(process.hThread);
    close_if_valid(process.hThread);
    close_if_valid(stdout_write);
    close_if_valid(stderr_write);

    auto const start{std::chrono::steady_clock::now()};
    std::atomic<std::int64_t> last_activity{start.time_since_epoch().count()};
    std::jthread stdout_thread{
        read_output, stdout_read, "stdout", std::cref(output), std::ref(last_activity)};
    std::jthread stderr_thread{
        read_output, stderr_read, "stderr", std::cref(output), std::ref(last_activity)};
    bool timed_out{};
    bool root_exited{};
    bool suspected{};
    std::int64_t activity_at_suspicion{};
    std::uint64_t previous_cpu_time{};
    bool tested_descendant_exit{};
    for (;;) {
        if (!root_exited) {
            auto const wait{WaitForSingleObject(process.hProcess, 100)};
            if (wait == WAIT_OBJECT_0) {
                root_exited = true;
            } else if (wait == WAIT_FAILED) {
                terminate(true);
                close_if_valid(process.hProcess);
                return std::unexpected(
                    Error{"process_wait_failed", "Waiting for the supervised process failed"});
            }
        }
        if (timeout && std::chrono::steady_clock::now() - start >= *timeout) {
            timed_out = true;
            terminate(true);
        }
        if (!connection_alive()) {
            terminate(false);
        }

        JOBOBJECT_BASIC_ACCOUNTING_INFORMATION accounting{};
        if (!QueryInformationJobObject(job,
                                       JobObjectBasicAccountingInformation,
                                       &accounting,
                                       sizeof(accounting),
                                       nullptr)) {
            terminate(true);
            close_if_valid(process.hProcess);
            return std::unexpected(
                Error{"job_query_failed", "Could not query the supervised process tree"});
        }
        if (root_exited && accounting.ActiveProcesses == 0) {
            break;
        }
        if (root_exited && accounting.ActiveProcesses != 0 && !tested_descendant_exit) {
            tested_descendant_exit = true;
            test_barrier("after_root_exit_with_descendants");
        }
        auto const cpu_time{static_cast<std::uint64_t>(accounting.TotalUserTime.QuadPart) +
                            static_cast<std::uint64_t>(accounting.TotalKernelTime.QuadPart)};
        if (cpu_time != previous_cpu_time) {
            previous_cpu_time = cpu_time;
            last_activity.store(std::chrono::steady_clock::now().time_since_epoch().count(),
                                std::memory_order_relaxed);
            if (suspected) {
                suspected = false;
                health(JobHealth::normal, {});
            }
        }
        if (suspect_after) {
            auto const last{
                std::chrono::steady_clock::time_point{std::chrono::steady_clock::duration{
                    last_activity.load(std::memory_order_relaxed)}}};
            auto const inactive_for{std::chrono::steady_clock::now() - last};
            if (suspected && inactive_for < *suspect_after) {
                suspected = false;
                health(JobHealth::normal, {});
            } else if (!suspected && inactive_for >= *suspect_after) {
                suspected = true;
                activity_at_suspicion = last_activity.load(std::memory_order_relaxed);
                health(JobHealth::suspected_hang,
                       "no stdout, stderr, or process-tree CPU activity");
            }
        }
    }

    DWORD exit_code{};
    GetExitCodeProcess(process.hProcess, &exit_code);
    close_if_valid(process.hProcess);
    stdout_thread.join();
    stderr_thread.join();
    if (suspected && last_activity.load(std::memory_order_relaxed) > activity_at_suspicion) {
        health(JobHealth::normal, {});
    }

    bool killed{};
    int termination_exit_code{};
    {
        std::scoped_lock const lock{mutex_};
        killed = kill_requested_ || cancellation_requested_;
        termination_exit_code = termination_exit_code_;
        close_if_valid(static_cast<HANDLE>(job_handle_));
        job_handle_ = nullptr;
    }
    return ProcessResult{
        .exit_code = static_cast<int>(exit_code),
        .termination_exit_code = termination_exit_code,
        .killed = killed,
        .timed_out = timed_out,
    };
}

void Supervisor::cancel() {
    terminate(false);
}
auto Supervisor::contains_process(std::uint32_t const process_id) -> bool {
    auto const process{OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, process_id)};
    if (process == nullptr) {
        return false;
    }
    BOOL contained{};
    {
        std::scoped_lock const lock{mutex_};
        if (job_handle_ != nullptr) {
            static_cast<void>(
                IsProcessInJob(process, static_cast<HANDLE>(job_handle_), &contained));
        }
    }
    CloseHandle(process);
    return contained != FALSE;
}

void Supervisor::kill() {
    terminate(true);
}

void Supervisor::terminate(bool const killed) {
    std::scoped_lock const lock{mutex_};
    cancellation_requested_ = true;
    kill_requested_ = kill_requested_ || killed;
    if (termination_exit_code_ == 0 || killed) {
        termination_exit_code_ = killed ? 137 : 130;
    }
    if (job_handle_ != nullptr) {
        TerminateJobObject(static_cast<HANDLE>(job_handle_), killed ? 137U : 130U);
    }
}
}
