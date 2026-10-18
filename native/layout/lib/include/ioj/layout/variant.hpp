#pragma once

#include <ioj/layout/variant_overrides.hpp>

#include <cstdint>
#include <string>

namespace ioj::layout {
struct Variant {
    std::uint64_t id{};
    std::string name;
    VariantOverrides overrides;
};
}
