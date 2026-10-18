#pragma once

#include <slate_codegen/child_kind.h>

#include <cstdint>
namespace slate_codegen::detail {

struct ChildId {
    std::uint32_t value{};
    [[nodiscard]] auto kind() const -> ChildKind { return static_cast<ChildKind>(value >> 24); }
    [[nodiscard]] auto index() const -> std::uint32_t { return value & 0x00ffffffu; }
    explicit operator bool() const { return value != 0; }
};

}
