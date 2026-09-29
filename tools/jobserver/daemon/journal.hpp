#pragma once
#include "jobserver/handles.hpp"

#include <nlohmann/json.hpp>

#include <deque>
#include <string>

namespace jobserver {
// Diagnostic history only. No scheduler state survives a daemon restart.
class Journal {
  public:
    void append(std::string const& event, ClientId client = {}, std::string const& name = {});
    [[nodiscard]] auto trace() const -> nlohmann::json;
  private:
    inline static constexpr std::size_t capacity{1000};
    std::deque<nlohmann::json> events_;
};
}
