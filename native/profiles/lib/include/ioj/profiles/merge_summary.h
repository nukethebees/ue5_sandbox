#pragma once

#include <cstddef>

namespace ioj::profiles {
struct MergeSummary {
    std::size_t source_profile_count{};
    std::size_t source_record_count{};
    std::size_t profile_count{};
    std::size_t unique_record_count{};
};
}
