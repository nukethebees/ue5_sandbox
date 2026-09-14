#include "jobserver/protocol.hpp"

#include <algorithm>
#include <array>
#include <cstring>

namespace jobserver::protocol {
namespace {
inline constexpr char base64_alphabet[]{
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"};

auto base64_value(char const character) -> int {
    auto const* found{
        std::find(std::begin(base64_alphabet), std::end(base64_alphabet) - 1, character)};
    return found == std::end(base64_alphabet) - 1
             ? -1
             : static_cast<int>(found - std::begin(base64_alphabet));
}
}

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

auto encode_base64(std::string const& bytes) -> std::string {
    std::string result;
    result.reserve(((bytes.size() + 2) / 3) * 4);
    for (std::size_t index{}; index < bytes.size(); index += 3) {
        auto const first{static_cast<unsigned char>(bytes[index])};
        auto const second{index + 1 < bytes.size() ? static_cast<unsigned char>(bytes[index + 1])
                                                   : static_cast<unsigned char>(0)};
        auto const third{index + 2 < bytes.size() ? static_cast<unsigned char>(bytes[index + 2])
                                                  : static_cast<unsigned char>(0)};
        auto const value{(static_cast<std::uint32_t>(first) << 16U) |
                         (static_cast<std::uint32_t>(second) << 8U) |
                         static_cast<std::uint32_t>(third)};
        result.push_back(base64_alphabet[(value >> 18U) & 0x3fU]);
        result.push_back(base64_alphabet[(value >> 12U) & 0x3fU]);
        result.push_back(index + 1 < bytes.size() ? base64_alphabet[(value >> 6U) & 0x3fU] : '=');
        result.push_back(index + 2 < bytes.size() ? base64_alphabet[value & 0x3fU] : '=');
    }
    return result;
}

auto decode_base64(std::string const& encoded) -> std::expected<std::string, Error> {
    if (encoded.size() % 4 != 0) {
        return std::unexpected(Error{"invalid_base64", "Output payload has invalid base64 length"});
    }
    std::string result;
    result.reserve((encoded.size() / 4) * 3);
    for (std::size_t index{}; index < encoded.size(); index += 4) {
        auto const third_padding{encoded[index + 2] == '='};
        auto const fourth_padding{encoded[index + 3] == '='};
        if ((third_padding && !fourth_padding) ||
            ((third_padding || fourth_padding) && index + 4 != encoded.size())) {
            return std::unexpected(
                Error{"invalid_base64", "Output payload contains invalid base64 padding"});
        }
        auto const first{base64_value(encoded[index])};
        auto const second{base64_value(encoded[index + 1])};
        auto const third{third_padding ? 0 : base64_value(encoded[index + 2])};
        auto const fourth{fourth_padding ? 0 : base64_value(encoded[index + 3])};
        if (first < 0 || second < 0 || third < 0 || fourth < 0) {
            return std::unexpected(
                Error{"invalid_base64", "Output payload contains invalid base64"});
        }
        if ((third_padding && (second & 0x0f) != 0) ||
            (fourth_padding && !third_padding && (third & 0x03) != 0)) {
            return std::unexpected(
                Error{"invalid_base64", "Output payload contains non-zero padding bits"});
        }
        auto const value{(static_cast<std::uint32_t>(first) << 18U) |
                         (static_cast<std::uint32_t>(second) << 12U) |
                         (static_cast<std::uint32_t>(third) << 6U) |
                         static_cast<std::uint32_t>(fourth)};
        result.push_back(static_cast<char>((value >> 16U) & 0xffU));
        if (!third_padding) {
            result.push_back(static_cast<char>((value >> 8U) & 0xffU));
        }
        if (!fourth_padding) {
            result.push_back(static_cast<char>(value & 0xffU));
        }
    }
    return result;
}
}
