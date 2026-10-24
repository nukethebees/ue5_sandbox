#pragma once

#include <ioj/constrained_numbers.h>
#include <ioj/frequency.h>

namespace ioj::settings {
struct TickSettings {
    Frequency tick_rate{60.0};
    PositiveFinite<double> time_scale{1.0};
};
}
