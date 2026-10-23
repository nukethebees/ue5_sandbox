#pragma once

#include <codegen/schema/external_scalar_kind.h>

#include <compare>
#include <cstdint>

namespace lispb::codegen {

struct ExternalScalarId {
    inline static constexpr std::uint32_t index_mask{0x00ffffffu};
    inline static constexpr std::uint32_t kind_shift{24};

    std::uint32_t value{};
    auto kind() const -> ExternalScalarKind {
        return static_cast<ExternalScalarKind>(value >> kind_shift);
    }
    auto index() const -> std::uint32_t { return value & index_mask; }
    auto operator<=>(ExternalScalarId const&) const = default;
};

} // namespace lispb::codegen
