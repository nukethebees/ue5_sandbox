#include <SandboxCore/countdown_timers.h>

#include <SandboxCore/array_math.h>

void FCountdownTimers::tick(float const dt) noexcept {
    ml::subtract_in_place(TArrayView<float>{remaining_times}, dt);
}

void FCountdownTimers::Add(float const t, int32 const count) {
    auto const offset{Num()};
    remaining_times.AddUninitialized(count);

    for (int32 i{0}; i < count; ++i) {
        remaining_times[offset + i] = t;
    }
}
void FCountdownTimers::AddZeroed(int32 const count) {
    remaining_times.AddZeroed(count);
}
