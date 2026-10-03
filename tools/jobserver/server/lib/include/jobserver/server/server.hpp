#pragma once
#include "jobserver/server/board.hpp"

#include <filesystem>

namespace jobserver::server {
class Server {
  public:
    explicit Server(std::filesystem::path endpoint);
    auto run() -> int;
  private:
    auto request(nlohmann::json const& message) -> nlohmann::json;
    std::filesystem::path endpoint_;
    JobsBoard board_;
    bool stopping_{};
};
}
