#pragma once

#include <CoreMinimal.h>

struct SPACEGAME_API FShipInputGestureRecognizer {
    explicit FShipInputGestureRecognizer(double const new_tap_window_seconds = 0.25)
        : tap_window_seconds_{new_tap_window_seconds} {}

    auto begin_press(double const time_seconds) -> bool {
        auto const recognised{last_tap_release_seconds_ >= 0.0 &&
                              time_seconds - last_tap_release_seconds_ <= tap_window_seconds_};
        press_started_seconds_ = time_seconds;
        last_tap_release_seconds_ = -1.0;
        return recognised;
    }

    void end_press(double const time_seconds) {
        if (press_started_seconds_ >= 0.0 &&
            time_seconds - press_started_seconds_ <= tap_window_seconds_) {
            last_tap_release_seconds_ = time_seconds;
        } else {
            last_tap_release_seconds_ = -1.0;
        }
        press_started_seconds_ = -1.0;
    }

    void reset() noexcept {
        press_started_seconds_ = -1.0;
        last_tap_release_seconds_ = -1.0;
    }
  private:
    double tap_window_seconds_{0.25};
    double press_started_seconds_{-1.0};
    double last_tap_release_seconds_{-1.0};
};
