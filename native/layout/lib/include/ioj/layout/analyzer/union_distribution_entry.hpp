#pragma once

#include <cstdint>
#include <string>

namespace ioj::layout {

struct UnionDistributionEntry {
    std::string alternative_name{};
    std::uint64_t weight{};
};

}
