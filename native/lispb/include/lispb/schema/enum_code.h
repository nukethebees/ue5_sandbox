#pragma once

#include <cstdint>
#include <string>

namespace lispb::schema {
struct EnumCode {
    bool negative{};
    std::uint64_t magnitude{};

    auto operator==(EnumCode const&) const -> bool = default;
};

auto format_enum_code(EnumCode value) -> std::string;
}
