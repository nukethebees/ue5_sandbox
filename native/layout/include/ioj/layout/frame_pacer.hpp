#pragma once

#include <chrono>

namespace ioj::layout {

enum class FramePacingMode { interactive, idle, background, suspended };

struct FramePacingState {
    bool focused{true};
    bool minimized{};
    bool explicit_refresh{};
    bool dragging{};
    std::chrono::steady_clock::time_point last_interaction{};
};

class FramePacer {
  public:
    static constexpr auto interactive_tail{std::chrono::milliseconds{750}};
    static constexpr auto idle_interval{std::chrono::milliseconds{83}};
    static constexpr auto background_interval{std::chrono::milliseconds{500}};

    static auto mode(FramePacingState const& state, std::chrono::steady_clock::time_point now)
        -> FramePacingMode;
    static auto wait_timeout(FramePacingMode mode) -> std::chrono::milliseconds;
};

} // namespace ioj::layout
