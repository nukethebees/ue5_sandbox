#pragma once

#include <ioj/input/flight_mapping.h>
#include <ioj/input/physical_key.h>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace ioj {
struct KeyCode {
    constexpr KeyCode() = default;
    constexpr KeyCode(PhysicalKey key, std::uint32_t codepoint = {})
        : key{key}
        , codepoint{codepoint} {}
    constexpr auto operator==(KeyCode const&) const -> bool = default;

    PhysicalKey key{};
    std::uint32_t codepoint{};
};
auto parse_mapping(std::string_view name) -> std::optional<FlightMapping>;
auto character_key(std::uint32_t character) -> KeyCode;
auto parse_key(std::string_view name) -> KeyCode;
auto serialized_key(KeyCode key) -> std::string;
}
