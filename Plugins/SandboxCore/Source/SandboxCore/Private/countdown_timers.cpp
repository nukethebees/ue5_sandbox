#include <SandboxCore/countdown_timers.h>

#include <SandboxCore/array_math.h>

void FCountdownTimers::tick(float const dt) noexcept {
    ml::subtract_in_place(TArrayView<float>{remaining_times}, dt);
}

