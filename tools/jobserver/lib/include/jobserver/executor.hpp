#pragma once
#include "jobserver/client.hpp"

namespace jobserver {
struct OutputFiles {
    std::filesystem::path standard_output;
    std::filesystem::path standard_error;
};
// A session owns containment. Individual leases end when their root command exits.
class LocalExecutor {
  public:
    LocalExecutor();
    ~LocalExecutor();
    LocalExecutor(LocalExecutor const&) = delete;
    auto operator=(LocalExecutor const&) -> LocalExecutor& = delete;
    auto run(Command const& command,
             Session& session,
             Grant const& grant,
             std::vector<GateClaim> const& gates,
             OutputFiles const* output = nullptr) -> std::expected<int, Error>;
  private:
    void* job_{};
};
[[nodiscard]] auto resolve_executable(std::filesystem::path const& path) -> std::filesystem::path;
[[nodiscard]] auto powershell_command(std::string const& text, std::filesystem::path cwd)
    -> Command;
}
