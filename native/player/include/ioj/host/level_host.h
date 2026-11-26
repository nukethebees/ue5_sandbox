#pragma once

#include <ioj/levels/launch_options.h>
#include <ioj/session/pending_level.h>
#include <ioj/sim/collision/collision_grid_spec.h>
#include <ioj/world_aabb.h>
#include <ioj/sim/player/sim.h>

#include <optional>
#include <span>
#include <string_view>

namespace ioj::sim {
struct LevelSim;
}
namespace ioj::profiles {
struct CompletedLevel;
}
namespace ioj::host {
#ifdef IOJ_PLAYER_EXPORTS
#define IOJ_LEVEL_API __declspec(dllexport)
#else
#define IOJ_LEVEL_API __declspec(dllimport)
#endif

IOJ_LEVEL_API auto level_world_ready() -> bool;
IOJ_LEVEL_API void open_menu(std::string_view preferred_level_id, std::string_view error);
IOJ_LEVEL_API void open_mission();
IOJ_LEVEL_API auto prepare_level(session::PendingLevel const& pending,
                                 std::optional<sim::player::PlayerSpawnData>& player) -> bool;
IOJ_LEVEL_API auto static_geometry(sim::collision::CollisionGridSpec grid)
    -> std::span<WorldAABB const>;
IOJ_LEVEL_API auto bind_level(session::PendingLevel const& pending, sim::LevelSim const& level)
    -> bool;
IOJ_LEVEL_API void level_start_failed(std::string_view error);
IOJ_LEVEL_API auto presents_level(sim::LevelSim const& level) -> bool;
IOJ_LEVEL_API auto take_frame_delta_seconds() -> std::optional<double>;
IOJ_LEVEL_API void present_level(double delta_seconds);
IOJ_LEVEL_API void simulation_paused();
IOJ_LEVEL_API void finish_level(profiles::CompletedLevel const& completed,
                                bool scored,
                                bool new_best,
                                bool show_results);
#undef IOJ_LEVEL_API
}
