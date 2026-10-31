#pragma once

#include <cstdint>

namespace ioj::presentation {
struct SparkSubmissionResult {
    std::int64_t requested{};
    std::int32_t admitted{};
    std::int32_t replaced_slots{};
};
}
