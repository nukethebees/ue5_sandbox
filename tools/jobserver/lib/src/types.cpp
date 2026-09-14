#include "jobserver/types.hpp"

namespace jobserver {
auto path_from_utf8(std::string const& value) -> std::filesystem::path {
    auto const* const begin{reinterpret_cast<char8_t const*>(value.data())};
    return std::filesystem::path{std::u8string{begin, begin + value.size()}};
}

auto path_to_utf8(std::filesystem::path const& value) -> std::string {
    auto const utf8{value.u8string()};
    return {reinterpret_cast<char const*>(utf8.data()), utf8.size()};
}

auto to_string(ClaimMode const value) -> std::string {
    switch (value) {
        case ClaimMode::counted:
            return "counted";
        case ClaimMode::shared:
            return "shared";
        case ClaimMode::exclusive:
            return "exclusive";
    }
    return "unknown";
}

auto claim_mode_from_string(std::string const& value) -> std::optional<ClaimMode> {
    if (value == "counted") {
        return ClaimMode::counted;
    }
    if (value == "shared") {
        return ClaimMode::shared;
    }
    if (value == "exclusive") {
        return ClaimMode::exclusive;
    }
    return std::nullopt;
}

auto to_string(JobState const value) -> std::string {
    switch (value) {
        case JobState::queued:
            return "QUEUED";
        case JobState::starting:
            return "STARTING";
        case JobState::running:
            return "RUNNING";
        case JobState::cancelling:
            return "CANCELLING";
        case JobState::succeeded:
            return "SUCCEEDED";
        case JobState::failed:
            return "FAILED";
        case JobState::timed_out:
            return "TIMED_OUT";
        case JobState::killed:
            return "KILLED";
        case JobState::interrupted:
            return "INTERRUPTED";
    }
    return "UNKNOWN";
}

auto to_string(JobHealth const value) -> std::string {
    switch (value) {
        case JobHealth::normal:
            return "NORMAL";
        case JobHealth::suspected_hang:
            return "SUSPECTED_HANG";
    }
    return "UNKNOWN";
}
}
