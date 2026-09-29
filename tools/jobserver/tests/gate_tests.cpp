#include "gate.hpp"

#include <gtest/gtest.h>

namespace jobserver {

TEST(GateQueue, SharedOverlapExclusiveBarrierAndDeterministicOrder) {
    Journal journal;
    GateQueue queue{journal};
    auto const a{queue.connect()};
    auto const b{queue.connect()};
    auto const c{queue.connect()};
    auto const d{queue.connect()};
    auto const e{queue.connect()};
    ASSERT_TRUE(queue.request(a, Mode::shared, "A"));
    ASSERT_TRUE(queue.request(b, Mode::shared, "B"));
    ASSERT_TRUE(queue.request(c, Mode::exclusive, "C"));
    ASSERT_TRUE(queue.request(d, Mode::shared, "D"));
    ASSERT_TRUE(queue.request(e, Mode::shared, "E"));
    EXPECT_TRUE(queue.find(a)->granted);
    EXPECT_TRUE(queue.find(b)->granted);
    EXPECT_FALSE(queue.find(c)->granted);
    EXPECT_FALSE(queue.find(d)->granted);
    EXPECT_TRUE(queue.release(a));
    EXPECT_FALSE(queue.find(c)->granted);
    EXPECT_TRUE(queue.release(b));
    EXPECT_TRUE(queue.find(c)->granted);
    EXPECT_FALSE(queue.find(d)->granted);
    EXPECT_TRUE(queue.release(c));
    EXPECT_TRUE(queue.find(d)->granted);
    EXPECT_TRUE(queue.find(e)->granted);
    auto grants = nlohmann::json::array();
    for (auto const& event : journal.trace()) {
        if (event["event"] == "granted") {
            grants.push_back(event["name"]);
        }
    }
    EXPECT_EQ(grants, nlohmann::json::array({"A", "B", "C", "D", "E"}));
}

TEST(GateQueue, RejectsSecondQueuedAndGrantedRequest) {
    Journal journal;
    GateQueue queue{journal};
    auto const a{queue.connect()};
    auto const b{queue.connect()};
    ASSERT_TRUE(queue.request(a, Mode::exclusive, "A"));
    ASSERT_TRUE(queue.request(b, Mode::shared, "B"));
    EXPECT_FALSE(queue.request(a, Mode::shared, "duplicate granted"));
    EXPECT_FALSE(queue.request(b, Mode::exclusive, "duplicate queued"));
    EXPECT_EQ(queue.status()["tickets"].size(), 2U);
}

TEST(GateQueue, QueuedDisconnectRemovesBarrierAndAdmitsShared) {
    Journal journal;
    GateQueue queue{journal};
    auto const a{queue.connect()};
    auto const b{queue.connect()};
    auto const c{queue.connect()};
    ASSERT_TRUE(queue.request(a, Mode::shared, "A"));
    ASSERT_TRUE(queue.request(b, Mode::exclusive, "B"));
    ASSERT_TRUE(queue.request(c, Mode::shared, "C"));
    queue.disconnect(b);
    EXPECT_EQ(queue.find(b), nullptr);
    EXPECT_TRUE(queue.find(c)->granted);
}

TEST(GateQueue, GrantedDisconnectAdmitsNextExclusive) {
    Journal journal;
    GateQueue queue{journal};
    auto const a{queue.connect()};
    auto const b{queue.connect()};
    ASSERT_TRUE(queue.request(a, Mode::exclusive, "A"));
    ASSERT_TRUE(queue.request(b, Mode::exclusive, "B"));
    queue.disconnect(a);
    EXPECT_TRUE(queue.find(b)->granted);
}

TEST(Journal, KeepsOnlyRecentDiagnosticEvents) {
    Journal journal;
    for (unsigned i{}; i < 1005; ++i) {
        journal.append("requested", ClientId{i + 1});
    }
    auto const events = journal.trace();
    EXPECT_EQ(events.size(), 1000U);
    EXPECT_EQ(events.front()["client"], 6);
    EXPECT_EQ(events.back()["client"], 1005);
}
}
