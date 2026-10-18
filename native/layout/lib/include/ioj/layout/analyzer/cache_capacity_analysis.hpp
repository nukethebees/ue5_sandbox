#pragma once

#include <cstdint>
#include <optional>

namespace ioj::layout {

struct CacheCapacityAnalysis {
    std::optional<std::uint64_t> working_set_bytes{};
    std::optional<std::uint64_t> l1_data_capacity_bytes{};
    std::optional<std::uint64_t> l2_capacity_bytes{};
    std::optional<std::uint64_t> l3_capacity_bytes{};
    std::optional<bool> fits_l1_data{};
    std::optional<bool> fits_l2{};
    std::optional<bool> fits_l3{};
};

}
