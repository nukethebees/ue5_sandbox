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

TEST(JobserverScheduler, NestedClaimsMustFitRunningParentEnvelope) {
    using jobserver::ClaimMode;
    for (auto const parent_mode : {ClaimMode::shared, ClaimMode::counted, ClaimMode::exclusive}) {
        for (auto const child_mode :
             {ClaimMode::shared, ClaimMode::counted, ClaimMode::exclusive}) {
            jobserver::Scheduler scheduler;
            scheduler.set_capacity("nested", 4);
            auto const parent{
                scheduler.enqueue(metadata("parent"),
                                  {{.name = "nested",
                                    .mode = parent_mode,
                                    .units = parent_mode == ClaimMode::counted ? 2U : 1U}})};
            EXPECT_FALSE(scheduler.validate_nested_claims(parent, shared("nested")));
            scheduler.set_state(parent, jobserver::JobState::running);
            auto const result{
                scheduler.validate_nested_claims(parent, {{.name = "nested", .mode = child_mode}})};
            EXPECT_EQ(result.has_value(),
                      parent_mode == ClaimMode::exclusive || parent_mode == child_mode);
            auto const missing{scheduler.validate_nested_claims(parent, shared("missing"))};
            ASSERT_FALSE(missing);
            EXPECT_EQ(missing.error().code, "nested_resource_not_held");
            if (parent_mode == ClaimMode::counted) {
                EXPECT_TRUE(
                    scheduler.validate_nested_claims(parent, {{.name = "nested", .units = 2}}));
                EXPECT_FALSE(
                    scheduler.validate_nested_claims(parent, {{.name = "nested", .units = 3}}));
            }
            scheduler.release(parent, jobserver::JobState::succeeded);
            auto const completed{scheduler.validate_nested_claims(parent, {})};
            ASSERT_FALSE(completed);
            EXPECT_EQ(completed.error().code, "nested_parent_not_active");
        }
    }
    jobserver::Scheduler scheduler;
    EXPECT_FALSE(scheduler.validate_nested_claims("unknown", {}));
    scheduler.set_capacity("cpu", 4);
    auto const running{scheduler.enqueue(metadata("running"), exclusive("cpu"))};
    auto const queued{scheduler.enqueue(metadata("queued"), exclusive("cpu"))};
    scheduler.set_state(running, jobserver::JobState::running);
    EXPECT_FALSE(scheduler.validate_nested_claims(queued, exclusive("cpu")));
    EXPECT_FALSE(scheduler.validate_nested_claims(running, {{.name = "cpu", .units = 5}}));
    EXPECT_FALSE(scheduler.validate_nested_claims(
        running,
        {{.name = "cpu", .mode = jobserver::ClaimMode::shared},
         {.name = "cpu", .mode = jobserver::ClaimMode::exclusive}}));
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

TEST(JobserverScheduler, AuditRecoversExpiredStartingJobAndGrantsNextWaiter) {
    jobserver::Scheduler scheduler;
    auto const expired{scheduler.enqueue(metadata("expired"), exclusive("machine"))};
    auto const waiting{scheduler.enqueue(metadata("waiting"), exclusive("machine"))};
    ASSERT_TRUE(scheduler.try_grant(expired));
    ASSERT_FALSE(scheduler.try_grant(waiting));

    auto const findings{scheduler.audit_and_recover({}, std::chrono::milliseconds{0})};

    EXPECT_FALSE(findings.empty());
    EXPECT_EQ(scheduler.state(expired), jobserver::JobState::interrupted);
    EXPECT_TRUE(scheduler.try_grant(waiting));
}

TEST(JobserverScheduler, AuditPreservesRunningJobWithRegisteredOwner) {
    jobserver::Scheduler scheduler;
    auto const job{scheduler.enqueue(metadata("owned"), exclusive("machine"))};
    ASSERT_TRUE(scheduler.try_grant(job));
    scheduler.set_state(job, jobserver::JobState::running);

    auto const findings{scheduler.audit_and_recover({job}, std::chrono::milliseconds{0})};

    EXPECT_TRUE(findings.empty());
    EXPECT_EQ(scheduler.state(job), jobserver::JobState::running);
}

TEST(JobserverScheduler, AuditRecoversRunningJobWithoutRegisteredOwner) {
    jobserver::Scheduler scheduler;
    auto const job{scheduler.enqueue(metadata("orphaned"), exclusive("machine"))};
    ASSERT_TRUE(scheduler.try_grant(job));
    scheduler.set_state(job, jobserver::JobState::running);

    auto const findings{scheduler.audit_and_recover({}, std::chrono::hours{1})};

    EXPECT_FALSE(findings.empty());
    EXPECT_EQ(scheduler.state(job), jobserver::JobState::interrupted);
    auto const snapshot{scheduler.snapshot()};
    auto const machine{
        std::ranges::find(snapshot.resources, "machine", &jobserver::ResourceUsage::name)};
    ASSERT_NE(machine, snapshot.resources.end());
    EXPECT_FALSE(machine->exclusive);
}

TEST(JobserverScheduler, TakingCompletedEntriesNeverRemovesLiveOwnership) {
    jobserver::Scheduler scheduler;
    auto const active{scheduler.enqueue(metadata("active"), exclusive("machine"))};
    auto const queued{scheduler.enqueue(metadata("queued"), shared("machine"))};
    EXPECT_FALSE(scheduler.take_completed(active));
    EXPECT_FALSE(scheduler.take_completed(queued));
    scheduler.set_state(active, jobserver::JobState::running);
    EXPECT_FALSE(scheduler.take_completed(active));
    scheduler.set_state(active, jobserver::JobState::cancelling);
    EXPECT_FALSE(scheduler.take_completed(active));
    scheduler.release(active, jobserver::JobState::succeeded);
    auto const completed{scheduler.take_completed(active)};
    ASSERT_TRUE(completed);
    EXPECT_EQ(completed->metadata.name, "active");
    EXPECT_EQ(completed->state, jobserver::JobState::succeeded);
    EXPECT_FALSE(scheduler.take_completed(active));
    EXPECT_EQ(scheduler.state(queued), jobserver::JobState::starting);
    auto const cancelled{scheduler.enqueue(metadata("cancelled"), exclusive("machine"))};
    EXPECT_TRUE(scheduler.cancel_queued(cancelled));
    auto const cancellation{scheduler.take_completed(cancelled)};
    ASSERT_TRUE(cancellation);
    EXPECT_EQ(cancellation->state, jobserver::JobState::killed);
}

TEST(JobserverScheduler, ThousandsOfRetiredJobsDoNotAccumulate) {
    jobserver::Scheduler scheduler;
    for (auto index{0}; index < 3000; ++index) {
        auto const id{scheduler.enqueue(metadata("short job"), shared("machine"))};
        scheduler.release(id, jobserver::JobState::succeeded);
        ASSERT_TRUE(scheduler.take_completed(id));
        ASSERT_TRUE(scheduler.snapshot().entries.empty());
    }
}
