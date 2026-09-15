#pragma once

#include "jobserver/types.hpp"

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <expected>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace jobserver {
struct QueueEntry {
    std::string id;
    JobMetadata metadata;
    std::vector<ResourceClaim> claims;
    JobState state{JobState::queued};
    JobHealth health{JobHealth::normal};
    std::string health_reason;
    std::vector<std::string> blockers;
    std::chrono::system_clock::time_point queued_at{std::chrono::system_clock::now()};
    std::chrono::system_clock::time_point started_at{};
    std::chrono::system_clock::time_point finished_at{};
};

struct ResourceUsage {
    std::string name;
    std::uint32_t used{};
    std::uint32_t capacity{};
    bool exclusive{};
};

struct SchedulerSnapshot {
    std::vector<QueueEntry> entries;
    std::vector<ResourceUsage> resources;
};

class Scheduler {
  public:
    Scheduler();

    void set_capacity(std::string name, std::uint32_t capacity);
    [[nodiscard]] auto validate_claims(std::vector<ResourceClaim> const& claims) const
        -> std::expected<void, Error>;
    [[nodiscard]] auto validate_nested_claims(std::string const& parent_id,
                                              std::vector<ResourceClaim> const& claims) const
        -> std::expected<void, Error>;
    [[nodiscard]] auto enqueue(JobMetadata metadata, std::vector<ResourceClaim> claims)
        -> std::string;
    [[nodiscard]] auto wait_until_granted(std::string const& id) -> bool;
    [[nodiscard]] auto try_grant(std::string const& id) -> bool;
    [[nodiscard]] auto state(std::string const& id) const -> std::optional<JobState>;
    void set_state(std::string const& id, JobState state);
    void set_health(std::string const& id, JobHealth health, std::string reason = {});
    void release(std::string const& id, JobState final_state);
    [[nodiscard]] auto cancel_queued(std::string const& id) -> bool;
    [[nodiscard]] auto snapshot() const -> SchedulerSnapshot;
    [[nodiscard]] auto take_completed(std::string const& id) -> std::optional<QueueEntry>;
    [[nodiscard]] auto audit_and_recover(std::unordered_set<std::string> const& owned_jobs,
                                         std::chrono::milliseconds maximum_starting_time)
        -> std::vector<std::string>;
  private:
    struct ResourceState {
        std::uint32_t capacity{1};
        std::uint32_t counted_used{};
        std::uint32_t shared_users{};
        bool exclusive{};
    };

    [[nodiscard]] auto find_entry(std::string const& id) -> std::vector<QueueEntry>::iterator;
    [[nodiscard]] auto can_grant(std::vector<QueueEntry>::const_iterator entry) const -> bool;
    [[nodiscard]] auto conflicts(ResourceClaim const& left, ResourceClaim const& right) const
        -> bool;
    void grant_available();
    void claim(std::vector<ResourceClaim> const& claims);
    void unclaim(std::vector<ResourceClaim> const& claims);
    void update_blockers();

    mutable std::mutex mutex_;
    std::condition_variable changed_;
    std::unordered_map<std::string, ResourceState> resources_;
    std::vector<QueueEntry> entries_;
};
}
