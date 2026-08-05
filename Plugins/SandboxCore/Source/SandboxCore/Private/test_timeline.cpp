#include <SandboxCore/test_timeline.h>

void FTestTimeline::tick(time_type const current_time) {
    while (!events_.empty() && current_time >= events_.front().time) {
        auto function{std::move(events_.front().function)};
        events_.pop();

        if (function) { function(); }
    }
}
