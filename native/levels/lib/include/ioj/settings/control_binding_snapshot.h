#pragma once

#include <ioj/input/key_code.h>
#include <ioj/settings/control_binding_address.h>

#include <cstdint>
#include <optional>

namespace ioj {
struct ControlBindingSnapshot {
    ControlBindingAddress address{};
    input::KeyCode current_key{};
    input::KeyCode default_key{};
    std::optional<ControlBindingAddress> chord{};
};
}
