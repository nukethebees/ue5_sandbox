#pragma once

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_set>

namespace jobserver {
class CappedLog {
  public:
    CappedLog(std::filesystem::path const& path, std::size_t maximum_bytes);
    void write(std::string const& text);
    void close();
    [[nodiscard]] auto available() const -> bool;
  private:
    std::ofstream output_;
    std::size_t maximum_bytes_;
    std::size_t written_{};
    bool truncated_{};
};

class LogStore;

class JobLogs {
  public:
    ~JobLogs();
    void write(std::string const& stream, std::string const& text);
    [[nodiscard]] auto available() const -> bool;
  private:
    friend class LogStore;
    JobLogs(LogStore& store,
            std::string id,
            std::filesystem::path const& directory,
            std::size_t maximum_bytes);
    LogStore& store_;
    std::string id_;
    CappedLog stdout_;
    CappedLog stderr_;
};

class LogStore {
  public:
    inline static constexpr std::size_t maximum_stream_bytes{4U * 1024U * 1024U};
    inline static constexpr std::size_t retained_jobs{100};
    explicit LogStore(std::filesystem::path directory,
                      std::size_t maximum_bytes = maximum_stream_bytes,
                      std::size_t retention = retained_jobs);
    [[nodiscard]] auto open(std::string const& id) -> std::unique_ptr<JobLogs>;
  private:
    friend class JobLogs;
    void finish(std::string const& id) noexcept;
    void prune();
    std::filesystem::path directory_;
    std::size_t maximum_bytes_;
    std::size_t retention_;
    std::mutex mutex_;
    std::unordered_set<std::string> active_;
};
}
