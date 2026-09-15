#pragma once

#include "jobserver/types.hpp"

#include <expected>
#include <span>
#include <string>
#include <vector>

namespace jobserver::protocol {
inline constexpr std::uint32_t major_version{1};
inline constexpr std::uint32_t minor_version{1};
inline constexpr std::uint32_t maximum_payload_size{1024U * 1024U};

[[nodiscard]] auto encode_frame(std::string const& payload)
    -> std::expected<std::vector<std::byte>, Error>;
[[nodiscard]] auto decode_header(std::span<std::byte const, 4> header)
    -> std::expected<std::uint32_t, Error>;
[[nodiscard]] auto encode_base64(std::string const& bytes) -> std::string;
[[nodiscard]] auto decode_base64(std::string const& encoded) -> std::expected<std::string, Error>;
}
