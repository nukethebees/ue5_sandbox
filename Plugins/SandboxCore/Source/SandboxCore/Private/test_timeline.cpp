#include <SandboxCore/test_timeline.h>

void FTestTimeline::tick(time_type const current_time) {
    if (is_finished_) { return; }

    while (next_event_index_ < events_.size() && current_time >= events_[next_event_index_].time) {
        auto const is_finish{events_[next_event_index_].is_finish};
        auto function{std::move(events_[next_event_index_].function)};

        if (function) { function(); }

        has_started_ = true;
        ++next_event_index_;

        if (is_finish) {
            is_finished_ = true;
            return;
        }
    }
}
