#pragma once

#include <ioj/input/key_code.h>
#include <ioj/settings/control_binding_address.h>

#include <cstdint>
#include <optional>

namespace ioj {
struct ControlBindingSnapshot {
    ControlBindingAddress address{};
    KeyCode current_key{};
    KeyCode default_key{};
    std::optional<ControlBindingAddress> chord{};
};
}
