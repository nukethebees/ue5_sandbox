#pragma once

#include <ioj/persistence/profile.h>

#include <chrono>

namespace ioj::persistence {
inline auto test_date(int year, unsigned month, unsigned day, int hour = 0, int minute = 0)
    -> Timestamp {
    using namespace std::chrono;
    return duration_cast<seconds>(
               sys_days{std::chrono::year{year} / month / day}.time_since_epoch())
               .count() +
           hour * 3600 + minute * 60;
}
}
