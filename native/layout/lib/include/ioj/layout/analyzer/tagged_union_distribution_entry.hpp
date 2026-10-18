#pragma once

#include <cstdint>
#include <string>

namespace ioj::layout {

struct TaggedUnionDistributionEntry {
    auto operator==(TaggedUnionDistributionEntry const&) const -> bool = default;

    std::string tag{};
    std::uint64_t weight{};
};

}
