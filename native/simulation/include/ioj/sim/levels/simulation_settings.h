#pragma once

#include <ioj/levels/diagnostics.h>
#include <ioj/settings/settings.h>
namespace ioj::sim {
struct LevelSimInitData;
namespace player {
struct PlayerSpawnData;
}

// Validate the complete result after all default and level overrides have been applied.
[[nodiscard]] auto validate_settings(settings::Settings const& settings)
    -> std::expected<void, levels::Diagnostics>;

// Apply the effective authored tuning and entity geometry.
void apply_settings(settings::Settings const& settings, LevelSimInitData& target);
void apply_settings(settings::Settings const& settings, player::PlayerSpawnData& target);
}
