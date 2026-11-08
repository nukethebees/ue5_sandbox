#include <ioj/sim/expressions/latches.h>

#include <ioj/sim/expressions/expr_manager.h>

#include <cassert>
#include <cstdint>

namespace ioj::sim {
auto Latches::add(LatchInput const input) -> LatchHandle {
    assert(input.when.is_valid() && input.value.is_valid());
    assert(states_.size() < LatchHandle::invalid_value);
    LatchHandle const handle{static_cast<std::uint32_t>(states_.size())};

    inputs_.push_back(input);
    states_.emplace_back();
    active_.push_back(handle);
    return handle;
}
auto Latches::latched_true(LatchHandle const handle) const -> bool {
    return state(handle) == LatchState::True;
}
auto Latches::latched_false(LatchHandle const handle) const -> bool {
    return state(handle) == LatchState::False;
}
void Latches::evaluate(ExprManager const& expressions) {
    for (auto const handle : active_) {
        auto const index{handle.index()};
        auto& state{states_[index]};
        auto const input{inputs_[index]};
        if (state.pending() == LatchState::Unset && expressions.result(input.when)) {
            auto const value{expressions.result(input.value)};
            state.set_pending(value ? LatchState::True : LatchState::False);
        }
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
auto Latches::state(LatchHandle const handle) const -> LatchState {
    assert(is_valid(handle));
    return states_[handle.index()].cached();
}
auto Latches::is_valid(LatchHandle const handle) const -> bool {
    return handle.is_valid() && handle.index() < states_.size();
}
}
