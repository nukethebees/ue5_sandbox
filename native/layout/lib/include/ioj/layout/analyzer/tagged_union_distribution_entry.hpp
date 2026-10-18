#pragma once

#include <cstdint>
#include <string>

namespace ioj::layout {

struct TaggedUnionDistributionEntry {
    std::string tag{};
    std::uint64_t weight{};
};

}
