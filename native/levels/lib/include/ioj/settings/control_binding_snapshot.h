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
    std::optional<ControlBindingAddress> chord{};
};
}
