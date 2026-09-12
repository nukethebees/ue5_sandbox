#include <sandbox/core/test_timeline.h>

#include <gtest/gtest.h>

#include <vector>

TEST(NativeCoreTestTimeline, RunsDueEventsOnceInScheduleOrder) {
    FTestTimeline timeline;
    std::vector<int> observed;
    timeline.at(1.0, [&] { observed.push_back(1); })
        .then_after(0.5, [&] { observed.push_back(2); })
        .finish_after(0.5);

    timeline.tick(0.9);
    EXPECT_TRUE(observed.empty());
    timeline.tick(1.5);
    EXPECT_EQ(observed, (std::vector<int>{1, 2}));
    EXPECT_EQ(timeline.pending_event_count(), 1u);
    timeline.tick(2.0);
    EXPECT_TRUE(timeline.is_finished());
}
