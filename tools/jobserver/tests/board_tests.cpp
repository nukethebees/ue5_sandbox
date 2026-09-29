#include "board.hpp"

#include <gtest/gtest.h>

namespace jobserver::tests {
TEST(JobsBoard, LeadingSharedTicketsBecomeReadyAndStartIndependently) {
    JobsBoard board;
    auto const a{board.request(Mode::shared, "build tools")->id};
    auto const b{board.request(Mode::shared, "build game")->id};
    EXPECT_LT(a, b);
    EXPECT_EQ(board.check(a)->state, State::ready);
    EXPECT_EQ(board.check(b)->state, State::ready);
    EXPECT_EQ(board.transition(b, State::running)->state, State::running);
    EXPECT_EQ(board.check(a)->state, State::ready);
    EXPECT_EQ(board.transition(a, State::running)->state, State::running);
    EXPECT_EQ(board.transition(a, State::done)->state, State::done);
    EXPECT_FALSE(board.check(a));
    EXPECT_EQ(board.transition(b, State::done)->state, State::done);
    EXPECT_TRUE(board.empty());
}

TEST(JobsBoard, ExclusiveWaitsForEarlierReadyAndRunningSharedTickets) {
    JobsBoard board;
    auto const a{board.request(Mode::shared, "build A")->id};
    auto const b{board.request(Mode::shared, "build B")->id};
    ASSERT_TRUE(board.transition(a, State::running));
    auto const c{board.request(Mode::exclusive, "benchmark")->id};
    auto const d{board.request(Mode::shared, "tests")->id};
    EXPECT_EQ(board.check(c)->state, State::queued);
    EXPECT_EQ(board.check(d)->state, State::queued);
    ASSERT_TRUE(board.transition(a, State::done));
    EXPECT_EQ(board.check(c)->state, State::queued);
    ASSERT_TRUE(board.transition(b, State::running));
    ASSERT_TRUE(board.transition(b, State::done));
    EXPECT_EQ(board.check(c)->state, State::ready);
    EXPECT_EQ(board.check(d)->state, State::queued);
    ASSERT_TRUE(board.transition(c, State::running));
    auto const e{board.request(Mode::shared, "more tests")->id};
    EXPECT_EQ(board.check(d)->state, State::queued);
    EXPECT_EQ(board.check(e)->state, State::queued);
    ASSERT_TRUE(board.transition(c, State::done));
    EXPECT_EQ(board.check(d)->state, State::ready);
    EXPECT_EQ(board.check(e)->state, State::ready);
}

TEST(JobsBoard, CancellationRecalculatesReadinessWithoutReordering) {
    JobsBoard board;
    auto const a{board.request(Mode::exclusive, "first")->id};
    auto const b{board.request(Mode::exclusive, "second")->id};
    auto const c{board.request(Mode::shared, "third")->id};
    EXPECT_EQ(board.check(a)->state, State::ready);
    EXPECT_EQ(board.check(b)->state, State::queued);
    EXPECT_EQ(board.transition(b, State::cancelled)->state, State::cancelled);
    EXPECT_FALSE(board.check(b));
    EXPECT_EQ(board.check(c)->state, State::queued);
    EXPECT_EQ(board.transition(a, State::cancelled)->state, State::cancelled);
    EXPECT_EQ(board.check(c)->state, State::ready);
    auto const d{board.request(Mode::exclusive, "fourth")->id};
    ASSERT_TRUE(board.transition(c, State::cancelled));
    EXPECT_EQ(board.check(d)->state, State::ready);
}

TEST(JobsBoard, RemovingQueuedExclusiveBarrierAdmitsLaterShared) {
    JobsBoard board;
    auto const a{board.request(Mode::shared, "build")->id};
    auto const b{board.request(Mode::exclusive, "benchmark")->id};
    auto const c{board.request(Mode::shared, "tests")->id};
    ASSERT_TRUE(board.transition(a, State::running));
    ASSERT_TRUE(board.transition(b, State::cancelled));
    EXPECT_EQ(board.check(a)->state, State::running);
    EXPECT_EQ(board.check(c)->state, State::ready);
}

TEST(JobsBoard, InvalidTransitionsLeaveTicketsUnchanged) {
    JobsBoard board;
    EXPECT_FALSE(board.request(Mode::shared, ""));
    auto const a{board.request(Mode::exclusive, "first")->id};
    auto const b{board.request(Mode::shared, "second")->id};
    EXPECT_FALSE(board.transition(b, State::running));
    EXPECT_FALSE(board.transition(b, State::done));
    EXPECT_FALSE(board.transition(a, State::done));
    EXPECT_EQ(board.check(a)->state, State::ready);
    EXPECT_EQ(board.check(b)->state, State::queued);
    ASSERT_TRUE(board.transition(a, State::running));
    EXPECT_FALSE(board.transition(a, State::running));
    EXPECT_FALSE(board.transition(a, State::cancelled));
    EXPECT_EQ(board.check(a)->state, State::running);
    ASSERT_TRUE(board.transition(a, State::done));
    EXPECT_FALSE(board.transition(a, State::done));
    EXPECT_EQ(board.check(b)->state, State::ready);
    EXPECT_FALSE(board.check(999));
    EXPECT_FALSE(board.transition(999, State::cancelled));
}

TEST(JobsBoard, StatusKeepsQueueOrderAndReportsNamesModesAndStates) {
    JobsBoard board;
    auto const a{board.request(Mode::shared, "tools")->id};
    auto const b{board.request(Mode::shared, "game")->id};
    auto const c{board.request(Mode::exclusive, "benchmark")->id};
    auto const d{board.request(Mode::shared, "analysis")->id};
    ASSERT_TRUE(board.transition(b, State::running));
    auto const status = board.status().at("tickets");
    ASSERT_EQ(status.size(), 4U);
    EXPECT_EQ(status[0], ticket_json(Ticket{a, Mode::shared, "tools", State::ready}));
    EXPECT_EQ(status[1], ticket_json(Ticket{b, Mode::shared, "game", State::running}));
    EXPECT_EQ(status[2], ticket_json(Ticket{c, Mode::exclusive, "benchmark", State::queued}));
    EXPECT_EQ(status[3], ticket_json(Ticket{d, Mode::shared, "analysis", State::queued}));
}
}
