#pragma once

#include <codegen/schema/external_scalar_kind.h>

#include <compare>
#include <cstdint>

namespace lispb::codegen {

struct ExternalScalarId {
    std::uint32_t value{};
    auto kind() const -> ExternalScalarKind { return static_cast<ExternalScalarKind>(value >> 24); }
    auto index() const -> std::uint32_t { return value & 0x00ffffff; }
    auto operator<=>(ExternalScalarId const&) const = default;
};

} // namespace lispb::codegen
