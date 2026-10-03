#include "commands.hpp"

#include "jobserver/client/request.hpp"
#include "jobserver/server/server.hpp"
#include "jobserver/ticket_id.hpp"

#include <Windows.h>

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <format>
#include <thread>

namespace jobserver::tests {
class Fixture {
  public:
    Fixture()
        : endpoint{std::format(
              LR"(\\.\pipe\NukeTheBees.JobsBoardTest.{}.{})", GetCurrentProcessId(), sequence_++)}
        , server_{endpoint}
        , thread_{[this] { EXPECT_EQ(server_.run(), 0); }} {
        auto const deadline{std::chrono::steady_clock::now() + std::chrono::seconds{5}};
        while (!WaitNamedPipeW(endpoint.c_str(), 10)) {
            if (std::chrono::steady_clock::now() >= deadline) {
                throw std::runtime_error{"Test jobs board did not start"};
            }
            std::this_thread::sleep_for(std::chrono::milliseconds{10});
        }
    }
    ~Fixture() {
        auto const status{client::request({{"type", "status"}}, endpoint)};
        if (status) {
            for (auto const& ticket : status->at("tickets")) {
                auto const type{ticket.at("state") == "Running" ? "end" : "cancel"};
                EXPECT_TRUE(client::request({{"type", type}, {"id", ticket.at("id")}}, endpoint));
            }
        }
        EXPECT_TRUE(client::request({{"type", "shutdown"}}, endpoint));
    }
    auto command(std::vector<std::string> const& args) const -> client::JsonResult {
        auto const message{client::cli::parse(args)};
        if (!message) {
            return client::JsonResult{std::unexpect, "usage", message.error().message};
        }
        return client::request(message->message, endpoint);
    }
    std::filesystem::path endpoint;
  private:
    inline static std::atomic<unsigned> sequence_{};
    server::Server server_;
    std::jthread thread_;
};

TEST(JobsCliServer, RequestCheckStartEndCancelAndStatusRoundTrip) {
    Fixture fixture;
    auto const a{fixture.command({"request", "shared", "build tools"})};
    ASSERT_TRUE(a);
    EXPECT_EQ(a->at("state"), "Ready");
    auto const aid{std::to_string(a->at("id").get<TicketId>())};
    EXPECT_EQ(fixture.command({"check", aid})->at("state"), "Ready");
    EXPECT_EQ(fixture.command({"start", aid})->at("state"), "Running");
    auto const b{fixture.command({"request", "exclusive", "benchmark"})};
    ASSERT_TRUE(b);
    EXPECT_EQ(b->at("state"), "Queued");
    auto const bid{std::to_string(b->at("id").get<TicketId>())};
    auto const c{fixture.command({"request", "shared", "tests"})};
    ASSERT_TRUE(c);
    auto const cid{std::to_string(c->at("id").get<TicketId>())};
    auto const status{fixture.command({"status"})};
    ASSERT_TRUE(status);
    ASSERT_EQ(status->at("tickets").size(), 3U);
    EXPECT_EQ(status->at("tickets")[0].at("id"), a->at("id"));
    EXPECT_EQ(status->at("tickets")[1].at("id"), b->at("id"));
    EXPECT_EQ(status->at("tickets")[2].at("id"), c->at("id"));
    auto const output{client::cli::format_reply(*status)};
    EXPECT_NE(output.find("Running\n"), std::string::npos);
    EXPECT_NE(output.find("Ready\n  -\n"), std::string::npos);
    EXPECT_LT(output.find("benchmark"), output.find("tests"));
    EXPECT_FALSE(fixture.command({"start", bid}));
    EXPECT_EQ(fixture.command({"end", aid})->at("state"), "Done");
    EXPECT_EQ(fixture.command({"check", bid})->at("state"), "Ready");
    EXPECT_EQ(fixture.command({"check", cid})->at("state"), "Queued");
    EXPECT_EQ(fixture.command({"start", bid})->at("state"), "Running");
    EXPECT_FALSE(fixture.command({"cancel", bid}));
    EXPECT_FALSE(fixture.command({"start", bid}));
    EXPECT_EQ(fixture.command({"end", bid})->at("state"), "Done");
    EXPECT_EQ(fixture.command({"check", cid})->at("state"), "Ready");
    EXPECT_EQ(fixture.command({"cancel", cid})->at("state"), "Cancelled");
    EXPECT_FALSE(fixture.command({"check", cid}));
    EXPECT_TRUE(fixture.command({"status"})->at("tickets").empty());
}

TEST(JobsCliServer, InvalidMessagesReturnErrorsAndLeaveBoardUsable) {
    Fixture fixture;
    EXPECT_FALSE(client::request({{"type", "request"}, {"mode", "invalid"}, {"name", "x"}},
                                 fixture.endpoint));
    EXPECT_FALSE(client::request({{"type", "check"}, {"id", "one"}}, fixture.endpoint));
    EXPECT_FALSE(client::request({{"type", "request"}}, fixture.endpoint));
    EXPECT_FALSE(fixture.command({"check", "999"}));
    auto const ticket{fixture.command({"request", "shared", "build"})};
    ASSERT_TRUE(ticket);
    EXPECT_FALSE(fixture.command({"shutdown"}));
    EXPECT_EQ(fixture.command({"status"})->at("tickets").size(), 1U);
}

}
