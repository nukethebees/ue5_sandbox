#pragma once

#include <kernel_codegen/expression_kind.h>

#include <cstdint>
namespace kernel_codegen::detail {

struct ExpressionId {
    std::uint32_t value{};
    auto kind() const -> ExpressionKind { return static_cast<ExpressionKind>(value >> 24); }
    auto index() const -> std::uint32_t { return value & 0x00ffffff; }
};

}
