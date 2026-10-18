#pragma once
#include <ioj/layout/type_facts.hpp>

#include <cstdint>
#include <optional>
#include <string>
namespace ioj::layout {
struct EnumBacking {
    std::string backing_type{};
    std::optional<TypeFacts> backing_facts{};
    std::optional<std::uint64_t> backing_bits{};
    std::optional<bool> backing_can_represent_domain{};
    std::optional<std::uint64_t> unused_backing_codes{};
};
}
