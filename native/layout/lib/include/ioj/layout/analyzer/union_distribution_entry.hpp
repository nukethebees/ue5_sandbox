#pragma once

#include <cstdint>
#include <string>

namespace ioj::layout {

struct UnionDistributionEntry {
    auto operator==(UnionDistributionEntry const&) const -> bool = default;

    std::string alternative_name{};
    std::uint64_t weight{};
};

}
