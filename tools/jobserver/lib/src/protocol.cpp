#include "jobserver/protocol.hpp"

#include <algorithm>
#include <array>
#include <cstring>

namespace jobserver::protocol {
auto encode_frame(std::string const& payload) -> std::expected<std::vector<std::byte>, Error> {
    if (payload.size() > maximum_payload_size) {
        return std::unexpected(Error{"payload_too_large", "Protocol payload exceeds one MiB"});
    }

    auto const size{static_cast<std::uint32_t>(payload.size())};
    std::vector<std::byte> frame(sizeof(size) + payload.size());
    frame[0] = static_cast<std::byte>(size & 0xffU);
    frame[1] = static_cast<std::byte>((size >> 8U) & 0xffU);
    frame[2] = static_cast<std::byte>((size >> 16U) & 0xffU);
    frame[3] = static_cast<std::byte>((size >> 24U) & 0xffU);
    std::memcpy(frame.data() + sizeof(size), payload.data(), payload.size());
    return frame;
}

auto decode_header(std::span<std::byte const, 4> const header)
    -> std::expected<std::uint32_t, Error> {
    auto const size{std::to_integer<std::uint32_t>(header[0]) |
                    (std::to_integer<std::uint32_t>(header[1]) << 8U) |
                    (std::to_integer<std::uint32_t>(header[2]) << 16U) |
                    (std::to_integer<std::uint32_t>(header[3]) << 24U)};
    if (size > maximum_payload_size) {
        return std::unexpected(Error{"payload_too_large", "Protocol payload exceeds one MiB"});
    }
    return size;
}

}
