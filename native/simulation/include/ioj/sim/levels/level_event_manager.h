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

    void activate(ExprManager& expressions);
    void collect_ready(ExprManager& expressions);
    void enqueue(EventHandle event);
    void clear_ready();
    auto ready_events() -> std::span<EventHandle> { return ready_events_; }
    auto data() const -> CompiledLevelEvents const& { return data_; }

  private:
    CompiledLevelEvents data_;
    std::pmr::vector<EventHandle> ready_events_;
};
}
