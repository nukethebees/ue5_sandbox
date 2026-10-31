#pragma once

#include <cstdint>
#include <string>

namespace ioj {
struct ControlBindingAddress {
    auto operator==(ControlBindingAddress const&) const -> bool = default;

    std::string profile_id;
    std::string mapping_name;
    std::string hardware_device_id;
    std::uint32_t slot{};
};
}
