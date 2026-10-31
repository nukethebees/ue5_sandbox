#pragma once

#include <ioj/settings/control_binding_address.h>

#include <cstdint>
#include <optional>
#include <string>

namespace ioj {
struct ControlBindingSnapshot {
    ControlBindingAddress address{};
    std::string current_key{};
    std::string default_key{};
    // An absent scope applies in every control context.
    std::optional<std::uint32_t> scope{};
    std::optional<ControlBindingAddress> chord{};
};
}
