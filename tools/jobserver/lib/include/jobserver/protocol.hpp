#pragma once

#include "jobserver/types.hpp"

#include <expected>
#include <span>
#include <string>
#include <vector>

namespace jobserver::protocol {
inline constexpr std::uint32_t major_version{4};
inline constexpr std::uint32_t maximum_payload_size{1024U * 1024U};

[[nodiscard]] auto encode_frame(std::string const& payload)
    -> std::expected<std::vector<std::byte>, Error>;
[[nodiscard]] auto decode_header(std::span<std::byte const, 4> header)
    -> std::expected<std::uint32_t, Error>;
}
