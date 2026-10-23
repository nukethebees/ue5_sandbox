#pragma once
#include <ioj/layout/analysis/physical_facts.hpp>

#include <lispb/indices.h>

#include <cstdint>
#include <optional>
namespace ioj::layout {
struct EnumBacking {
    lispb::IdentifierId backing_type{};
    std::optional<PhysicalFacts> backing_facts{};
    std::optional<std::uint64_t> backing_bits{};
    std::optional<bool> backing_can_represent_domain{};
    std::optional<std::uint64_t> unused_backing_codes{};
};
}
