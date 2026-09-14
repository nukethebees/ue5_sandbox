#include "scheduler.hpp"

#include <gtest/gtest.h>

namespace {
auto metadata(std::string name) -> jobserver::JobMetadata {
    return jobserver::JobMetadata{.name = std::move(name), .kind = "test", .worktree = {}};
}

auto shared(std::string name) -> std::vector<jobserver::ResourceClaim> {
    return {{.name = std::move(name), .mode = jobserver::ClaimMode::shared}};
}

auto exclusive(std::string name) -> std::vector<jobserver::ResourceClaim> {
    return {{.name = std::move(name), .mode = jobserver::ClaimMode::exclusive}};
}
}

TEST(JobserverScheduler, TwoSharedJobsCanRunTogether) {
    jobserver::Scheduler scheduler;
    auto const first{scheduler.enqueue(metadata("first"), shared("machine"))};
    auto const second{scheduler.enqueue(metadata("second"), shared("machine"))};
    EXPECT_TRUE(scheduler.try_grant(first));
    EXPECT_TRUE(scheduler.try_grant(second));
}

TEST(JobserverScheduler, ExclusiveWaitsForSharedWorkToDrain) {
    jobserver::Scheduler scheduler;
    auto const build{scheduler.enqueue(metadata("build"), shared("machine"))};
    auto const benchmark{scheduler.enqueue(metadata("benchmark"), exclusive("machine"))};
    EXPECT_TRUE(scheduler.try_grant(build));
    EXPECT_FALSE(scheduler.try_grant(benchmark));
    scheduler.release(build, jobserver::JobState::succeeded);
    EXPECT_TRUE(scheduler.try_grant(benchmark));
}

TEST(JobserverScheduler, NewSharedWorkCannotBypassWaitingExclusiveJob) {
    jobserver::Scheduler scheduler;
    auto const first{scheduler.enqueue(metadata("first"), shared("machine"))};
    auto const benchmark{scheduler.enqueue(metadata("benchmark"), exclusive("machine"))};
    auto const later{scheduler.enqueue(metadata("later"), shared("machine"))};
    EXPECT_TRUE(scheduler.try_grant(first));
    EXPECT_FALSE(scheduler.try_grant(benchmark));
    EXPECT_FALSE(scheduler.try_grant(later));
    scheduler.release(first, jobserver::JobState::succeeded);
    EXPECT_TRUE(scheduler.try_grant(benchmark));
    EXPECT_FALSE(scheduler.try_grant(later));
}

TEST(JobserverScheduler, ClaimsMultipleResourcesAtomically) {
    jobserver::Scheduler scheduler;
    auto const gpu{scheduler.enqueue(metadata("gpu"), exclusive("gpu"))};
    auto const benchmark{
        scheduler.enqueue(metadata("benchmark"),
                          {
                              {.name = "machine", .mode = jobserver::ClaimMode::exclusive},
                              {.name = "gpu", .mode = jobserver::ClaimMode::exclusive},
                          })};
    EXPECT_TRUE(scheduler.try_grant(gpu));
    EXPECT_FALSE(scheduler.try_grant(benchmark));
    auto const snapshot{scheduler.snapshot()};
    auto const machine{
        std::ranges::find(snapshot.resources, "machine", &jobserver::ResourceUsage::name)};
    ASSERT_NE(machine, snapshot.resources.end());
    EXPECT_FALSE(machine->exclusive);
}

TEST(JobserverScheduler, CountedCapacityIsEnforced) {
    jobserver::Scheduler scheduler;
    scheduler.set_capacity("cpu", 4);
    auto const first{scheduler.enqueue(metadata("first"), {{.name = "cpu", .units = 3}})};
    auto const second{scheduler.enqueue(metadata("second"), {{.name = "cpu", .units = 2}})};
    EXPECT_TRUE(scheduler.try_grant(first));
    EXPECT_FALSE(scheduler.try_grant(second));
    scheduler.release(first, jobserver::JobState::succeeded);
    EXPECT_TRUE(scheduler.try_grant(second));
}

TEST(JobserverScheduler, RejectsClaimsThatCouldNeverRun) {
    jobserver::Scheduler scheduler;
    scheduler.set_capacity("cpu", 4);
    auto const oversized{scheduler.validate_claims({{.name = "cpu", .units = 5}})};
    EXPECT_FALSE(oversized.has_value());
    EXPECT_EQ(oversized.error().code, "resource_request_exceeds_capacity");

    auto const duplicate{scheduler.validate_claims({
        {.name = "machine", .mode = jobserver::ClaimMode::shared},
        {.name = "machine", .mode = jobserver::ClaimMode::exclusive},
    })};
    EXPECT_FALSE(duplicate.has_value());
    EXPECT_EQ(duplicate.error().code, "duplicate_resource");
}

TEST(JobserverScheduler, CancellingQueuedJobLetsLaterWorkProceed) {
    jobserver::Scheduler scheduler;
    auto const active{scheduler.enqueue(metadata("active"), shared("machine"))};
    auto const benchmark{scheduler.enqueue(metadata("benchmark"), exclusive("machine"))};
    auto const later{scheduler.enqueue(metadata("later"), shared("machine"))};
    ASSERT_TRUE(scheduler.try_grant(active));
    ASSERT_FALSE(scheduler.try_grant(benchmark));
    ASSERT_FALSE(scheduler.try_grant(later));

    EXPECT_TRUE(scheduler.cancel_queued(benchmark));
    EXPECT_TRUE(scheduler.try_grant(later));
}

TEST(JobserverScheduler, TerminalTransitionsAndReleaseAreIdempotent) {
    jobserver::Scheduler scheduler;
    auto const job{scheduler.enqueue(metadata("job"), {{.name = "cpu", .units = 1}})};
    ASSERT_TRUE(scheduler.try_grant(job));
    scheduler.set_state(job, jobserver::JobState::running);
    scheduler.release(job, jobserver::JobState::succeeded);
    scheduler.release(job, jobserver::JobState::failed);
    scheduler.set_state(job, jobserver::JobState::running);
    scheduler.release("unknown", jobserver::JobState::failed);

    ASSERT_EQ(scheduler.state(job), jobserver::JobState::succeeded);
    auto const snapshot{scheduler.snapshot()};
    auto const cpu{std::ranges::find(snapshot.resources, "cpu", &jobserver::ResourceUsage::name)};
    ASSERT_NE(cpu, snapshot.resources.end());
    EXPECT_EQ(cpu->used, 0U);
}

TEST(JobserverScheduler, ReleasingQueuedJobDoesNotAlterOwnership) {
    jobserver::Scheduler scheduler;
    auto const active{scheduler.enqueue(metadata("active"), exclusive("machine"))};
    auto const queued{scheduler.enqueue(metadata("queued"), shared("machine"))};
    ASSERT_TRUE(scheduler.try_grant(active));
    ASSERT_FALSE(scheduler.try_grant(queued));

    scheduler.release(queued, jobserver::JobState::failed);
    EXPECT_EQ(scheduler.state(queued), jobserver::JobState::queued);
    auto const snapshot{scheduler.snapshot()};
    auto const machine{
        std::ranges::find(snapshot.resources, "machine", &jobserver::ResourceUsage::name)};
    ASSERT_NE(machine, snapshot.resources.end());
    EXPECT_TRUE(machine->exclusive);
}
