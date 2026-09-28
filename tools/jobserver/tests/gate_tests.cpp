#include "gate.hpp"

#include <gtest/gtest.h>

namespace {
using namespace jobserver;
struct GateTest : testing::Test {
    Journal journal{{}, 1000};
    GateQueue queue{journal};
    auto request(LeaseMode mode = LeaseMode::shared, std::string name = "machine") -> Admission {
        return *queue.acquire(queue.connect(), {{std::move(name), mode}}, nlohmann::json::object());
    }
    void release(Admission const& a) { ASSERT_TRUE(queue.release(a.client, a.lease, 0)); }
};
TEST_F(GateTest, SharedOverlapExclusiveDrainsAndLaterSharedResumeTogether) {
    auto a{request()};
    auto b{request()};
    auto c{request()};
    EXPECT_TRUE(a.granted && b.granted && c.granted);
    auto d{request(LeaseMode::exclusive)};
    auto e{request()};
    auto f{request()};
    EXPECT_FALSE(d.granted || e.granted || f.granted);
    release(a);
    release(b);
    EXPECT_FALSE(queue.find(d.client)->granted);
    release(c);
    EXPECT_TRUE(queue.find(d.client)->granted);
    EXPECT_FALSE(queue.find(e.client)->granted);
    release(d);
    EXPECT_TRUE(queue.find(e.client)->granted && queue.find(f.client)->granted);
}
TEST_F(GateTest, ExclusiveRequestsKeepTheirOrder) {
    auto a{request(LeaseMode::exclusive)};
    auto b{request(LeaseMode::exclusive)};
    auto c{request(LeaseMode::exclusive)};
    release(a);
    EXPECT_TRUE(queue.find(b.client)->granted);
    EXPECT_FALSE(queue.find(c.client)->granted);
    release(b);
    EXPECT_TRUE(queue.find(c.client)->granted);
}
TEST_F(GateTest, DisconnectCancelsQueuedAndReleasesBothModes) {
    auto a{request()};
    auto b{request(LeaseMode::exclusive)};
    auto c{request()};
    queue.disconnect(b.client);
    EXPECT_EQ(queue.find(b.client), nullptr);
    EXPECT_TRUE(queue.find(c.client)->granted);
    auto d{request(LeaseMode::exclusive)};
    queue.disconnect(a.client);
    queue.disconnect(c.client);
    EXPECT_TRUE(queue.find(d.client)->granted);
    auto e{request()};
    queue.disconnect(d.client);
    EXPECT_TRUE(queue.find(e.client)->granted);
}
TEST_F(GateTest, NamedGatesAreIndependentAndMultiGateAdmissionIsAtomic) {
    auto a{request(LeaseMode::exclusive, "integration/dev")};
    auto b{request()};
    auto c{*queue.acquire(
        queue.connect(),
        {{"integration/dev", LeaseMode::exclusive}, {"machine", LeaseMode::exclusive}},
        nlohmann::json::object())};
    auto d{request()};
    EXPECT_TRUE(a.granted && b.granted);
    EXPECT_FALSE(c.granted || d.granted);
    release(b);
    EXPECT_FALSE(queue.find(c.client)->granted);
    release(a);
    EXPECT_TRUE(queue.find(c.client)->granted);
}
TEST_F(GateTest, StatusAndTraceIdentifyReservationAndExclusiveOwner) {
    auto a{request()};
    auto b{request(LeaseMode::exclusive)};
    auto c{request()};
    EXPECT_EQ(queue.find(c.client)->blockers, std::vector<LeaseId>{b.lease});
    EXPECT_EQ(queue.status()["gates"][0]["exclusive_owner"], 0);
    release(a);
    EXPECT_EQ(queue.status()["gates"][0]["exclusive_owner"], b.lease.value);
    auto events = journal.trace(
        {.client = b.client, .lease = b.lease, .kind = EventKind::exclusive_granted});
    ASSERT_EQ(events.size(), 1);
    EXPECT_EQ(events[0]["command"], b.command.value);
}
TEST_F(GateTest, ClearCanRelinquishRacingGrantButCannotCancelStartedCommand) {
    auto a{request()};
    EXPECT_TRUE(queue.cancel(a.client, a.lease));
    auto b{request()};
    EXPECT_TRUE(queue.started(b.client, b.lease, 123));
    EXPECT_FALSE(queue.cancel(b.client, b.lease));
}
}
