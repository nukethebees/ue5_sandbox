#pragma once

#include "log_store.hpp"
#include "scheduler.hpp"
#include "supervisor.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <stop_token>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace jobserver {
class Server {
  public:
    [[nodiscard]] auto run() -> int;
  private:
    void serve_client(void* pipe, bool control);
    [[nodiscard]] auto write_client(void* pipe, std::string const& message)
        -> std::expected<void, Error>;
    void send_error(void* pipe, Error const& error);
    void handle_acquire(void* pipe, std::string const& message);
    void handle_submit(void* pipe, std::string const& message);
    void handle_validate_nested(void* pipe, std::string const& message);
    void handle_status(void* pipe, bool include_history);
    void handle_cancel(void* pipe, std::string const& message, bool kill);
    void handle_shutdown(void* pipe);
    void load_history();
    void record_history(std::string const& id) noexcept;
    void finish_job(std::string const& id) noexcept;
    void audit_loop(std::stop_token stop_token);

    Scheduler scheduler_;
    std::mutex supervisors_mutex_;
    std::unordered_map<std::string, std::shared_ptr<Supervisor>> supervisors_;
    std::mutex leases_mutex_;
    std::unordered_set<std::string> leases_;
    std::mutex diagnostics_mutex_;
    std::vector<std::string> diagnostics_;
    std::mutex history_mutex_;
    std::filesystem::path history_path_;
    std::vector<std::string> history_;
    std::unique_ptr<LogStore> log_store_;
    std::atomic<bool> stopping_{};
    std::stop_source connection_stop_;
    std::mutex admission_mutex_;
    bool accepting_jobs_{true};
    std::mutex handlers_mutex_;
    std::condition_variable handlers_finished_;
    std::size_t active_handlers_{};
    std::size_t active_job_handlers_{};
    std::size_t active_control_handlers_{};
    std::atomic<std::uint64_t> rejected_clients_{};
    std::chrono::system_clock::time_point started_at_{};
    std::chrono::steady_clock::time_point started_steady_{};
    std::atomic<std::int64_t> last_audit_ms_{};
};
}
