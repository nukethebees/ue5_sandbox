#pragma once

#include <ioj/input/physical_key.h>
#include <ioj/settings/control_binding_address.h>

#include <cstdint>
#include <optional>

namespace ioj {
struct ControlBindingSnapshot {
    ControlBindingAddress address{};
    PhysicalKey current_key{};
    PhysicalKey default_key{};
    std::optional<ControlBindingAddress> chord{};
};
}
