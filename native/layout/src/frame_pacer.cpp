#include <ioj/layout/frame_pacer.hpp>

namespace ioj::layout {

auto FramePacer::mode(FramePacingState const& state,
                      std::chrono::steady_clock::time_point const now) -> FramePacingMode {
    if (state.minimized) {
        return FramePacingMode::suspended;
    }
    if (!state.focused) {
        return FramePacingMode::background;
    }
    if (state.explicit_refresh || state.dragging ||
        now - state.last_interaction <= interactive_tail) {
        return FramePacingMode::interactive;
    }
    return FramePacingMode::idle;
}

auto FramePacer::wait_timeout(FramePacingMode const mode) -> std::chrono::milliseconds {
    switch (mode) {
        case FramePacingMode::interactive:
            return std::chrono::milliseconds{0};
        case FramePacingMode::idle:
            return idle_interval;
        case FramePacingMode::background:
            return background_interval;
        case FramePacingMode::suspended:
            return std::chrono::milliseconds{-1};
    }
    return std::chrono::milliseconds{0};
}

} // namespace ioj::layout
