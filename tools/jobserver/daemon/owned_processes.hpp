#pragma once

#include "supervisor.hpp"

#include "jobserver/types.hpp"

#include <chrono>
#include <expected>
#include <filesystem>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace jobserver {
struct OwnedProcessGroup {
    std::string job_id;
    JobMetadata metadata;
    Command command;
    ProcessIdentity root;
    std::chrono::system_clock::time_point started_at;
};

class OwnedProcessStore {
  public:
    explicit OwnedProcessStore(std::filesystem::path path);

    [[nodiscard]] auto add(OwnedProcessGroup group) -> std::expected<void, Error>;
    void remove(std::string const& job_id) noexcept;
    [[nodiscard]] auto snapshot() const -> std::vector<OwnedProcessGroup>;
    [[nodiscard]] auto recovered_record_count() const -> std::size_t;
  private:
    [[nodiscard]] auto persist_locked() -> std::expected<void, Error>;
    void discard_recovered_records() noexcept;

    std::filesystem::path path_;
    std::unordered_map<std::string, OwnedProcessGroup> groups_;
    std::size_t recovered_record_count_{};
    mutable std::mutex mutex_;
};
}
