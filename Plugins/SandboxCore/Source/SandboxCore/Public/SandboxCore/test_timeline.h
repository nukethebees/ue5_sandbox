#pragma once

#include <Misc/AssertionMacros.h>

#include <cstddef>
#include <functional>
#include <queue>
#include <utility>

/**
 * Deterministic, one-shot callbacks for simulation tests.
 *
 * Events are added in nondecreasing absolute-time order and are run by tick().
 */
class SANDBOXCORE_API FTestTimeline {
  public:
    using time_type = double;
    using function_type = std::function<void()>;

    template <typename Function>
    FTestTimeline& at(time_type const absolute_time, Function&& function) {
        return add_event(absolute_time, std::forward<Function>(function));
    }

    template <typename Function>
    FTestTimeline& then_after(time_type const delta_time, Function&& function) {
        check(delta_time >= time_type{0});
        return at(last_scheduled_time_ + delta_time, std::forward<Function>(function));
    }

    FTestTimeline& finish_at(time_type const absolute_time) {
        return add_event(absolute_time, function_type{});
    }

    FTestTimeline& finish_after(time_type const delta_time) {
        check(delta_time >= time_type{0});
        return finish_at(last_scheduled_time_ + delta_time);
    }

    void tick(time_type current_time);

    [[nodiscard]] bool is_empty() const noexcept { return events_.empty(); }
    [[nodiscard]] bool is_finished() const noexcept { return is_empty(); }
    [[nodiscard]] std::size_t pending_event_count() const noexcept { return events_.size(); }

  private:
    struct FEvent {
        time_type time;
        function_type function;
    };

    template <typename Function>
    FTestTimeline& add_event(time_type const absolute_time, Function&& function) {
        check(absolute_time >= time_type{0});
        check(absolute_time >= last_scheduled_time_);

        events_.push(FEvent{absolute_time, function_type{std::forward<Function>(function)}});
        last_scheduled_time_ = absolute_time;
        return *this;
    }

    std::queue<FEvent> events_;
    time_type last_scheduled_time_{0};
};
