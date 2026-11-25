#pragma once

#include "../engine.h"
#include "system.h"

struct SDL_Window;

namespace ioj::player {
auto native_handle(SDL_Window* window) -> HWND;
auto connect_unreal_window(SDL_Window* window) -> Result;
void disconnect_unreal_window(SDL_Window* window);
}
