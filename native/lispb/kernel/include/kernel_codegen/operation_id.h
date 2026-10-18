#pragma once

#include <kernel_codegen/operation_kind.h>

#include <compare>
#include <cstdint>
namespace kernel_codegen::detail {

struct OperationId {
    std::uint32_t value{};
    auto kind() const -> OperationKind { return static_cast<OperationKind>(value >> 24); }
    auto index() const -> std::uint32_t { return value & 0x00ffffff; }
    auto operator<=>(OperationId const&) const = default;
};

}
