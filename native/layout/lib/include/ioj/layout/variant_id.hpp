#pragma once

#include <compare>
#include <cstdint>

namespace ioj::layout {

struct VariantId {
    auto operator<=>(VariantId const&) const = default;

    std::uint64_t value{};
};

}
