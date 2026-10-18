#pragma once

#include <cstdint>

namespace ioj::layout {

struct ExactCodeCount {
    auto operator==(ExactCodeCount const&) const -> bool = default;

    std::uint64_t value{};
    bool two_to_64{};
};

}
