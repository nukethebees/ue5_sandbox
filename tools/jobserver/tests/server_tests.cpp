#include "jobserver/client.hpp"
#include "jobserver/executor.hpp"
#include "jobserver/protocol.hpp"
#include "jobserver/transport.hpp"

#include <Windows.h>

#include <gtest/gtest.h>
#include <tlhelp32.h>

#include <array>
#include <fstream>
#include <future>
#include <thread>

namespace jobserver_test {
using namespace jobserver;
using namespace std::chrono_literals;
using Json = nlohmann::json;
struct Process {
    HANDLE process{}, input{}, output{};
    DWORD pid{};
    ~Process() {
        if (process) {
            TerminateProcess(process, 1);
            WaitForSingleObject(process, 5000);
            CloseHandle(process);
        }
        if (input) {
            CloseHandle(input);
        }
        if (output) {
            CloseHandle(output);
        }
    }
    auto launch(std::filesystem::path const& executable,
                std::wstring args = {},
                bool broker = false) -> bool {
        SECURITY_ATTRIBUTES inherit{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
        HANDLE read{}, write{};
        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        if (broker) {
            if (!CreatePipe(&read, &input, &inherit, 0) ||
                !CreatePipe(&output, &write, &inherit, 0)) {
                return false;
            }
            SetHandleInformation(input, HANDLE_FLAG_INHERIT, 0);
            SetHandleInformation(output, HANDLE_FLAG_INHERIT, 0);
            startup.dwFlags = STARTF_USESTDHANDLES;
            startup.hStdInput = read;
            startup.hStdOutput = write;
            startup.hStdError = write;
        }
        auto line{L"\"" + executable.wstring() + L"\" " + args};
        PROCESS_INFORMATION info{};
        auto const created{CreateProcessW(executable.c_str(),
                                          line.data(),
                                          nullptr,
                                          nullptr,
                                          broker,
                                          CREATE_NO_WINDOW,
                                          nullptr,
                                          nullptr,
                                          &startup,
                                          &info)};
        if (read) {
            CloseHandle(read);
            CloseHandle(write);
        }
        if (!created) {
            return false;
        }
        process = info.hProcess;
        pid = info.dwProcessId;
        CloseHandle(info.hThread);
        return true;
    }
    void send(Json const& json) {
        auto const frame{protocol::encode_frame(json.dump())};
        ASSERT_TRUE(frame);
        DWORD written{};
        ASSERT_TRUE(
            WriteFile(input, frame->data(), static_cast<DWORD>(frame->size()), &written, nullptr));
        ASSERT_EQ(written, frame->size());
    }
    auto read_exact(void* buffer, DWORD size) -> bool {
        auto const deadline{GetTickCount64() + 5000};
        auto cursor{static_cast<char*>(buffer)};
        while (size && GetTickCount64() < deadline) {
            DWORD available{};
            if (!PeekNamedPipe(output, nullptr, 0, nullptr, &available, nullptr)) {
                return false;
            }
            if (!available) {
                Sleep(5);
                continue;
            }
            DWORD read{};
            if (!ReadFile(output, cursor, std::min(size, available), &read, nullptr)) {
                return false;
            }
            cursor += read;
            size -= read;
        }
        return size == 0;
    }
    auto next() -> Json {
        std::array<std::byte, 4> header{};
        if (!read_exact(header.data(), 4)) {
            return Json{{"type", "test_timeout"}};
        }
        auto size{protocol::decode_header(header)};
        if (!size) {
            return Json{{"type", "invalid_frame"}};
        }
        std::string payload(*size, '\0');
        if (!read_exact(payload.data(), *size)) {
            return Json{{"type", "truncated_frame"}};
        }
        return Json::parse(payload);
    }
    auto until(std::string const& type) -> Json {
        for (int i{}; i < 20; ++i) {
            auto result = next();
            if (result.value("type", "") == type || result.value("type", "") == "error" ||
                result.value("type", "") == "test_timeout") {
                return result;
            }
        }
        return Json{{"type", "too_many_messages"}};
    }
};
struct Integration : testing::Test {
    inline static std::filesystem::path directory;
    Process daemon;
    std::vector<std::unique_ptr<Process>> brokers;
    static void SetUpTestSuite() {
        directory = std::filesystem::temp_directory_path() /
                    ("lease-gate-tests-" + std::to_string(GetCurrentProcessId()));
        std::filesystem::create_directories(directory);
        auto const pipe{LR"(\\.\pipe\NukeTheBees.LeaseTests.)" +
                        std::to_wstring(GetCurrentProcessId())};
        _wputenv_s(L"NUKETHEBEES_JOBSERVER_TEST_PIPE", pipe.c_str());
        _wputenv_s(L"NUKETHEBEES_JOBSERVER_TEST_DATA", directory.c_str());
        _wputenv_s(L"NUKETHEBEES_JOBSERVER_TEST_HEARTBEAT_MS", L"40");
        _wputenv_s(L"NUKETHEBEES_JOBSERVER_TEST_WATCHDOG_MS", L"500");
    }
    static void TearDownTestSuite() {
        std::filesystem::remove_all(directory);
        _wputenv_s(L"NUKETHEBEES_JOBSERVER_TEST_PIPE", L"");
        _wputenv_s(L"NUKETHEBEES_JOBSERVER_TEST_DATA", L"");
        _wputenv_s(L"NUKETHEBEES_JOBSERVER_TEST_HEARTBEAT_MS", L"");
        _wputenv_s(L"NUKETHEBEES_JOBSERVER_TEST_WATCHDOG_MS", L"");
    }
    void SetUp() override {
        ASSERT_TRUE(daemon.launch(JOBSERVER_DAEMON_PATH));
        auto status{Client::status()};
        ASSERT_TRUE(status) << (status ? "" : status.error().message);
    }
    void TearDown() override {
        brokers.clear();
        for (int i{}; i < 100; ++i) {
            auto status{Client::status()};
            if (!status || Json::parse(*status)["jobs"].empty()) {
                break;
            }
            Sleep(10);
        }
        static_cast<void>(Client::shutdown());
        EXPECT_EQ(WaitForSingleObject(daemon.process, 5000), WAIT_OBJECT_0);
    }
    auto session() -> std::unique_ptr<Session> {
        auto result{Session::connect()};
        if (!result) {
            ADD_FAILURE() << result.error().message;
            return {};
        }
        return std::move(*result);
    }
    auto acquire(Session& session, LeaseMode mode = LeaseMode::shared) -> Grant {
        auto result{session.acquire({{"machine", mode}}, Json{{"name", "test"}})};
        if (!result) {
            ADD_FAILURE() << result.error().message;
            return {};
        }
        return *result;
    }
    auto broker() -> Process& {
        auto process{std::make_unique<Process>()};
        EXPECT_TRUE(process->launch(JOBSERVER_CLI_PATH, L"broker", true));
        EXPECT_EQ(process->next().value("type", ""), "ready");
        brokers.push_back(std::move(process));
        return *brokers.back();
    }
    auto command(std::string text, LeaseMode mode = LeaseMode::shared) -> Json {
        return {{"type", "command"},
                {"text", text},
                {"cwd", path_to_utf8(directory)},
                {"metadata", {{"name", "broker test"}, {"task", "lease tests"}}},
                {"gates",
                 Json::array({{{"name", "machine"},
                               {"mode", mode == LeaseMode::exclusive ? "exclusive" : "shared"}}})}};
    }
    auto child_path() -> std::filesystem::path {
        return directory / ("child-" + std::to_string(GetTickCount64()) + ".pid");
    }
    auto child(std::filesystem::path const& file) -> HANDLE {
        auto const deadline{GetTickCount64() + 5000};
        while (GetTickCount64() < deadline) {
            std::ifstream input{file};
            DWORD pid{};
            if (input >> pid; pid) {
                return OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
            }
            Sleep(5);
        }
        return nullptr;
    }
    auto tree_text(std::filesystem::path const& path, bool stays = false) -> std::string {
        return "& '" + std::string{JOBSERVER_TEST_HELPER_PATH} + "' tree '" + path_to_utf8(path) +
               "' " + (stays ? "stay" : "exit");
    }
};
TEST_F(Integration, SessionIdsPersistAcrossCommandsAndChangeOnReconnect) {
    auto a{session()};
    auto const id{a->id()};
    auto first{acquire(*a)};
    EXPECT_TRUE(a->release(first, 0));
    auto second{acquire(*a)};
    EXPECT_EQ(first.client, second.client);
    EXPECT_NE(first.command, second.command);
    a.reset();
    auto b{session()};
    EXPECT_NE(b->id(), id);
}
TEST_F(Integration, QueuedDisconnectWakesLaterSharedAndGrantedDisconnectReleasesExclusive) {
    auto a{session()};
    auto first{acquire(*a)};
    auto& b{broker()};
    b.send(command("exit 99", LeaseMode::exclusive));
    EXPECT_EQ(b.until("queued").value("type", ""), "queued");
    auto& c{broker()};
    c.send(command("exit 0"));
    EXPECT_EQ(c.until("queued").value("type", ""), "queued");
    TerminateProcess(b.process, 1);
    EXPECT_EQ(c.until("completed").value("exit_code", -1), 0);
    EXPECT_TRUE(a->release(first, 0));
    auto exclusive{acquire(*a, LeaseMode::exclusive)};
    static_cast<void>(exclusive);
    a.reset();
    auto d{session()};
    auto shared{acquire(*d)};
    EXPECT_TRUE(d->release(shared, 0));
}
TEST_F(Integration, PendingClearNeverLaunchesAndBrokerRemainsUsable) {
    auto a{session()};
    auto held{acquire(*a, LeaseMode::exclusive)};
    auto const marker{child_path()};
    auto& b{broker()};
    b.send(command("Set-Content -LiteralPath '" + path_to_utf8(marker) + "' launched"));
    ASSERT_EQ(b.until("queued").value("type", ""), "queued");
    b.send({{"type", "clear"}});
    EXPECT_EQ(b.until("cancelled").value("type", ""), "cancelled");
    EXPECT_EQ(b.until("idle").value("type", ""), "idle");
    EXPECT_TRUE(a->release(held, 0));
    b.send(command("exit 7"));
    EXPECT_EQ(b.until("completed").value("exit_code", -1), 7);
    EXPECT_FALSE(std::filesystem::exists(marker));
}
TEST_F(Integration, PersistentDescendantDoesNotHoldSharedLeaseOrBlockExclusiveBenchmark) {
    auto const marker{child_path()};
    auto& b{broker()};
    b.send(command(tree_text(marker)));
    auto result = b.until("completed");
    ASSERT_EQ(result.value("exit_code", -1), 0) << result.dump();
    auto descendant{child(marker)};
    ASSERT_NE(descendant, nullptr);
    EXPECT_EQ(WaitForSingleObject(descendant, 0), WAIT_TIMEOUT);
    auto exclusive_session{session()};
    auto grant{acquire(*exclusive_session, LeaseMode::exclusive)};
    EXPECT_EQ(WaitForSingleObject(descendant, 0), WAIT_TIMEOUT);
    EXPECT_TRUE(exclusive_session->release(grant, 0));
    TerminateProcess(b.process, 1);
    EXPECT_EQ(WaitForSingleObject(descendant, 5000), WAIT_OBJECT_0);
    CloseHandle(descendant);
}
TEST_F(Integration, BrokerCrashKillsSessionTreeAndDisconnectReleasesLease) {
    auto const marker{child_path()};
    auto& b{broker()};
    b.send(command(tree_text(marker, true)));
    ASSERT_EQ(b.until("starting").value("type", ""), "starting");
    auto descendant{child(marker)};
    ASSERT_NE(descendant, nullptr);
    auto& c{broker()};
    c.send(command("exit 0", LeaseMode::exclusive));
    EXPECT_EQ(c.until("queued").value("type", ""), "queued");
    TerminateProcess(b.process, 1);
    EXPECT_EQ(WaitForSingleObject(descendant, 5000), WAIT_OBJECT_0);
    EXPECT_EQ(c.until("completed").value("exit_code", -1), 0);
    CloseHandle(descendant);
}
TEST_F(Integration, HeartbeatsKeepQueuedWatchdogHealthy) {
    auto a{session()};
    auto held{acquire(*a, LeaseMode::exclusive)};
    auto& b{broker()};
    b.send(command("exit 0"));
    ASSERT_EQ(b.until("queued").value("type", ""), "queued");
    Sleep(1200);
    EXPECT_EQ(WaitForSingleObject(b.process, 0), WAIT_TIMEOUT);
    EXPECT_TRUE(a->release(held, 0));
    EXPECT_EQ(b.until("completed").value("exit_code", -1), 0);
}
TEST_F(Integration, WedgedServerTripsWatchdog) {
    auto a{session()};
    auto held{acquire(*a, LeaseMode::exclusive)};
    static_cast<void>(held);
    auto& b{broker()};
    b.send(command("exit 0"));
    ASSERT_EQ(b.until("queued").value("type", ""), "queued");
    HANDLE snapshot{CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0)};
    THREADENTRY32 entry{};
    entry.dwSize = sizeof(entry);
    std::vector<HANDLE> threads;
    if (Thread32First(snapshot, &entry)) {
        do {
            if (entry.th32OwnerProcessID == daemon.pid) {
                auto thread{OpenThread(THREAD_SUSPEND_RESUME, FALSE, entry.th32ThreadID)};
                if (thread) {
                    SuspendThread(thread);
                    threads.push_back(thread);
                }
            }
        } while (Thread32Next(snapshot, &entry));
    }
    CloseHandle(snapshot);
    auto response = b.until("error");
    for (auto thread : threads) {
        ResumeThread(thread);
        CloseHandle(thread);
    }
    EXPECT_EQ(response.value("code", ""), "server_unresponsive") << response.dump();
}
TEST_F(Integration, RestartHasNoStaleLeaseAndKeepsTrace) {
    auto a{session()};
    auto held{acquire(*a, LeaseMode::exclusive)};
    TerminateProcess(daemon.process, 1);
    WaitForSingleObject(daemon.process, 5000);
    CloseHandle(daemon.process);
    daemon.process = nullptr;
    EXPECT_EQ(WaitForSingleObject(a->lost_event(), 3000), WAIT_OBJECT_0);
    a.reset();
    ASSERT_TRUE(daemon.launch(JOBSERVER_DAEMON_PATH));
    auto b{session()};
    auto grant{acquire(*b, LeaseMode::exclusive)};
    EXPECT_NE(grant.client, held.client);
    auto trace{Client::trace(Json{{"lease", held.lease.value}})};
    ASSERT_TRUE(trace);
    EXPECT_FALSE(Json::parse(*trace)["events"].empty());
    EXPECT_TRUE(b->release(grant, 0));
}
TEST_F(Integration, ShutdownRefusesActiveLeaseAndCliUsesLocalRootExitCode) {
    auto a{session()};
    auto held{acquire(*a)};
    EXPECT_FALSE(Client::shutdown());
    EXPECT_TRUE(a->release(held, 0));
    auto& b{broker()};
    b.send(command("exit 23"));
    EXPECT_EQ(b.until("completed").value("exit_code", -1), 23);
}
TEST_F(Integration, OneShotRunsLocallyAndPropagatesRootExit) {
    Process command;
    ASSERT_TRUE(command.launch(JOBSERVER_CLI_PATH,
                               L"run --shared machine --name one-shot -- \"" +
                                   path_from_utf8(JOBSERVER_TEST_HELPER_PATH).wstring() +
                                   L"\" exit 19",
                               true));
    ASSERT_EQ(WaitForSingleObject(command.process, 5000), WAIT_OBJECT_0);
    DWORD code{};
    ASSERT_TRUE(GetExitCodeProcess(command.process, &code));
    EXPECT_EQ(code, 19U);
}
TEST_F(Integration, RecoveryRefusesResponsiveDaemonAndProtocolMismatchDoesNotAdmit) {
    auto recovery{Client::force_recover_daemon()};
    EXPECT_FALSE(recovery);
    EXPECT_TRUE(Client::ping());
    auto pipe{CreateFileW(transport::pipe_name().c_str(),
                          GENERIC_READ | GENERIC_WRITE,
                          0,
                          nullptr,
                          OPEN_EXISTING,
                          FILE_FLAG_OVERLAPPED,
                          nullptr)};
    ASSERT_NE(pipe, INVALID_HANDLE_VALUE);
    EXPECT_TRUE(transport::write_message(
        pipe, Json{{"type", "hello"}, {"protocol", {{"major", 1}, {"minor", 0}}}}.dump(), 1s));
    auto response{transport::read_message(pipe, 1s)};
    CloseHandle(pipe);
    // The incompatible connection is rejected and cannot create a client session.
    if (response) {
        EXPECT_EQ(Json::parse(*response).value("code", ""), "protocol_mismatch");
    }
    EXPECT_TRUE(Client::ping());
}
TEST_F(Integration, DaemonLossMakesBrokerCloseItsSessionTree) {
    auto const marker{child_path()};
    auto& b{broker()};
    b.send(command(tree_text(marker, true)));
    ASSERT_EQ(b.until("starting").value("type", ""), "starting");
    auto descendant{child(marker)};
    ASSERT_NE(descendant, nullptr);
    TerminateProcess(daemon.process, 1);
    WaitForSingleObject(daemon.process, 5000);
    EXPECT_EQ(b.until("error").value("type", ""), "error");
    EXPECT_EQ(WaitForSingleObject(descendant, 5000), WAIT_OBJECT_0);
    CloseHandle(descendant);
    CloseHandle(daemon.process);
    daemon.process = nullptr;
    ASSERT_TRUE(daemon.launch(JOBSERVER_DAEMON_PATH));
    EXPECT_TRUE(Client::ping());
}
}
