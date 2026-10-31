#pragma once

namespace ioj::profiles {
struct ProfileDebugSettings {
    auto operator==(ProfileDebugSettings const&) const -> bool = default;

    bool unlock_all_missions{};
    bool start_levels_paused{};
};
}
