#include "journal.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <fstream>

namespace {
TEST(JobserverJournal, RingRetainsRecentEventsAndFiltersAllHandles) {
    jobserver::Journal journal{{}, 3};
    for (std::uint64_t index{1}; index <= 5; ++index) {
        journal.append({.kind = jobserver::EventKind::lease_granted,
                        .client = jobserver::ClientId{index},
                        .command = jobserver::CommandId{index},
                        .lease = jobserver::LeaseId{index},
                        .gate = jobserver::GateId{2}},
                       "machine");
    }
    auto const all = journal.trace();
    ASSERT_EQ(all.size(), 3);
    EXPECT_EQ(all[0]["client"], 3);
    EXPECT_EQ(all[0]["payload_id"], all[2]["payload_id"]);
    EXPECT_EQ(journal
                  .trace({.client = jobserver::ClientId{4},
                          .command = jobserver::CommandId{4},
                          .lease = jobserver::LeaseId{4},
                          .gate = jobserver::GateId{2},
                          .kind = jobserver::EventKind::lease_granted})
                  .size(),
              1);
    EXPECT_TRUE(journal.trace({.kind = jobserver::EventKind::request_cancelled}).empty());
}
TEST(JobserverJournal, RotationReloadsRecentHistoryAndIgnoresInterruptedTail) {
    auto const directory{
        std::filesystem::temp_directory_path() /
        ("jobserver-journal-" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()))};
    {
        jobserver::Journal journal{directory, 100, 200};
        for (std::uint64_t index{1}; index <= 30; ++index) {
            journal.append({.kind = jobserver::EventKind::command_received,
                            .client = jobserver::ClientId{index}},
                           "opaque command metadata");
        }
    }
    EXPECT_TRUE(std::filesystem::exists(directory / "events.jsonl.3"));
    EXPECT_FALSE(std::filesystem::exists(directory / "events.jsonl.4"));
    {
        std::ofstream tail{directory / "events.jsonl", std::ios::app};
        tail << "[broken";
    }
    {
        jobserver::Journal journal{directory, 100, 200};
        auto const events = journal.trace();
        ASSERT_FALSE(events.empty());
        EXPECT_LT(events.size(), 30);
        EXPECT_EQ(events.back()["client"], 30);
    }
    std::filesystem::remove_all(directory);
}
}
