#pragma once

#include <ioj/sim/expressions/expr_handle.h>
#include <ioj/sim/expressions/latch_handle.h>
#include <ioj/sim/expressions/latch_memory.h>

#include <span>
#include <vector>

namespace ioj::sim {
class Latches {
  public:
    struct Inputs {
        ExprHandle when;
        ExprHandle value;
    };

    auto add(ExprHandle when, ExprHandle value) -> LatchHandle;
    auto inputs(LatchHandle handle) const -> Inputs;
    auto active() const -> std::span<LatchHandle const> { return active_; }
    auto state(LatchHandle handle) const -> LatchState;
    auto latched_true(LatchHandle handle) const -> bool;
    auto latched_false(LatchHandle handle) const -> bool;

    // Boolean inputs are samples from the current logic cycle; observations remain cached.
    void evaluate(LatchHandle handle, bool when, bool value);
    // Publish the previous cycle's captures before sampling the next cycle's expressions.
    void commit();

  private:
    std::vector<Inputs> inputs_;
    std::vector<LatchMemory> states_;
    std::vector<LatchHandle> active_;
};
}
