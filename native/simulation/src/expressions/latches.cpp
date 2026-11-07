#include <ioj/sim/expressions/latches.h>

#include <cassert>
#include <cstdint>

namespace ioj::sim {
auto Latches::add(ExprHandle const when, ExprHandle const value) -> LatchHandle {
    assert(when.is_valid() && value.is_valid());
    assert(states_.size() < LatchHandle::invalid_value);
    LatchHandle const handle{static_cast<std::uint32_t>(states_.size())};

    inputs_.push_back({when, value});
    states_.emplace_back();
    active_.push_back(handle);
    return handle;
}
auto Latches::inputs(LatchHandle const handle) const -> Inputs {
    assert(handle.index() < inputs_.size());
    return inputs_[handle.index()];
}
auto Latches::state(LatchHandle const handle) const -> LatchState {
    assert(handle.index() < states_.size());
    return states_[handle.index()].cached();
}
auto Latches::latched_true(LatchHandle const handle) const -> bool {
    return state(handle) == LatchState::True;
}
auto Latches::latched_false(LatchHandle const handle) const -> bool {
    return state(handle) == LatchState::False;
}
void Latches::evaluate(LatchHandle const handle, bool const when, bool const value) {
    assert(handle.index() < states_.size());
    auto& state{states_[handle.index()]};
    if (when && state.pending() == LatchState::Unset) {
        state.set_pending(value ? LatchState::True : LatchState::False);
    }
}
void Latches::commit() {
    for (auto remaining{active_.size()}; remaining > 0; --remaining) {
        auto const index{remaining - 1};
        auto& state{states_[active_[index].index()]};
        auto const pending{state.pending()};
        if (pending != LatchState::Unset) {
            state.set_cached(pending);
            active_[index] = active_.back();
            active_.pop_back();
        }
    }
}
}
