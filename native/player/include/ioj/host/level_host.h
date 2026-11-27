#pragma once

#include <ioj/input/dispatch/events.h>
#include <ioj/presentation/level_frame.h>
#include <ioj/presentation/level_init.h>

#include <string_view>

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
IOJ_LEVEL_API void open_level();
IOJ_LEVEL_API auto presents_level() -> bool;
#undef IOJ_LEVEL_API
}
