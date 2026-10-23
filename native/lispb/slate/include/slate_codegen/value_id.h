#pragma once

#include <slate_codegen/value_kind.h>

#include <cstdint>
namespace slate_codegen::detail {

struct ValueId {
    inline static constexpr std::uint32_t index_mask{0x00ffffffu};
    inline static constexpr std::uint32_t kind_shift{24};

    std::uint32_t value{};
    [[nodiscard]] auto kind() const -> ValueKind {
        return static_cast<ValueKind>(value >> kind_shift);
    }
    [[nodiscard]] auto index() const -> std::uint32_t { return value & index_mask; }
    explicit operator bool() const { return value != 0; }
};

}
