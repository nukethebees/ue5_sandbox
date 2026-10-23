#pragma once

#include <kernel_codegen/expression_kind.h>

#include <cstdint>
namespace kernel_codegen::detail {

struct ExpressionId {
    inline static constexpr std::uint32_t index_mask{0x00ffffffu};
    inline static constexpr std::uint32_t kind_shift{24};

    std::uint32_t value{};
    auto kind() const -> ExpressionKind { return static_cast<ExpressionKind>(value >> kind_shift); }
    auto index() const -> std::uint32_t { return value & index_mask; }
};

}
