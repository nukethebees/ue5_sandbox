#include "journal.hpp"

#include <chrono>

namespace jobserver {
void Journal::append(std::string const& event, ClientId const client, std::string const& name) {
    auto const now{std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count()};
    events_.push_back(
        {{"event", event}, {"client", client.value}, {"name", name}, {"time_ms", now}});
    if (events_.size() > capacity) {
        events_.pop_front();
    }
}
auto Journal::trace() const -> nlohmann::json {
    return nlohmann::json(events_);
}
}
