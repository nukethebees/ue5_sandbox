#include "jobserver/protocol.hpp"

#include <cstring>

namespace jobserver::protocol {
auto encode_frame(std::string const& payload) -> FrameResult {
    if (payload.size() > maximum_payload_size) {
        return FrameResult{std::unexpect, "payload_too_large", "Protocol payload exceeds one MiB"};
    }

    auto const size{static_cast<PayloadSize>(payload.size())};
    FrameResult result{std::in_place, header_size + payload.size()};
    auto& frame{*result};
    for (std::size_t index{}; index < header_size; ++index) {
        frame[index] = static_cast<std::byte>((size >> (index * 8U)) & 0xffU);
    }

    std::memcpy(frame.data() + header_size, payload.data(), payload.size());
    return result;
}

auto decode_header(std::span<std::byte const, header_size> const header) -> PayloadSizeResult {
    PayloadSize size{};
    auto const byte_count{header.size()};
    for (std::size_t index{}; index < byte_count; ++index) {
        size |= std::to_integer<PayloadSize>(header[index]) << (index * 8U);
    }

    if (size > maximum_payload_size) {
        return PayloadSizeResult{
            std::unexpect, "payload_too_large", "Protocol payload exceeds one MiB"};
    }
    return PayloadSizeResult{std::in_place, size};
}

}
