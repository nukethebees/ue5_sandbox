#pragma once
#include <ioj/sim/levels/compiled_level_events.h>

#include <cstddef>
#include <memory_resource>
#include <optional>
#include <span>
#include <vector>

namespace ioj::sim {
class ExprManager;

class LevelEventManager {
  public:
    explicit LevelEventManager(CompiledLevelEvents data, std::pmr::memory_resource& memory);

    void collect_ready(ExprManager const& expressions);
    void enqueue(EventHandle event);
    void enqueue_stop(telemetry::RunEndReason reason);
    auto consume_tick_end_event() -> std::optional<EventHandle>;
    auto consume_preparation_event() -> std::optional<EventHandle>;
    auto tick_end_events() const -> std::span<EventHandle const> { return tick_end_events_; }
    auto preparation_events() const -> std::span<EventHandle const> { return preparation_events_; }
    auto pending_tick_end_events() const -> std::span<EventHandle const> {
        return tick_end_events().subspan(tick_end_cursor_);
    }
    auto pending_preparation_events() const -> std::span<EventHandle const> {
        return preparation_events().subspan(preparation_cursor_);
    }
    void publish(SignalHandle signal) { signals_[signal.index()] = true; }
    auto signals() const -> std::span<std::uint8_t const> { return signals_; }
    void stop() { stopped_ = true; }
    auto data() const -> CompiledLevelEvents const& { return data_; }

  private:
    CompiledLevelEvents data_;
    std::vector<std::uint8_t> signals_;
    bool stopped_{};
    std::pmr::vector<EventHandle> tick_end_events_;
    std::pmr::vector<EventHandle> preparation_events_;
    std::size_t tick_end_cursor_{};
    std::size_t preparation_cursor_{};
};
}
