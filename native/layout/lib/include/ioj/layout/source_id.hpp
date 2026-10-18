#pragma once

#include <ioj/layout/instruction_id.hpp>

#include <compare>
#include <cstdint>

namespace ioj::layout {
struct SourceId {
    auto operator<=>(SourceId const&) const = default;

    InstructionId instruction{};
    std::uint32_t ordinal{};
};
}
