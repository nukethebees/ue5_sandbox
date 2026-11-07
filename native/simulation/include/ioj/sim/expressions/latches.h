#pragma once

#include <ioj/sim/expressions/expr_handle.h>
#include <ioj/sim/expressions/latch_handle.h>
#include <ioj/sim/expressions/latch_memory.h>

#include <vector>

namespace ioj::sim {
class ExprManager;

struct LatchInput {
    ExprHandle when;
    ExprHandle value;
};

class Latches {
  public:
    auto add(LatchInput input) -> LatchHandle;
    auto latched_true(LatchHandle handle) const -> bool;
    auto latched_false(LatchHandle handle) const -> bool;

    void evaluate(ExprManager const& expressions);
    void commit();

  private:
    auto state(LatchHandle handle) const -> LatchState;
    auto is_valid(LatchHandle handle) const -> bool;

    std::vector<LatchInput> inputs_;
    std::vector<LatchMemory> states_;
    std::vector<LatchHandle> active_;
};
}
