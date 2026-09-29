#pragma once
#include "board.hpp"

#include <string>

namespace jobserver {
class Server {
  public:
    explicit Server(std::wstring endpoint);
    auto run() -> int;
  private:
    auto request(nlohmann::json const& message) -> nlohmann::json;
    std::wstring endpoint_;
    JobsBoard board_;
    bool stopping_{};
};
}
