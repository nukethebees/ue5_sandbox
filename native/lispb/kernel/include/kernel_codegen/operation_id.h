#pragma once

#include <kernel_codegen/operation_kind.h>

#include <compare>
#include <cstdint>
namespace kernel_codegen::detail {

struct OperationId {
    inline static constexpr std::uint32_t index_mask{0x00ffffffu};
    inline static constexpr std::uint32_t kind_shift{24};

    std::uint32_t value{};
    auto kind() const -> OperationKind { return static_cast<OperationKind>(value >> kind_shift); }
    auto index() const -> std::uint32_t { return value & index_mask; }
    auto operator<=>(OperationId const&) const = default;
};

}
