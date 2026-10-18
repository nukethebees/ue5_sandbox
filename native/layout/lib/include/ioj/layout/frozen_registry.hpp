#pragma once

#include <ioj/layout/frozen_sources.hpp>

#include <cstddef>
#include <cstdint>

namespace ioj::layout {
struct FrozenRegistry {
    FrozenSources sources;
    std::vector<std::size_t> parents;
    std::vector<lispb::IdentifierId> includes;
    std::vector<std::size_t> targets;
    std::vector<std::uint32_t> begins;
    std::vector<std::uint32_t> ends;
};
}
