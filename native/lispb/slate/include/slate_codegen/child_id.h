#pragma once

#include <slate_codegen/child_kind.h>

#include <cstdint>
namespace slate_codegen::detail {

struct ChildId {
    static constexpr std::uint32_t index_mask{0x00ffffffu};
    static constexpr std::uint32_t kind_shift{24};

    std::uint32_t value{};
    [[nodiscard]] auto kind() const -> ChildKind {
        return static_cast<ChildKind>(value >> kind_shift);
    }
    [[nodiscard]] auto index() const -> std::uint32_t { return value & index_mask; }
    explicit operator bool() const { return value != 0; }
};

}
