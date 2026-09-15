#include "scheduler.hpp"

#include <algorithm>
#include <array>
#include <iomanip>
#include <random>
#include <sstream>
#include <thread>
#include <unordered_set>

namespace jobserver {
namespace {
auto make_id() -> std::string {
    std::random_device random;
    std::array<std::uint32_t, 4> words{};
    for (auto& word : words) {
        word = random();
    }
    std::ostringstream output;
    output << std::hex << std::setfill('0');
    for (auto const word : words) {
        output << std::setw(8) << word;
    }
    return output.str();
}

auto is_terminal(JobState const state) -> bool {
    return state == JobState::succeeded || state == JobState::failed ||
           state == JobState::timed_out || state == JobState::killed ||
           state == JobState::interrupted;
}
}

Scheduler::Scheduler() {
    auto const cpu_count{std::max(1U, std::thread::hardware_concurrency())};
    resources_.emplace("cpu", ResourceState{.capacity = cpu_count});
    resources_.emplace("benchmark", ResourceState{});
    resources_.emplace("gpu", ResourceState{});
    resources_.emplace("machine", ResourceState{});
}

void Scheduler::set_capacity(std::string name, std::uint32_t const capacity) {
    std::scoped_lock const lock{mutex_};
    resources_[std::move(name)].capacity = std::max(1U, capacity);
}

auto Scheduler::validate_claims(std::vector<ResourceClaim> const& claims) const
    -> std::expected<void, Error> {
    std::scoped_lock const lock{mutex_};
    std::unordered_set<std::string> names;
    for (auto const& claim : claims) {
        if (claim.name.empty() || claim.units == 0) {
            return std::unexpected(
                Error{"invalid_resource", "Resource names and unit counts must be non-empty"});
        }
        if (!names.insert(claim.name).second) {
            return std::unexpected(Error{"duplicate_resource",
                                         "A job may claim each resource only once: " + claim.name});
        }
        if (claim.mode != ClaimMode::counted && claim.units != 1) {
            return std::unexpected(Error{"invalid_resource",
                                         "Shared and exclusive resource claims must use one unit"});
        }
        auto const found{resources_.find(claim.name)};
        auto const capacity{found == resources_.end() ? 1U : found->second.capacity};
        if (claim.mode == ClaimMode::counted && claim.units > capacity) {
            return std::unexpected(Error{"resource_request_exceeds_capacity",
                                         "Requested " + std::to_string(claim.units) + " units of " +
                                             claim.name + ", but its capacity is " +
                                             std::to_string(capacity)});
        }
    }
    return {};
}

auto Scheduler::validate_nested_claims(std::string const& parent_id,
                                       std::vector<ResourceClaim> const& claims) const
    -> std::expected<void, Error> {
    if (auto valid{validate_claims(claims)}; !valid) {
        return valid;
    }
    std::scoped_lock const lock{mutex_};
    auto const parent{std::ranges::find(entries_, parent_id, &QueueEntry::id)};
    if (parent == entries_.end() || parent->state != JobState::running) {
        return std::unexpected(Error{"nested_parent_not_active", "Nested parent is not running"});
    }
    for (auto const& claim : claims) {
        auto const held{std::ranges::find(parent->claims, claim.name, &ResourceClaim::name)};
        auto const covered{held != parent->claims.end() &&
                           (held->mode == ClaimMode::exclusive ||
                            (held->mode == claim.mode &&
                             (claim.mode == ClaimMode::shared || claim.units <= held->units)))};
        if (!covered) {
            return std::unexpected(
                Error{"nested_resource_not_held",
                      "Parent does not hold the requested claim: " + claim.name});
        }
    }
    return {};
}

auto Scheduler::enqueue(JobMetadata metadata, std::vector<ResourceClaim> claims) -> std::string {
    std::scoped_lock const lock{mutex_};
    auto const id{make_id()};
    for (auto const& request : claims) {
        resources_.try_emplace(request.name, ResourceState{});
    }
    entries_.push_back(QueueEntry{
        .id = id,
        .metadata = std::move(metadata),
        .claims = std::move(claims),
        .health_reason = {},
        .blockers = {},
    });
    grant_available();
    return id;
}

auto Scheduler::wait_until_granted(std::string const& id) -> bool {
    std::unique_lock lock{mutex_};
    changed_.wait(lock, [&] {
        auto const entry{find_entry(id)};
        return entry == entries_.end() || entry->state != JobState::queued;
    });
    auto const entry{find_entry(id)};
    return entry != entries_.end() && entry->state == JobState::starting;
}

auto Scheduler::try_grant(std::string const& id) -> bool {
    std::scoped_lock const lock{mutex_};
    grant_available();
    auto const entry{find_entry(id)};
    return entry != entries_.end() && entry->state == JobState::starting;
}

auto Scheduler::state(std::string const& id) const -> std::optional<JobState> {
    std::scoped_lock const lock{mutex_};
    auto const entry{std::ranges::find(entries_, id, &QueueEntry::id)};
    if (entry == entries_.end()) {
        return std::nullopt;
    }
    return entry->state;
}

void Scheduler::set_state(std::string const& id, JobState const state) {
    std::scoped_lock const lock{mutex_};
    auto const entry{find_entry(id)};
    if (entry != entries_.end() && !is_terminal(entry->state)) {
        auto const valid_transition{
            (entry->state == JobState::starting && state == JobState::running) ||
            (entry->state == JobState::running && state == JobState::cancelling)};
        if (!valid_transition) {
            return;
        }
        entry->state = state;
        if ((state == JobState::starting || state == JobState::running) &&
            entry->started_at == std::chrono::system_clock::time_point{}) {
            entry->started_at = std::chrono::system_clock::now();
        }
        changed_.notify_all();
    }
}

void Scheduler::set_health(std::string const& id, JobHealth const health, std::string reason) {
    std::scoped_lock const lock{mutex_};
    auto const entry{find_entry(id)};
    if (entry != entries_.end()) {
        entry->health = health;
        entry->health_reason = std::move(reason);
    }
}

void Scheduler::release(std::string const& id, JobState const final_state) {
    std::scoped_lock const lock{mutex_};
    auto const entry{find_entry(id)};
    if (entry == entries_.end() || is_terminal(entry->state) || entry->state == JobState::queued) {
        return;
    }
    unclaim(entry->claims);
    entry->state = final_state;
    entry->finished_at = std::chrono::system_clock::now();
    grant_available();
    changed_.notify_all();
}

auto Scheduler::cancel_queued(std::string const& id) -> bool {
    std::scoped_lock const lock{mutex_};
    auto const entry{find_entry(id)};
    if (entry == entries_.end() || entry->state != JobState::queued) {
        return false;
    }
    entry->state = JobState::killed;
    entry->finished_at = std::chrono::system_clock::now();
    grant_available();
    changed_.notify_all();
    return true;
}

auto Scheduler::take_completed(std::string const& id) -> std::optional<QueueEntry> {
    std::scoped_lock const lock{mutex_};
    auto const entry{find_entry(id)};
    if (entry == entries_.end() || !is_terminal(entry->state)) {
        return std::nullopt;
    }
    auto completed{std::move(*entry)};
    entries_.erase(entry);
    return completed;
}

auto Scheduler::snapshot() const -> SchedulerSnapshot {
    std::scoped_lock const lock{mutex_};
    SchedulerSnapshot result{.entries = entries_, .resources = {}};
    result.resources.reserve(resources_.size());
    for (auto const& [name, state] : resources_) {
        result.resources.push_back(ResourceUsage{
            .name = name,
            .used = state.counted_used + state.shared_users,
            .capacity = state.capacity,
            .exclusive = state.exclusive,
        });
    }
    std::ranges::sort(result.resources, {}, &ResourceUsage::name);
    return result;
}

auto Scheduler::audit_and_recover(std::unordered_set<std::string> const& owned_jobs,
                                  std::chrono::milliseconds const maximum_starting_time)
    -> std::vector<std::string> {
    std::scoped_lock const lock{mutex_};
    std::vector<std::string> diagnostics;
    auto const now{std::chrono::system_clock::now()};

    for (auto& entry : entries_) {
        auto const starting_expired{entry.state == JobState::starting &&
                                    entry.started_at != decltype(entry.started_at){} &&
                                    now - entry.started_at >= maximum_starting_time};
        auto const running_without_owner{
            (entry.state == JobState::running || entry.state == JobState::cancelling) &&
            !owned_jobs.contains(entry.id)};
        if (starting_expired || running_without_owner) {
            diagnostics.push_back(starting_expired
                                      ? "recovered expired STARTING job " + entry.id
                                      : "recovered RUNNING job without an owner " + entry.id);
            entry.state = JobState::interrupted;
            entry.finished_at = now;
        }
    }

    auto expected{resources_};
    for (auto& [name, state] : expected) {
        static_cast<void>(name);
        state.counted_used = 0;
        state.shared_users = 0;
        state.exclusive = false;
    }
    for (auto const& entry : entries_) {
        if (entry.state != JobState::starting && entry.state != JobState::running &&
            entry.state != JobState::cancelling) {
            continue;
        }
        for (auto const& claim : entry.claims) {
            auto& state{expected.at(claim.name)};
            if (claim.mode == ClaimMode::exclusive) {
                state.exclusive = true;
            } else if (claim.mode == ClaimMode::shared) {
                ++state.shared_users;
            } else {
                state.counted_used += claim.units;
            }
        }
    }
    for (auto& [name, state] : resources_) {
        auto const& wanted{expected.at(name)};
        if (state.counted_used != wanted.counted_used ||
            state.shared_users != wanted.shared_users || state.exclusive != wanted.exclusive) {
            diagnostics.push_back("repaired resource accounting for " + name);
            state.counted_used = wanted.counted_used;
            state.shared_users = wanted.shared_users;
            state.exclusive = wanted.exclusive;
        }
    }
    grant_available();
    return diagnostics;
}

auto Scheduler::find_entry(std::string const& id) -> std::vector<QueueEntry>::iterator {
    return std::ranges::find(entries_, id, &QueueEntry::id);
}

auto Scheduler::can_grant(std::vector<QueueEntry>::const_iterator const entry) const -> bool {
    for (auto const& request : entry->claims) {
        auto const& state{resources_.at(request.name)};
        if (request.mode == ClaimMode::exclusive) {
            if (state.exclusive || state.counted_used != 0 || state.shared_users != 0) {
                return false;
            }
        } else if (state.exclusive) {
            return false;
        } else if (request.mode == ClaimMode::counted &&
                   request.units > state.capacity - std::min(state.capacity, state.counted_used)) {
            return false;
        }
    }

    for (auto older{entries_.cbegin()}; older != entry; ++older) {
        if (older->state != JobState::queued) {
            continue;
        }
        for (auto const& older_claim : older->claims) {
            if (std::ranges::any_of(entry->claims, [&](auto const& claim) {
                    return conflicts(older_claim, claim);
                })) {
                return false;
            }
        }
    }
    return true;
}

auto Scheduler::conflicts(ResourceClaim const& left, ResourceClaim const& right) const -> bool {
    if (left.name != right.name) {
        return false;
    }
    return left.mode == ClaimMode::exclusive || right.mode == ClaimMode::exclusive ||
           (left.mode == ClaimMode::counted && right.mode == ClaimMode::counted);
}

void Scheduler::grant_available() {
    for (auto entry{entries_.begin()}; entry != entries_.end(); ++entry) {
        if (entry->state == JobState::queued && can_grant(entry)) {
            claim(entry->claims);
            entry->state = JobState::starting;
            entry->started_at = std::chrono::system_clock::now();
        }
    }
    update_blockers();
    changed_.notify_all();
}

void Scheduler::claim(std::vector<ResourceClaim> const& claims) {
    for (auto const& request : claims) {
        auto& state{resources_.at(request.name)};
        if (request.mode == ClaimMode::exclusive) {
            state.exclusive = true;
        } else if (request.mode == ClaimMode::shared) {
            ++state.shared_users;
        } else {
            state.counted_used += request.units;
        }
    }
}

void Scheduler::unclaim(std::vector<ResourceClaim> const& claims) {
    for (auto const& request : claims) {
        auto& state{resources_.at(request.name)};
        if (request.mode == ClaimMode::exclusive) {
            state.exclusive = false;
        } else if (request.mode == ClaimMode::shared) {
            --state.shared_users;
        } else {
            state.counted_used -= request.units;
        }
    }
}

void Scheduler::update_blockers() {
    for (auto entry{entries_.begin()}; entry != entries_.end(); ++entry) {
        entry->blockers.clear();
        if (entry->state != JobState::queued) {
            continue;
        }
        for (auto older{entries_.begin()}; older != entry; ++older) {
            if (older->state == JobState::queued || older->state == JobState::starting ||
                older->state == JobState::running) {
                auto const overlaps{std::ranges::any_of(older->claims, [&](auto const& old_claim) {
                    return std::ranges::any_of(entry->claims, [&](auto const& claim) {
                        return conflicts(old_claim, claim);
                    });
                })};
                if (overlaps) {
                    entry->blockers.push_back(older->id);
                }
            }
        }
    }
}
}
