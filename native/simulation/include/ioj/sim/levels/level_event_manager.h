#pragma once
#include <ioj/sim/levels/compiled_level_events.h>

#include <memory_resource>
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
    void clear_tick_end_events();
    void clear_preparation_events();
    auto tick_end_events() -> std::span<EventHandle> { return tick_end_events_; }
    auto preparation_events() -> std::span<EventHandle> { return preparation_events_; }
    auto data() const -> CompiledLevelEvents const& { return data_; }

  private:
    CompiledLevelEvents data_;
    std::pmr::vector<EventHandle> tick_end_events_;
    std::pmr::vector<EventHandle> preparation_events_;
};
}
