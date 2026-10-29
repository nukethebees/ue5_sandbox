#pragma once

#include <cstdint>

namespace ioj::testing {
struct MoveOnlyValue {
    explicit MoveOnlyValue(std::int32_t const initial_value)
        : value{initial_value} {}

    MoveOnlyValue(MoveOnlyValue const&) = delete;
    auto operator=(MoveOnlyValue const&) -> MoveOnlyValue& = delete;

    MoveOnlyValue(MoveOnlyValue&& other) noexcept
        : value{other.value} {
        other.value = -1;
    }

    auto operator=(MoveOnlyValue&& other) noexcept -> MoveOnlyValue& {
        value = other.value;
        other.value = -1;
        return *this;
    }

    std::int32_t value{};
};
}
