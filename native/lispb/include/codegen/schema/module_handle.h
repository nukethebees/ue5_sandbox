#pragma once

#include <codegen/schema/module_kind.h>

#include <cassert>
#include <compare>
#include <cstdint>

namespace lispb::codegen {

struct ModuleHandle {
    inline static constexpr std::uint32_t index_mask{0x00ffffffu};
    inline static constexpr std::uint32_t kind_shift{24};

    std::uint32_t value{};
    [[nodiscard]] auto kind() const -> ModuleKind {
        return static_cast<ModuleKind>(value >> kind_shift);
    }
    [[nodiscard]] auto index() const -> std::uint32_t {
        assert((value & index_mask) != 0);
        return (value & index_mask) - 1;
    }
    auto operator<=>(ModuleHandle const&) const = default;
};

} // namespace lispb::codegen
