#pragma once

#include <cstdint>
#include <optional>

namespace ioj::layout {

// The containing column determines whether these quantities are bits or bytes.
struct PayloadFootprint {
    auto operator==(PayloadFootprint const&) const -> bool = default;

    std::optional<std::uint64_t> useful{};
    std::optional<std::uint64_t> read{};
    std::optional<std::uint64_t> write{};
    std::optional<std::uint64_t> logical_read{};
    std::optional<std::uint64_t> logical_write{};
};

}
