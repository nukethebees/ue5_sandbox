#pragma once

#include <Misc/AssertionMacros.h>

#include <cstddef>
#include <functional>
#include <utility>
#include <vector>

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
        return add_event(absolute_time, std::forward<Function>(function), false);
    }

    template <typename Function>
    FTestTimeline& then_after(time_type const delta_time, Function&& function) {
        check(delta_time >= time_type{0});
        return at(last_scheduled_time_ + delta_time, std::forward<Function>(function));
    }

    FTestTimeline& finish_at(time_type const absolute_time) {
        return add_event(absolute_time, function_type{}, true);
    }

    FTestTimeline& finish_after(time_type const delta_time) {
        check(delta_time >= time_type{0});
        return finish_at(last_scheduled_time_ + delta_time);
    }

    template <typename Function>
    FTestTimeline& finish_at(time_type const absolute_time, Function&& final_function) {
        return add_event(absolute_time, std::forward<Function>(final_function), true);
    }

    template <typename Function>
    FTestTimeline& finish_after(time_type const delta_time, Function&& final_function) {
        check(delta_time >= time_type{0});
        return finish_at(last_scheduled_time_ + delta_time, std::forward<Function>(final_function));
    }

    void tick(time_type current_time);

    [[nodiscard]] bool is_finished() const noexcept { return is_finished_; }
    [[nodiscard]] bool has_started() const noexcept { return has_started_; }
    [[nodiscard]] std::size_t pending_event_count() const noexcept {
        return events_.size() - next_event_index_;
    }

  private:
    struct FEvent {
        time_type time;
        function_type function;
        bool is_finish;
    };

    template <typename Function>
    FTestTimeline& add_event(time_type const absolute_time, Function&& function, bool const is_finish) {
        check(absolute_time >= time_type{0});
        check(!has_finish_event_);
        check(absolute_time >= last_scheduled_time_);

        events_.push_back(
            FEvent{absolute_time, function_type{std::forward<Function>(function)}, is_finish});
        last_scheduled_time_ = absolute_time;
        has_finish_event_ = is_finish;
        return *this;
    }

    std::vector<FEvent> events_;
    std::size_t next_event_index_{0};
    time_type last_scheduled_time_{0};
    bool has_started_{false};
    bool has_finish_event_{false};
    bool is_finished_{false};
};
