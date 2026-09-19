#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace jobserver {
enum class ClaimMode {
    counted,
    shared,
    exclusive,
};

struct ResourceClaim {
    std::string name;
    ClaimMode mode{ClaimMode::counted};
    std::uint32_t units{1};

    auto operator==(ResourceClaim const&) const -> bool = default;
};

struct JobMetadata {
    std::string name;
    std::string kind;
    std::string task{};
    std::filesystem::path worktree;
    std::filesystem::path submit_directory{};
};

struct Command {
    struct EnvironmentChange {
        std::string name;
        std::optional<std::string> value;
    };

    std::filesystem::path executable;
    std::vector<std::string> arguments;
    std::filesystem::path working_directory;
    std::vector<EnvironmentChange> environment;
};

enum class DisconnectPolicy {
    cancel,
    continue_job,
};

struct SubmitRequest {
    JobMetadata metadata;
    Command command;
    std::vector<ResourceClaim> resources;
    std::optional<std::chrono::milliseconds> timeout;
    std::optional<std::chrono::milliseconds> suspect_after;
    DisconnectPolicy disconnect_policy{DisconnectPolicy::cancel};
};

struct AcquireRequest {
    JobMetadata metadata;
    std::vector<ResourceClaim> resources;
};

enum class JobState {
    queued,
    starting,
    running,
    cancelling,
    succeeded,
    failed,
    timed_out,
    killed,
    interrupted,
};

enum class JobHealth {
    normal,
    suspected_hang,
};

struct Error {
    std::string code;
    std::string message;
};

[[nodiscard]] auto path_from_utf8(std::string const& value) -> std::filesystem::path;
[[nodiscard]] auto path_to_utf8(std::filesystem::path const& value) -> std::string;
[[nodiscard]] auto to_string(ClaimMode value) -> std::string;
[[nodiscard]] auto claim_mode_from_string(std::string const& value) -> std::optional<ClaimMode>;
[[nodiscard]] auto to_string(JobState value) -> std::string;
[[nodiscard]] auto to_string(JobHealth value) -> std::string;
}
