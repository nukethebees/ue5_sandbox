#pragma once

#include <codegen/schema/module_kind.h>

#include <cassert>
#include <compare>
#include <cstdint>

namespace lispb::codegen {

struct ModuleHandle {
    std::uint32_t value{};
    [[nodiscard]] auto kind() const -> ModuleKind { return static_cast<ModuleKind>(value >> 24); }
    [[nodiscard]] auto index() const -> std::uint32_t {
        assert((value & 0x00ffffff) != 0);
        return (value & 0x00ffffff) - 1;
    }
    auto operator<=>(ModuleHandle const&) const = default;
};

} // namespace lispb::codegen
