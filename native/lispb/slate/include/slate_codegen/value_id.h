#pragma once

#include <slate_codegen/value_kind.h>

#include <cstdint>
namespace slate_codegen::detail {

struct ValueId {
    std::uint32_t value{};
    [[nodiscard]] auto kind() const -> ValueKind { return static_cast<ValueKind>(value >> 24); }
    [[nodiscard]] auto index() const -> std::uint32_t { return value & 0x00ffffffu; }
    explicit operator bool() const { return value != 0; }
};

}
