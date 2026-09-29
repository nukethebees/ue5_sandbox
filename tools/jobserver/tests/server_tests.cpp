#include "server.hpp"

#include "jobserver/protocol.hpp"
#include "jobserver/transport.hpp"

#include <Windows.h>

#include <gtest/gtest.h>

#include <chrono>
#include <thread>

namespace jobserver::tests {
using Json = nlohmann::json;

class Pipe {
  public:
    explicit Pipe(std::wstring const& name) {
        auto const deadline{std::chrono::steady_clock::now() + std::chrono::seconds{5}};
        do {
            handle_ = CreateFileW(name.c_str(),
                                  GENERIC_READ | GENERIC_WRITE,
                                  0,
                                  nullptr,
                                  OPEN_EXISTING,
                                  FILE_FLAG_OVERLAPPED,
                                  nullptr);
            if (handle_ != INVALID_HANDLE_VALUE) {
                return;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds{10});
        } while (std::chrono::steady_clock::now() < deadline);
        throw std::runtime_error{"Test pipe unavailable"};
    }
    ~Pipe() { CloseHandle(handle_); }
    Pipe(Pipe const&) = delete;
    auto operator=(Pipe const&) -> Pipe& = delete;
    auto exchange(Json const& request) -> Json {
        auto const sent{transport::write_message(handle_, request.dump(), std::chrono::seconds{5})};
        if (!sent) {
            throw std::runtime_error{sent.error().message};
        }
        auto const response{transport::read_message(handle_, std::chrono::seconds{5})};
        if (!response) {
            throw std::runtime_error{response.error().message};
        }
        return Json::parse(*response);
    }
    auto hello() -> Json {
        return exchange({{"type", "hello"}, {"protocol", {{"major", protocol::major_version}}}});
    }
  private:
    HANDLE handle_{INVALID_HANDLE_VALUE};
};

class Fixture {
  public:
    explicit Fixture(std::filesystem::path codex)
        : endpoint{LR"(\\.\pipe\NukeTheBees.AdmissionTest.)" +
                   std::to_wstring(GetCurrentProcessId()) + L"." +
                   std::to_wstring(GetTickCount64())}
        , server{endpoint, std::move(codex)}
        , thread{[this] { EXPECT_EQ(server.run(), 0); }} {}
    ~Fixture() {
        Pipe control{endpoint + L".control"};
        EXPECT_EQ(control.hello()["type"], "hello_ack");
        EXPECT_EQ(control.exchange({{"type", "shutdown"}})["type"], "accepted");
    }
    std::wstring endpoint;
    Server server;
    // Declared last so its join occurs before server destruction.
    std::jthread thread;
};

auto current_executable() -> std::filesystem::path {
    std::wstring path(32768, L'\0');
    path.resize(GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size())));
    return path;
}

TEST(JobserverServer, RejectsNonCodexButAllowsReadOnlyDiagnostics) {
    Fixture fixture{L"C:/not-the-canonical-codex.exe"};
    Pipe scheduling{fixture.endpoint};
    EXPECT_EQ(scheduling.hello()["code"], "client_rejected");
    Pipe control{fixture.endpoint + L".control"};
    ASSERT_EQ(control.hello()["type"], "hello_ack");
    EXPECT_EQ(control.exchange({{"type", "status"}})["tickets"].size(), 0U);
    EXPECT_EQ(control.exchange({{"type", "trace"}})["type"], "trace");
    EXPECT_EQ(
        control.exchange({{"type", "request"}, {"mode", "shared"}, {"name", "forbidden"}})["code"],
        "unknown_message");
    EXPECT_EQ(control.exchange({{"type", "release"}})["code"], "unknown_message");
}

TEST(JobserverServer, ValidatesProcessAndRejectsDuplicateConnection) {
    Fixture fixture{current_executable()};
    Pipe scheduling{fixture.endpoint};
    ASSERT_EQ(scheduling.hello()["type"], "hello_ack");
    Pipe duplicate{fixture.endpoint};
    EXPECT_EQ(duplicate.hello()["code"], "duplicate_client");
    EXPECT_EQ(
        scheduling.exchange({{"type", "request"}, {"mode", "shared"}, {"name", "build"}})["type"],
        "granted");
    EXPECT_EQ(scheduling.exchange(
                  {{"type", "request"}, {"mode", "exclusive"}, {"name", "again"}})["code"],
              "ticket_exists");
    EXPECT_EQ(scheduling.exchange({{"type", "release"}})["type"], "released");
    EXPECT_EQ(scheduling.exchange({{"type", "run"}})["code"], "unknown_message");
    EXPECT_EQ(scheduling.exchange({{"type", "broker"}})["code"], "unknown_message");
}
}
