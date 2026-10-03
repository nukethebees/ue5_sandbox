#pragma once

#include "jobserver/error.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <vector>

namespace jobserver::protocol {
using Version = std::uint32_t;
using PayloadSize = std::uint32_t;
using Frame = std::vector<std::byte>;
using FrameResult = std::expected<Frame, Error>;
using PayloadSizeResult = std::expected<PayloadSize, Error>;

inline constexpr Version major_version{5};
inline constexpr PayloadSize maximum_payload_size{1024U * 1024U};
inline constexpr std::size_t header_size{sizeof(PayloadSize)};

[[nodiscard]] auto encode_frame(std::string const& payload) -> FrameResult;
[[nodiscard]] auto decode_header(std::span<std::byte const, header_size> header)
    -> PayloadSizeResult;
}
