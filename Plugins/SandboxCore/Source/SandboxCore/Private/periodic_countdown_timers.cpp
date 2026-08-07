#include <SandboxCore/periodic_countdown_timers.h>

#include <SandboxCore/array_math.h>

void FPeriodicCountdownTimers::tick(float const dt) noexcept {
    ml::subtract_in_place(TArrayView<float>{remaining_times}, dt);
}

void FPeriodicCountdownTimers::add_started(float const period) {
    remaining_times.Add(period);
    periods.Add(period);
}

void FPeriodicCountdownTimers::add_started(float const period, int32 const count) {
    for (int32 i{0}; i < count; ++i) {
        add_started(period);
    }
}

void FPeriodicCountdownTimers::add_started(ConstView const new_periods) {
    remaining_times.Append(new_periods);
    periods.Append(new_periods);
}

void FPeriodicCountdownTimers::add_zeroed(float const period) {
    remaining_times.Add(0.f);
    periods.Add(period);
}

void FPeriodicCountdownTimers::add_zeroed(float const period, int32 const count) {
    remaining_times.AddZeroed(count);
    for (int32 i{0}; i < count; ++i) {
        periods.Add(period);
    }
}

void FPeriodicCountdownTimers::add_zeroed(ConstView const new_periods) {
    remaining_times.AddZeroed(new_periods.Num());
    periods.Append(new_periods);
}

void FPeriodicCountdownTimers::Append(ConstView const new_remaining_times,
                                      ConstView const new_periods) {
    check(new_remaining_times.Num() == new_periods.Num());
    remaining_times.Append(new_remaining_times);
    periods.Append(new_periods);
}
