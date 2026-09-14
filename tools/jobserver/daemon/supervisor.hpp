#pragma once

#include "jobserver/types.hpp"

#include <expected>
#include <functional>
#include <mutex>
#include <string>

namespace jobserver {
struct ProcessResult {
    int exit_code{};
    int termination_exit_code{};
    bool killed{};
    bool timed_out{};
};

class Supervisor {
  public:
    using Output = std::function<void(std::string const&, std::string const&)>;
    using Health = std::function<void(JobHealth, std::string)>;

    Supervisor() = default;
    Supervisor(Supervisor const&) = delete;
    auto operator=(Supervisor const&) -> Supervisor& = delete;
    ~Supervisor();

    [[nodiscard]] auto run(Command const& command,
                           std::optional<std::chrono::milliseconds> timeout,
                           std::optional<std::chrono::milliseconds> suspect_after,
                           Output output,
                           Health health,
                           std::function<bool()> connection_alive)
        -> std::expected<ProcessResult, Error>;
    void cancel();
    void kill();
  private:
    void terminate(bool killed);

    std::mutex mutex_;
    void* job_handle_{};
    bool cancellation_requested_{};
    bool kill_requested_{};
    int termination_exit_code_{};
};
}
