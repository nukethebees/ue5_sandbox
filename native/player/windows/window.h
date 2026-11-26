#pragma once

#include "player/engine.h"
#include "player/windows/system.h"

struct SDL_Window;

namespace ioj {
auto native_handle(SDL_Window* window) -> HWND;
auto connect_unreal_window(SDL_Window* window) -> ApplicationResult;
void disconnect_unreal_window(SDL_Window* window);
}
