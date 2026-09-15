#pragma once

#include "jobserver/types.hpp"

#include <expected>
#include <functional>
#include <mutex>
#include <stop_token>
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
    [[nodiscard]] auto output_stop_token() const -> std::stop_token {
        return output_stop_.get_token();
    }
    [[nodiscard]] auto contains_process(std::uint32_t process_id) -> bool;
  private:
    void terminate(bool killed);

    std::mutex mutex_;
    std::stop_source output_stop_;
    void* job_handle_{};
    bool cancellation_requested_{};
    bool kill_requested_{};
    int termination_exit_code_{};
};
}
