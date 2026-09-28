#pragma once

#include "jobserver/handles.hpp"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace jobserver {
enum class EventKind : std::uint16_t {
    daemon_started,
    daemon_stopped,
    client_connected,
    client_disconnected,
    command_received,
    lease_requested,
    lease_queued,
    blocker_changed,
    lease_granted,
    command_started,
    command_completed,
    lease_released,
    request_cancelled,
    exclusive_queued,
    exclusive_granted,
    exclusive_released,
    server_health,
    protocol_error,
};
[[nodiscard]] auto event_name(EventKind kind) -> std::string;

struct Event {
    std::int64_t timestamp{};
    EventKind kind{};
    ClientId client{};
    CommandId command{};
    LeaseId lease{};
    GateId gate{};
    LeaseId related{};
    std::int64_t value{};
    PayloadId payload{};
};
struct TraceFilter {
    ClientId client{};
    CommandId command{};
    LeaseId lease{};
    GateId gate{};
    std::optional<EventKind> kind{};
    std::size_t limit{100};
};

class RotatingLog {
  public:
    RotatingLog(std::filesystem::path path, std::size_t segment_bytes, std::size_t segments = 4);
    void write(std::string const& line);
    [[nodiscard]] auto paths() const -> std::vector<std::filesystem::path>;
  private:
    std::filesystem::path path_;
    std::size_t segment_bytes_;
    std::size_t segments_;
    std::size_t written_{};
    std::ofstream output_;
};

// The server serializes scheduler transitions; trace readers may run concurrently.
class Journal {
  public:
    inline static constexpr std::size_t event_capacity{1'000'000};
    inline static constexpr std::size_t payload_capacity{32U * 1024U * 1024U};
    Journal(std::filesystem::path directory = {},
            std::size_t capacity = event_capacity,
            std::size_t segment_bytes = 32U * 1024U * 1024U);
    void append(Event event, std::string payload = {});
    [[nodiscard]] auto trace(TraceFilter const& filter = {}) const -> nlohmann::json;
  private:
    void insert(Event event, std::string const& payload);
    void evict();
    void load();
    [[nodiscard]] auto row(std::size_t index) const -> Event;

    mutable std::mutex mutex_;
    std::size_t capacity_;
    std::size_t first_{};
    std::size_t size_{};
    std::size_t payload_bytes_{};
    std::uint64_t next_payload_{1};
    std::vector<std::int64_t> timestamps_;
    std::vector<EventKind> kinds_;
    std::vector<ClientId> clients_;
    std::vector<CommandId> commands_;
    std::vector<LeaseId> leases_;
    std::vector<GateId> gates_;
    std::vector<LeaseId> related_;
    std::vector<std::int64_t> values_;
    std::vector<PayloadId> payloads_;
    struct Payload {
        std::string text;
        std::size_t references{};
    };
    std::unordered_map<std::uint64_t, Payload> strings_;
    std::unordered_map<std::string, PayloadId> interned_;
    RotatingLog disk_;
    RotatingLog human_;
};
}
