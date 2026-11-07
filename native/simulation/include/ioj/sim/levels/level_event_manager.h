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

    void activate_trigger_expressions(ExprManager& expressions);
    void collect_ready(ExprManager& expressions);
    void enqueue(EventHandle event);
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
    auto data() const -> CompiledLevelEvents const& { return data_; }

  private:
    CompiledLevelEvents data_;
    std::pmr::vector<EventHandle> tick_end_events_;
    std::pmr::vector<EventHandle> preparation_events_;
    std::size_t tick_end_cursor_{};
    std::size_t preparation_cursor_{};
};
}
