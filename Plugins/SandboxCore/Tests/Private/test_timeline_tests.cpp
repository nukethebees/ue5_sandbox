#include <SandboxCore/test_timeline.h>

#include "CoreMinimal.h"
#include "TestHarness.h"

#include <vector>

TEST_CASE("SandboxCore.TestTimeline.New timeline is empty") {
    FTestTimeline timeline;

    CHECK(timeline.is_empty());
    CHECK(timeline.is_finished());
    CHECK(timeline.pending_event_count() == 0);
}

TEST_CASE("SandboxCore.TestTimeline.Events run at and after their scheduled time") {
    SECTION("before scheduled time") {
        FTestTimeline timeline;
        int32 calls{0};
        timeline.at(1.0, [&] { ++calls; });

        timeline.tick(0.5);

        CHECK(calls == 0);
        CHECK_FALSE(timeline.is_empty());
    }

    SECTION("at scheduled time") {
        FTestTimeline timeline;
        int32 calls{0};
        timeline.at(1.0, [&] { ++calls; });

        timeline.tick(1.0);

        CHECK(calls == 1);
        CHECK(timeline.is_empty());
    }

    SECTION("after scheduled time") {
        FTestTimeline timeline;
        int32 calls{0};
        timeline.at(1.0, [&] { ++calls; });

        timeline.tick(2.0);

        CHECK(calls == 1);
    }
}

TEST_CASE("SandboxCore.TestTimeline.Events execute exactly once") {
    FTestTimeline timeline;
    int32 calls{0};
    timeline.at(1.0, [&] { ++calls; });

    timeline.tick(1.0);
    timeline.tick(1.0);
    timeline.tick(2.0);

    CHECK(calls == 1);
    CHECK(timeline.is_empty());
    CHECK(timeline.pending_event_count() == 0);
}

TEST_CASE("SandboxCore.TestTimeline.Pending events decrease as they execute") {
    FTestTimeline timeline;
    timeline.at(1.0, [] {}).at(2.0, [] {}).finish_at(3.0);

    CHECK(timeline.pending_event_count() == 3);

    timeline.tick(1.0);
    CHECK(timeline.pending_event_count() == 2);

    timeline.tick(2.0);
    CHECK(timeline.pending_event_count() == 1);

    timeline.tick(3.0);
    CHECK(timeline.pending_event_count() == 0);
}

TEST_CASE("SandboxCore.TestTimeline.Crossed events run in scheduled and insertion order") {
    FTestTimeline timeline;
    std::vector<int32> calls;

    timeline.at(1.0, [&] { calls.push_back(1); }).at(2.0, [&] { calls.push_back(2); }).at(2.0, [&] { calls.push_back(3); }).at(3.0, [&] {
        calls.push_back(4);
    });

    timeline.tick(3.0);

    std::vector<int32> const expected{1, 2, 3, 4};
    CHECK(calls == expected);
}

TEST_CASE("SandboxCore.TestTimeline.Relative and absolute scheduling use absolute times") {
    FTestTimeline timeline;
    std::vector<int32> calls;

    timeline.then_after(1.0, [&] { calls.push_back(1); })
        .then_after(2.0, [&] { calls.push_back(2); })
        .at(4.0, [&] { calls.push_back(3); })
        .then_after(2.0, [&] { calls.push_back(4); });

    timeline.tick(2.9);
    CHECK(calls == std::vector<int32>{1});

    timeline.tick(3.0);
    std::vector<int32> const expected_at_three{1, 2};
    CHECK(calls == expected_at_three);

    timeline.tick(4.0);
    std::vector<int32> const expected_at_four{1, 2, 3};
    CHECK(calls == expected_at_four);

    timeline.tick(6.0);
    std::vector<int32> const expected_at_six{1, 2, 3, 4};
    CHECK(calls == expected_at_six);
}

TEST_CASE("SandboxCore.TestTimeline.Finish events keep the timeline active until their time") {
    SECTION("finish_at") {
        FTestTimeline timeline;
        timeline.finish_at(2.0);

        timeline.tick(1.0);
        CHECK_FALSE(timeline.is_empty());
        CHECK_FALSE(timeline.is_finished());
        CHECK(timeline.pending_event_count() == 1);

        timeline.tick(2.0);
        CHECK(timeline.is_empty());
        CHECK(timeline.is_finished());
        CHECK(timeline.pending_event_count() == 0);
    }

    SECTION("finish_after uses the last scheduled time") {
        FTestTimeline timeline;
        int32 calls{0};
        timeline.then_after(1.0, [&] { ++calls; }).finish_after(2.0);

        timeline.tick(2.9);
        CHECK(calls == 1);
        CHECK_FALSE(timeline.is_finished());

        timeline.tick(3.0);
        CHECK(timeline.is_finished());
    }
}

TEST_CASE("SandboxCore.TestTimeline.Final callbacks observe an empty queue") {
    FTestTimeline timeline;
    bool was_empty_during_callback{false};

    timeline.then_after(1.0, [&] { was_empty_during_callback = timeline.is_empty(); });
    timeline.tick(1.0);

    CHECK(was_empty_during_callback);
    CHECK(timeline.is_finished());
}

TEST_CASE("SandboxCore.TestTimeline.Large time jumps run all events and completion") {
    FTestTimeline timeline;
    std::vector<int32> calls;

    timeline.at(1.0, [&] { calls.push_back(1); }).then_after(1.0, [&] { calls.push_back(2); }).then_after(1.0, [&] { calls.push_back(3); });

    timeline.tick(100.0);
    timeline.tick(200.0);

    std::vector<int32> const expected{1, 2, 3};
    CHECK(calls == expected);
    CHECK(timeline.is_finished());
    CHECK(timeline.pending_event_count() == 0);
}

TEST_CASE("SandboxCore.TestTimeline.Callbacks can append events") {
    FTestTimeline timeline;
    std::vector<int32> calls;

    timeline.at(1.0, [&] {
        calls.push_back(1);
        timeline.at(2.0, [&] { calls.push_back(2); });
    });

    timeline.tick(2.0);

    std::vector<int32> const expected{1, 2};
    CHECK(calls == expected);
    CHECK(timeline.is_empty());
}

// Unreal's check macro terminates the test process, so invalid-operation cases cannot be
// isolated safely in this Catch2 target. Production checks intentionally remain enabled.
