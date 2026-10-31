#pragma once

#include <cstdint>
#include <string>

namespace ioj {
struct ControlBindingIdentity {
    auto operator==(ControlBindingIdentity const&) const -> bool = default;

    std::string mapping_name;
    std::string hardware_device_id;
    std::uint32_t slot{};
    std::uint32_t device_type{};
};
}
