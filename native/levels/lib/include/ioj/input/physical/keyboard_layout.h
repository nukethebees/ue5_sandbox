#pragma once

#include <ioj/input/physical/physical_key.h>

#include <array>
#include <string>
#include <string_view>
#include <utility>

namespace ioj {
struct KeyboardLayout {
    auto label(PhysicalKey key) const -> std::string_view {
        if (is_keyboard_key(key)) {
            auto const& label{names[std::to_underlying(key)]};
            if (!label.empty()) {
                return label;
            }
        }
        return to_display_string(key);
    }

    std::array<std::string, std::to_underlying(PhysicalKey::RightCommand) + 1> names;
};

}
