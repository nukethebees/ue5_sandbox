#pragma once

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
    void serve_client(void* pipe);
    void handle_acquire(void* pipe, std::string const& message);
    void handle_submit(void* pipe, std::string const& message);
    void handle_status(void* pipe, bool include_history);
    void handle_cancel(void* pipe, std::string const& message, bool kill);
    void handle_shutdown(void* pipe);
    void load_history();
    void record_history(std::string const& id) noexcept;
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
    std::atomic<bool> stopping_{};
    std::atomic<void*> listener_{};
    std::mutex admission_mutex_;
    bool accepting_jobs_{true};
    std::mutex handlers_mutex_;
    std::condition_variable handlers_finished_;
    std::size_t active_handlers_{};
    std::chrono::system_clock::time_point started_at_{};
    std::chrono::steady_clock::time_point started_steady_{};
    std::atomic<std::int64_t> last_audit_ms_{};
};
}
